#!/usr/bin/env python3
"""Generate deterministic dependency evidence for an ADTimePix3 build."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit


DEPENDENCIES = {
    "EPICS_BASE": {
        "name": "EPICS Base",
        "version_file": "configure/CONFIG_BASE_VERSION",
        "version_macros": (
            "EPICS_VERSION",
            "EPICS_REVISION",
            "EPICS_MODIFICATION",
            "EPICS_PATCH_LEVEL",
            "EPICS_DEV_SNAPSHOT",
        ),
    },
    "ASYN": {
        "name": "asyn",
        "version_file": "asyn/asynDriver/asynDriver.h",
        "version_macros": ("ASYN_VERSION", "ASYN_REVISION", "ASYN_MODIFICATION"),
    },
    "ADCORE": {
        "name": "ADCore",
        "version_file": "ADApp/ADSrc/ADCoreVersion.h",
        "version_macros": ("ADCORE_VERSION", "ADCORE_REVISION", "ADCORE_MODIFICATION"),
    },
}

BUNDLED_DEPENDENCIES = (
    {
        "name": "cpr",
        "version_file": "tpx3Support/cpr/cpr/cprver.h",
        "version_macros": ("CPR_VERSION_MAJOR", "CPR_VERSION_MINOR", "CPR_VERSION_PATCH"),
    },
    {
        "name": "nlohmann/json",
        "version_file": "tpx3Support/json/json_fwd.hpp",
        "version_macros": (
            "NLOHMANN_JSON_VERSION_MAJOR",
            "NLOHMANN_JSON_VERSION_MINOR",
            "NLOHMANN_JSON_VERSION_PATCH",
        ),
    },
)


def run_git(repo, *args):
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip()


def normalize_repository_url(url):
    """Remove credentials and presentation-only suffixes from a Git URL."""
    url = url.strip()
    match = re.fullmatch(r"(?:[^@]+@)?([^:]+):(.+)", url)
    if match and "://" not in url:
        url = "https://{}/{}".format(match.group(1), match.group(2))
    elif "://" in url:
        parts = urlsplit(url)
        hostname = parts.hostname or ""
        if parts.port:
            hostname += ":{}".format(parts.port)
        url = urlunsplit((parts.scheme, hostname, parts.path, "", ""))
    url = url.rstrip("/")
    return url[:-4] if url.endswith(".git") else url


def repository_url(repo, preferred_remote="origin"):
    remotes = run_git(repo, "remote").splitlines()
    remote = preferred_remote if preferred_remote in remotes else "origin"
    if remote not in remotes:
        return "unknown"
    return normalize_repository_url(run_git(repo, "remote", "get-url", remote))


def parse_assignments(path):
    assignments = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(?::?=)\s*(.*?)\s*$", line)
        if match:
            assignments[match.group(1)] = match.group(2).strip().strip('"\'')

    macro = re.compile(r"\$\(([^)]+)\)|\${([^}]+)}")
    for _ in range(len(assignments) + 1):
        changed = False
        for name, value in tuple(assignments.items()):
            expanded = macro.sub(
                lambda item: assignments.get(item.group(1) or item.group(2), item.group(0)),
                value,
            )
            if expanded != value:
                assignments[name] = expanded
                changed = True
        if not changed:
            break
    return assignments


def parse_macro_values(path, names):
    text = path.read_text(encoding="utf-8")
    values = {}
    for name in names:
        patterns = (
            r"^\s*#\s*define\s+{}\s+([^\s/]+)".format(re.escape(name)),
            r"^\s*{}\s*(?::?=)\s*(.*?)\s*$".format(re.escape(name)),
        )
        for pattern in patterns:
            match = re.search(pattern, text, re.MULTILINE)
            if match:
                values[name] = match.group(1).strip().strip('"\'')
                break
        if name not in values:
            values[name] = ""
    return values


def dotted_version(values, names):
    return ".".join(values[name] for name in names)


def dependency_version(key, values):
    if key == "EPICS_BASE":
        parts = [
            values["EPICS_VERSION"],
            values["EPICS_REVISION"],
            values["EPICS_MODIFICATION"],
        ]
        if values["EPICS_PATCH_LEVEL"] not in ("", "0"):
            parts.append(values["EPICS_PATCH_LEVEL"])
        return ".".join(parts) + values["EPICS_DEV_SNAPSHOT"]
    return dotted_version(values, DEPENDENCIES[key]["version_macros"])


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_selected_ref(repo, selected_ref):
    try:
        refs = [run_git(repo, "symbolic-ref", "--quiet", "--short", "HEAD")]
    except subprocess.CalledProcessError:
        refs = run_git(repo, "tag", "--points-at", "HEAD").splitlines()
    if selected_ref not in refs:
        actual = ", ".join(refs) if refs else "detached commit without an exact tag"
        raise ValueError(
            "expected ref {!r} is not checked out in {} (actual: {})".format(
                selected_ref, repo, actual
            )
        )


def git_component(name, repo, selected_ref=None, preferred_remote="origin"):
    component = {
        "name": name,
        "repository": repository_url(repo, preferred_remote),
        "commit": run_git(repo, "rev-parse", "HEAD"),
    }
    if selected_ref is not None:
        component["selected_ref"] = selected_ref
    return component


def build_report(repository_root, release_file, selected_refs):
    root = repository_root.resolve()
    release_values = parse_assignments(release_file.resolve())

    source_dependencies = []
    for key, definition in DEPENDENCIES.items():
        if key not in release_values:
            raise ValueError("{} is missing from {}".format(key, release_file))
        if key not in selected_refs:
            raise ValueError("selected ref is missing for {}".format(key))
        dependency_root = Path(release_values[key]).resolve()
        verify_selected_ref(dependency_root, selected_refs[key])
        version_path = dependency_root / definition["version_file"]
        values = parse_macro_values(version_path, definition["version_macros"])
        component = git_component(definition["name"], dependency_root, selected_refs[key])
        component["version"] = dependency_version(key, values)
        component["version_evidence"] = definition["version_file"]
        source_dependencies.append(component)

    bundled_dependencies = []
    for definition in BUNDLED_DEPENDENCIES:
        version_path = root / definition["version_file"]
        values = parse_macro_values(version_path, definition["version_macros"])
        bundled_dependencies.append(
            {
                "name": definition["name"],
                "version": dotted_version(values, definition["version_macros"]),
                "version_evidence": definition["version_file"],
                "sha256": sha256(version_path),
            }
        )

    project_version_file = "tpx3App/src/ADTimePix.h"
    project_version_macros = (
        "ADTIMEPIX_VERSION",
        "ADTIMEPIX_REVISION",
        "ADTIMEPIX_MODIFICATION",
    )
    project_values = parse_macro_values(root / project_version_file, project_version_macros)
    project = git_component("ADTimePix3", root, preferred_remote="upstream")
    project["version"] = dotted_version(project_values, project_version_macros)
    project["version_evidence"] = project_version_file

    hook_path = root / ".ci-local/adcore-config.sh"
    return {
        "schema_version": 1,
        "project": project,
        "source_dependencies": source_dependencies,
        "bundled_dependencies": bundled_dependencies,
        "build_inputs": [
            git_component("epics-base/ci-scripts", root / ".ci"),
            {
                "name": "ADCore CI configuration hook",
                "path": ".ci-local/adcore-config.sh",
                "sha256": sha256(hook_path),
            },
        ],
    }


def markdown_escape(value):
    return str(value).replace("|", "\\|")


def render_markdown(report):
    project = report["project"]
    lines = [
        "# ADTimePix3 dependency/version evidence",
        "",
        "This file is generated deterministically from the checked-out build inputs.",
        "It intentionally contains no generation timestamp or local filesystem paths.",
        "",
        "## Project",
        "",
        "| Repository | Version | Commit | Version evidence |",
        "|---|---|---|---|",
        "| {} | `{}` | `{}` | `{}` |".format(
            markdown_escape(project["repository"]),
            markdown_escape(project["version"]),
            markdown_escape(project["commit"]),
            markdown_escape(project["version_evidence"]),
        ),
        "",
        "## Source dependencies",
        "",
        "| Dependency | Selected ref | Version | Commit | Repository | Version evidence |",
        "|---|---|---|---|---|---|",
    ]
    for item in report["source_dependencies"]:
        lines.append(
            "| {} | `{}` | `{}` | `{}` | {} | `{}` |".format(
                *(
                    markdown_escape(item[key])
                    for key in (
                        "name",
                        "selected_ref",
                        "version",
                        "commit",
                        "repository",
                        "version_evidence",
                    )
                )
            )
        )

    lines.extend(
        [
            "",
            "## Bundled dependencies",
            "",
            "| Dependency | Version | Version evidence | SHA-256 |",
            "|---|---|---|---|",
        ]
    )
    for item in report["bundled_dependencies"]:
        lines.append(
            "| {} | `{}` | `{}` | `{}` |".format(
                *(
                    markdown_escape(item[key])
                    for key in ("name", "version", "version_evidence", "sha256")
                )
            )
        )

    lines.extend(["", "## Build inputs", ""])
    for item in report["build_inputs"]:
        if "commit" in item:
            lines.append(
                "- {}: `{}` ({})".format(
                    markdown_escape(item["name"]),
                    markdown_escape(item["commit"]),
                    markdown_escape(item["repository"]),
                )
            )
        else:
            lines.append(
                "- {}: `{}` SHA-256 `{}`".format(
                    markdown_escape(item["name"]),
                    markdown_escape(item["path"]),
                    markdown_escape(item["sha256"]),
                )
            )
    lines.append("")
    return "\n".join(lines)


def parse_selected_refs(items):
    result = {}
    for item in items:
        if "=" not in item:
            raise ValueError("selected ref must use NAME=REF: {}".format(item))
        name, value = item.split("=", 1)
        if name not in DEPENDENCIES or not value:
            raise ValueError("invalid selected ref: {}".format(item))
        result[name] = value
    return result


def write_report(report, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "dependency-versions.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_dir / "dependency-versions.md").write_text(
        render_markdown(report), encoding="utf-8"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--release-file", type=Path, default=Path("configure/RELEASE.local"))
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--selected-ref", action="append", default=[], metavar="NAME=REF")
    args = parser.parse_args()

    report = build_report(
        args.repository_root, args.release_file, parse_selected_refs(args.selected_ref)
    )
    write_report(report, args.output_dir)
    print("Wrote deterministic dependency evidence to {}".format(args.output_dir))


if __name__ == "__main__":
    main()
