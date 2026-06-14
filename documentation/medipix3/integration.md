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

## Impact on Timepix3 operation

Medipix3 work is **additive**: one driver binary serves both families. After `GET /detector`, the driver sets `detectorFamily_` and branches MPX3-only logic; it does **not** replace the Timepix3 IOC profile or calibration paths.

**For normal TPX3 use** (`st.cmd` / `st_base.cmd`, TPX3 hardware, default PVs), behaviour should match pre-integration operation. Use **`st_mpx3.cmd` only with MPX3** — that profile enables dual threshold, integrated preview plugins, and `vendor/mpx3/` calibration by design.

### What stays separate

| Layer | Timepix3 (TPX3) | Medipix3 (MPX3) |
|--------|-----------------|-----------------|
| IOC startup | `st.cmd`, `init_detector_*.cmd` | `st_mpx3.cmd`, `init_detector_mpx3.cmd` |
| Calibration | `vendor/tpx3-*` (via `init_detector_paths.cmd`) | `vendor/mpx3/` |
| Phoebus | `TimePix3.bob`; legacy `op/opi/*.opi` (CSS) | `MediPix3/*.bob` |
| NDArray / PVA | Pva1; driver addrs **0**–**3** (typical) | Pva1–Pva6; addrs **0**, **8**, **9**, **10**, **11**, **12** |

MPX3-only ND plugins (`imageTh1`, `imageInt1`, band-pass on addr 11/12, etc.) are loaded only in **`st_mpx3.cmd`**, not in the default Timepix3 startup.

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
5. **`NDARRAY_MAX_ADDR = 13`** — larger internal callback table for all families; TPX3 sites that only use addrs 0–3 are unaffected in routing.

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
- preview on TCP **8088** (`PrvImgThs` **0,1**, jsonimage format)
- **`BothCounters=Yes`**, **`TriggerMode=AutoTrgSt_TmrSp` (4)**, **4 triggers**, **0.5 s** period (Erik Accos recipe)
- `PrvImg1` (integrated preview on 8089) — **`prvImg1WorkerThread`**, NDArray addr **9**/**10**, PVA **Pva3**/**Pva4**
- BPC/DACS: `$(ADTIMEPIX)/vendor/mpx3/eq-01.bpc` and `eq-01.dacs` (uploaded in `init_detector_hw_mpx3.cmd`)

**Phoebus:** open `tpx3App/op/bob/MediPix3/MediPix3.bob` with the same `P`/`R` macros (preview TCP config in `Acquire/Mpx3PreviewChannels.bob` via `Mpx3ServerFileWriter.bob`, live images in `Acquire/Mpx3PrvImgMonitor.bob`, detector config in `Detector/Mpx3DetectorConfig.bob`). **`Detector/TimePixDetectorHealth.bob`** and **`TimePixDetectorVoltages.bob`** (under `op/bob/Detector`) cross-link for health readbacks. For `PrvImgThs` (CHAR waveform), use the Phoebus text field or IOC `dbpf` — plain `caput` with a quoted string clears the array.

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

## Serval channel model (Preview vs Image)

Serval `GET http://localhost:8081/` shows `Server.Destination`:

| Serval path | IOC consumer | MPX3 v1 default |
|-------------|--------------|-----------------|
| `Preview.ImageChannels[0]` (`PrvImg`) | Yes — `prvImgWorker` TCP client, NDArray/PVA | `WritePrvImg=1`, TCP 8088 |
| `Preview.ImageChannels[1]` (`PrvImg1`) | Yes — `prvImg1Worker` TCP client | `WritePrvImg1=1`, TCP 8089 |
| `Image[]` (main image channel) | Only if `WriteImg=1` and TCP + accumulation | `WriteImg=0` (file path unused) |

The reference `serval_mpx3.json` and the **Serval manual** (destination example, §4 / pp. 18–19) configure **two preview TCP streams**: current frame and an image **integrated from the start of the measurement**. For EPICS v1, enable only the first preview TCP channel — matching what you validated in Phoebus FileWrite. A second preview TCP stream with no reader fills Serval’s queue (`Preview buffer full` on 8089) and breaks repeat acquire.

**Serval manual** (`20251202_ASIServer_TPX3_manual_V4.1.3.pdf`, table 4.3, pp. 19–20): `IntegrationSize` **0 or 1** = no integration; **-1** = integrate all preview samples **from measurement start**; **2…32** = integrate over the last *n* images. `IntegrationMode` is **sum**, **average**, or **last** (ASI’s two-channel example uses **`last`** on 8089, not sum).

