# Eight-chip (2×4 / dual SPIDR) support

This document tracks IOC and driver changes for detectors with up to eight TimePix3 chips (e.g. two SPIDR boards × four chips).

## Implemented

- **IOC:** `load_chips.cmd` loads `Chips.template` for `CHIP0`…`CHIP7` (asyn `ADDR` 0–7). Included from `st_base.cmd`.
- **Rails:** `OperatingVoltage.template` instances `Pwr0`…`Pwr5` at asyn `ADDR` 0–5. `ADDR` 0–2 are the three `VDD`/`AVDD` readings from Serval `Health[0]`; `ADDR` 3–5 are from `Health[1]` when present (second SPIDR). When only one board exists, `Pwr3`–`Pwr5` read back zero.
- **Driver (`getDetector`, Serval 4):**
  - Merges all `Health[].ChipTemperatures` into one JSON array for `ChipTemps_RBV` (flat list, e.g. eight entries for two boards).
  - If multiple `Health` blocks: `VDD_RBV` / `AVDD_RBV` strings become a JSON array of per-board rail arrays (e.g. `[[r0,r1,r2],[r0,r1,r2]]`). Single-board keeps the previous flat array of three numbers.
  - **Boards:** `TPX3_BOARDS2_ID`, `TPX3_BOARDS2_IP`, `TPX3_BOARDS_CH5`…`CH8` from `Info.Boards[1]` when available (`CH5`…`CH8` are that board’s four chip JSON objects).
- **Database:** `ADTimePix3.template` adds `BChBoard2Id_RBV`, `IpAddr2_RBV`, `Chip5_RBV`…`Chip8_RBV`.
- **Mask / BPC indexing (`mask_io.cpp`):** For `numChips == 8` and **DetectorOrientation UP (0)** only, `pelIndex` and `bcp2ImgIndex` use a **rectangular mosaic**: BPC chip order `chip = Y_CHIP * xChips + X_CHIP` with `xChips` / `yChips` from `rowsCols()` (`RowLen` and chip count from Serval). Intra-chip pixel order matches **single-chip UP** (same convention as one quadrant of the existing 2×2 implementation).

## Mask BPC database (`MASK_BPC_NELEMENTS`)

`st_base.cmd` loads `MaskBPC.template` with `NELEMENTS=$(MASK_BPC_NELEMENTS)`. That macro **must** be defined in **`unique.cmd`** before the database load (after `< envPaths` and `< unique.cmd`). If it is missing, iocsh reports `macLib: macro MASK_BPC_NELEMENTS is undefined` and mask-related PVs never connect.

`unique.cmd` documents three standard sizes (pick **one** active `epicsEnvSet`):

| Chips | Typical mosaic | Image (px) | `MASK_BPC_NELEMENTS` |
|-------|----------------|------------|----------------------|
| 1 | 1×1 | 256×256 | 65536 |
| 4 | 2×2 | 512×512 | 262144 |
| 8 | 2×4 | 1024×512 | 524288 |

`envPaths` does **not** set this macro; it only notes that `unique.cmd` must define it.

## Verify on hardware

- **BPC ↔ image mapping:** The 8-chip branch assumes linear chip order matches Serval’s concatenated BPC and a uniform grid. If your `Layout` uses per-chip rotations like the 2×2 module, confirm masks and PixelConfigDiff on real data; extend orientations using `Layout` JSON if needed.
- **Phoebus / BOY:** Screens still embed four chip panels in places; add tabs, a chip selector, or extra embeds for `CHIP4`…`CHIP7`. Extend `TimePixDetectorVoltages.opi` (and BOB equivalents) if operators need `Pwr3`–`Pwr5` on screen.
- **Large images:** For 8-chip, set `MASK_BPC_NELEMENTS` to `524288` (or larger if `PixCount` is greater), and raise `EPICS_CA_MAX_ARRAY_BYTES` and ND plugin `NELEMENTS` if waveforms exceed limits.

## Calibration

Per-chip / per-geometry calibration policy is outside this IOC change; use naming or paths that include chip count and layout id.
