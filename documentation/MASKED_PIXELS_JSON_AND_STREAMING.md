# Masked pixels: export format, `NDPluginBadPixel`, and streaming

This document describes how to **publish** calibration-derived TPX3 and MPX3 masked pixels. For both families, **bit 0** of the packed pixel value is the mask; all other bits are calibration or control fields and are preserved.

1. **ADCore** image correction -- `NDPluginBadPixel` and its expected JSON.
2. **Downstream analysis** -- dense image coordinates and stable cross-refs.
3. **Event / TCP streaming (future)** -- chip id and local tile coordinates `(lx, ly)` and file position for serialization and debugging.

The TPX3-only offline tools under `maskTpx3/xyChip`—especially **`check_bit.c`** and **`xyChip.sh`**—remain useful legacy cross-checks based on `ADTimePix::bpc2ImgIndex`. Driver-generated JSON uses the same family-specific mapping as the mask and diff images: `pelIndex` for TPX3 and Serval layout metadata for MPX3. See [COORDINATE_MAP.md](COORDINATE_MAP.md) for orientations where the legacy TPX3 mappings differ.

## Why not only the `NDPluginBadPixel` JSON?

`NDPluginBadPixel` (`ADCore/ADApp/pluginSrc/NDPluginBadPixel.cpp`) loads a JSON file and reads the array **`"Bad pixels"`** only. Each entry needs **`"Pixel": [x, y]`** (image indices) and one correction mode: **`"Set"`**, **`"Replace": [dx,dy]`**, or **`"Median": [wx, wy]`** (see ADCore for semantics).

That format is **minimal and plugin-specific**: it has **no** chip index, no BPC file offset, no serial index, and no `lx, ly` on the ASIC. For **streaming**, hardware paths often key events by **chip and local address** (or a linear id derived from the same layout as the `.bpc`). For **reproducibility** and support tickets, you want a **stable counter** and the **on-disk** `Position` (BPC index) used by calibration tools.

**Recommendation:** use **one "expanded" JSON** that contains both:

- A **`"Bad pixels"`** array: **sufficient** for `NDPluginBadPixel` (same file, no second sync step).
- A **`"masked_pels"`** (or `entries`) array: **authoritative** list with chip, BPC index, value, local coords, image coords, and a **serial** `index` for cross-reference.

The plugin **ignores** unknown top-level keys (it only reads `"Bad pixels"`), so extra sections do not break it as long as the file remains valid JSON and `"Bad pixels"` is present and well-formed.

Alternatively, maintain **two** files (e.g. `tpx3_mask_full.json` + `bad_pixel_plugin.json`) if you want zero risk of a parser quirk; the tradeoff is **desynchronization** if only one is updated. Prefer a **single artifact** with a `format_version` field unless tooling forces a split.

## Coordinate conventions (must stay consistent)

| Field | Meaning |
|-------|--------|
| **`bpc_index`** | Byte offset of the packed pixel in the `.bpc` file. It advances by 1 for TPX3 and 2 for MPX3. Chip blocks are consecutive in chip-number order. |
| **`bpc_pixel_index`** | Logical packed-pixel index across all chip blocks. Within a chip, **x (column within tile) is fast** and **y (row within tile) is slow**. |
| **`chip`, `lx`, `ly`** | Chip id in file order; local pixel inside the `chip_pel_width x chip_pel_width` tile. |
| **`i`, `j` (image)** | **Column, row** in the **global** assembled image, **top-left origin**. |
| **Mask / export / PixelConfigDiff image PVs** | Row-major `j * cols + i`. TPX3 maps through **`pelIndex(i, j)`**; MPX3 uses Serval's per-chip rotated layout. See **[COORDINATE_MAP.md](COORDINATE_MAP.md)**, `documentation/PIXELCONFIG_BPC_DIFF.md`, and `mask_io.cpp` comments. |

When generating **`"Bad pixels"`** for the plugin, use **`"Pixel": [i, j]`** with the **same** `(i, j)` as the dense image and `NDArray` dimensions (column `i`, row `j`).

