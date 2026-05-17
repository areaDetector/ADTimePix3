#!/usr/bin/env python3
"""
Verify coordinate_map_vectors.json against reference implementations of
pelIndex() and bpc2ImgIndex() from tpx3App/src/mask_io.cpp.

Does not require EPICS. Run from repo root:
  python3 test/verify_coordinate_map.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
VECTORS_PATH = Path(__file__).resolve().parent / "coordinate_map_vectors.json"


def chip_from_bpc(bpc_index: int, pel_width: int) -> tuple[int, int, int]:
    """chip id and local (lx, ly) from linear BPC index."""
    count = pel_width * pel_width
    chip = bpc_index // count if count > 0 else 0
    local = bpc_index - chip * count
    return chip, local % pel_width, local // pel_width


def image_cols(num_chips: int, pel_width: int, x_chips: int) -> int:
    """Column stride used to decode bpc2ImgIndex linear index (matches mask_io.cpp)."""
    w = pel_width
    if num_chips == 1:
        return w
    if num_chips == 4:
        return 2 * w
    if num_chips == 8:
        return x_chips * w
    y_chips = num_chips // x_chips
    return y_chips * w


def bpc2img_index(
    bpc_index: int, pel_width: int, num_chips: int, x_chips: int, orientation: int
) -> int:
    """Mirror ADTimePix::bpc2ImgIndex; returns linear image index or -1."""
    w = pel_width
    chip_pel_count = w * w
    chip = bpc_index // chip_pel_count if chip_pel_count > 0 else 0
    i = j = 0

    if num_chips == 1:
        if orientation == 0:
            i = bpc_index % w
            j = w - 1 - bpc_index // w
        elif orientation == 1:
            i = bpc_index // w
            j = bpc_index % w
        elif orientation == 2:
            i = w - 1 - bpc_index % w
            j = bpc_index // w
        elif orientation == 3:
            i = w - 1 - bpc_index // w
            j = w - 1 - bpc_index % w
        elif orientation == 4:
            i = w - 1 - bpc_index % w
            j = w - 1 - bpc_index // w
        elif orientation == 5:
            i = w - 1 - bpc_index // w
            j = bpc_index % w
        elif orientation == 6:
            i = bpc_index % w
            j = bpc_index // w
        elif orientation == 7:
            i = bpc_index // w
            j = w - 1 - bpc_index % w
        else:
            return -1
        return i + w * j

    if num_chips == 4:
        if orientation == 0:
            if chip == 0:
                i = w + bpc_index % w
                j = 2 * w - 1 - bpc_index // w
            elif chip == 1:
                i = 2 * w - 1 - (bpc_index - chip_pel_count) % w
                j = (bpc_index - chip_pel_count) // w
            elif chip == 2:
                i = w - 1 - (bpc_index - 2 * chip_pel_count) % w
                j = (bpc_index - 2 * chip_pel_count) // w
            elif chip == 3:
                i = (bpc_index - 3 * chip_pel_count) % w
                j = 2 * w - 1 - (bpc_index - 3 * chip_pel_count) // w
            else:
                return -1
        elif orientation == 3:
            if chip == 3:
                i = 2 * w - 1 - (bpc_index - 3 * chip_pel_count) // w
                j = 2 * w - 1 - (bpc_index - 3 * chip_pel_count) % w
            elif chip == 0:
                i = 2 * w - 1 - bpc_index // w
                j = bpc_index % w
            elif chip == 1:
                i = (bpc_index - chip_pel_count) // w
                j = (bpc_index - chip_pel_count) % w
            elif chip == 2:
                i = (bpc_index - 2 * chip_pel_count) // w
                j = w + (bpc_index - 2 * chip_pel_count) % w
            else:
                return -1
        else:
            return -1  # other quad orientations: add when extending vectors
        return i + 2 * w * j

    if num_chips == 8:
        if orientation != 0:
            return -1
        y_chips = num_chips // x_chips
        if x_chips * y_chips != 8:
            return -1
        local = bpc_index - chip * chip_pel_count
        lx = local % w
        ly = local // w
        x_chip = chip % x_chips
        y_chip = chip // x_chips
        i = x_chip * w + lx
        j = y_chip * w + (w - 1 - ly)
        return i + (x_chips * w) * j

    return -1


def pel_index(
    i: int, j: int, pel_width: int, num_chips: int, x_chips: int, orientation: int
) -> int:
    """Mirror ADTimePix::pelIndex; returns BPC index or -1."""
    w = pel_width
    x_chip = i // w
    y_chip = j // w
    ii = i - w
    jj = j - w

    if num_chips == 1:
        if orientation == 0:
            return i + (w - 1 - j) * w
        if orientation == 1:
            return j + i * w
        if orientation == 2:
            return (w - 1 - i) + j * w
        if orientation == 3:
            return (w - 1 - j) + (w - 1 - i) * w
        if orientation == 4:
            return (w - 1 - i) + (w - 1 - j) * w
        if orientation == 5:
            return j + (w - 1 - i) * w
        if orientation == 6:
            return i + j * w
        if orientation == 7:
            return (w - 1 - j) + i * w
        return -1

    if num_chips == 4:
        if orientation == 0:
            if x_chip == 1 and y_chip == 1:
                return ii - (jj - (w - 1)) * w
            if x_chip == 1 and y_chip == 0:
                return w * w + ((w - 1) - ii) + j * w
            if x_chip == 0 and y_chip == 0:
                return 2 * w * w + ((w - 1) - i) + j * w
            if x_chip == 0 and y_chip == 1:
                return 3 * w * w + i - (jj - (w - 1)) * w
        elif orientation == 3:
            if x_chip == 1 and y_chip == 1:
                return (
                    3 * w * w
                    + ((w - 1) - jj)
                    + ((w - 1) - ii) * w
                )
            if x_chip == 1 and y_chip == 0:
                return ((w - 1) - j) + ((w - 1) - ii) * w
            if x_chip == 0 and y_chip == 0:
                return w * w + j + i * w
            if x_chip == 0 and y_chip == 1:
                return 2 * w * w + jj + i * w
        return -1

    if num_chips == 8:
        if orientation != 0:
            return -1
        y_chips = num_chips // x_chips
        if x_chips * y_chips != 8 or x_chip >= x_chips or y_chip >= y_chips:
            return -1
        lx = i - x_chip * w
        ly = j - y_chip * w
        chip_idx = y_chip * x_chips + x_chip
        return chip_idx * w * w + lx + ((w - 1) - ly) * w

    return -1


def linear_to_ij(img_linear: int, num_chips: int, pel_width: int, x_chips: int) -> tuple[int, int]:
    c = image_cols(num_chips, pel_width, x_chips)
    return img_linear % c, img_linear // c


def run_case(case: dict) -> list[str]:
    errors: list[str] = []
    name = case["name"]
    w = case["pel_width"]
    n = case["num_chips"]
    x = case.get("x_chips", 1 if n == 1 else 2)
    orient = case["orientation"]

    if "bpc_index" in case and "expect_chip" in case:
        chip, lx, ly = chip_from_bpc(case["bpc_index"], w)
        if chip != case["expect_chip"]:
            errors.append(f"{name}: chip {chip} != {case['expect_chip']}")
        if lx != case.get("expect_lx"):
            errors.append(f"{name}: lx {lx} != {case.get('expect_lx')}")
        if ly != case.get("expect_ly"):
            errors.append(f"{name}: ly {ly} != {case.get('expect_ly')}")
        return errors

    if "bpc_index" in case and "expect_bpc2img" in case:
        k = case["bpc_index"]
        exp = case["expect_bpc2img"]
        got_linear = bpc2img_index(k, w, n, x, orient)
        if got_linear < 0:
            errors.append(f"{name}: bpc2img returned {got_linear}")
        else:
            gi, gj = linear_to_ij(got_linear, n, w, x)
            if gi != exp["i"] or gj != exp["j"]:
                errors.append(
                    f"{name}: bpc2img ij ({gi},{gj}) != ({exp['i']},{exp['j']})"
                )
            if got_linear != exp["img_linear"]:
                errors.append(
                    f"{name}: img_linear {got_linear} != {exp['img_linear']}"
                )
        if "expect_pel_index_at_ij" in case:
            pi, pj = exp["i"], exp["j"]
            pk = pel_index(pi, pj, w, n, x, orient)
            if pk != case["expect_pel_index_at_ij"]:
                errors.append(
                    f"{name}: pelIndex({pi},{pj})={pk} != {case['expect_pel_index_at_ij']}"
                )

    if "expect_pel_index" in case and "i" in case:
        got = pel_index(case["i"], case["j"], w, n, x, orient)
        if got != case["expect_pel_index"]:
            errors.append(
                f"{name}: pelIndex({case['i']},{case['j']})={got} != {case['expect_pel_index']}"
            )

    if "expect_bpc_index_from_pel" in case:
        got = pel_index(case["i"], case["j"], w, n, x, orient)
        if got != case["expect_bpc_index_from_pel"]:
            errors.append(f"{name}: pelIndex roundtrip {got} != {case['expect_bpc_index_from_pel']}")
        lin = bpc2img_index(got, w, n, x, orient)
        if lin < 0:
            errors.append(f"{name}: bpc2img roundtrip failed")
        else:
            ri, rj = linear_to_ij(lin, n, w, x)
            if ri != case["i"] or rj != case["j"]:
                errors.append(
                    f"{name}: bpc2img roundtrip ij ({ri},{rj}) != ({case['i']},{case['j']})"
                )

    return errors


def main() -> int:
    data = json.loads(VECTORS_PATH.read_text(encoding="utf-8"))
    all_errors: list[str] = []
    for case in data["cases"]:
        all_errors.extend(run_case(case))

    if all_errors:
        print(f"FAIL: {len(all_errors)} error(s):", file=sys.stderr)
        for e in all_errors:
            print(f"  - {e}", file=sys.stderr)
        return 1

    n_cases = len(data["cases"])
    planned = data.get("planned_cases", [])
    n_planned = len(planned)
    print(f"OK: {n_cases} coordinate_map_vectors cases passed", end="")
    if n_planned:
        print(f"; {n_planned} planned case(s) skipped (SpIDR 2×4 — see COORDINATE_MAP.md)")
    else:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
