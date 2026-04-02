# PixelConfig vs on-disk BPC (SERVAL vs file)

This note explains what the **PixelConfig** refresh and **`PixelConfigDiff`** waveform represent, and how they relate to **mask / BPC** PVs. It is aimed at operators and integrators debugging “chip programmed differently than my file.”

## Two different things

| Source | Meaning |
|--------|--------|
| **SERVAL PixelConfig** (per chip) | The **current** 65536-byte pixel configuration SERVAL holds for that chip—what the detector is actually using after loads, applies, and API updates. |
| **On-disk `.bpc` file** | The file the IOC reads using **`BPCFilePath`** + **`BPCFileName`**. Used for mask editing, upload, and **this comparison**. |

They can differ if, for example: the file was edited on disk but not uploaded; **`ApplyConfig` / `WriteData`** was not run; another client changed SERVAL; or the IOC points at a different path than you expect.

The driver does **not** guess which side is “correct”; it reports **equality or byte differences**.

## What “Refresh PixelConfig” does

1. Reads the on-disk BPC into memory (same read path as other mask/BPC operations).
2. For each chip `i`, GET `/detector/chips/<i>/PixelConfig`, parse JSON, base64-decode to bytes.
3. Compares decoded bytes to file bytes at offset **`i × 65536`** for up to **65536** bytes per chip (quad file = 262144 bytes total).
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
- **`MaskBPC` when read from disk** (masked bits highlighted): still uses **`bcp2ImgIndex(k, …)`** to place file byte `k` on the image; that path may not match **`pelIndex`** for every orientation. The **in-memory** mask (`maskCircle`, etc.) and **`PixelConfigDiff`** are consistent with **`pelIndex`**.

## `PixelConfigDiff` values

Each element is **`abs(byte_SERVAL − byte_BPC)`** for the same logical pel after mapping. Bit-level mask changes (e.g. bit 0) typically show as small integers (often **1**) where that byte differs.

## Related PVs and UI

- **`RefreshPixelConfig`**: `Dashboard.template`; forward-links to **`PixelConfigDiff.PROC`** so the waveform record processes after the driver updates the buffer.
- **Phoebus**: `tpx3App/op/bob/Mask/PixelConfigMaskPanel.bob` (embedded from `Mask.bob`).

## Release history

See **`RELEASE.md`**, section **R1-6-2**, for implementation details (asyn array type, callback length, `bcp2ImgIndex` chip-index fix, **`pelIndex`**-based **`PixelConfigDiff`**).
