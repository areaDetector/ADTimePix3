#!/usr/bin/env python3
"""Static regression checks for ADTimePix3 service-prefix inheritance."""

from pathlib import Path
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
BOB_ROOT = ROOT / "tpx3App" / "op" / "bob"
LEGACY_PREFIXES = ("SERVAL-TEST:", "EMU-TEST:")

CHILD_SCREENS = {
    BOB_ROOT / "Serval/tpx3serval.bob": ("TPX3-TEST:", "Serval:"),
    BOB_ROOT / "Emulator/emulator.bob": ("TPX3-TEST:", "Emulator:"),
    BOB_ROOT / "Emulator/emulator_tpx3_embed.bob": ("TPX3-TEST:", "Emulator:"),
    BOB_ROOT / "Emulator/emulator_mpx3_embed.bob": ("MPX3-TEST:", "Emulator:"),
}

ENTRY_SCREENS = {
    BOB_ROOT / "TimePix3.bob": ("TPX3-TEST:", "profiles/tpx3/main.bob"),
    BOB_ROOT / "MediPix3.bob": ("MPX3-TEST:", "profiles/mpx3/main.bob"),
    BOB_ROOT / "MediPix3/MediPix3.bob": ("MPX3-TEST:", "../profiles/mpx3/main.bob"),
}

LAUNCHERS = {
    BOB_ROOT / "profiles/tpx3/TimePix3Status.bob": "TPX3-TEST:",
    BOB_ROOT / "profiles/tpx3/Acquire/DetectorConfig.bob": "TPX3-TEST:",
    BOB_ROOT / "profiles/mpx3/Mpx3Status.bob": "MPX3-TEST:",
    BOB_ROOT / "profiles/mpx3/Detector/Mpx3DetectorConfig.bob": "MPX3-TEST:",
}

TARGETS = {
    "emulator.bob": "Emulator:",
    "tpx3serval.bob": "Serval:",
}

STANDALONE_DEFAULT_SCREENS = {
    BOB_ROOT / "Events/tpx3RawStream.bob",
}


def relative(path: Path) -> str:
    return str(path.relative_to(ROOT))


def main() -> int:
    errors: list[str] = []
    parsed: dict[Path, ET.Element] = {}
    allowed_default_screens = (
        set(CHILD_SCREENS)
        | set(ENTRY_SCREENS)
        | set(LAUNCHERS)
        | STANDALONE_DEFAULT_SCREENS
    )

    for path in sorted(BOB_ROOT.rglob("*.bob")):
        text = path.read_text(encoding="utf-8")
        for prefix in LEGACY_PREFIXES:
            if prefix in text:
                errors.append(f"{relative(path)}: obsolete prefix {prefix}")
        if "$(P=" in text and path not in allowed_default_screens:
            errors.append(
                f"{relative(path)}: P fallback belongs at a display boundary, "
                "not in an ordinary embedded panel"
            )
        try:
            parsed[path] = ET.fromstring(text)
        except ET.ParseError as exc:
            errors.append(f"{relative(path)}: invalid XML: {exc}")

    for path, (fallback, service_r) in CHILD_SCREENS.items():
        root = parsed.get(path)
        if root is None:
            errors.append(f"{relative(path)}: child screen was not parsed")
            continue

        if root.find("./macros/P") is not None:
            errors.append(f"{relative(path)}: display-level P overrides callers")
        if root.findtext("./macros/R") != service_r:
            errors.append(f"{relative(path)}: expected display-level R={service_r}")

        expected = f"$(P={fallback})$(R)"
        pv_names = [node.text or "" for node in root.iter("pv_name")]
        if not any(expected in value for value in pv_names):
            errors.append(f"{relative(path)}: missing fallback {fallback}")
        for value in pv_names:
            if "$(P" in value and expected not in value:
                errors.append(
                    f"{relative(path)}: unexpected P expression in PV {value!r}"
                )

    for path, (fallback, profile_file) in ENTRY_SCREENS.items():
        root = parsed.get(path)
        if root is None:
            errors.append(f"{relative(path)}: entry screen was not parsed")
            continue

        if root.find("./macros/P") is not None or root.find("./macros/R") is not None:
            errors.append(f"{relative(path)}: display-level P/R overrides callers")

        embedded = next(
            (
                widget
                for widget in root.findall("./widget[@type='embedded']")
                if widget.findtext("file") == profile_file
            ),
            None,
        )
        if embedded is None:
            errors.append(f"{relative(path)}: missing embedded profile {profile_file}")
            continue

        expected_p = f"$(P={fallback})"
        actual_p = embedded.findtext("macros/P")
        actual_r = embedded.findtext("macros/R")
        if actual_p != expected_p:
            errors.append(
                f"{relative(path)}: embedded profile passes P={actual_p!r}, "
                f"expected {expected_p!r}"
            )
        if actual_r != "$(R=cam1:)":
            errors.append(
                f"{relative(path)}: embedded profile passes R={actual_r!r}, "
                "expected '$(R=cam1:)'"
            )

    action_count = 0
    for path, root in parsed.items():
        for action in root.findall(".//action[@type='open_display']"):
            target_file = action.findtext("file") or ""
            matching_target = next(
                (target for target in TARGETS if target_file.endswith(target)), None
            )
            if matching_target is None:
                continue

            action_count += 1
            fallback = LAUNCHERS.get(path)
            if fallback is None:
                errors.append(
                    f"{relative(path)}: unclassified launcher action for {target_file}"
                )
                continue

            expected_p = f"$(P={fallback})"
            expected_r = TARGETS[matching_target]
            actual_p = action.findtext("macros/P")
            actual_r = action.findtext("macros/R")
            if actual_p != expected_p:
                errors.append(
                    f"{relative(path)}: {target_file} passes P={actual_p!r}, "
                    f"expected {expected_p!r}"
                )
            if actual_r != expected_r:
                errors.append(
                    f"{relative(path)}: {target_file} passes R={actual_r!r}, "
                    f"expected {expected_r!r}"
                )

    if action_count != 8:
        errors.append(f"expected 8 Serval/emulator launcher actions, found {action_count}")

    if errors:
        print("ADTimePix3 screen checks failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        f"Validated {len(parsed)} BOB files, {len(ENTRY_SCREENS)} entry screens, "
        f"{len(CHILD_SCREENS)} child screens, and {action_count} "
        "Serval/emulator launcher actions."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
