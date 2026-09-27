# Bounded Rolling Accumulation

This document defines the retained-memory contract for the image (`Img`) and
preview-histogram (`PrvHst`) sum-of-last-N products introduced for R1-8-0.

## Behavior

The driver maintains each sum-of-N product incrementally. For every incoming
frame it subtracts the frame leaving the window and adds the new frame. The
work per update is therefore O(elements), independent of the requested window
length. The running sum over the whole acquisition is a separate product and
is not part of this rolling-window budget.

Both channels default to a 512 MiB retained-storage budget:

| Channel | Requested window | Budget | Window capacity | Current fill | Status |
|---|---|---|---|---|---|
| Img | `ImgFramesToSum` | `ImgRetentionLimitMB` | `ImgEffectiveFrames_RBV` | `ImgFramesSummed_RBV` | `ImgRetentionStatus_RBV` |
| PrvHst | `PrvHstFramesToSum` | `PrvHstRetentionLimitMB` | `PrvHstEffectiveFrames_RBV` | `PrvHstFramesSummed_RBV` | `PrvHstRetentionStatus_RBV` |

The budget inputs accept 1 through 4096 MiB and are autosaved. A budget is a
ceiling, not an up-front allocation: retained frames are allocated as they
arrive. The existing requested-window inputs continue to accept 1 through
100000 frames and retain their default of 10.

The effective window is:

```text
frame_bytes = elements * sizeof(uint32)
sum_bytes   = elements * sizeof(uint64)
effective_N = min(requested_N, floor((budget_bytes - sum_bytes) / frame_bytes))
```

At least `sum_bytes + frame_bytes` is required. If the configured budget
cannot hold the rolling sum and one retained frame, the effective window is
zero, retained rolling-window storage is released, and the status PV explains
the failure. The acquisition, current-frame product, and whole-acquisition
running sum remain separate; the sum-of-N product is unavailable until a valid
budget and geometry are configured.

When the requested window exceeds the capacity for the current geometry, the
driver uses the largest window that fits and reports `Capped` with the
requested and effective frame counts. `*EffectiveFrames_RBV` is that permitted
capacity; `*FramesSummed_RBV` is the live number of frames currently retained
after startup, reset, or reconfiguration. Once the window fills, current fill
equals the effective capacity.

Changing the window or budget at runtime preserves the newest frames that
still fit. Increasing a limit preserves the existing history and fills the
larger window with subsequent frames. A geometry change starts a new rolling
window.

## Capacity examples

With the default 512 MiB budget:

| Geometry | Elements | Largest effective window |
|---|---:|---:|
| 512 x 512 image | 262144 | 510 frames |
| 1024 x 512 image | 524288 | 254 frames |
| 100000-bin histogram | 100000 | 1340 frames |
| 1000000-bin histogram | 1000000 | 132 frames |

These figures cover retained rolling-window storage: one uint64 sum plus
uint32 copies of retained frames. A frame-sized temporary allocation may exist
while replacing the oldest frame. Other driver state, current/running-sum
buffers, network buffers, and areaDetector NDArrays are outside this budget;
use `ImgMemoryUsage`, `PrvHstMemoryUsage_RBV`, and process-level monitoring for
the broader footprint.

At the maximum 4096 MiB budget, a 512 x 512 rolling image can retain
4094 frames (about 6.8 minutes at 10 Hz or 68 minutes at 1 Hz). Longer exact
windows require correspondingly more retained memory; the separate
whole-acquisition running sum remains constant-memory.

## Publication interval

`ImgSumUpdateInterval` and `PrvHstSumUpdateInterval` are publication intervals,
not accumulation intervals. The internal rolling sum subtracts the outgoing
frame and adds every incoming frame, so every published product is an exact
sum of its last N input frames. A value greater than one reduces waveform,
NDArray, file-plugin, and display callback load, but the visible product can
lag the newest input by at most interval minus one frames. Use an interval of
one when phase-transition feedback requires the freshest possible product.

During initial fill, normalize a newly published sum using the matching
`*FramesSummed_RBV`, not the capacity readback. If publication is decimated,
consume the count with the waveform callback or use interval one to avoid
reading a newer live count alongside an older waveform. Once the window is
full, current fill and capacity are equal.

## Preview cadence and nominal time span

`PrvPeriod_RBV` is the configured period shared by the Serval preview image and
preview histogram products. The additive `PrvNominalRate_RBV` readback is
`1 / PrvPeriod_RBV`; it is a configured nominal rate, not a measurement of
product delivery. The full-rate `Img` stream is independent of `PrvPeriod`.

`PrvHstAcqRate_RBV` is preserved for compatibility and reports preview detector
frame progression calculated from detector `frameNumber` changes. It is not a
count of histogram products delivered per second.

`PrvHstWindowSpan_RBV` is the nominal time span represented by the current
histogram rolling-window fill:

```text
PrvHstWindowSpan_RBV = PrvHstFramesSummed_RBV * PrvPeriod_RBV
```

This value grows during initial fill and after a capacity increase. Sampling,
gating, or missing preview products can make the actual elapsed span differ, so
the readback is explicitly nominal.

## Performance readbacks

`ImgMemoryUsage` and `PrvHstMemoryUsage_RBV` are estimates of driver-owned
accumulation buffers, not process resident memory. They rise while retained
frames fill the requested or capped window, then plateau because each new frame
replaces the oldest. Allocator capacity and other IOC/plugin memory can make
process-level usage differ.

`ImgProcessingTime` and `PrvHstProcessingTime` are moving averages of
driver-side frame processing, not end-to-end feedback or downstream plugin
latency. The rolling update is O(elements), so its cost is intentionally nearly
independent of N. A larger publication interval reduces periodic waveform,
NDArray, file-plugin, and display work; it does not skip internal frames.

## Numeric range contract

Accumulated count waveforms and NDArrays now publish native unsigned types.
Running sums saturate at `UINT64_MAX`; a rolling-window update that cannot
remain exact is rejected. Legacy signed scalar total-count readbacks clamp at
`INT64_MAX` instead of becoming negative. Each channel exposes
a latched alarm and detail status, cleared by its accumulation reset. On-demand
Sum products are `NDUInt64`; Average products are checked `NDUInt32`. See
[Accumulation Numeric Range Contract](NUMERIC_RANGE_CONTRACT.md) for the complete
type, unit, saturation, alarm, metadata, and client-compatibility contract.
CA exposes these unsigned records as doubles, but every supported last-N value
remains below `2^53` and is therefore integer-exact. Use PVA or `NDUInt64` HDF5
for exact whole-acquisition values above that boundary.

## Operator checks

After setting the geometry, requested window, or budget:

1. Confirm the appropriate `EffectiveFrames_RBV` capacity is nonzero.
2. Read the matching `RetentionStatus_RBV`; `OK` means the request fits and
   `Capped` means the driver safely reduced it.
3. Monitor `FramesSummed_RBV` for the number of frames currently contributing.
   Use this current-fill count for normalization while the window is filling.
4. Choose the publication interval from the acceptable feedback latency; use
   one for per-frame phase-transition feedback.
5. Monitor the channel memory-usage PV and IOC resident memory for the complete
   operational footprint.

The Phoebus image-accumulation and preview-histogram screens expose the
requested window, publication interval, budget, window capacity, current fill,
and status together.
