# Serval IOC Phoebus display

Shared **Serval** process control for Timepix3 / Medipix3 (service identity is `R=Serval:`, not camera `R=cam1:`).

| File | Role |
|------|------|
| `tpx3serval.bob` | Serval IOC control panel |

## PV macros

- **Prefix**: `$(P=TPX3-TEST:)` inherits `P` from the calling display and uses `TPX3-TEST:` only when `P` is undefined
- **Record**: `R=Serval:` → the direct-open lab default START PV is `TPX3-TEST:Serval:START`
- The fallback matches the IOC's `TPX3_PREFIX=TPX3-TEST:` default
- For beamline use, configure the Serval IOC with the ADTimePix3 system prefix and pass that same prefix as `P`; TPX3 launchers use `$(P=TPX3-TEST:)` and MPX3 launchers use `$(P=MPX3-TEST:)`
- The child screen deliberately has no display-level `P`, so an incoming caller macro is not overridden

## Sync

Source of truth: `/epics/iocs/serval/tpx3servalApp/op/bob/tpx3serval.bob` → this directory.

## Opened from

- `profiles/tpx3/Acquire/DetectorConfig.bob` (Vendor SW)
- `profiles/tpx3/TimePix3Status.bob`
- `profiles/mpx3/Mpx3Status.bob`
- `profiles/mpx3/Detector/Mpx3DetectorConfig.bob`
