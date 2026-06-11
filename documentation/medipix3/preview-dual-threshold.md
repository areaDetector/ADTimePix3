# Medipix3 preview and dual-threshold delivery (vendor notes)

**Status:** Open — awaiting ASI (Erik) answers to follow-up questions (2026-06-08).  
**Related:** [integration.md](integration.md) (v1 IOC profile and validated single-channel preview).

This note captures correspondence with ASI on how Serval delivers Medipix3 preview and threshold images, and the planned EPICS driver work. Update the **Open questions** and **Implementation plan** sections when Erik responds.

---

## Summary (Erik / Accos model)

| Topic | Erik’s input (2026-06) |
|-------|------------------------|
| Dual threshold | With two thresholds active, Serval sends **two separate images** — **low/first threshold, then high/second**, in rapid succession (similar to X-Spectrum Lambda). |
| Header metadata | **Accos reads `thresholdID` from the jsonimage header** (Serval manual p. 22) and routes display/save accordingly. |
| Detector config | Behaviour is driven by **detector configuration**, not only by the destination endpoint. **`BothCounters`** is `true` when two threshold images are produced. |
| Emulator | Medipix3 emulator may show a **moving column of zero-count pixels** per image (useful when inspecting raw headers). |

**Not yet confirmed by Erik:** same TCP socket vs separate channels for dual threshold; whether dual-threshold applies to `Preview`, `Image`, or both. **Serval manual (p. 22):** jsonimage header field is **`thresholdID`** (integer). Channel 8089 with `IntegrationSize: -1` is **integrated from measurement start** (manual p. 20), independent of dual threshold — ASI example uses `IntegrationMode: last`, not sum.

---

## Two concepts (do not conflate)

Serval configuration mixes two independent axes:

### A. Dual threshold (low / high counter)

- **Product:** two count images per logical update, distinguished by **`thresholdID`** in the jsonimage header (Serval manual p. 22; Accos/Erik).
- **Driver implication:** demux on **one TCP stream** (parse header → route to two NDArray addresses or tag with NDAttribute).
- **Config signal:** `BothCounters` in detector config (not yet read by ADTimePix3).

### B. Dual preview channel (Serval manual §4, `serval_mpx3.json`)

Per **ASI Serval manual** (`20251202_ASIServer_TPX3_manual_V4.1.3.pdf`, destination example pp. 18–19, table 4.3 pp. 19–20): two preview TCP streams — **current frame** and **integrated image from the start of the measurement**.

- **8088** — `IntegrationSize: 0` or `1` (no integration; current frame)
- **8089** — `IntegrationSize: -1` (integrate all samples from measurement start); ASI example uses **`IntegrationMode: last`** (non-zero pixels overwrite), not `sum`

- **Driver implication:** second **`PrvImg1` TCP worker** (orthogonal to threshold demux).
- **v1 choice:** `WritePrvImg1=0` — no consumer on 8089 avoids `Preview buffer full` and broken repeat acquire.
- **IOC note:** `init_detector_paths_mpx3.cmd` sets `PrvImg1IntgMode=0` (**sum**) when enabled; align with manual **`last`** or **`sum`** as needed.

These can coexist (e.g. dual threshold on 8088 **and** integrated preview on 8089) but serve different purposes.

---

## Preview vs Image (`Destination`)

Same **channel schema** and **`jsonimage` wire format**; **not equivalent** in Serval or ADTimePix3:

| | `Preview.ImageChannels[]` | `Image[]` |
|---|---------------------------|-----------|
| Serval role | Throttled live preview (`Preview.Period`, `SamplingMode`) | Full-rate acquisition output |
| ADTimePix3 worker | `prvImgWorker` | `imgWorker` (if accumulation enabled) |
| NDArray address | **0** | **1** (+ 2/3 accumulation) |
| MPX3 v1 default | `WritePrvImg=1`, TCP 8088 | `WriteImg=0` |

**Recommendation:** EPICS live view (Phoebus/PVA) should consume **`Preview.ImageChannels[0]`**. Use **`Image[]`** TCP for full-rate file saving and Img accumulation.

---

## Current driver gap (ADTimePix3)

