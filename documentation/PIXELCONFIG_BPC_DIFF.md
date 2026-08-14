# PixelConfig vs on-disk BPC (SERVAL vs file)

This note explains what the **PixelConfig** refresh and **`PixelConfigDiff`** waveform represent, and how they relate to **mask / BPC** PVs. It is aimed at operators and integrators debugging “chip programmed differently than my file.”

## Two different things

| Source | Meaning |
|--------|--------|
| **SERVAL PixelConfig** (per chip) | The **current** pixel configuration SERVAL holds for that chip—what the detector is actually using after loads, applies, and API updates. **Timepix3:** 65536 bytes per chip. **Medipix3 (dual counter):** **131072 bytes** per chip (see below). |
| **On-disk `.bpc` file** | The file the IOC reads using **`BPCFilePath`** + **`BPCFileName`**. Used for mask editing, upload, and **this comparison**. |

They can differ if, for example: the file was edited on disk but not uploaded; **`ApplyConfig` / `WriteData`** was not run; another client changed SERVAL; or the IOC points at a different path than you expect.

The driver does **not** guess which side is “correct”; it reports **equality or byte differences**.

## Medipix3 dual-threshold BPC layout (Aug 2026)

On a **4-chip MPX3 quad** with **`BothCounters`**, Serval **`GET /detector/chips/<i>/PixelConfig`** base64-decodes to **131072 bytes** for **each** chip (not 65536). Verified against `vendor/mpx3/eq-01.bpc` (524288 B = **4 × 131072**) and a full Serval root dump (`documentation/medipix3/drafts/serval-mpx3-quad-root-2026-08-14.json`, local/gitignored).

**Per-chip file layout** (chip index `i`, byte offset in `.bpc` = **`i × 131072`**):

| Slice | Byte range within chip block | Role |
|-------|------------------------------|------|
| Threshold 0 | `[0 .. 65535]` | Pixel config for counter 0 |
| Threshold 1 | `[65536 .. 131071]` | Pixel config for counter 1 |

Each slice is **1 byte per pixel** (256×256). The two slices are **independent** (byte values differ between th0 and th1 on most pixels). Conceptually **two config bytes per image pixel per chip**, stored as **concatenated slices** `[th0 block][th1 block]`, not as interleaved byte pairs.

**Byte semantics (important):** On **`vendor/tpx3-demo.bpc`**, Accos bad pixels are **byte value 31** (`0b00011111`), not “any bit-0 set” — see below. On **`vendor/mpx3/eq-01.bpc`**, ~25% of bytes per slice have bit 0 set (values 1, 3, 5, 7) in **clustered** patterns consistent with **equalization/trim encoding**, not ~10 scattered dead pixels per chip. **Do not assume MPX3 bit 0 = disable counting** until ASI documents the bit map (Email 2). The driver’s legacy **`BPCn`** / bit-0 export must not be used as an MPX3 bad-pixel list.

**Mask code:** Timepix3 paths in `mask_io.cpp` assume one slice; Medipix3 needs **`DetectorFamily::MPX3`** / `bpcThresholdSlices == 2` (see `detector_family.h`) for chip stride, `RefreshPixelConfig`, and threshold-aware `pelIndex`.

**Compare rule (correct for MPX3):** decoded Serval bytes vs file bytes at **`offset = i × 131072`**, length **131072** → **0 mismatches** for all four chips against `eq-01.bpc`.

**Driver status:** `refreshPixelConfigFromServal()` in `serval_http.cpp` still uses **`kPixelConfigBytes = 65536`** and offset **`i × 65536`**. That produces CHIP0 “length mismatch (131072 vs 65536)” and ~56k false mismatches on chips 1–3. **Fix pending:** MPX3-aware chip stride and **`PixelConfigDiff`** threshold selection.

## Timepix3 Accos bad pixels vs operator mask (Aug 2026)

On **`vendor/tpx3-demo.bpc`** (4-chip quad, 66 bad pixels total):

