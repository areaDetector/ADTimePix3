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
  Emulator/                        # shell + tpx3/mpx3 embeds (inherited detector P)
  Serval/                          # shared Serval panel (inherited detector P)
```

## Launch

| Entry screen | Direct-open macros | Profile body |
|--------------|--------------------|--------------|
| `TimePix3.bob` | `P=TPX3-TEST:`, `R=cam1:` | `profiles/tpx3/main.bob` |
| `MediPix3.bob` | `P=MPX3-TEST:`, `R=cam1:` | `profiles/mpx3/main.bob` |
| `MediPix3/MediPix3.bob` | `P=MPX3-TEST:`, `R=cam1:` | same (legacy path) |
| `Emulator/emulator.bob` | `P=TPX3-TEST:`, `R=Emulator:` | shared TPX3/MPX3 emulator shell |
| `Serval/tpx3serval.bob` | `P=TPX3-TEST:`, `R=Serval:` | shared Serval process IOC |

Callers may override **`P`**, **`R`**, and **`pathADCore`** at launch. The root launchers use fallback syntax so caller values win while direct opens use the defaults shown above. Profile `main.bob` files inherit **`P`/`R`** from the launcher; do not hardcode beamline prefixes there.
Mask image / PixelConfig panels use **`$(P)$(R)`** (same as `MaskStatus`); do not pass `Sys`/`Dev`/`Cam`.
ADTimePix3, Serval, and emulator IOCs are configured independently but use the same detector system prefix. TPX3 launch actions pass **`P=$(P=TPX3-TEST:)`**; MPX3 actions pass **`P=$(P=MPX3-TEST:)`**. The caller's `P` therefore wins, while a directly opened family screen retains a useful test fallback. Service identity remains in **`R=Serval:`** or **`R=Emulator:`**.

The emulator shell and TPX3 embed use the TPX3 fallback when opened directly; the MPX3 embed uses the MPX3 fallback. No Serval or emulator child display defines a display-level `P`, because that would override an incoming caller macro.

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

## Validation

Run `python3 test/validate_bob_prefix_macros.py` from the repository root to verify every entry screen, Serval/emulator child, and launcher macro contract.

## Site overlays

Beamline-specific display forks: gitignored `*_site.bob` or local Phoebus paths — do not fork whole profiles in git.

## Adding TPX4

```bash
cp -r profiles/tpx3 profiles/tpx4
# Edit profiles/tpx4/main.bob, unique acquire panels
# Add bob/Tpx4.bob launcher at root
```
