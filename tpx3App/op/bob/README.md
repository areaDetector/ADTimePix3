# Phoebus screens (`tpx3App/op/bob/`)

Operator displays for **ADServal** (Display Builder `.bob` / legacy `.opi` fragments). Layout mirrors IOC boot **Option C** (`common/` + **`profiles/<family>/`**); see `iocBoot/iocTimePix/profiles/README.md`.

## Layout (R1-7-2)

```
bob/
  TimePix3.bob, MediPix3.bob       # thin launchers (macros P, R, pathADCore)
  MediPix3/MediPix3.bob            # legacy bookmark shim → profiles/mpx3/main.bob
  common/                          # shared panels ($(P)$(R) macros)
    ADSetup.bob, ConnectionStatus.bob
    Acquire/, Detector/, Mask/, Setup/, Measurement/
  profiles/
    tpx3/                          # main.bob, TimePix3Detector, status toolbar, TPX3 acquire
    mpx3/                          # main.bob, Mpx3Status, Mpx3* acquire/detector panels
    tpx4/                          # placeholder README
  Emulator/                        # shell + tpx3/mpx3 embed panels (EMU-TEST:)
  Serval/                          # shared Serval process IOC panel (SERVAL-TEST:)
```

## Launch

| Entry screen | Default prefix | Profile body |
|--------------|----------------|--------------|
| `TimePix3.bob` | `TPX3-TEST:` | `profiles/tpx3/main.bob` |
| `MediPix3.bob` | `MPX3-TEST:` | `profiles/mpx3/main.bob` |
| `MediPix3/MediPix3.bob` | `MPX3-TEST:` | same (legacy path) |
| `Emulator/emulator.bob` | `EMU-TEST:` | shared TPX3/MPX3 emulator shell |
| `Serval/tpx3serval.bob` | `SERVAL-TEST:` | shared Serval process IOC |

Set macros **`P`**, **`R`** (`cam1:`), and **`pathADCore`** at launch or edit defaults in the root launcher.
Mask image / PixelConfig panels use **`$(P)$(R)`** (same as `MaskStatus`); do not pass `Sys`/`Dev`/`Cam`.
Emulator opens with explicit **`P=EMU-TEST:`** / **`R=Emulator:`** (not camera `P`); override with `EMU-$(BL):` to match a beamline IOC.
Serval opens with explicit **`P=SERVAL-TEST:`** / **`R=Serval:`**; override with `SERVAL-$(BL):`.

## Profile contract

1. Root launchers embed **`profiles/<family>/main.bob`** and pass macros.
2. **`common/`** — panels safe for all families.
3. **`profiles/<family>/`** — main shell, status toolbar, family-only acquire/detector panels.
4. Cross-links from a profile use **`../../common/...`**, **`../../Emulator/...`**, or **`../../Serval/...`**.

## Family-specific vs shared

| `common/` | `profiles/tpx3/` | `profiles/mpx3/` |
|-----------|------------------|------------------|
| ADSetup, ConnectionStatus, ADCollect, Mask, chip health | PrvHstHistogram, PrvImgMonitor, DetectorConfig, stream BOBs | Mpx3Preview/Image/HDF panels, Mpx3DetectorConfig |
| ServerFileWriter, WriteFiles, ImgAccumulation | TimePix3Status toolbar (incl. Emulator/Serval), TimePix3Detector, TimePix3Alarm/API | Mpx3Status toolbar, Mpx3Alarm |

Legacy CS-Studio **`.opi`**: **`tpx3App/op/opi/`** (not updated in R1-7-2).

## Site overlays

Beamline-specific display forks: gitignored `*_site.bob` or local Phoebus paths — do not fork whole profiles in git.

## Adding TPX4

```bash
cp -r profiles/tpx3 profiles/tpx4
# Edit profiles/tpx4/main.bob, unique acquire panels
# Add bob/Tpx4.bob launcher at root
```
