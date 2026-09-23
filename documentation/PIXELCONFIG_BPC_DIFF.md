# PixelConfig vs on-disk BPC (SERVAL vs file)

This note explains what the **PixelConfig** refresh and **`PixelConfigDiff`** waveform represent, and how they relate to **mask / BPC** PVs. It is aimed at operators and integrators debugging “chip programmed differently than my file.”

## Two different things

| Source | Meaning |
|--------|--------|
| **SERVAL PixelConfig** (per chip) | The **current** pixel configuration SERVAL holds for that chip—what the detector is actually using after loads, applies, and API updates. **Timepix3:** 65536 bytes per chip. **Medipix3 (dual counter):** **131072 bytes** per chip (see below). |
| **On-disk `.bpc` file** | The file the IOC reads using **`BPCFilePath`** + **`BPCFileName`**. Used for mask editing, upload, and **this comparison**. |

They can differ if, for example: the file was edited on disk but not uploaded; **`ApplyConfig` / `WriteData`** was not run; another client changed SERVAL; or the IOC points at a different path than you expect.

The driver does **not** guess which side is “correct”; it reports **equality or byte differences**.

## Medipix3 packed-word BPC layout (September 2026)

On a **4-chip MPX3 quad** with **`BothCounters`**, Serval **`GET /detector/chips/<i>/PixelConfig`** base64-decodes to **131072 bytes** for **each** chip (not 65536). Verified against `vendor/mpx3/eq-01.bpc` (524288 B = **4 × 131072**) and a full Serval root dump (`documentation/medipix3/drafts/serval-mpx3-quad-root-2026-08-14.json`, local/gitignored).

**Per-chip file layout** (chip index `i`, byte offset in `.bpc` = **`i × 131072`**):

| Unit | Layout |
|------|--------|
| Pixel word | One **big-endian 16-bit word** (`2 bytes`) |
| Pixel `p` within chip | Bytes `[2p, 2p+1]` |
| Chip block | `65536 words × 2 bytes = 131072 bytes` |

The vendor-provided ImageJ decoder defines the packed fields as:

| Bits | Field |
|------|-------|
| `0` | Mask shared by threshold/counter 0 and 1 |
| `1..5` | Threshold-0 adjustment |
| `6..10` | Threshold-1 adjustment |
| `11..15` | Meaning pending vendor confirmation; preserve unchanged |

Interpreting `vendor/mpx3/eq-01.bpc` this way produces coherent 5-bit adjustment distributions, zero set mask bits, and zero values in bits 11–15. The prior byte-slice interpretation produced the misleading appearance that many ordinary calibration values had a mask bit set.

**Emulator validation:** A test copy set only bit 0 in five words per chip (20 pixels total), preserving every other bit. Serval accepted it, returned four matching 131072-byte blocks, and reported exactly five differing byte positions per chip against the original file. Identical acquisitions before and after the load differed at exactly those 20 pixels: every selected pixel changed from nonzero to zero in both threshold images, with no other image differences.

**Mask code:** MPX3 operator mask read/write/export remains blocked while the follow-up vendor confirmation of set/clear and reserved-bit rules is pending. Calibration upload and complete PixelConfig comparison remain supported. Timepix3 behavior is described below.

**Compare rule (correct for MPX3):** decoded Serval bytes vs file bytes at **`offset = i × 131072`**, length **131072** → **0 mismatches** for all four chips against `eq-01.bpc`.

**Driver status:** `refreshPixelConfigFromServal()` uses the detector-family capabilities for the per-chip size and stride. MPX3 compares each full 131072-byte chip block and decodes each heatmap sample as one big-endian 16-bit value. `CounterSelectIn` does not select a separate BPC slice because both adjustment fields occupy the same word.

## Timepix3 Accos bad pixels vs operator mask (Aug 2026)

On **`vendor/tpx3/2x2/tpx3-demo.bpc`** (4-chip quad, 66 bad pixels total):

