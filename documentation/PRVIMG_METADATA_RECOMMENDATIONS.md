# PrvImg TCP streaming metadata (recommendations and current status)

## Overview

This document recorded which metadata from **jsonimage** TCP streaming was worth exposing as EPICS PVs, and (originally) whether a dedicated screen should be added. **Most of the high-priority items are now implemented in ADTimePix3**; this file is updated so it matches the code and **can be versioned in git** without misleading readers.

## Current implementation status (driver / DB / UI)

| Item | tpx3image-style name | Asyn / PV (typical) | Status |
|------|----------------------|----------------------|--------|
| Frame number | `FRAME_NUMBER` | `TPX3_PRVIMG_FRAME_NUMBER` → `PrvImgFrameNumber_RBV` | **Implemented** — set in `ADTimePix.cpp` from JSON `frameNumber` |
| Time at frame | `TIME_AT_FRAME_NS` | `TPX3_PRVIMG_TIME_AT_FRAME` → `PrvImgTimeAtFrame_RBV` | **Implemented** — from JSON `timeAtFrame` (nanoseconds) |
| Acquisition rate (calculated) | — | `TPX3_PRVIMG_ACQ_RATE` → `PrvImgAcqRate_RBV` (or similar in `Server.template`) | **Implemented** — rolling calculation in the PrvImg path |
| Width / height | `FRAME_WIDTH` / `FRAME_HEIGHT` | `NDArraySizeX` / `NDArraySizeY` | As before |
| Pixel format | `FRAME_PIXEL_FORMAT` | `NDDataType` | As before |
| Frame loss (optional) | — | (none dedicated) | **Not implemented** as a first-class PV; still a possible enhancement |

**Monitoring screen:** `tpx3App/op/bob/Acquire/PrvImgMonitor.bob` exists and is included from `TimePix3Status.bob` (see embedded file reference in that BOB). The earlier “Phase 2: create PrvImgMonitor.bob” plan is **done** at least at the level of a dedicated screen; extend that screen if new PVs are added.

## Original jsonimage header (reference)

Serval can send a JSON header with fields such as:

```json
{
  "width": 256,
  "height": 256,
  "pixelFormat": "uint16",
  "frameNumber": 12345,
  "timeAtFrame": 1234567890.123456
}
```

## Historical comparison (kept for context)

The table below reflects the **earlier** gap analysis vs tpx3image; the “Status in ADTimePix3” column is superseded by the **Current implementation status** section above.

| PV Name (tpx3image) | Description | Old note |
|---------------------|-------------|----------|
| `TIME_AT_FRAME_NS` | Timestamp at frame (ns) | Was missing; **now in driver** |
| `FRAME_NUMBER` | Frame number from JSON | Was missing; **now in driver** |
| `FRAME_WIDTH` / `FRAME_HEIGHT` | Dimensions | `NDArraySizeX` / `Y` |
| `FRAME_PIXEL_FORMAT` | Pixel format | `NDDataType` |
| `ACQUISITION_RATE` | Calculated fps | **now in driver** as `TPX3_PRVIMG_ACQ_RATE` |

## Remaining optional work

- **`TPX3_PRVIMG_FRAME_LOSS` (or similar):** not implemented; can still be derived from `frameNumber` gaps if required later.
- **Doc / OPI sync:** if PrvImg metadata PVs or record names change, update **`Server.template`**, this file, and **RELEASE** as appropriate.

## Conclusion (updated)

The original recommendation—to expose **frame number** and **time at frame**, add a **calculated rate**, and provide a **dedicated PrvImg monitor**—is largely satisfied in tree. This document is retained as a short **status and history** note; for exact PV names, prefer **`ADTimePix.h`**, **`Server.template`**, and **`PrvImgMonitor.bob`**.
