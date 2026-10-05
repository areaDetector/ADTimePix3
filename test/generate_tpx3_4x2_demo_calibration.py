#!/usr/bin/env python3
"""Generate or verify the synthetic TPX3 4x2 demo BPC/DACS pair.

The eight-chip fixture repeats the four per-chip BPC blocks and DACS values
from vendor/tpx3/2x2. It is suitable for deterministic software and emulator
tests, not as detector-specific equalization data.
"""

# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = REPO_ROOT / "vendor" / "tpx3" / "2x2"
OUTPUT_DIR = REPO_ROOT / "vendor" / "tpx3" / "4x2"
SOURCE_BPC = SOURCE_DIR / "tpx3-demo.bpc"
SOURCE_DACS = SOURCE_DIR / "tpx3-demo.dacs"
OUTPUT_BPC = OUTPUT_DIR / "tpx3-demo-4x2-synthetic.bpc"
OUTPUT_DACS = OUTPUT_DIR / "tpx3-demo-4x2-synthetic.dacs"
CHIP_BYTES = 256 * 256
EXPECTED_SOURCE_BPC_SHA256 = (
    "7c065b252dce0fc56aa991957eadaebfadb8c9e068072b17530dbb3775ef338b"
)
EXPECTED_SOURCE_DACS_SHA256 = (
    "df3fba01f99d4080df518c6953ec4ebdfbf2974a6cb9a92d41adaf16687969a1"
)
EXPECTED_OUTPUT_BPC_SHA256 = (
    "6cd7f48d3833dc29934e0048ed611f4665efceed68c543dcf762a762277d474c"
)
EXPECTED_OUTPUT_DACS_SHA256 = (
    "00f08da4d06a876bf5d7d6286fd618da3d90810eeb552c29ba65e1c5f40aeec9"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require_sha256(path: Path, data: bytes, expected: str) -> None:
    actual = sha256(data)
    if actual != expected:
        raise ValueError(
            f"{path.relative_to(REPO_ROOT)} sha256 is {actual}; expected {expected}"
        )


def compose_bpc() -> bytes:
    source = SOURCE_BPC.read_bytes()
    require_sha256(SOURCE_BPC, source, EXPECTED_SOURCE_BPC_SHA256)
    expected_size = 4 * CHIP_BYTES
    if len(source) != expected_size:
        raise ValueError(
            f"{SOURCE_BPC.relative_to(REPO_ROOT)} is {len(source)} bytes; "
            f"expected {expected_size}"
        )
    return source + source


def compose_dacs() -> bytes:
    source_bytes = SOURCE_DACS.read_bytes()
    require_sha256(SOURCE_DACS, source_bytes, EXPECTED_SOURCE_DACS_SHA256)
    source = source_bytes.decode("utf-8")
    headings = re.findall(r"^\[Chip(\d+)\]$", source, flags=re.MULTILINE)
    if headings != ["0", "1", "2", "3"]:
        raise ValueError(
            f"{SOURCE_DACS.relative_to(REPO_ROOT)} sections are {headings}; "
            "expected Chip0 through Chip3 exactly once and in order"
        )
    second_quad = re.sub(
        r"^\[Chip([0-3])\]$",
        lambda match: f"[Chip{int(match.group(1)) + 4}]",
        source,
        flags=re.MULTILINE,
    )
    result = (source + second_quad).encode("utf-8")
    result_headings = re.findall(
        rb"^\[Chip(\d+)\]$", result, flags=re.MULTILINE
    )
    if result_headings != [str(index).encode() for index in range(8)]:
        raise ValueError("generated DACS sections are not Chip0 through Chip7")
    return result


def verify_file(path: Path, expected: bytes) -> bool:
    if not path.is_file():
        print(f"ERROR: missing {path.relative_to(REPO_ROOT)}", file=sys.stderr)
        return False
    actual = path.read_bytes()
    if actual != expected:
        print(
            f"ERROR: {path.relative_to(REPO_ROOT)} differs from deterministic output",
            file=sys.stderr,
        )
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify checked-in outputs without changing them",
    )
    args = parser.parse_args()

    try:
        bpc = compose_bpc()
        dacs = compose_dacs()
        require_sha256(OUTPUT_BPC, bpc, EXPECTED_OUTPUT_BPC_SHA256)
        require_sha256(OUTPUT_DACS, dacs, EXPECTED_OUTPUT_DACS_SHA256)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    if args.check:
        if not verify_file(OUTPUT_BPC, bpc) or not verify_file(OUTPUT_DACS, dacs):
            return 1
        action = "Validated"
    else:
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        OUTPUT_BPC.write_bytes(bpc)
        OUTPUT_DACS.write_bytes(dacs)
        action = "Generated"

    print(
        f"{action} TPX3 4x2 demo calibration: "
        f"BPC={len(bpc)} bytes sha256={sha256(bpc)}, "
        f"DACS={len(dacs)} bytes sha256={sha256(dacs)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
