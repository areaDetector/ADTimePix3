# Medipix3 preview and dual-threshold delivery (vendor notes)

**Status:** Phases 0–4 complete (2026-06) — Erik confirmed Accos behaviour; driver demux, integrated preview (`prvImg1Worker`), IXS band-pass (addr 11/12), and Phoebus (2×3 `Mpx3PrvImgMonitor`, health/voltages cross-links) validated on emulator.  
**Related:** [integration.md](integration.md) (MPX3 IOC profile — dual preview TCP 8088/8089, dual-threshold demux, and Erik’s test recipes).

This note captures correspondence with ASI on how Serval delivers Medipix3 preview and threshold images, and tracks EPICS driver work against the Accos reference client.

---

## Summary (Erik / Accos model)

| Topic | Erik’s input |
|-------|----------------|
| Dual threshold | With **`BothCounters=true`**, Serval sends **two separate jsonimage messages per trigger** on preview TCP **8088** — **`thresholdID=1` first, then `thresholdID=0`**, same `frameNumber` (Accos, 2026-06-12). |
| Frame count | **4 triggers → 8 preview frames** (two jsonimage messages per trigger). Serval **`nTriggers`** / EPICS **`NumImages`** count triggers, not jsonimage messages. |
| Header metadata | Accos reads **`thresholdID`** from the jsonimage header (Serval manual p. 22) and stores `latestImage[thresholdID]`. EPICS driver routes the same way. |
| Delivery path | **Same TCP socket** (8088) — consecutive `jsonimage` messages; **not** separate destination channels per threshold. |
| `Thresholds` list | **`[0, 1]` on one preview channel** (`PrvImgThs` / destination `Thresholds`). |
| Integrated preview | **8089** uses the **same threshold demux** when integration is enabled — Accos reads integrated socket in parallel and routes by `thresholdID`. Orthogonal to dual-threshold on 8088 (integration axis, not second threshold). |
| Detector config | **`BothCounters=true`** in detector config enables dual-counter readout. **`TriggerMode`** must not be **`CONTINUOUS`** (Serval rejects the combination). |
| Image size | Full **512×512** per message when readout completes; partial UDP loss shows as dropped frames or split images, not as half-height jsonimage headers. |
| Emulator | Medipix3 emulator may show a **moving column of zero-count pixels** per image (useful when inspecting raw headers). |

**Serval manual (p. 22):** jsonimage header field is **`thresholdID`** (integer). Channel 8089 with `IntegrationSize: -1` is **integrated from measurement start** (manual p. 20), independent of dual threshold — ASI example uses `IntegrationMode: last`, not sum.

---

## Two concepts (do not conflate)

Serval configuration mixes two independent axes:

### A. Dual threshold (low / high counter)

- **Product:** two count images per logical update, distinguished by **`thresholdID`** in the jsonimage header (Serval manual p. 22; Accos/Erik).
- **Driver implication:** demux on **one TCP stream** (parse header → route to two NDArray addresses or tag with NDAttribute).
- **Config signal:** `BothCounters` in detector config; driver exposes **`BothCounters_RBV`** and writes the PV on change.

### B. Dual preview channel (Serval manual §4, `serval_mpx3.json`)

Per **ASI Serval manual** (`20251202_ASIServer_TPX3_manual_V4.1.3.pdf`, destination example pp. 18–19, table 4.3 pp. 19–20): two preview TCP streams — **current frame** and **integrated image from the start of the measurement**.

- **8088** — `IntegrationSize: 0` or `1` (no integration; current frame)
- **8089** — `IntegrationSize: -1` (integrate all samples from measurement start); ASI example uses **`IntegrationMode: last`** (non-zero pixels overwrite), not `sum`

- **Driver implication:** second **`PrvImg1` TCP worker** (orthogonal to threshold demux).
- **MPX3 IOC default:** `WritePrvImg1=1`, TCP **8089**, `PrvImg1IntgSize=-1`, `PrvImg1IntgMode=2` (**last**). ASI recommends **`last`** over sum at higher frame rates.
- **Caution:** if Serval destination enables 8089 but the IOC does not connect (`WritePrvImg1=0`), Serval logs **`Preview buffer full`** and repeat acquire can fail — either connect a reader or disable the channel in destination and `WriteData=1`.

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

## Driver status (ADTimePix3)

| Area | Status |
|------|--------|
| jsonimage parser | Parses **`thresholdID`**, `integrationSize`, `frameNumber`, dimensions, etc. |
| NDArray routing | **`thresholdID=0` → addr 0** (Pva1); **`thresholdID=1` → addr 8** (Pva2) |
| IXS band-pass | **`T0−T1` or `max(0,T0−T1)` on addr 11/12** (Pva5/Pva6) when **`BothCounters=Yes`**; mode from **`PrvImgThreshDiffClip`** (default Clip) |
| NDAttributes | **`ThresholdID`** on each array; **`PrvImgThresholdID_RBV`** |
| Detector config | **`BothCounters`** read/write; trigger guardrails (no Continuous + BothCounters) |
| Second preview TCP | **`prvImg1WorkerThread`** — NDArray addr **9** / **10**, PVA **Pva3** / **Pva4** |
| IOC / PVA | **`NDStdArrays` + Pva2** on address 8 when dual threshold enabled |

Code references: `processPrvImgDataLine()` in `tpx3App/src/serval_stream.cpp`; destination push in `configureImageChannel()` in `tpx3App/src/serval_http.cpp`.

Accos reference (Erik, 2026-06-12): one loop reads jsonimage from the preview socket; `latestImage[getThresholdFromHeader(header)] = image`. EPICS `prvImgWorker` is equivalent.

---

## Open questions — resolved (Erik, 2026-06-12)

