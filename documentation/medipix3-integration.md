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
- single-layer preview on TCP port 8088 (frame channel; port rotates on repeat acquire)
- `PrvImg1` (running sum) disabled until a driver TCP worker exists
- `count` image mode with Serval `Thresholds[]` when family is MPX3
- BPC/DACS defaults: `$(ADTIMEPIX)/vendor/mpx3/eq-01.bpc` and `eq-01.dacs` (uploaded in `init_detector_hw_mpx3.cmd`)

## Emulator workflow

1. Start Serval with a Medipix3 emulator profile (see ADMediPix3 `configs/serval/`).
2. Build this module: `make -j` from the module root.
3. Start IOC with `st_mpx3.sh`.
4. Confirm `MPX3-TEST:cam1:DetectorFamily_RBV` = `MPX3` after connect.
5. Push channel config: `caput MPX3-TEST:cam1:WriteData 1`
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
| `Preview.ImageChannels[1]` (`PrvImg1`) | **No** — no worker thread yet | `WritePrvImg1=0` |
| `Image[]` (main image channel) | Only if `WriteImg=1` and TCP + accumulation | `WriteImg=0` (file path unused) |

The reference `serval_mpx3.json` configures **two preview layers** (frame + running sum). For EPICS v1, enable only the first preview TCP channel — matching what you validated in Phoebus FileWrite. A second preview TCP stream with no reader fills Serval’s queue (`Preview buffer full` on 8089) and breaks repeat acquire.

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

Serval does **not** send “two threshold images” on one TCP socket by default. Configuration controls what each **preview channel** produces:

| Concept | MPX3 (Medipix3) | TPX3 (Timepix3) |
|---------|-----------------|-----------------|
| Counter / threshold indices | `Thresholds: [0..7]` — eight virtual counters | Often ToT/TDC modes; different `Mode` strings |
| Preview `Mode` | `count` — pixel values are counter hits | Often `tot`, `count`, etc. |
| Dual-layer preview (reference `serval_mpx3.json`) | **Two TCP channels**, not one stream with two images | Usually one preview TCP channel |
| Channel 0 | `IntegrationSize: 1` — **current frame** count image | Frame preview |
| Channel 1 | `IntegrationSize: -1`, `IntegrationMode: sum` — **running sum** | Optional second preview (TPX3 often uses disk for PrvImg1) |

So the “two images” in the MPX3 reference config are **frame vs running-sum**, both in `count` mode with the same `Thresholds` list. They are **not** automatically “low threshold vs high threshold” planes — those would be selected by which indices appear in `Thresholds` and how many preview channels you configure.

**EPICS v1:** one preview TCP consumer (`PrvImg` → NDArray address 0). You see one jsonimage stream (one frame type). A second channel needs `PrvImg1` worker support or an external TCP client.

Each jsonimage line on the wire is: JSON header (`width`, `height`, `frameNumber`, …) + binary pixel array. The driver maps that to one NDArray; it does not demux multiple threshold planes from a single Serval channel.

## Preview TCP ports and acquisition

Preview uses `tcp://listen@localhost:PORT` so **Serval binds** the port and the IOC **connects** as a client (`PrvImg` worker thread).

Serval 4.x may **leave preview TCP listeners bound** after `measurement/stop`. If the configured port is still listening, the driver picks the next free port and re-pushes `WriteData`. With **one preview channel** and clean stop, repeat acquire on **8088** often works without rotation.

**Driver behavior:**

- Before connect, `syncTcpStreamEndpoints()` re-reads `PrvImgFilePath` into the cached host/port (fixes “Serval on 8089, IOC still connecting to 8088”).
- Port rotation runs **only when the configured port is already in use**, or on bind-failure retry.
- `acquireStop` disconnects the IOC TCP client before `measurement/stop`.

**MPX3 v1 profile:** only `WritePrvImg=1` (TCP 8088). Keep `WritePrvImg1=0` unless a consumer exists.

**Manual recovery if Serval is wedged:** restart Serval, then restart the IOC and `WriteData=1`.

## v1 scope (out of scope)

Spectral mode, TDC, and Timepix-only ToF histogram paths are flagged off for MPX3 via capability PVs. See ADMediPix3 `docs/implementation-plan.md`.