After a clean IOC start, confirm Serval shows one preview channel:

```bash
curl -s http://localhost:8081/ | python3 -m json.tool | grep -A2 ImageChannels
```

Or `caget MPX3-TEST:cam1:WritePrvImg1` → `0` and `WriteData=1`.

**Autosave:** if `auto_settings.sav` was saved with `WritePrvImg1=1`, delete or rewrite it once, or `caput MPX3-TEST:cam1:WritePrvImg1 0` then `WriteData=1`.

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
| Channel 1 (e.g. 8089) | `IntegrationSize: -1` — **integrated from measurement start**; ASI example uses `IntegrationMode: last` (non-zero overwrite). IOC default for disabled `PrvImg1` uses `IntgMode=sum` if enabled — choose mode to match intent | Optional second preview (`PrvImg1`) |

So the “two images” on **two TCP ports** are **current frame vs time-integrated preview**, not “low threshold vs high threshold”. Dual-threshold images (when **`BothCounters`**) arrive as **consecutive jsonimage messages on 8088**, distinguished by **`thresholdID`**.

**EPICS v1:** preview TCP **8088** routes by **`thresholdID`** to addr **0** / **8** (Pva1 / Pva2). Integrated preview on **8089** routes to addr **9** / **10** (Pva3 / Pva4) via **`prvImg1Worker`**. With **`BothCounters=Yes`**, the driver emits **T0−T1** on addr **11** / **12** (Pva5 / Pva6). **`PrvImgThreshDiffClip=Clip`** (default) applies **`max(0, diff)`** for IXS display; **Signed** mode keeps raw signed diff for pairing diagnostics.

### BothCounters operational notes (2026-06)

Serval **rejects** `BothCounters=true` with **`TriggerMode: CONTINUOUS`** (`TriggerMode` PV index **5**). The MPX3 IOC startup profile uses **`TriggerMode=4`** (`AUTOTRIGSTART_TIMERSTOP`) with **`BothCounters=Yes`**. The driver auto-switches **5→4** when **`BothCounters=Yes`** is written later and blocks acquire if both are still active.

Recommended checklist when enabling dual threshold:

1. **DetConfig:** `BothCounters=Yes` (driver sets `PrvImgThs` to `0,1`; run **WriteData** to push destination).
2. **TriggerMode** not Continuous (4 or 6).
3. **AcquirePeriod** long enough for dual-counter readout — Erik’s Accos reference uses **0.5 s**; shorter periods may log `Dropping frame … missing UDP packet(s) … (2/4)`.
4. Acquire; check **`PrvImgThresholdID_RBV`**, **Pva1** / **Pva2** on `Mpx3PrvImgMonitor`; band-pass **Pva5** / **Pva6** (addr 11/12) with **`PrvImgThreshDiffClip`** as needed.

UDP `(2/4)` drops and a **horizontal split at y=256** in the image mean half the chip UDP packets did not arrive before Serval assembled the frame — usually trigger rate or hardware/emulator limits, not EPICS preview TCP.

Each jsonimage line on the wire is: JSON header + binary pixel array. The driver parses header fields and demuxes by **`thresholdID`**: frame preview on **8088** → addr 0/8; integrated preview on **8089** → addr 9/10.

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

Serval 4.x may **leave preview TCP listeners bound** after `measurement/stop`. If the configured port is still listening, the driver picks the next free port and re-pushes `WriteData`. With **one preview channel** and clean stop, repeat acquire on **8088** often works without rotation.

**Driver behavior:**

- Before connect, `syncTcpStreamEndpoints()` re-reads `PrvImgFilePath` into the cached host/port (fixes “Serval on 8089, IOC still connecting to 8088”).
- Port rotation runs **only when the configured port is already in use**, or on bind-failure retry.
- `acquireStop` disconnects the IOC TCP client before `measurement/stop`.

**MPX3 v1 profile:** `WritePrvImg=1` (TCP 8088) and `WritePrvImg1=1` (TCP 8089 integrated preview). Disable either channel if you do not need it.

**Manual recovery if Serval is wedged:** restart Serval, then restart the IOC and `WriteData=1`.

## v1 scope (out of scope)

Spectral mode, TDC, and Timepix-only ToF histogram paths are flagged off for MPX3 via capability PVs. See ADMediPix3 `docs/implementation-plan.md`.