## Proposed single-file schema (illustrative)

```json
{
  "format_version": 1,
  "source": {
    "bpc_path": "/path/Eq_neg_cfg1.bpc",
    "num_chips": 4,
    "detector_orientation": 3,
    "chip_pel_width": 256,
    "bpc_bytes_per_pixel": 1,
    "tool": "ADTimePix RefreshPixelConfig mask export",
    "exported_at_utc": "2026-04-24T12:00:00Z"
  },
  "counts": {
    "masked_pels": 4
  },
  "masked_pels": [
    {
      "index": 1,
      "bpc_index": 18291,
      "bpc_pixel_index": 18291,
      "value": 31,
      "chip": 0,
      "lx": 115,
      "ly": 71,
      "i": 440,
      "j": 115
    }
  ],
  "Bad pixels": [
    { "Pixel": [440, 115], "Median": [1, 1] },
    { "Pixel": [440, 116], "Median": [1, 1] }
  ]
}
```

- **`index`**: 1-based serial, matching a human `check_bit` run (optional; can also be 0-based if documented).
- **`bpc_index`** is a byte offset; **`bpc_pixel_index`** is the logical pixel index. They are equal for TPX3, while MPX3 byte offsets are twice the logical index.
- **`Bad pixels`**: generated from the same rows; correction mode is a **policy** choice (median vs replace) -- not implied by the BPC alone.
- Optional top-level objects **`detector`** and **`acquisition`** (or a single **`epics_snapshot`**) can follow the **Optional metadata** section below.

## Optional metadata for detector and data acquisition

The mask list is defined by the **.bpc** and **geometry** (`num_chips`, orientation, `chip_pel_width`). For **reproducible science** and **downstream DAQ** (e.g. HDF5 attributes, file viewers, or offline replay), the same JSON can carry **extra optional sections** that do not change how `NDPluginBadPixel` works.

| Section | Purpose |
|--------|--------|
| **`source` (expanded)** | Already may include BPC path, `num_chips`, `detector_orientation`, `chip_pel_width`, export timestamp. Add **host / IOC** or **git hash** only if you need traceability. |
| **`detector` (optional)** | Stable instrument identity: detector model, **serial number**, firmware/serval version strings -- values available from existing PVs if the driver exposes them. Use for **provenance** in acquired data files. |
| **`acquisition` or `epics_snapshot` (optional)** | A **small, curated** set of PVs relevant to "how this mask relates to a run": e.g. same-prefix **`BPCFileName`**, **`BPCFilePath`**, `TPX3_DET_ORIENTATION`, threshold or bias PVs, **acquisition time mode** if applicable. **Avoid** duplicating an entire run log or rapidly changing readbacks unless you need them for debugging. |
| **Policy** | Prefer **versioned** `format_version` and **optional** objects so old consumers ignore unknown fields. If run-specific data belongs elsewhere (NeXus, EPICS archiver), keep this file **light** and reference an external id instead of pasting long blobs. |

Rationale: **Data acquisition** pipelines often need "what detector + what calibration file + what key EPICS setpoints" in one place. Putting a **redundant snapshot** in the mask JSON is convenient for **one-file** hand-off to analysis; the **archiver** remains authoritative for time series.

## Driver / IOC integration (implemented)

### File location and name

- **Directory:** the same as the calibration file -- **`BPCFilePath`** (e.g. `TPX3-TEST:cam1:BPCFilePath`).
- **Filename:** derive from **`BPCFileName`**: replace a trailing **`*.bpc`** (case-insensitive) with **`_masked_pels.json`** (e.g. `Eq_neg_cfg1.bpc` -> `Eq_neg_cfg1_masked_pels.json`). If the name has no **`.bpc`** suffix, **`_masked_pels.json`** is appended to the full basename.
- **Readback PVs (asyn -> EPICS in `tpx3App/Db/File.template`):**
  - **`TPX3_MASKED_PELS_JSON_RBV`:** full path to the file last written (record `MaskedPelsJson_RBV`).
  - **`TPX3_MASKED_PELS_COUNT_RBV`:** number of packed pixels whose mask bit 0 is set (record `MaskedPelsCount_RBV`); **-1** means unavailable because the family or required layout is unsupported.
  - **`TPX3_MASKED_PELS_EXPORT_STATUS_RBV`:** short status (record `MaskedPelsExportStatus_RBV`), e.g. `OK: wrote N...`, `Skipped:...`, or `Write failed:...`.
