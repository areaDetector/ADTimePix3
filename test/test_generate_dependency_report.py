#!/usr/bin/env python3
"""Tests for generate_dependency_report.py."""

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

import generate_dependency_report as report


def run(*args, cwd):
    subprocess.run(args, cwd=cwd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def make_repo(path, files, remote, branch="master"):
    path.mkdir(parents=True)
    for relative, contents in files.items():
        destination = path / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(contents, encoding="utf-8")
    run("git", "init", "-q", "-b", branch, cwd=path)
    run("git", "config", "user.name", "Dependency Report Test", cwd=path)
    run("git", "config", "user.email", "dependency-report@example.invalid", cwd=path)
    run("git", "remote", "add", "origin", remote, cwd=path)
    run("git", "add", ".", cwd=path)
    run("git", "commit", "-q", "-m", "fixture", cwd=path)
    return path


class DependencyReportTests(unittest.TestCase):
    def test_report_is_deterministic_and_contains_no_local_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            root = make_repo(
                workspace / "driver",
                {
                    "tpx3Support/cpr/cpr/cprver.h": """
#define CPR_VERSION_MAJOR 1
#define CPR_VERSION_MINOR 14
#define CPR_VERSION_PATCH 2
""",
                    "tpx3Support/json/json_fwd.hpp": """
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 12
#define NLOHMANN_JSON_VERSION_PATCH 0
""",
                    ".ci-local/adcore-config.sh": "#!/bin/sh\nexit 0\n",
                    "tpx3App/src/ADTimePix.h": """
#define ADTIMEPIX_VERSION 1
#define ADTIMEPIX_REVISION 8
#define ADTIMEPIX_MODIFICATION 0
""",
                },
                "https://github.com/example/fork.git",
            )
            run(
                "git",
                "remote",
                "add",
                "upstream",
                "https://github.com/areaDetector/ADTimePix3.git",
                cwd=root,
            )
            make_repo(root / ".ci", {"README": "fixture\n"}, "git@github.com:epics-base/ci-scripts.git")
            make_repo(
                workspace / "base-7.0",
                {
                    "configure/CONFIG_BASE_VERSION": """
EPICS_VERSION = 7
EPICS_REVISION = 0
EPICS_MODIFICATION = 10
EPICS_PATCH_LEVEL = 1
EPICS_DEV_SNAPSHOT = -DEV
"""
                },
                "https://github.com/epics-base/epics-base.git",
                branch="7.0",
            )
            make_repo(
                workspace / "asyn-master",
                {
                    "asyn/asynDriver/asynDriver.h": """
#define ASYN_VERSION 4
#define ASYN_REVISION 45
#define ASYN_MODIFICATION 0
"""
                },
                "https://github.com/epics-modules/asyn.git",
            )
            adcore = make_repo(
                workspace / "adcore-master",
                {
                    "ADApp/ADSrc/ADCoreVersion.h": """
#define ADCORE_VERSION 3
#define ADCORE_REVISION 14
#define ADCORE_MODIFICATION 0
"""
                },
                "https://user:secret@github.com/areaDetector/ADCore.git?token=secret",
            )
            release = workspace / "RELEASE.local"
            release.write_text(
                "ROOT={}\nEPICS_BASE=$(ROOT)/base-7.0\nASYN=${{ROOT}}/asyn-master\nADCORE={}\n".format(
                    workspace, adcore
                ),
                encoding="utf-8",
            )
            selected = {"EPICS_BASE": "7.0", "ASYN": "master", "ADCORE": "master"}

            first = report.build_report(root, release, selected)
            second = report.build_report(root, release, selected)
            first_json = json.dumps(first, indent=2, sort_keys=True) + "\n"
            second_json = json.dumps(second, indent=2, sort_keys=True) + "\n"
            markdown = report.render_markdown(first)
            first_output = workspace / "first-output"
            second_output = workspace / "second-output"
            report.write_report(first, first_output)
            report.write_report(second, second_output)

            self.assertEqual(first_json, second_json)
            self.assertEqual(first["project"]["repository"], "https://github.com/areaDetector/ADTimePix3")
            self.assertEqual(first["project"]["version"], "1.8.0")
            self.assertEqual(first["source_dependencies"][0]["version"], "7.0.10.1-DEV")
            self.assertEqual(first["source_dependencies"][1]["version"], "4.45.0")
            self.assertEqual(first["bundled_dependencies"][0]["version"], "1.14.2")
            self.assertNotIn(str(workspace), first_json)
            self.assertNotIn("secret", first_json)
            self.assertIn("ADTimePix3 dependency/version evidence", markdown)
            for name in ("dependency-versions.json", "dependency-versions.md"):
                self.assertEqual(
                    (first_output / name).read_bytes(), (second_output / name).read_bytes()
                )
            mismatched = dict(selected)
            mismatched["ASYN"] = "R4-44"
            with self.assertRaisesRegex(ValueError, "expected ref 'R4-44'"):
                report.build_report(root, release, mismatched)

    def test_selected_ref_validation(self):
        self.assertEqual(report.parse_selected_refs(["EPICS_BASE=7.0"]), {"EPICS_BASE": "7.0"})
        with self.assertRaises(ValueError):
            report.parse_selected_refs(["invalid"])
        with self.assertRaises(ValueError):
            report.parse_selected_refs(["UNKNOWN=master"])

    def test_report_requires_every_selected_ref(self):
        with tempfile.TemporaryDirectory() as temporary:
            release = Path(temporary) / "RELEASE.local"
            release.write_text("EPICS_BASE=/not/used\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "selected ref is missing for EPICS_BASE"):
                report.build_report(Path("."), release, {})


if __name__ == "__main__":
    unittest.main()
