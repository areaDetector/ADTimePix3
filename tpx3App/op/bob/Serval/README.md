# Serval IOC Phoebus display

Shared **Serval** process control for Timepix3 / Medipix3 (not camera `cam1:` PVs).

| File | Role |
|------|------|
| `tpx3serval.bob` | Serval IOC control panel |

## PV macros

- **Default**: `P=SERVAL-TEST:`, `R=Serval:` → e.g. `SERVAL-TEST:Serval:START`
- Matches `/epics/iocs/serval/iocBoot/ioctpx3serval/st.cmd` (`Sys=SERVAL-TEST:`)
- Beamline override: open with `P=SERVAL-$(BL):` (e.g. `SERVAL-BL7:`) and set IOC `Sys` to match
- **Do not inherit camera `P`** — every Serval `open_display` should pass `P`/`R` explicitly

## Sync

Source of truth: `/epics/iocs/serval/tpx3servalApp/op/bob/tpx3serval.bob` → this directory.

## Opened from

- `profiles/tpx3/Acquire/DetectorConfig.bob` (Vendor SW)
- `profiles/mpx3/Mpx3Status.bob`
