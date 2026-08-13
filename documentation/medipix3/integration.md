# Medipix3 integration (unified ADServal driver)

**ADServal** is the product name for the unified Serval driver; the EPICS module and repository remain **ADTimePix3** ([NAMING.md](../NAMING.md)).

## Development history

Medipix3 was integrated for release **R1-7-0** (driver **1.7.0**, August 2026):

- Merged to [areaDetector/ADTimePix3](https://github.com/areaDetector/ADTimePix3) via PR #15 from [kgofron/ADTimePix3](https://github.com/kgofron/ADTimePix3) (tag **R1-7-0**).
- Feature work lived on branch **`medipix3-integration`** on kgofron/ADTimePix3 (removed after merge).
- Local developer backup: **`/epics/support2/areaDetector/ADTimePix3_mpx3.bkp`** — not an official release tree; use **areaDetector/ADTimePix3** `master` / **R1-7-0** as reference.
- Planning notes: [ADMediPix3](https://github.com/kgofron/ADMediPix3) (separate repo; scope absorbed into the unified driver).

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

## Impact on Timepix3 operation

Medipix3 work is **additive**: one driver binary serves both families. After `GET /detector`, the driver sets `detectorFamily_` and branches MPX3-only logic; it does **not** replace the Timepix3 IOC profile or calibration paths.

**For normal TPX3 use** (`st.cmd` / `st_base.cmd`, TPX3 hardware, default PVs), behaviour should match pre-integration operation. Use **`st_mpx3.cmd` only with MPX3** — that profile enables dual threshold, integrated preview plugins, and `vendor/mpx3/` calibration by design.

### What stays separate

| Layer | Timepix3 (TPX3) | Medipix3 (MPX3) |
|--------|-----------------|-----------------|
| IOC startup | `st.cmd`, `init_detector_*.cmd` | `st_mpx3.cmd`, `init_detector_mpx3.cmd` |
| Calibration | `vendor/tpx3-*` (via `init_detector_paths.cmd`) | `vendor/mpx3/` |
| Phoebus | `TimePix3.bob`; legacy `op/opi/*.opi` (CSS) | `MediPix3/*.bob` |
| NDArray / PVA | Pva1; driver addrs **0**–**3** (typical) | Pva1–Pva8; addrs **0**, **1**, **8**–**13** |

MPX3-only ND plugins (`imageTh1`, `imageInt1`, band-pass on addr 9/12, etc.) are loaded only in **`st_mpx3.cmd`**, not in the default Timepix3 startup.

### Driver behaviour gated on family

| Feature | TPX3 | MPX3 |
|---------|------|------|
| `applyFamilyDefaults()` (GainMode, `PrvImgThs` 0–7, …) | Skipped | Applied once after connect |
| Serval `PUT /detector/config` | TDC, `TriggerDelay`, `PeriphClk80`, … | `BothCounters`, `GainMode`, `IDelayConfig`, … |
| `BothCounters` + Continuous trigger guard | No effect (`detectorFamily_` check) | Blocks acquire / auto-switches mode |
| `emitPreviewThresholdDiff()` (T0−T1 band) | No effect unless `BothCounters=Yes` | Active when `BothCounters=Yes` |
| Capability PVs | `CapTdc=1`, `CapTofHist=1`, … | TDC/ToF off; `CapImgThresholds=1` |

Code: `detectDetectorFamily()` / `applyFamilyDefaults()` in `tpx3App/src/detector_family.cpp` and `ADTimePix.cpp`; config split in `serval_http.cpp` (`detectorFamily_ != MPX3` vs MPX3 branch).

### Shared code paths (both families)

These run for all detectors but are intended to remain backward compatible for TPX3:

1. **Refactored preview** (`processPreviewJsonimageLine` in `serval_stream.cpp`) — `thresholdID=0` → NDArray addr **0** (unchanged for typical TPX3 preview). Addr **8** is used only when Serval sends `thresholdID=1` (MPX3 dual-counter stream).
2. **Preview TCP lifecycle** — disconnect on `acquireStop`, `syncTcpStreamEndpoints()`, port rotation when the configured port is busy. Improves repeat acquire for both families.
3. **`prvImg1Worker`** — starts only when `WritePrvImg1` has a TCP path. Standard TPX3 profile usually leaves `WritePrvImg1=0`.
4. **New DB records** in `Server.template` (`BothCounters`, `GainMode`, `PrvImgThreshDiffClip`, …) — loaded on every IOC; inactive unless written.
5. **`NDARRAY_MAX_ADDR = 14`** — larger internal callback table for all families; TPX3 sites that only use addrs 0–3 are unaffected in routing.

### Regression check (recommended before merge)

On a TPX3 or Timepix3 emulator instance, with the **standard** IOC profile:

1. `DetectorFamily_RBV` = TPX3 after connect.
2. Acquire; confirm preview on **Pva1** (addr 0).
3. ToF / histogram paths if used at your site.
4. Stop → start acquire (preview TCP stability).

Do **not** enable MPX3-only PVs (`BothCounters`, integrated `WritePrvImg1` with extra ND plugins) on a TPX3 IOC unless you intentionally test cross-family configuration.

## Medipix3 IOC profile

Use the Medipix startup profile instead of the default Timepix3 IOC:

```bash
cd iocs/tpx3IOC/iocBoot/iocTimePix
../../bin/linux-x86_64/tpx3App st_mpx3.cmd
```

(`st_mpx3.cmd` has a `tpx3App` shebang; you can also run `./st_mpx3.cmd` if executable.) **libcpr** is built into the module (`tpx3Support/cpr` → `lib/linux-x86_64/libcpr.so`); `tpx3App` RUNPATH resolves it — no separate `LD_LIBRARY_PATH` to `cprSrc` is needed.

Profile contents: `unique_mpx3.cmd`, `init_detector_mpx3.cmd`, and MPX3 ND/PVA wiring in `st_mpx3.cmd`.

Defaults (from `init_detector_mpx3.cmd`):

- PV prefix `MPX3-TEST:`
- asyn port `MPX3`
- 512×512 mask size (`MASK_BPC_NELEMENTS=262144`)
- preview on TCP **8088** / **8089** (`PrvImgThs` / `PrvImg1Ths` **0,1**, jsonimage)
- full-rate **Image[]** TCP **8086** configured but **`WriteImg=0`** (opt-in via `init_detector_img_mpx3.cmd`)
- **`BothCounters=Yes`**, **`TriggerMode=AutoTrgSt_TmrSp` (4)**, **4 triggers**, **0.5 s** period (Erik Accos recipe)
- `PrvImg1` (integrated preview on 8089) — **`prvImg1WorkerThread`**, NDArray addr **10**/**11**, PVA **Pva3**/**Pva4**
- BPC/DACS: `$(ADTIMEPIX)/vendor/mpx3/eq-01.bpc` and `eq-01.dacs` (uploaded in `init_detector_hw_mpx3.cmd`)

**Phoebus:** main screen is **`tpx3App/op/bob/MediPix3/MediPix3.bob`** (subdirectory `MediPix3/`, not `op/bob/MediPix3.bob`). Defaults **`P=MPX3-TEST:`**, **`R=cam1:`**. Related: destination writer **`Acquire/Mpx3ServerFileWriter.bob`** embeds **Preview** (`Mpx3PreviewChannels.bob`, 8088/8089) and **Image[]** (`Mpx3ImageChannels.bob`: Img[0] 8086 + Img[1] file/8087 + HDF status strip); **`Acquire/Mpx3HdfImgConfig.bob`** (HDFImgT0/T1 path/Capture); **`Acquire/Mpx3ImgMonitor.bob`** (Pva7/Pva8 — low rate only); live preview images in `Acquire/Mpx3PrvImgMonitor.bob`; detector config in `Detector/Mpx3DetectorConfig.bob`. **`Detector/TimePixDetectorHealth.bob`** and **`TimePixDetectorVoltages.bob`** (under `op/bob/Detector`) cross-link for health readbacks. For `PrvImgThs` / `ImgThs` / `Img1Ths` (CHAR waveform), use the Phoebus text field or IOC `dbpf` — plain `caput` with a quoted string clears the array.

**Image / profile Y-origin:** NDArray and `NDStats` profiles use **top-left, Y down** (see [COORDINATE_MAP.md](../COORDINATE_MAP.md)). Row/column profiles for the MPX3 IOC are loaded via **`$(ADCORE)/iocBoot/stats_profiles.cmd`** (`NDStatsProfiles.template`) after `commonPlugins.cmd`; `init_detector_hw_mpx3.cmd` processes `StatsProfInit_` after `iocInit`. Facility ADet image+profile `.bob` screens (`/epics/GUI/SNS/bob`) are adjusted so plot axes match that convention (`$(P)$(R)Cal:…`).

## Emulator workflow

1. Start Serval with a Medipix3 emulator profile (see ADMediPix3 `configs/serval/`).
2. Build this module: `make -j` from the module root.
3. Start IOC: `../../bin/linux-x86_64/tpx3App st_mpx3.cmd` from `iocBoot/iocTimePix`.
4. Confirm `MPX3-TEST:cam1:DetectorFamily_RBV` = `MPX3` after connect.
5. Confirm startup pushed config: `BothCounters_RBV`, `TriggerMode`, `WriteData` (or `dbpf` `WriteData=1` if Serval was late).
6. Start acquisition: `caput MPX3-TEST:cam1:Acquire 1`

## Calibration files (`vendor/mpx3/`)

Default MPX3 BPC/DACS ship under `vendor/mpx3/` (not `vendor/tpx3-demo.*`):

| File | Role |
|------|------|
| `eq-01.bpc` | Pixel config (524288 bytes for 8-chip layout) |
| `eq-01.dacs` | Threshold[0..7] and chip DACs per Medipix3 format |

IOC paths (via `init_detector_paths_mpx3.cmd`):

- `BPCFilePath` = `$(ADTIMEPIX)/vendor/mpx3/`
- `DACSFilePath` = `$(ADTIMEPIX)/vendor/mpx3/`

Upload on startup via `WriteBPCFile=1` and `WriteDACSFile=1` in `init_detector_hw_mpx3.cmd`. SERVAL must resolve these paths on **its** host. For a different calibration set, change `BPCFileName` / `DACSFileName` or point paths at a site-specific directory.

## ASI hardware checkout (Erik, June 2026)

Erik’s guidance for **first physical Medipix3 bring-up** and **equalization** (email, June 2026). This is **not** the same profile as the emulator / Accos **dual-counter** recipe in `init_detector_hw_mpx3.cmd` — use one or the other depending on the test goal.

| Goal | IOC script | Key settings |
|------|------------|--------------|
| Equalization / first hardware checkout | `init_detector_hw_mpx3_equalize.cmd` | Single threshold, 12-bit, 20 frames, super-low gain |
| Dual-counter live view / IXS band-pass (emulator or post-cal) | `init_detector_hw_mpx3.cmd` | `BothCounters=1`, thresholds 0+1, 4 triggers @ 0.5 s |

### Erik’s equalization checklist

| Item | Erik’s recommendation | EPICS / notes |
|------|----------------------|---------------|
| Thresholds | **One** threshold, **12-bit** depth | `BothCounters=0`, `PixelDepth=12`, `PrvImgThs` / `PrvImg1Ths` = `0` |
| Timing | **495 ms** shutter high, **5 ms** shutter down | `AcquireTime=0.495`, `AcquirePeriod≥0.5` (period must cover exposure + shutter down) |
| Frames | **20** frames | `NumImages=20`, `ImageMode=1` (Multiple) |
| Gain | **Super-low gain** mode | `GainMode` PV — **confirm exact Serval string with ASI** (`LGM` is a placeholder in `init_detector_hw_mpx3_equalize.cmd`) |
| Threshold level | DAC **~50–90** so noise pixels are visible | Set via **Accos equalization** or chip DAC / `.dacs` — not automated in the IOC script |
| Bias / sensor | **100 V**, Si **300 µm**, **positive** polarity | `BiasVolt=100`, `Polarity=0` (Positive); see detector delivery sheet if different |
| Calibration | **Equalization with Accos** | Run Accos first; then upload resulting BPC/DACS via `WriteBPCFile` / `WriteDACSFile` |
| ChargeSumming / Colour | **Inactive** for now | `ChargeSumming=0`, `Colour=0` — Erik to follow up with slides / meeting |
| Integrated preview | **`IntegrationMode: last`**, not sum | Already IOC default: `PrvImg1IntgMode=2` on 8089 |

### Live view vs full-rate saving (Erik)

| Use case | Serval path | MPX3 IOC default |
|----------|-------------|------------------|
| Beamline live view | Preview (frame + integrated) + count histogram | `WritePrvImg=1` (8088), `WritePrvImg1=1` (8089) |
| Data saving at full rate | `Image[]` | TCP **8086**, `WriteImg=0` until needed — `< init_detector_img_mpx3.cmd` |

Medipix3 has **no Timepix3-style raw `.tpx3` stream**. Highest practical rates (fast PC + SSD): **~2000 Hz** (12-bit continuous); **~750 Hz** (24-bit or dual 12-bit counter). Sequential shutter down: **~5 ms** safe, **~2 ms** minimum; **0.5 ms** (12-bit) / **1.3 ms** (24-bit) in other modes.

## Family TCP port map (TPX3 / MPX3 / TPX4)

Convention: port = **8084 + slot**. Documented for the unified driver; MPX3 Phase A adopts Image **8086** now. TPX3 Raw primary remains **8085** in legacy `init_detector_paths.cmd` until a dedicated migration (target Raw[0]=**8084**).

| Port | Slot | Role | MPX3 default | TPX3 today | TPX4 (future) |
|-----:|------|------|--------------|------------|---------------|
| 8080 | — | Serval HTTP | — | — | ASI examples |
| 8081 | — | Serval HTTP | yes | yes | alternate |
| 8084 | Raw[0] | Raw TCP primary | unused (file) | migrate target | yes |
| 8085 | Raw[1] | Raw TCP secondary | unused | legacy Raw[0] / Raw1 | yes |
| **8086** | **Image[0]** | Full-rate Image TCP | **paths ready, WriteImg=0** | legacy often 8087 | yes |
| 8087 | Image[1] | Second Image[] (file or TCP companion) | **paths + UI** (`WriteImg1=0`); file-first; TCP needs reader (`img1Worker` deferred) | optional Img1 | optional |
| **8088** | Preview[0] | Preview frame | **On** | optional | yes |
| **8089** | Preview[1] | Preview integrated | **On** | often PrvImg | yes |
| 8451 | — | Preview histogram | off | yes | TBD |

**Dual threshold (BothCounters):** T0 and T1 share **one** TCP socket (e.g. Preview 8088 or Image 8086), demuxed by jsonimage **`thresholdID`**. Ports 8086/8087 are Image channel 0 vs 1 (frame vs optional companion), **not** T0 vs T1. Image[1] defaults to **`file:/media/nvme/img1`** with `IntgSize=-1` / `last` (Preview-8089-like role on Serval); switch path to `tcp://listen@localhost:8087` and `Img1FileFmt=jsonimage` only when a TCP consumer exists.

## Full-rate Image mode (Phase A)

Preview stays the operator path (`PrvPeriod` throttle). `Image[]` is a separate Serval destination for every frame (no Period). Keep them on **different ports**.

### Destination shape (when WriteImg=1)

```json
{
  "Image": [{
    "Base": "tcp://listen@localhost:8086",
    "FilePattern": "f%MdHms_",
    "Format": "jsonimage",
    "Mode": "count",
    "Thresholds": [0, 1],
    "IntegrationSize": 1,
    "StopMeasurementOnDiskLimit": false,
    "QueueSize": 1024
  }],
  "Preview": {
    "Period": 0.5,
    "SamplingMode": "skipOnFrame",
    "ImageChannels": [
      { "Base": "tcp://listen@localhost:8088", "Format": "jsonimage", "Mode": "count",
        "Thresholds": [0, 1], "IntegrationSize": 1, "QueueSize": 16 },
      { "Base": "tcp://listen@localhost:8089", "Format": "jsonimage", "Mode": "count",
        "Thresholds": [0, 1], "IntegrationSize": -1, "IntegrationMode": "last", "QueueSize": 16 }
    ]
  }
}
```

Serval requires the same **Mode** on all output channels (manual limitation).

### Enable / disable

Paths are set in `init_detector_paths_mpx3.cmd` with `WriteImg=0`. To turn full-rate Image on:

```bash
# iocsh:
< init_detector_img_mpx3.cmd
# then acquire — imgWorker connects when WriteImg=1, tcp path, ImgAccumulationEnable=1
```

Disable: `dbpf WriteImg 0` then `WriteData 1`. Do **not** point Phoebus PVA at full-rate Image for 750–2000 Hz — use HDF5 plugins (`HDFImgT0` / `HDFImgT1`) or accumulation.

### Phase C — HDF5 / rate soak

Plugins in `st_mpx3.cmd` (fixed address at configure):

| Plugin | NDArray addr | Threshold | PV prefix |
|--------|-------------:|-----------|-----------|
| `FileHDFImgT0` | **1** | T0 | `HDFImgT0:` |
| `FileHDFImgT1` | **13** | T1 | `HDFImgT1:` |

```bash
# Host shell once:
mkdir -p /tmp/mpx3_hdf

# iocsh (restart IOC after rebuild so HDF plugins load):
< init_detector_img_mpx3.cmd
< init_detector_hdf5_img_mpx3.cmd
dbpf("$(PREFIX)cam1:Acquire","1")              # latch dimensions (Capture not armed yet)
# wait until DA_IDLE / ImgFrameNumber advances, then:
< init_detector_hdf5_img_mpx3_arm.cmd          # Capture=1 (needs ≥1 array first)
dbpf("$(PREFIX)cam1:Acquire","1")              # Stream writes up to NumCapture
```

If you arm Capture before any Image NDArray, NDFile logs:  
`ERROR, must collect an array to get dimensions first` — acquire once, then arm.

If HDF5 logs `H5Fcreate(): invalid file name` / empty path after colon: **`FileTemplate` was empty** (common after autosave). Re-run `init_detector_hdf5_img_mpx3.cmd` (sets `%s%s_%3.3d.h5` and path with trailing `/`), confirm:

```bash
caget -S MPX3-TEST:HDFImgT0:FileTemplate_RBV MPX3-TEST:HDFImgT0:FilePath_RBV
# expect: %s%s_%3.3d.h5   and   /tmp/mpx3_hdf/
```

Check during/after acquire:

```bash
caget MPX3-TEST:cam1:ImgAcqRate_RBV MPX3-TEST:cam1:ImgFrameNumber_RBV
caget MPX3-TEST:HDFImgT0:Capture_RBV MPX3-TEST:HDFImgT0:NumCaptured_RBV
caget MPX3-TEST:HDFImgT1:NumCaptured_RBV
ls $(MPX3_HDF_PATH=/tmp/mpx3_hdf)
```

**Soak ladder:** start with current Accos timing (0.5 s, 4 frames) → raise `NumImages` / shorten `AcquirePeriod` on emulator → hardware toward ~750 Hz (24-bit / dual) or ~2000 Hz (12-bit). Raise `FileHDFImgT*: queue` in `st_mpx3.cmd` if dropped frames. Keep **Pva7/Pva8** disabled at high rate (`init_detector_hdf5_img_mpx3.cmd` does this).

**Deferred:** Image[1] **`img1Worker`** (IOC TCP consumer for 8087). Config/UI for Image[1] is available (file or TCP path, `WriteImg1`); Serval file writes work without a worker. Do not enable TCP 8087 without a reader.

### Image mode phase status

| Phase | Work |
|-------|------|
| **A** | ~~IOC TCP Image destination + docs~~ **done** (8086, opt-in `WriteImg`) |
| **B** | ~~Img `thresholdID` demux~~ **done** — T0→addr **1** (Pva7), T1→addr **13** (Pva8); `ImgThresholdID_RBV`; accumulate T0 only |
| **C** | ~~HDF5 plugins + soak recipe~~ **done** (scaffolding); run soak on emulator/hardware as rates allow |

**Verify Phase B after rebuild/restart:** enable Image (`< init_detector_img_mpx3.cmd`), acquire, then:

```bash
caget MPX3-TEST:cam1:ImgThresholdID_RBV MPX3-TEST:cam1:ImgFrameNumber_RBV
# During BothCounters acquire, ImgThresholdID_RBV should alternate 1 then 0 per trigger.
# Low-rate check: pva://MPX3-TEST:Pva7:Image (T0) and Pva8:Image (T1). Disable PVA at high Hz.
```

### Optional equalization startup

Paths still come from `init_detector_paths_mpx3.cmd`. For first hardware checkout, **replace** the hardware push in `init_detector_mpx3.cmd` temporarily:

```bash
# iocsh after iocInit, or edit init_detector_mpx3.cmd for one session:
< init_detector_paths_mpx3.cmd
< init_detector_hw_mpx3_equalize.cmd
```

After equalization, restore the dual-counter profile:

```bash
< init_detector_hw_mpx3.cmd
```

### Known follow-ups on hardware

- **First-frame artefacts:** physical Timepix3 can show a corrupt first frame after calibration load; emulator may show a brief Signed diff flicker on Pva6 — treat both as hardware/emulator follow-ups.
- **`GainMode` string** for “super low gain” — pending Erik confirmation.
- **ChargeSumming / Colour** — keep off until ASI review.

## Serval channel model (Preview vs Image)

Serval `GET http://localhost:8081/` shows `Server.Destination`:

| Serval path | IOC consumer | MPX3 default |
|-------------|--------------|--------------|
| `Preview.ImageChannels[0]` (`PrvImg`) | Yes — `prvImgWorker` TCP client, NDArray/PVA | `WritePrvImg=1`, TCP 8088 |
| `Preview.ImageChannels[1]` (`PrvImg1`) | Yes — `prvImg1Worker` TCP client | `WritePrvImg1=1`, TCP 8089 |
| `Image[]` (full-rate) | `imgWorker` when TCP + accumulation | TCP **8086**, `WriteImg=0` (opt-in `init_detector_img_mpx3.cmd`) |

The reference `serval_mpx3.json` and the **Serval manual** (destination example, §4 / pp. 18–19) configure **two preview TCP streams**: current frame and an image **integrated from the start of the measurement**. The MPX3 IOC profile (`st_mpx3.cmd`) enables **both** channels: `WritePrvImg=1` on TCP **8088** (`prvImgWorker`) and `WritePrvImg1=1` on TCP **8089** (`prvImg1Worker`). Each stream must have an IOC TCP client during acquire — if Serval pushes to 8089 with no reader, Serval logs **`Preview buffer full`** and repeat acquire can fail.

**Serval manual** (`20251202_ASIServer_TPX3_manual_V4.1.3.pdf`, table 4.3, pp. 19–20): `IntegrationSize` **0 or 1** = no integration; **-1** = integrate all preview samples **from measurement start**; **2…32** = integrate over the last *n* images. `IntegrationMode` is **sum**, **average**, or **last**. ASI recommends **`last`** on 8089 (not sum) — Serval sum integration is costly at higher frame rates. IOC default: `PrvImg1IntgMode=2` (**last**).

After a clean IOC start, confirm Serval shows both preview channels and the IOC has them enabled:

```bash
curl -s http://localhost:8081/ | python3 -m json.tool | grep -A6 ImageChannels
caget MPX3-TEST:cam1:WritePrvImg MPX3-TEST:cam1:WritePrvImg1
```

Both should read **1** after `init_detector_paths_mpx3.cmd` and `WriteData=1`.

**If you disable integrated preview:** set `WritePrvImg1=0`, run `WriteData=1`, and restart acquire so Serval stops binding 8089. Leaving 8089 enabled in Serval destination without an IOC reader will fill the preview queue.

**Autosave:** if `auto_settings.sav` disagrees with the intended profile (e.g. saved with `WritePrvImg1=0` while you need integrated preview), fix the PVs and `WriteData=1`, or delete/rewrite the autosave file once.

## IOC startup warnings

On first boot (before `WriteData=1`), Serval may log many:

`Failed HTTP request GET /server/destination … Destination is not set.`

This is expected: init `dbpf` on `WriteRaw` / `WritePrvImg` / … PVs triggers a readback from Serval before `init_detector_hw_mpx3.cmd` pushes the destination. Harmless; it stops after `WriteData=1`.

## How Medipix3 preview images differ from Timepix3

**Vendor notes (ASI):** With **`BothCounters`** enabled, Serval sends **two consecutive jsonimage messages per trigger** on TCP **8088** (`thresholdID=1` then `0`); Accos and the EPICS driver demux by **`thresholdID`** in the header. That is separate from the **frame vs integrated-preview** split on two TCP ports (8088 / 8089). See **[preview-dual-threshold.md](preview-dual-threshold.md)** for Erik’s confirmed Accos behaviour and the implementation plan.

| Concept | MPX3 (Medipix3) | TPX3 (Timepix3) |
|---------|-----------------|-----------------|
| Counter / threshold indices | `Thresholds: [0..7]` — eight virtual counters | Often ToT/TDC modes; different `Mode` strings |
| Preview `Mode` | `count` — pixel values are counter hits | Often `tot`, `count`, etc. |
| Dual-layer preview (Serval manual §4, `serval_mpx3.json`) | **Two TCP channels** — frame vs integrated-from-measurement-start | Usually one preview TCP channel |
| Channel 0 (e.g. 8088) | `IntegrationSize: 0` or `1` — **current frame** (no integration) | Frame preview |
| Channel 1 (e.g. 8089) | `IntegrationSize: -1` — **integrated from measurement start**; ASI example uses `IntegrationMode: last` (non-zero overwrite). IOC default: `PrvImg1IntgMode=2` (**last**) | Enabled in `st_mpx3.cmd` (`PrvImg1` / Pva3–Pva4) |

So the “two images” on **two TCP ports** are **current frame vs time-integrated preview**, not “low threshold vs high threshold”. Dual-threshold images (when **`BothCounters`**) arrive as **consecutive jsonimage messages on 8088**, distinguished by **`thresholdID`**.

**EPICS:** preview TCP **8088** routes by **`thresholdID`** to addr **0** / **8** (Pva1 / Pva2) and emits **T0−T1** on addr **9** (Pva5). Integrated preview on **8089** routes to addr **10** / **11** (Pva3 / Pva4) via **`prvImg1Worker`**, with **T0−T1** on addr **12** (Pva6). **`PrvImgThreshDiffClip=Clip`** (default) applies **`max(0, diff)`** for IXS display; **Signed** mode keeps raw signed diff for pairing diagnostics. Full-rate **Image[]** TCP **8086** demuxes the same way to addr **1** / **13** (Pva7 / Pva8); running-sum accumulation uses **threshold 0 only**.

### BothCounters operational notes (2026-06)

Serval **rejects** `BothCounters=true` with **`TriggerMode: CONTINUOUS`** (`TriggerMode` PV index **5**). The MPX3 IOC startup profile uses **`TriggerMode=4`** (`AUTOTRIGSTART_TIMERSTOP`) with **`BothCounters=Yes`**. The driver auto-switches **5→4** when **`BothCounters=Yes`** is written later and blocks acquire if both are still active.

Recommended checklist when enabling dual threshold:

1. **DetConfig:** `BothCounters=Yes` (driver sets `PrvImgThs` to `0,1`; run **WriteData** to push destination).
2. **TriggerMode** not Continuous (4 or 6).
3. **AcquirePeriod** long enough for dual-counter readout — Erik’s Accos reference uses **0.5 s**; shorter periods may log `Dropping frame … missing UDP packet(s) … (2/4)`.
4. Acquire; check **`PrvImgThresholdID_RBV`**, **Pva1** / **Pva2** on `Mpx3PrvImgMonitor`; band-pass **Pva5** / **Pva6** (addr 9/12) with **`PrvImgThreshDiffClip`** as needed.

UDP `(2/4)` drops and a **horizontal split at y=256** in the image mean half the chip UDP packets did not arrive before Serval assembled the frame — usually trigger rate or hardware/emulator limits, not EPICS preview TCP.

Each jsonimage line on the wire is: JSON header + binary pixel array. The driver parses header fields and demuxes by **`thresholdID`**: frame preview on **8088** → addr 0/8 (band 9); integrated preview on **8089** → addr 10/11 (band 12).

### MPX3 detector fields not in Serval manual §4

| Field | Role | Notes |
|-------|------|-------|
| **`GainMode`** | Pre-amplifier gain on the Medipix3 chip | Serval **`Config.GainMode`** string (not preview-specific). Erik’s Accos example: **`HGM`**. Driver IOC default: **`SHGM`**. Typical ASI strings include **`LGM`**, **`HGM`**, **`SHGM`** — confirm allowed values with ASI or chip documentation; written via **`GainMode`** PV → `PUT /detector/config`. |
| **`Preview period`** | Throttle live preview rate | Serval **`Preview.Period`** (seconds), separate from **`TriggerPeriod`**. Set via **`PrvPeriod`** PV; pushed on **`WriteData`**. Erik’s working UI used **0.5 s** preview period with **0.5 s** trigger period. **`PrvSmplgMode`**: `skipOnFrame` (0) or `skipOnPeriod` (1). |
| **`BiasVoltage`** | Sensor bias | Erik: **100 V**. Low values (e.g. 12) can prevent useful counts on hardware/emulator. |
| **`PixelDepth`** | Counter bit depth | Erik: **12**. Driver pushes integer to Serval; readback may show string `"12"`. |
| **`IDelayConfig`** | Inter-chip delay tuning | Erik: `[15, 15, 15, 10]` — on **`Mpx3DetectorConfig.bob`**. |

### Erik’s validated dual-threshold recipe (Accos, 2026-06-12)

Erik confirmed **4 triggers → 8 preview frames** on TCP **8088** (`thresholdID=1` then `0` per trigger, full 512×512). Matching EPICS settings:

```bash
caput MPX3-TEST:cam1:BothCounters 1
caput MPX3-TEST:cam1:TriggerMode 4          # AutoTrgSt_TmrSp (AUTOTRIGSTART_TIMERSTOP)
caput MPX3-TEST:cam1:ImageMode 1            # finite (Multiple)
caput MPX3-TEST:cam1:NumImages 4
caput MPX3-TEST:cam1:AcquirePeriod 0.5
caput MPX3-TEST:cam1:AcquireTime 0.495
caput MPX3-TEST:cam1:PrvImgThs "0,1"
caput MPX3-TEST:cam1:WriteData 1
caput MPX3-TEST:cam1:Acquire 1
```

During acquire, **`PrvImgThresholdID_RBV`** should alternate **1 → 0** as **`PrvImgFrameNumber_RBV`** steps 0…3. **`NumImagesCounter`** / Serval **`FrameCount`** count **triggers** (4), not jsonimage messages (8). Rebuild **`tpx3App`** after driver changes (`make -C iocs/tpx3IOC install`) and restart the IOC before testing.

See **[preview-dual-threshold.md](preview-dual-threshold.md)** for Accos code reference and resolved open questions.

## Preview TCP ports and acquisition

Preview uses `tcp://listen@localhost:PORT` so **Serval binds** the port and the IOC **connects** as a client (`PrvImg` worker thread).

Serval 4.x may **leave preview TCP listeners bound** after `measurement/stop`. If the configured port is still listening, the driver picks the next free port and re-pushes `WriteData`. With **both preview channels** connected and a clean stop, repeat acquire on **8088** / **8089** usually works without rotation.

**Driver behavior:**

- Before connect, `syncTcpStreamEndpoints()` re-reads `PrvImgFilePath` into the cached host/port (fixes “Serval on 8089, IOC still connecting to 8088”).
- Port rotation runs **only when the configured port is already in use**, or on bind-failure retry.
- `acquireStop` disconnects the IOC TCP client before `measurement/stop`.

**MPX3 v1 profile:** `WritePrvImg=1` (TCP 8088) and `WritePrvImg1=1` (TCP 8089 integrated preview). Disable either channel if you do not need it.

**Manual recovery if Serval is wedged:** restart Serval, then restart the IOC and `WriteData=1`.

## v1 scope (out of scope)

Spectral mode, TDC, and Timepix-only ToF histogram paths are flagged off for MPX3 via capability PVs. See ADMediPix3 `docs/implementation-plan.md`.
