# Phoebus screens (`tpx3App/op/bob/`)

Operator displays for **ADServal** (Display Builder `.bob` / legacy `.opi` fragments). IOC boot uses the same **detector-family** idea under `iocBoot/iocTimePix/profiles/`; screen layout will align in **R1-7-2** (see root `RELEASE.md`).

## Current layout (R1-7-1)

```
bob/
  TimePix3.bob, TimePix3Detector.bob, TimePix3Status.*   # TPX3 main / tools (root)
  MediPix3/MediPix3.bob, Mpx3Status.bob, Acquire/Mpx3*  # MPX3 profile (partial)
  ADSetup.bob, ConnectionStatus.bob                     # shared (both families)
  Acquire/, Detector/, Mask/, Setup/, Measurement/       # mostly shared ($(P)$(R) macros)
  Emulator/                                             # shell + tpx3/mpx3 embed panels
  tpx3*.bob, ADtpx3stream*.bob                          # TPX3-only utilities
```

**MPX3** already lives partly under **`MediPix3/`** and references shared panels via `../`. **TPX3** screens mostly sit at **`bob/`** root — asymmetric naming vs `profiles/tpx3` / `profiles/mpx3` in boot.

Launch with macros **`P`** (prefix, e.g. `TPX3-TEST:` or `MPX3-TEST:`) and **`R`** (`cam1:`). **`MediPix3.bob`** hardcodes `P=MPX3-TEST:`; **`TimePix3.bob`** expects macros at Phoebus launch.

| Entry screen | Typical prefix | Notes |
|--------------|----------------|-------|
| `TimePix3.bob` | `TPX3-TEST:` | Main TPX3 areaDetector shell |
| `MediPix3/MediPix3.bob` | `MPX3-TEST:` (embedded) | Main MPX3 shell |
| `TimePix3Detector.bob` | `$(P)$(R)` | Deep detector / file / mask tools |
| `Emulator/emulator.bob` | `TPX3-TEST:` + `Emulator:` | Multi-family emulator IOC |

Legacy CS-Studio **`.opi`** tree: **`tpx3App/op/opi/`** (not updated with R1-7-1 boot changes; deprecation TBD).

## Planned layout (R1-7-2, Option C)

Mirror `iocBoot/iocTimePix`:

```
bob/
  TimePix3.bob, MediPix3.bob     # thin launchers → profiles/<family>/main.bob
  common/                        # ADSetup, ConnectionStatus, shared Acquire/Detector/Mask/…
  profiles/
    tpx3/                        # TPX3-only: PrvHst, stream/histogram BOBs, status toolbar
    mpx3/                        # git mv from MediPix3/ (Mpx3* acquire/detector panels)
    tpx4/                        # placeholder when hardware lands
  Emulator/                      # unchanged
```

**Contract** (same as IOC profiles):

1. Root launchers set **`P`**, **`R`**, **`pathADCore`** only.
2. **`common/`** — panels safe for all families (PV-driven via macros).
3. **`profiles/<family>/`** — navigation shell, status toolbar, family-only acquire/detector panels.
4. Cross-links: profile → `../../common/...`; keep root launcher paths stable for site bookmarks.

## Family-specific vs shared (today)

| Shared (`common/` candidate) | Family-specific |
|------------------------------|-----------------|
| `ADSetup.bob`, `ConnectionStatus.bob` | `PrvHstHistogram.bob`, `ADtpx3stream*.bob` (TPX3) |
| `Acquire/ADCollect.bob`, `FileStatus.bob` | `MediPix3/Acquire/Mpx3*.bob` (MPX3 dual-threshold, Image[], HDF) |
| `Detector/TimePixDetectorHealth.bob` (generic PVs) | `MediPix3/Detector/Mpx3DetectorConfig.bob` |
| `Mask/`, `Setup/FileBPCdacs.bob` | `Mpx3Status.bob` vs `TimePix3Status.bob` toolbars |
| `Acquire/DetectorConfig.bob` (TPX3-oriented) | — |

Do **not** duplicate shared panels per family unless PV sets differ (already forked: **`Mpx3DetectorConfig`** vs root **`DetectorConfig`**).

## R1-7-2 migration phases

1. **`git mv MediPix3` → `profiles/mpx3`**; fix embed paths; add root **`MediPix3.bob`** shim if needed.
2. Move TPX3-only root BOBs into **`profiles/tpx3/`**; extract **`common/`**; update embed paths.
3. Standardize **`P`/`R`** macros on both main shells (remove hardcoded `MPX3-TEST:`).
4. **`bob/README.md`** + `RELEASE.md` update; optional note in site Phoebus **`settings.ini`**.

## Site overlays

Beamline-specific display forks belong outside git (or gitignored `*_site.bob`), same pattern as `profiles/<family>/init/paths_site.cmd` for IOC boot.