- Every Accos bad pixel has **byte value 31** (`0b00011111`, bits 0–4 set). **Bit 0 alone is not the Accos disable pattern** — e.g. 6073 pixels at value 30 (`0b11110`) are not masked.
- The IOC **`BPCn`** / “read from bpc” path counts **bit 0**; on this file that matches 31 (bit 0 ⟺ 31), but **edited** files (`mask.bpc`) can have bit 0 without 31 from operator **`|= bit0`** mask writes.
- **Operator mask workflow:** `MaskWrite` reads **`BPCFileName`** (cal), ORs mask bits onto a copy, writes **`mask.bpc`**, uploads to Serval. **Undo** = reload original cal via **`WriteBPCFile`** — not per-pixel unmask in `mask.bpc`.
- **Planned driver fix:** operator mask ON → write **31** (Accos pattern), not bit 0 only; **`BPCn`** → count **`byte == 31`** on TPX3.

## What “Refresh PixelConfig” does

1. Reads the on-disk BPC into memory (same read path as other mask/BPC operations).
2. For each chip `i`, GET `/detector/chips/<i>/PixelConfig`, parse JSON, base64-decode to bytes.
3. Compares decoded bytes to file bytes at offset **`i × 65536`** for up to **65536** bytes per chip (**Timepix3 / legacy path**). Medipix3 should use **`i × 131072`** and **131072** bytes when dual-threshold (see above).
4. Updates per-chip status PVs and fills **`PixelConfigDiff`**.

## Match codes (`PixelConfigMatchBPC_RBV`)

| Code | Meaning |
|------|--------|
| -1 | Error (HTTP, JSON, decode, etc.) |
| 0 | Bytes differ (`PixelConfigMismatchBytes_RBV` = count of differing byte positions in the compared range) |
| 1 | Compared range matches |
| 2 | No BPC file (or empty read) |
| 3 | Length / size mismatch (decoded length vs expected chip slice) |

## Waveform indexing: `BPC` vs `MaskBPC` vs `PixelConfigDiff`

- **`BPC` PV** (`TPX3_BPC_PEL`): **Linear file order**—index `k` is byte `k` in the `.bpc` file.
- **`PixelConfigDiff`**: **Image order** = **`j × cols + i`** (same row-major convention as **`maskCircle`** / mask write), sample **`(i, j)`** = **`abs(SERVAL[k] − BPC[k])`** where **`k = pelIndex(i, j)`**. That is the **same** mapping used when a mask is written into the `.bpc` file (`pelIndex` in `mask_io.cpp`). **`DetOrient` / `TPX3_DET_ORIENTATION`** is included in **`pelIndex`**, so rotated layouts match the mask editor.
- **`MaskBPC` when read from disk** (“read from bpc” / **`MaskPel`**): fills **`value[j*COLS+i]`** from **`bufBPC[pelIndex(i, j)]`**, same as mask **write** and **`PixelConfigDiff`** (no **`bpc2ImgIndex`** on this path).

## `PixelConfigDiff` values

Each element is **`abs(byte_SERVAL − byte_BPC)`** for the same logical pel after mapping. On TPX3, a change from trim code to **31** shows as **31 − old** in that pel.

## Coordinate map

See **[COORDINATE_MAP.md](COORDINATE_MAP.md)** for `pelIndex` vs `bpc2ImgIndex`, orientations, and golden test vectors (`test/coordinate_map_vectors.json`).

## Related PVs and UI

- **`RefreshPixelConfig`**: `Dashboard.template`; forward-links to **`PixelConfigDiff.PROC`** so the waveform record processes after the driver updates the buffer.
- **Phoebus**: `tpx3App/op/bob/Mask/PixelConfigMaskPanel.bob` (embedded from `Mask.bob`).

## Release history

See **`RELEASE.md`**, section **R1-6-2**, for implementation details (asyn array type, callback length, `bpc2ImgIndex` chip-index fix, **`pelIndex`**-based **`PixelConfigDiff`**).
