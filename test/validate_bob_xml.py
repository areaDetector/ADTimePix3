#!/usr/bin/env python3
"""Validate every repository Phoebus BOB file as XML."""

from pathlib import Path
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
BOB_ROOT = ROOT / "tpx3App" / "op"


def main() -> int:
    files = sorted(BOB_ROOT.rglob("*.bob"))
    if not files:
        print(f"ERROR: no BOB files found below {BOB_ROOT.relative_to(ROOT)}", file=sys.stderr)
        return 1

    failures = []
    for path in files:
        try:
            ET.parse(path)
        except (ET.ParseError, OSError) as exc:
            failures.append((path.relative_to(ROOT), exc))

    if failures:
        for path, exc in failures:
            print(f"ERROR: {path}: {exc}", file=sys.stderr)
        return 1

    print(f"Validated {len(files)} Phoebus BOB files as XML.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
