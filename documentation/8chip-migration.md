# Eight-chip (2×4 / dual SPIDR) support

This document tracks IOC and driver changes for detectors with up to eight TimePix3 chips (e.g. two SPIDR boards × four chips).

## Implemented

- **IOC:** `load_chips.cmd` loads `Chips.template` for `CHIP0`…`CHIP7` (asyn `ADDR` 0–7). Included from `profiles/tpx3/st.cmd`.
- **Rails:** `OperatingVoltage.template` instances `Pwr0`…`Pwr5` at asyn `ADDR` 0–5. `ADDR` 0–2 are the three `VDD`/`AVDD` readings from Serval `Health[0]`; `ADDR` 3–5 are from `Health[1]` when present (second SPIDR). When only one board exists, `Pwr3`–`Pwr5` read back zero.
- **Driver (`updateDetectorHealthFromJson` / `getDetector`; `Health` JSON array vs object):**
  - Merges all `Health[].ChipTemperatures` into one JSON array for `ChipTemps_RBV` (flat list, e.g. eight entries for two boards).
  - If multiple `Health` blocks: `VDD_RBV` / `AVDD_RBV` strings become a JSON array of per-board rail arrays (e.g. `[[r0,r1,r2],[r0,r1,r2]]`). Single-board keeps the previous flat array of three numbers.
  - **Boards:** `TPX3_BOARDS2_ID`, `TPX3_BOARDS2_IP`, `TPX3_BOARDS_CH5`…`CH8` from `Info.Boards[1]` when available (`CH5`…`CH8` are that board’s four chip JSON objects).
- **Database:** `ADTimePix3.template` adds `BChBoard2Id_RBV`, `IpAddr2_RBV`, `Chip5_RBV`…`Chip8_RBV`.
- **Mask / BPC indexing (`mask_io.cpp`):** For `numChips == 8` and
  **DetectorOrientation UP (0)** only, `pelIndex` and `bpc2ImgIndex` contain a
  linear rectangular-mosaic mapping. Live 4x2 mask operation is not yet
  qualified because `rowsCols()` still derives square-layout dimensions; see
  [Known two-quad mask limitations](#known-two-quad-mask-limitations).

## NDStats profile dimensions

`profiles/tpx3/unique.cmd` sets the NDStats waveform capacities to
`XSIZE=1024` and `YSIZE=512`, matching the largest supported 2x4 mosaic.
The Phoebus horizontal cursor profile reads `Stats1:ProfileCursorX_RBV`, whose
record `NELM` is set from `XSIZE`; a value of 512 therefore truncates a
1024-pixel row profile. Smaller detector geometries remain supported because
the waveform's actual element count is reported through `NORD`.

The former TPX3 `NELMT` setting was not consumed by the current IOC startup
tree or ADCore plugin databases and has been removed. It did not control
NDStats, mask/BPC, or image-accumulation waveform sizes.

## Mask BPC database (`MASK_BPC_NELEMENTS`)

`profiles/tpx3/st.cmd` loads `MaskBPC.template` with `NELEMENTS=$(MASK_BPC_NELEMENTS)`. That macro **must** be defined in **`profiles/tpx3/unique.cmd`** before the database load (after `< envPaths` and `< profiles/tpx3/unique.cmd`). If it is missing, iocsh reports `macLib: macro MASK_BPC_NELEMENTS is undefined` and mask-related PVs never connect.

`profiles/tpx3/unique.cmd` documents three standard sizes and selects the
eight-chip capacity by default:

| Chips | Typical mosaic | Image (px) | `MASK_BPC_NELEMENTS` |
|-------|----------------|------------|----------------------|
| 1 | 1×1 | 256×256 | 65536 |
| 4 | 2×2 | 512×512 | 262144 |
| 8 | 2×4 | 1024×512 | 524288 |

`envPaths` does **not** set this macro; it only notes that `profiles/tpx3/unique.cmd` must define it.

The configured value is record capacity only. It prevents EPICS waveform
truncation but does not create a matching calibration or validate the mapping.

## Known two-quad mask limitations

Runtime qualification with a 1024x512 two-quad detector passed the full-width
horizontal profile, image-accumulation displays, and preview histogram. Mask
qualification remains blocked by two independent issues:

1. The available TPX3 calibration is a four-chip, 262144-byte BPC. Serval
   accepts one selected pixel-configuration file, so an eight-chip detector
   needs one validated 524288-byte BPC containing the chip blocks in final
   Serval chip order. The DACS file likewise needs unique `[Chip0]` through
   `[Chip7]` sections; two unchanged quad DACS files cannot simply be appended.
2. `rowsCols()` currently calculates `chipPelWidth = NumberOfRows / RowLen`.
   For 512 rows and four chips per row this produces 128, causing
   `RefreshPixelConfig` to expect 16384 bytes per chip instead of 65536. The
   rectangular geometry calculation and compiled tests require a separate
   driver change.

Do not use the eight-chip mask, BPC comparison, or PixelConfig-difference
results until both the geometry fix and a detector-specific combined
calibration have been validated. Calibration files are hardware-specific and
should remain site-local; a repository change may provide deterministic
composition and validation tooling without publishing facility calibration.

## Verify on hardware

- **BPC ↔ image mapping:** The 8-chip branch assumes **linear** chip order (`chip = Y_CHIP * xChips + X_CHIP`) on a uniform grid. **Full-rate SpIDR 2×4** (single board, `chipboardId` `84…`) uses a different mosaic and per-chip rotations — see **[COORDINATE_MAP.md](COORDINATE_MAP.md)** (*SpIDR 2×4 — planned*). Confirm masks and PixelConfigDiff on real data before relying on 8-chip mask paths on that hardware.
- **Phoebus:** `profiles/tpx3/TimePix3Detector.bob` embeds eight chip panels in a **4×2** grid (geometry maintained in the BOB). See also `profiles/tpx3/Acquire/DetectorConfig.bob` (Vth), `common/Mask/` and `common/Acquire/ImgAccumulation.bob` for **1024×512** at `PixCount_RBV == 524288`, and **`common/Detector/TimePixDetectorVoltages.bob`** (Display Builder) or **`TimePixDetectorVoltages.opi`** (legacy BOY under **`op/opi/Detector`**) for **Pwr3–Pwr5**. **`common/Detector/TimePixDetectorHealth.bob`** summarizes health and cross-links to voltages.
- **Large images:** For 8-chip, keep `MASK_BPC_NELEMENTS` at `524288` (or larger if `PixCount` is greater), `XSIZE` at least `1024`, and `YSIZE` at least `512`. Raise `EPICS_CA_MAX_ARRAY_BYTES` and other ND plugin `NELEMENTS` values if waveforms exceed their configured limits.

## Calibration

Per-chip / per-geometry calibration policy is outside this IOC change; use naming or paths that include chip count and layout id.