- Every Accos bad pixel has **byte value 31** (`0b00011111`, bits 0–4 set). **Bit 0 alone is not the Accos disable pattern** — e.g. 6073 pixels at value 30 (`0b11110`) are not masked.
- The IOC **`BPCn`** / “read from bpc” path counts only bytes exactly equal to **31**. Values that merely have bit 0 set are not classified as masked.
- **Operator mask workflow:** `MaskWrite` reads **`BPCFileName`** (cal), replaces selected bytes with **31** in a copy, writes **`mask.bpc`**, and uploads it to Serval. **Undo** = reload the original calibration with **`WriteBPCFile`**; there is no per-pixel restoration of the overwritten calibration byte in `mask.bpc`.
- The value-31 rule matches the observed Accos TPX3 calibration. Confirmation that replacement (rather than setting only bits 0–4 while preserving upper bits) is the authoritative vendor operation remains pending.

## What “Refresh PixelConfig” does

1. Reads the on-disk BPC into memory (same read path as other mask/BPC operations).
2. For each chip `i`, GET `/detector/chips/<i>/PixelConfig`, parse JSON, base64-decode to bytes.
3. Compares the exact decoded chip block to the file block at the family-specific offset: **`i × 65536`** for TPX3 or **`i × 131072`** for dual-threshold MPX3.
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
- **`PixelConfigDiff`**: **Image order** = **`j × cols + i`**. TPX3 uses `k = pelIndex(i,j)` and compares one-byte values. MPX3 compares complete big-endian words and places them using Serval's per-chip rotated layout (`Chip`, `X`, `Y`, and `Orientation`) rather than the TPX3 quad map.
- MPX3 per-chip match and mismatch counts still cover the complete 131072-byte block. `PixelConfigMismatchBytes_RBV` counts differing **bytes**, so changing only word bit 0 produces one byte mismatch per masked pixel.
- **`MaskBPC` when read from disk** (“read from bpc” / **`MaskPel`**): this TPX3-only path fills **`value[j*COLS+i]`** from **`bufBPC[pelIndex(i, j)]`**, matching TPX3 mask write and diff behavior.

## `PixelConfigDiff` values

Each element is the absolute difference between the packed unsigned pixel values after mapping. The value is 8-bit for TPX3 and big-endian 16-bit for MPX3. An MPX3 bit-0-only mask difference therefore displays as **1**.

For the validated MPX3 `UP` layout, Serval reports chip 1 at top-left and chip 0 at top-right with `RtLBtT`; chip 2 is bottom-left and chip 3 bottom-right with `LtRTtB`. The controlled five-pixel-per-chip BPC therefore appeared at exactly the same 20 coordinates in `PixelConfigDiff` and both saved acquisition arrays.

## Coordinate map

See **[COORDINATE_MAP.md](COORDINATE_MAP.md)** for `pelIndex` vs `bpc2ImgIndex`, orientations, and golden test vectors (`test/coordinate_map_vectors.json`).

## Related PVs and UI

- **`RefreshPixelConfig`**: `Dashboard.template`; forward-links to **`PixelConfigDiff.PROC`** so the waveform record processes after the driver updates the buffer.
- **Phoebus**: `tpx3App/op/bob/common/Mask/PixelConfigMaskPanel.bob` (embedded from `Mask.bob`).

## Live IOC validation

With Channel Access configured for the target IOC, run the repository script:

```sh
test/validate_pixel_config.sh TPX3-TEST:cam1:
# or
test/validate_pixel_config.sh MPX3-TEST:cam1:
```

It auto-detects detector family and chip count, triggers the refresh, prints
CHAR-waveform status values as strings, and exits nonzero unless every chip has
the expected decoded length and matches the configured BPC file. The script
does not set site-specific `EPICS_CA_*` variables.

## Release history

See **`RELEASE.md`**, section **R1-6-2**, for implementation details (asyn array type, callback length, `bpc2ImgIndex` chip-index fix, **`pelIndex`**-based **`PixelConfigDiff`**).
