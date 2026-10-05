# Eight-chip 4×2 (two-quad / dual-SPIDR) support

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
  **DetectorOrientation UP (0)** only, `pelIndex` and `bpc2ImgIndex` implement
  the two-quad 4x2 placement `2,1,4,7 / 3,0,5,6`. The right-hand quad is
  rotated 180 degrees relative to the left-hand quad. The distinct single-board
  full-rate SpidrTurbo mapping remains unimplemented pending golden hardware
  vectors.
- **Geometry:** the captured TPX3 Serval response reports `Info.RowLen=4`
  chips across, `NumberOfRows=512`, `NumberOfChips=8`, and
  `PixCount=524288`. The eight-chip-only geometry path derives a 4x2 grid of
  256x256 chips and a 1024x512 raster. The established one- and four-chip path
  is unchanged. Runtime mask mapping also requires two distinct `41...` TPX3
  quad board IDs; the different single-board `84...` SpidrTurbo layout fails
  closed.
- **Synthetic calibration fixture:** `vendor/tpx3/4x2/` contains a reproducible
  524288-byte BPC and a DACS file with unique `[Chip0]` through `[Chip7]`
  sections for software and emulator testing. It is not hardware equalization.

## NDStats profile dimensions

`profiles/tpx3/unique.cmd` sets the NDStats waveform capacities to
`XSIZE=1024` and `YSIZE=512`, matching the largest supported 4x2 mosaic.
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
| 8 | 4×2 | 1024×512 | 524288 |

`envPaths` does **not** set this macro; it only notes that `profiles/tpx3/unique.cmd` must define it.

The configured value is record capacity only. It prevents EPICS waveform
truncation but does not create a matching calibration or validate the mapping.

## Two-quad calibration and qualification boundary

Runtime qualification on 2026-10-04 with the 1024x512 two-quad TPX3 emulator
passed the full-width horizontal profile, image-accumulation displays, preview
histogram, and mask workflow. The synthetic BPC and DACS uploads both returned
HTTP 200. All eight PixelConfig responses decoded to 65536 bytes and matched
their BPC slices with zero mismatches. A rectangular mask propagated through
the 1024x512 mask preview and the masked-pixel JSON export. The geometry and
deterministic two-quad composition prerequisites are also covered by compiled
and portable tests, but physical-detector mask qualification remains pending.

The checked-in 4x2 BPC repeats the four source chip blocks for chips 4-7. The
DACS file repeats each source chip's values under a unique destination section
name. These files verify file sizing, upload selection, per-chip offsets, and
software/emulator behavior only. Real calibration files are hardware-specific
and should remain site-local unless redistribution is explicitly appropriate.

Do not claim the eight-chip mask, BPC comparison, or PixelConfig-difference
path as hardware-qualified until a detector-specific BPC/DACS pair and an
asymmetric mask have confirmed all eight chip placements.

## Verify on hardware

- **BPC ↔ image mapping:** The implemented 8-chip path is specifically the
  **two-quad** placement `2,1,4,7 / 3,0,5,6`. **Full-rate SpidrTurbo 4×2**
  (single board, `chipboardId` `84…`) uses a different mosaic and per-chip
  rotations—see [COORDINATE_MAP.md](COORDINATE_MAP.md). Do not use the
  two-quad mapping on that hardware.
- **Phoebus:** `profiles/tpx3/TimePix3Detector.bob` embeds eight chip panels in a **4×2** grid (geometry maintained in the BOB). See also `profiles/tpx3/Acquire/DetectorConfig.bob` (Vth), `common/Mask/` and `common/Acquire/ImgAccumulation.bob` for **1024×512** at `PixCount_RBV == 524288`, and **`common/Detector/TimePixDetectorVoltages.bob`** (Display Builder) or **`TimePixDetectorVoltages.opi`** (legacy BOY under **`op/opi/Detector`**) for **Pwr3–Pwr5**. **`common/Detector/TimePixDetectorHealth.bob`** summarizes health and cross-links to voltages.
- **Large images:** For 8-chip, keep `MASK_BPC_NELEMENTS` at `524288` (or larger if `PixCount` is greater), `XSIZE` at least `1024`, and `YSIZE` at least `512`. Raise `EPICS_CA_MAX_ARRAY_BYTES` and other ND plugin `NELEMENTS` values if waveforms exceed their configured limits.

## Calibration

The repository's `vendor/tpx3/4x2/` pair is deliberately named `synthetic` and
is opt-in. For physical operation, use names or site-local paths that identify
the detector and layout, with independently generated values for all eight
chips.