| # | Question (2026-06-08) | Answer |
|---|------------------------|--------|
| 1 | Same TCP stream vs separate channels for dual threshold? | **Same stream (8088)** — two consecutive jsonimage messages per trigger. |
| 2 | **`thresholdID`** delivery path? | **Header field** on each message; Accos and ADTimePix3 demux by `thresholdID`. Order per trigger: **1 then 0**. |
| 3 | **`Thresholds`** on one channel or split? | **`[0, 1]` on one preview channel** (`PrvImgThs` / destination JSON). |
| 4 | Is 8089 independent of dual threshold? | **Yes** — 8089 is **integrated-from-start** preview; dual threshold still applies on 8089 when integration is enabled (same header demux). |
| 5 | Other detector fields besides **`BothCounters`**? | Erik’s working example used standard config: **`TriggerMode`**, **`TriggerPeriod`**, **`ExposureTime`**, **`nTriggers`**, **`GainMode`**, **`PixelDepth`**, **`IDelayConfig`**, etc. No extra hidden flag beyond **`BothCounters`** for dual-threshold preview on 8088. |

---

## Implementation plan

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

- [x] `prvImg1WorkerThread` mirroring `prvImgWorker`.
- [x] Port sync / rotation in `ensurePreviewTcpPortsFree()`.
- [x] Enable `WritePrvImg1=1` only with a consumer (integrated preview on 8089, `IntegrationSize: -1`).

### Phase 3 — Configuration alignment

- [x] Driver sets **`PrvImgThs` to `0,1`** when **`BothCounters=Yes`** is written; user runs **WriteData** to push destination.
- [x] Phoebus: dual image widgets (`Mpx3PrvImgMonitor.bob`, Pva1/Pva2).

### Phase 4 — Driver threshold band-pass (IXS / ID10-style)

- [x] After each paired **T1→T0** jsonimage on PrvImg / PrvImg1, emit **`NDInt32`** **T0−T1** on NDArray addr **11** (frame) and **12** (integrated).
- [x] **`PrvImgThreshDiffClip`**: **Clip** (default) → **`max(0, T0−T1)`** on addrs 11/12; **Signed** → raw signed diff for pairing diagnostics.
- [x] Gated on **`BothCounters=Yes`**; pairs T1→T0 per trigger (warns on `frameNumber` mismatch, pairs by order).
- [x] MPX3 IOC: **`imageDiff1`** / **`imageIntDiff1`** + **Pva5** / **Pva6** only (no separate clip addrs).
- [x] Phoebus 2×3 layout on `Mpx3PrvImgMonitor.bob` (frame row + integrated row; band T0−T1 in col 3, Pva5/Pva6 + **PrvImgThreshDiffClip**).

Legacy **`NDPluginProcess`** sketch (`ixs_thresh_diff.template`) remains as optional fallback; driver pairing has no scan latency.

---

## Erik’s validated Accos run (2026-06-12)

Serval config pushed before acquire:

```json
{
  "TriggerMode": "AUTOTRIGSTART_TIMERSTOP",
  "nTriggers": 4,
  "TriggerPeriod": 0.5,
  "ExposureTime": 0.495,
  "BothCounters": true,
  "GainMode": "HGM",
  "ChargeSumming": false,
  "PixelDepth": "12",
  "IDelayConfig": [15, 15, 15, 10]
}
```

Result: **8 jsonimage frames** on TCP **8088** — for each `frameNumber` 0…3, **`thresholdID=1`** then **`thresholdID=0`**, each **512×512**. See [integration.md](integration.md) for the matching EPICS `caput` recipe.

---

## Correspondence log

| Date | From | Notes |
|------|------|-------|
| 2026-06 | Kaz → Erik | Asked how Serval sends low/high threshold images vs Lambda; referenced `serval_mpx3.json` two preview channels. |
| 2026-06 | Erik | Two separate images per trigger; **`BothCounters`**; Accos uses **header `thresholdID`**. |
| 2026-06-08 | Kaz → Erik | Follow-up: five questions (delivery path, header field, Thresholds list, 8089 meaning, detector fields). |
| 2026-06-10 | Manual review | **Serval V4.1.3 manual** §4: 8088 frame / 8089 `IntegrationSize:-1` integrated from measurement start; example `IntegrationMode: last`; jsonimage **`thresholdID`**. |
| 2026-06-12 | Erik → Kaz | Accos code + log: **8088**, **`thresholdID` 1 then 0**, 4 triggers → 8 frames, integrated socket **8089** same demux; working **`TriggerPeriod=0.5`**, **`ExposureTime=0.495`**, **`AUTOTRIGSTART_TIMERSTOP`**. |
| 2026-06 | Erik → Kaz | Hardware checkout: single threshold 12-bit, 495 ms / 5 ms down / 20 frames, super-low gain, threshold DAC ~50–90, 100 V bias positive, equalize in Accos; preview + integrated for live view, **`Image[]`** for saving; no sum on 8089; ChargeSumming/Colour off. |

---

## References

- **ASI Serval manual** `20251202_ASIServer_TPX3_manual_V4.1.3.pdf` (bundled with Serval 4.1.5) — destination example pp. 18–19; `IntegrationSize` / `IntegrationMode` table 4.3 pp. 19–20; jsonimage header p. 22 (`thresholdID`).
- ADMediPix3 `configs/serval/serval_mpx3.json` — reference destination (frame + integrated preview).
- [integration.md](integration.md) — MPX3 IOC profile (8088 + 8089), troubleshooting, hardware notes.
- [PROCESSED_IMAGE_FILE_SAVING.md](../PROCESSED_IMAGE_FILE_SAVING.md) — NDArray address map (address **8** = PrvImg threshold 1 / Pva2).