- Use **`MaskedPelsJson_RBV`** for **`NDPluginBadPixel`** `BAD_PIXEL_FILE_NAME` (or a symlink) when you want the plugin to load the same file.
- **Phoebus:** **`tpx3App/op/bob/Mask/PixelConfigMaskPanel.bob`** (embedded from **`Mask.bob`**) shows **Count**, **Status**, and **Path** after a refresh. **`Mask.bob`** does not list them again (same embedded panel). No change required to **`MaskStatus.bob`** for this feature.

This keeps the mask artifact next to the BPC the IOC is configured to use, which matches operator expectations and backup policies.

### Trigger: export when `RefreshPixelConfig` is processed

The driver action **`TPX3_REFRESH_PIXEL_CONFIG`** (record **`RefreshPixelConfig`**, e.g. `TPX3-TEST:cam1:RefreshPixelConfig`) already:

- GETs per-chip **PixelConfig** from SERVAL, and  
- **Reads the on-disk BPC** (when present) to compare bytes and fill **`PixelConfigDiff`**.

**Writing the single-file mask JSON in the same code path (after a successful BPC read used for that comparison) is a reasonable integration point:**

- **Alignment:** the operator already uses **Refresh** when they care that **SERVAL**, **on-disk BPC**, and **live** config are in step; emitting the **mask list from the same file** the driver just read keeps "what's on disk for this path/name" in lockstep with that workflow.
- **No extra file selection:** `BPCFilePath` + `BPCFileName` are already in context.

**Caveats (document for operators):**

1. The exported mask is **from the on-disk `.bpc`**, not from decoded **SERVAL** PixelConfig alone. If SERVAL and file **diverge**, the JSON still describes the **file**; mismatch is visible via existing **`PixelConfigMatchBPC_***` PVs.
2. MPX3 export requires complete supported Serval chip-layout entries so its packed words can be placed at assembled-image coordinates. The export fails closed and reports count **-1** if the layout is incomplete or unsupported.
3. **Side effects:** Refresh may already trigger network traffic to each chip. Adding a local JSON write is cheap; if a site needs **re-export without SERVAL round-trips**, a **separate** "export mask JSON only" action (or PROC) can be added later.
4. If **`BPCFilePath`/`BPCFileName`** are empty or the file is missing, the driver should **skip** or **error** the export and set status PVs clearly (same as no-BPC for PixelConfig compare).

### Other integration steps (unchanged in intent)

1. **Image pipeline:** set **`NDPluginBadPixel`'s** `BAD_PIXEL_FILE_NAME` to the written JSON (manually, via an alias/substitution, or automation when the readback path updates).
2. **Streaming / offline:** consumers load **`masked_pels`** (or a columnar export derived from it) to filter or relabel events by `chip` / `(lx,ly)` or global `(i,j)`.

## Related documentation

- `documentation/PIXELCONFIG_BPC_DIFF.md` -- `BPC` vs `MaskBPC` vs `PixelConfigDiff`, `pelIndex` vs `bpc2ImgIndex`.
- ADCore: `ADApp/pluginSrc/NDPluginBadPixel.cpp` -- JSON format for `"Bad pixels"`.
- Off-repo reference: `maskTpx3/xyChip/check_bit.c`, `xyChip.sh` -- reference listing for validation and tooling.

## Release / tracking

Documented in **RELEASE.md** (section **R1-6-2**): `*_masked_pels.json` next to **`BPCFilePath`**, trigger **`RefreshPixelConfig`**, readback PVs, **`format_version` == 1**.
