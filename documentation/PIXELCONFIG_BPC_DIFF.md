# PixelConfig vs on-disk BPC (SERVAL vs file)

This note explains what the **PixelConfig** refresh and **`PixelConfigDiff`** waveform represent, and how they relate to **mask / BPC** PVs. It is aimed at operators and integrators debugging “Serval's configured value differs from my file.”

## Two different things

| Source | Meaning |
|--------|--------|
| **SERVAL PixelConfig** (per chip) | The Base64-encoded pixel configuration returned by `GET /detector/chips/<i>/PixelConfig`. **Timepix3:** 65536 bytes per chip. **Medipix3 (dual counter):** **131072 bytes** per chip (see below). ASI indicates this is converted from the BPC and is not a direct detector-register readback. Treat it as Serval's stored/configured value. |
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
| `11` | Test-pulse injection; normally clear for sensor operation |
| `12..15` | Unused; normally zero; preserve unchanged |

Interpreting `vendor/mpx3/eq-01.bpc` this way produces coherent 5-bit adjustment distributions, zero set mask bits, and zero values in bits 11–15. The prior byte-slice interpretation produced the misleading appearance that many ordinary calibration values had a mask bit set.

**Emulator validation:** A test copy set only bit 0 in five words per chip (20 pixels total), preserving every other bit. Serval accepted it, returned four matching 131072-byte blocks, and reported exactly five differing byte positions per chip against the original file. Identical acquisitions before and after the load differed at exactly those 20 pixels: every selected pixel changed from nonzero to zero in both threshold images, with no other image differences.

**Mask code:** ASI confirmed that bit 0 masks both counters, clearing bit 0 restores normal operation, and mask changes must preserve the other word fields. MPX3 operator mask read/write/count/export therefore uses the packed-word rule and Serval's per-chip image layout. Unknown layouts fail closed. Bits 12–15 are unused and normally zero, but remain preserved defensively.

**Compare rule (correct for MPX3):** decoded Serval bytes vs file bytes at **`offset = i × 131072`**, length **131072** → **0 mismatches** for all four chips against `eq-01.bpc`.

ASI confirmed that BPC blocks and the corresponding `detector-chips.json` array
are ordered **chip 0, 1, 2, 3**. The supplied `eq-02` fixture under
`test/fixtures/mpx3/eq-02/` independently matches each decoded array element to
the same-index BPC block byte-for-byte.

**Interpretation limit:** a zero diff proves that the file and the value held
and returned by Serval agree. Because ASI indicates `PixelConfig` is converted
from the BPC rather than read back from detector registers, it does **not** by
itself prove that every hardware pixel register contains that value.

**Driver status:** `refreshPixelConfigFromServal()` uses the detector-family capabilities for the per-chip size and stride. MPX3 compares each full 131072-byte chip block and decodes each heatmap sample as one big-endian 16-bit value. `CounterSelectIn` does not select a separate BPC slice because both adjustment fields occupy the same word.

## Timepix3 mask bit and observed Accos values

On **`vendor/tpx3/2x2/tpx3-demo.bpc`** (4-chip quad, 66 bad pixels total):

- ASI confirmed bit 0 as the mask, bits 1–4 as adjustment, and bit 5 as test-pulse injection. Masking sets only bit 0 and unmasking clears only bit 0 while preserving every other bit.
- ASI identifies bits 6–7 as unused and normally zero; the driver nevertheless preserves them.
- Every masked pixel in the available Accos reference calibration happens to have **byte value 31** (`0b00011111`): the mask and all four adjustment bits are set. Value 31 is therefore an observed combination, not a required replacement byte.
- The IOC **`BPCn`**, “read from bpc,” and masked-pels export paths classify any byte with bit 0 set as masked.
- **Operator mask workflow:** `MaskWrite` reads **`BPCFileName`**, sets bit 0 at selected image coordinates in a copy, writes **`mask.bpc`**, and uploads it to Serval. Adjustment, test-pulse, and unknown bits are unchanged. Reload the original calibration with **`WriteBPCFile`** to restore the complete original mask selection.

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

- **`BPC` PV** (`TPX3_BPC_PEL`): **Linear pixel order**. TPX3 publishes each byte value; MPX3 publishes each decoded big-endian word. For display, a masked TPX3 value also has bit 8 set and a masked MPX3 value has bit 16 set.
- **`PixelConfigDiff`**: **Image order** = **`j × cols + i`**. TPX3 uses `k = pelIndex(i,j)` and compares one-byte values. MPX3 compares complete big-endian words and places them using Serval's per-chip rotated layout (`Chip`, `X`, `Y`, and `Orientation`) rather than the TPX3 quad map.
- MPX3 per-chip match and mismatch counts still cover the complete 131072-byte block. `PixelConfigMismatchBytes_RBV` counts differing **bytes**, so changing only word bit 0 produces one byte mismatch per masked pixel.
- **`MaskBPC` when read from disk** (“read from bpc” / **`MaskPel`**): fills **`value[j*COLS+i]`** using `pelIndex(i,j)` for TPX3 or Serval's explicit per-chip layout for MPX3, matching each family's mask write, export, and diff behavior.

## `PixelConfigDiff` values

Each element is the absolute difference between the packed unsigned pixel values after mapping. The value is 8-bit for TPX3 and big-endian 16-bit for MPX3. An MPX3 bit-0-only mask difference therefore displays as **1**.

For the MPX3 `UP` layout, Serval reports chips 1/0 at `Y=0` with `RtLBtT` and
chips 2/3 at `Y=256` with `LtRTtB`. Serval tile Y is bottom-origin, so the
top-left/Y-down image places chips 2/3 on the top row and 1/0 on the bottom
row. The direction tokens are literal: `RtLBtT` reverses both image-local axes,
while `LtRTtB` is identity. The controlled five-pixel-per-chip BPC validated
packed-word comparison and chip assignment, but a later asymmetric operator
mask was required to expose the former inverse interpretation of the local
directions and the independent tile-origin conversion.

The per-chip `PixelConfig` comparison can still match the selected unmodified
BPC and the file-derived masked-pixel count can remain zero during a transient
operator-mask test. That is expected: these PVs describe BPC-derived
configuration and the selected on-disk file, not a fresh readback of transient
mask state from detector registers. After the direction and bottom-origin tile
corrections, the asymmetric operator mask aligned with both threshold
acquisition images in all eight global detector orientations. These cases
collectively exercise every Serval chip-orientation string.

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
