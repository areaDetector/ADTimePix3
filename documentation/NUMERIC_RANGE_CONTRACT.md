<!--
Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
SPDX-License-Identifier: MIT
-->

# Accumulation Numeric Range Contract

This document defines the public numeric types, units, saturation behavior, and
operator alarms for Img and PrvHst accumulation products in R1-8-0.

## Public products

All count arrays are unsigned. Existing PV names and NDArray addresses are
preserved.

| Product | Waveform field type | NDArray type | Address | Units |
|---|---|---|---:|---|
| Img running sum | `UINT64` | `NDUInt64` | 2 | counts |
| Img rolling sum | `UINT64` | `NDUInt64` | 3 | counts |
| Img current frame | `ULONG` | raw Img stream type | 1 | counts |
| PrvHst rolling sum | `UINT64` | `NDUInt64` | 4 | counts |
| PrvHst running sum | `UINT64` | `NDUInt64` | 5 | counts |
| PrvHst current frame | `ULONG` | `NDUInt32` | 6 | counts |
| PrvHst ToF axis | `DOUBLE` | `NDFloat64` | 7 | ms |

The asyn array interfaces remain `asynInt64Array` and `asynInt32Array` because
those are the available callback interfaces. The driver copies unsigned values
into those callback buffers by preserving their bits. The waveform records use
`UINT64` or `ULONG` `FTVL` to interpret the bits as unsigned.

The client transport determines how those database types appear:

- PVAccess exposes `UINT64` waveforms as `ulong[]`, `ULONG` waveforms as
  `uint[]`, and signed `int64in` scalars as `long`.
- Channel Access has no unsigned-32 or 64-bit integer wire type. It exposes all
  of those records as `DBF_DOUBLE`; this is a transport conversion, not a
  change to the database or NDArray type.

CA doubles represent every integer exactly through `2^53`. At the maximum
supported rolling window of 100000 frames, the largest possible element is
`UINT32_MAX * 100000 = 429496729500000`, which is below `2^53`. Therefore all
supported Img and PrvHst last-N waveform values remain integer-exact over CA.
Whole-acquisition running sums and total-count readbacks can eventually exceed
`2^53`; use PVA or the in-process `NDUInt64` HDF5 path when those values must
remain exact.

The on-demand `ProcessedImgOutputType` and `ProcessedHstOutputType` selections
publish Sum as `NDUInt64` counts and Average as `NDUInt32` counts/frame. The
average conversion divides before checking the 32-bit destination and clamps
to `UINT32_MAX` if necessary. Use Average for TIFF plugins that do not support
64-bit integer arrays; use Sum with HDF5 for exact quantitative storage.

Count NDArrays include these attributes:

- `ADTimePixNumericRange`: `uint64` or `uint32`
- `ADTimePixCountUnits`: `counts` or `counts/frame`
- `ADTimePixRangeSaturated`: zero or one

PrvHst count NDArrays retain their uniform ToF-axis attributes as well.

## Accumulation and saturation

Running sums and rolling sums use unsigned 64-bit elements. Addition is checked
before it is performed:

- values through `UINT64_MAX` are exact;
- a running-sum addition beyond `UINT64_MAX` stores `UINT64_MAX` and latches
  the channel range alarm;
- a rolling-window update beyond `UINT64_MAX` is rejected, leaving the prior
  exact window intact, and latches the range alarm.

Whole-acquisition total-count state also saturates at `UINT64_MAX`. The scalar
total-count PVs publish signed `int64in` records. `ImgTotalCounts` retains its
existing type; `PrvHstTotalCounts_RBV` is corrected from `ai` to `int64in` so
integer values remain exact through `INT64_MAX`. Larger unsigned totals publish
`INT64_MAX` rather than becoming negative.

## Range status and reset

| Channel | Alarm readback | Detail readback |
|---|---|---|
| Img | `ImgRangeSaturated_RBV` | `ImgRangeStatus_RBV` |
| PrvHst | `PrvHstRangeSaturated_RBV` | `PrvHstRangeStatus_RBV` |

The alarm readback is a `bi`: `OK` is zero and `Saturated` is one with MAJOR
severity. The detail is `OK`, `Published output clamped`, or `Accumulator
saturated`. The state is latched so a transient overflow cannot disappear from
operator view. Reset it with `ImgImageDataReset` or `PrvHstDataReset`; a geometry
change that starts a new accumulation epoch also clears it.

A latched output clamp does not imply that the unsigned waveform or
NDArray data was lost. A latched accumulator saturation means a running or
total sum reached its maximum, or a rolling-window update could not remain
exact. Quantitative consumers must treat that accumulation epoch as range-limited.

## Compatibility

No existing PV was renamed. The deliberate database and PVA interface changes
are:

- accumulated waveforms are `UINT64` / PVA `ulong[]`;
- current-frame count waveforms are `ULONG` / PVA `uint[]`;
- `PrvHstTotalCounts_RBV` is `int64in` / PVA `long`.

PVA clients that declared the former signed-array or double schema must update
their schema or request conversion explicitly. CA clients continue to see
`DBF_DOUBLE`, so their transport schema does not change; the `2^53` precision
boundary still applies.

For command-line PVA diagnostics, note that `EPICS_CA_ADDR_LIST` and
`EPICS_CA_AUTO_ADDR_LIST` do not configure PVA discovery. A one-shot PVA check
can be constrained to the configured CA path with:

```bash
EPICS_PVA_ADDR_LIST="$EPICS_CA_ADDR_LIST" \
EPICS_PVA_AUTO_ADDR_LIST=NO \
pvinfo -p pva PV_NAME
```

The Phoebus Img and PrvHst accumulation screens display a green/red alarm LED
and the detailed range status. They do not infer overflow from a negative value.