| Area | Today | Needed for dual threshold |
|------|--------|---------------------------|
| jsonimage parser | Reads `width`, `height`, `frameNumber`, `timeAtFrame`, `pixelFormat` only | Parse **`thresholdID`** from header (Serval manual p. 22) |
| NDArray routing | All PrvImg frames → address **0** | Route by `thresholdID` (e.g. 0 → addr 0, 1 → new addr) |
| NDAttributes | Standard AD attrs only | e.g. `ThresholdID` / `thresholdID` on each array |
| Detector config | Family/threshold **lists** in destination JSON | Read **`BothCounters`** from `GET /detector` |
| Second preview TCP | `PrvImg1` PVs exist; **no worker thread** | Optional: clone worker for integrated preview on 8089 |
| IOC / PVA | One `NDStdArrays` on address 0 | Second PVA instance when dual threshold enabled |

Code references: `processPrvImgDataLine()` / `processImgDataLine()` in `tpx3App/src/serval_stream.cpp`; destination push in `configureImageChannel()` in `tpx3App/src/serval_http.cpp`.

---

## Open questions (sent to Erik, 2026-06-08)

1. With **`BothCounters = true`**, are both threshold images on the **same TCP stream** (two consecutive `jsonimage` messages) or on **separate destination channels**?
2. Confirm **`thresholdID`** behaviour and dual-threshold delivery path (manual documents the field; Erik pending on same-socket vs split channels).
3. With **`BothCounters = true`**, should `Thresholds` be **`[0, 1]` on one channel** or split across channels/ports?
4. Is **`Preview.ImageChannels[1]`** (8089, `IntegrationSize: -1`) independent of dual threshold? *(Manual: yes — integrated-from-start preview, not second threshold.)*
5. Besides **`BothCounters`**, which other **detector-side fields** should clients read?

---

## Implementation plan (after Erik confirms)

### Phase 0 — Observe (low risk)

- [x] Log full jsonimage headers during acquire (`PrvImgLogHeaders`, default 3 per acquire).
- [x] Parse `thresholdID` and `integrationSize` from header; `PrvImgThresholdID_RBV`.
- [x] Update this document and [integration.md](integration.md) (Serval manual alignment).

### Phase 1 — Header demux (dual threshold)

- [x] Parse **`thresholdID`** in `processPrvImgDataLine()`.
- [x] NDAttribute **`ThresholdID`** + RBV `PrvImgThresholdID_RBV`.
- [x] Route to NDArray addresses **0** (threshold 0) and **8** (threshold 1); `maxAddr=9`.
- [x] Read **`BothCounters`** in `getDetector()`; expose `BothCounters_RBV`.
- [x] MPX3 IOC: second `NDStdArrays` (`imageTh1`) on address 8.

### Phase 2 — Optional second preview TCP (`PrvImg1`)

- [ ] `prvImg1WorkerThread` mirroring `prvImgWorker`.
- [ ] Port sync / rotation in `ensurePreviewTcpPortsFree()`.
- [ ] Enable `WritePrvImg1=1` only with a consumer (integrated preview on 8089, `IntegrationSize: -1`).

### Phase 3 — Configuration alignment

- [ ] Family defaults for `PrvImgThs` when `BothCounters=1` (per Erik).
- [x] Phoebus: dual image widgets (`Mpx3PrvImgMonitor.bob`, Pva1/Pva2).

---

## Correspondence log

| Date | From | Notes |
|------|------|-------|
| 2026-06 | Kaz → Erik | Asked how Serval sends low/high threshold images vs Lambda; referenced `serval_mpx3.json` two preview channels. |
| 2026-06 | Erik | Two separate images (low then high); **`BothCounters`**; Accos uses **header threshold index**. |
| 2026-06-08 | Kaz → Erik | Follow-up email: five questions (delivery path, header field, Thresholds list, 8089 meaning, detector fields). *Pending.* |

| 2026-06-10 | Manual review | **Serval V4.1.3 manual** §4: 8088 frame / 8089 `IntegrationSize:-1` integrated from measurement start; example `IntegrationMode: last`; jsonimage **`thresholdID`**. |

---

## References

- **ASI Serval manual** `20251202_ASIServer_TPX3_manual_V4.1.3.pdf` (bundled with Serval 4.1.5) — destination example pp. 18–19; `IntegrationSize` / `IntegrationMode` table 4.3 pp. 19–20; jsonimage header p. 22 (`thresholdID`).
- ADMediPix3 `configs/serval/serval_mpx3.json` — reference destination (frame + integrated preview).
- [integration.md](integration.md) — validated v1 single-channel preview and troubleshooting.
- [PROCESSED_IMAGE_FILE_SAVING.md](../PROCESSED_IMAGE_FILE_SAVING.md) — NDArray address map (0–7 in use; address 8+ TBD for second threshold preview).
