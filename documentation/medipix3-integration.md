# Medipix3 integration (unified ADTimePix3 driver)

Development branch: `medipix3-integration` on [kgofron/ADTimePix3](https://github.com/kgofron/ADTimePix3).

Planning and scope live in the separate [ADMediPix3](https://github.com/kgofron/ADMediPix3) repository.

## Runtime detector family

After `GET /detector`, the driver classifies the connected hardware:

| Signal | Medipix3 (MPX3) | Timepix3 (TPX3) |
|--------|-----------------|-----------------|
| `ChipType` | `MPX3` | `TPX3` |
| `MpxType` | 5 | 6 |
| `ChipboardId` prefix | `51…` | `41…` |

EPICS PVs (prefix `cam1:`):

- `DetectorFamily_RBV` — 0=Unknown, 1=TPX3, 2=MPX3
- `ChipType_RBV` — Serval `Info.ChipType`
- `CapTdc_RBV`, `CapTofHist_RBV`, `CapDualPreview_RBV`, `CapImgThresholds_RBV` — capability flags

## Medipix3 IOC profile

Use the Medipix startup profile instead of the default Timepix3 IOC:

```bash
cd iocs/tpx3IOC/iocBoot/iocTimePix
./st_mpx3.sh
```

Or from iocsh: `st_mpx3.cmd` (includes `unique_mpx3.cmd` and `init_detector_mpx3.cmd`).

Defaults:

- PV prefix `MPX3-TEST:`
- asyn port `MPX3`
- 512×512 mask size (`MASK_BPC_NELEMENTS=262144`)
- two-layer preview on TCP ports 8088 (frame) and 8089 (running sum)
- `count` image mode with Serval `Thresholds[]` when family is MPX3

## Emulator workflow

1. Start Serval with a Medipix3 emulator profile (see ADMediPix3 `configs/serval/`).
2. Build this module: `make -j` from the module root.
3. Start IOC with `st_mpx3.sh`.
4. Confirm `MPX3-TEST:cam1:DetectorFamily_RBV` = `MPX3` after connect.

## v1 scope (out of scope)

Spectral mode, TDC, and Timepix-only ToF histogram paths are flagged off for MPX3 via capability PVs. See ADMediPix3 `docs/implementation-plan.md`.
