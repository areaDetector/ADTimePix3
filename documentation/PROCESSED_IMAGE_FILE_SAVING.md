# Saving Processed Images and Histograms to File (TIFF/HDF5)

## Status

**Option A is implemented** (driver pushes processed Img as NDArrays on addresses 2 and 3):

- **WriteProcessedImg** (boolean PV): Write 1 to push the current running sum (ImgImageData) and, if available, sum-of-N (ImgImageSumNFrames) as NDArrays to addresses 2 and 3. File plugins (NDFileTIFF, NDFileHDF5) with `NDArrayAddress=2` or `3` will receive and save them.
- **ProcessedImgOutputType**: 0 = Sum (NDInt64, for HDF5); 1 = Average (NDInt32, sum/N_frames for TIFF). Set to Average for NDFileTIFF compatibility.
- **Address 2** = running sum (ImgImageData); **Address 3** = sum of last N frames (ImgImageSumNFrames). Configure e.g. `TIFF2:NDArrayAddress=2`, `HDF53:NDArrayAddress=3` in your IOC.

### Troubleshooting: TIFF images identical for different NDArrayAddress

If changing `TIFF1:NDArrayAddress` (e.g. to 0, 1, 2, or 3) produces **binary-identical** files, the file plugin is almost certainly still receiving callbacks only for the address it had at **startup**. In areaDetector, many plugins register for NDArray callbacks once at init using the initial `NDArrayAddress`; changing that PV at runtime often does **not** re-register the plugin for the new address.

**What to do:**

1. **Use separate plugin instances**, each with a fixed address at startup:
   - Load e.g. TIFF1, TIFF2, TIFF3 (and optionally TIFF4) in `commonPlugins.cmd`, and set **at startup** (in the same cmd or in a startup script):  
     `TIFF1:NDArrayAddress=0`, `TIFF2:NDArrayAddress=1`, `TIFF3:NDArrayAddress=2`, `TIFF4:NDArrayAddress=3`.  
   - Then: address 0 = PrvImg, 1 = Img current frame, 2 = running sum (only when `WriteProcessedImg` is pressed), 3 = sum-of-N (only when `WriteProcessedImg` is pressed). Each plugin will then save a different stream.
2. **Addresses 2 and 3**: Data is sent **only** when `WriteProcessedImg` is set to 1 (one-shot). During normal acquisition, only addresses 0 (PrvImg) and 1 (Img) receive callbacks. So to get files from address 2 or 3, you must press **WriteProcessedImg** (and have that plugin’s Capture/Write enabled); changing only `NDArrayAddress` and running acquisition will not fill address 2/3.
3. If you must use a single TIFF and switch streams, try **restarting the IOC** after changing `NDArrayAddress` so the plugin re-inits with the new address (not guaranteed on all ADCore versions).

## Goal

Enable writing to file:

- **Img channel (processed):**
  - `ImgImageData` (running sum, INT64)
  - `ImgImageSumNFrames` (sum of last N frames, INT64)
- **Histogram (processed):**
  - `PrvHstHistogramData` (running sum) — already exposed as NDArray on address 5
  - Optionally: sum-of-N histogram and/or time axis (`PrvHstHistogramTimeMs`) for context

## Current Behavior

| Data | PV / source | NDArray to plugins? | Address |
|------|-------------|----------------------|--------|
| Img current frame | From TCP stream | Yes | 1 |
| Img running sum | `ImgImageData` (waveform) | No | — |
| Img sum-of-N | `ImgImageSumNFrames` (waveform) | No | — |
| PrvHst running sum | `PrvHstHistogramData` (waveform) | Yes (1D NDInt64) | 5 |
| PrvHst time axis | `PrvHstHistogramTimeMs` (waveform) | No (metadata/attributes could carry it) | — |

The driver already builds an NDArray from the histogram running sum and pushes it with `doCallbacksGenericPointer(..., 5)` in `histogram_io.cpp`. File plugins (e.g. NDFileHDF5) with `NDArrayAddress=5` can save `PrvHstHistogramData`. So **processed histogram (PrvHstHistogramData) is already writable** via existing plugins.

## Options for ImgImageData / ImgImageSumNFrames

### Option A: Driver pushes processed Img as NDArrays (recommended)

**Idea:** Reuse the same pattern as the histogram: in the driver, allocate NDArrays for the processed buffers, fill them from `imgRunningSum_` and the sum-of-N buffer, and call `doCallbacksGenericPointer` on **new NDArray addresses** (e.g. 2 and 3). Existing file plugins (NDFileTIFF, NDFileHDF5) then subscribe to those addresses.

**Details:**

- **Addresses:** Driver uses `maxAddr=6` (addresses 0–5). Addresses **2** and **3** are free. Assign e.g.:
  - **2** = ImgImageData (running sum)
  - **3** = ImgImageSumNFrames (sum of last N)
- **Data type and conversion for TIFF vs HDF5:**
  - **NDFileTIFF** supports NDInt8/16/32, NDUInt8/16/32, NDFloat32/64. It does **not** support NDInt64. So for TIFF, the driver must convert INT64 to a supported type. Three practical options:
    1. **Divide by number of frames (recommended for TIFF):** Compute *average counts per frame* = sum / N_frames (and for sum-of-N: sum / N). The result is a physically meaningful value (counts per pixel per frame) and typically fits in **NDInt32** or **NDUInt32** even for long runs, so NDFileTIFF can write it directly. Use the driver’s frame count (e.g. from the same accumulation path or a dedicated counter); handle N_frames=0 (e.g. write zeros or skip).
    2. **NDFloat64:** Copy sum (or average) as double. No overflow; NDFileTIFF supports it. Slight precision loss for very large integers; good when you want one type for both TIFF and HDF5.
    3. **NDInt32 with scaling/clipping:** Scale or clip the sum to fit Int32 (e.g. divide by a constant or cap at 2^31-1). Loses range or resolution; use only when average-per-frame is not desired and Float64 is not.
  - **NDFileHDF5** supports all NDArray types, including **NDInt64**. Use full 64-bit when you need exact sums (see “When to use full 64-bit vs average-per-frame” below).
- **When to push:**
  - **On-demand:** New PV(s) (e.g. `WriteProcessedImg`, or re-use Capture) trigger a one-time build of one or two NDArrays and `doCallbacksGenericPointer(..., 2)` and/or `(..., 3)`. Fits “save current accumulation” workflow.
  - **Per-frame:** Push every time the Img frame is processed (like PrvHst). Heavier (two extra NDArrays per frame) but keeps file stream in sync with live data.
- **Implementation outline:**
  - In `processImgFrame` (or a new helper called from there / from a write trigger): allocate 2D NDArray(s) from `pNDArrayPool`. For **running sum:** copy from `imgRunningSum_`; for **sum-of-N:** copy from the sum-of-N buffer. For **TIFF (average):** compute pixel[i] = sum[i] / N_frames (or sum[i] / N for sum-of-N), store as NDInt32/NDUInt32; handle N_frames=0 or N=0 (e.g. zeros). For **HDF5 (full range):** copy as NDInt64. Optionally also push NDFloat64 for a single type that works with both. Set dimensions and do not overwrite shared params (same pattern as histogram: save/restore shared params or use NDArray attributes only). Call `doCallbacksGenericPointer(pArray, NDArrayData, 2)` and/or `(..., 3)`. Handle NDArrayCounter like histogram (optional: do not increment, or use a separate counter for “processed” writes).
- **Config:** User sets e.g. `TIFF2:NDArrayAddress=2`, `TIFF3:NDArrayAddress=3` (or HDF5 plugins) to save running sum and sum-of-N.

**Pros:** Reuses existing plugins; consistent with areaDetector and with current PrvHst design; no new plugin to maintain.  
**Cons:** Driver code grows; need to choose trigger (on-demand vs per-frame) and type (NDInt64 vs NDFloat64/NDInt32) per use case.

#### When to use full 64-bit (HDF5) vs average-per-frame (TIFF)

- **Full 64-bit (NDInt64, e.g. NDFileHDF5)** is better when:
  - You need **exact running sums** for later analysis (e.g. background subtraction, merging multiple runs, or recomputing averages with different N).
  - You are doing **quantitative work** where rounding or loss of precision is unacceptable.
  - You want to **combine or sum files** from multiple acquisitions without re-acquiring.
  - You need to **recover the total count** without storing N_frames separately (the file is self-contained as “sum”).
- **Average-per-frame (sum/N as NDInt32/NDUInt32 for TIFF)** is better when:
  - The main goal is **viewing or quick inspection** (average counts per frame is intuitive and usually fits in 32-bit).
  - You want **smaller, TIFF-compatible files** that open in standard image viewers and analysis tools.
  - You do not need to merge runs or recover exact totals; the “per-frame average” is the desired product.
- **Summary:** Use HDF5 + NDInt64 for archival and quantitative pipelines; use TIFF + (sum/N) for visualization and compatibility. The driver can support both by pushing the same logical data as different types on different addresses (e.g. address 2 = NDInt64 for HDF5, address 4 = NDInt32 average for TIFF), or by a PV that selects “output type” (sum vs average) when building the NDArray.

#### Other alternatives for data type / storage

- **NDFloat64 for both TIFF and HDF5:** One output type; no overflow; works everywhere. Slight precision loss for very large integers; acceptable for many use cases.
- **Scale to 16-bit (NDUInt16/NDInt16):** e.g. scale average or sum to 0–65535 for maximum compatibility and smaller files; loses dynamic range unless you scale by (max/65535) or use a known range.
- **Store both sum and average:** e.g. HDF5 file with two datasets: one NDInt64 (running sum), one NDInt32 or NDFloat64 (average). Requires two NDArray callbacks or one plugin that writes multiple arrays; or two separate file plugins with different addresses.
- **Configurable output in driver:** A PV (e.g. “ProcessedImgOutputType”: Sum, Average, or Float64) controls what the driver pushes on addresses 2/3. Same address can then feed either TIFF (average) or HDF5 (sum) depending on configuration.
- **Compression:** HDF5 supports compression; TIFF can use compression. 32-bit average often compresses better than 64-bit sum; helps with disk and I/O.
- **Attributes/metadata:** Store N_frames (and N for sum-of-N) in NDArray attributes or file metadata so that readers can recover “sum” from “average × N” if needed.

### Option B: New areaDetector plugin that reads waveform PVs

**Idea:** A new plugin that subscribes to waveform PVs (e.g. `cam1:ImgImageData`, `cam1:ImgImageSumNFrames`) via CA or asyn, and on trigger (software PV or periodic) reads the array and writes TIFF/HDF5.

**Pros:** No change to driver; one plugin could serve Img and later PrvHst (and other waveform arrays); can support full INT64 in HDF5.  
**Cons:** New plugin to develop and maintain; trigger model differs from NDArray pipeline (no standard “Capture” from driver); depends on reading PVs (latency, size limits).

### Option C: External script / small IOC

**Idea:** Script (e.g. Python) or small IOC that `caget`s the waveform PVs and writes TIFF/HDF5.

**Pros:** Quick to implement.  
**Cons:** Not integrated with areaDetector capture; no standard trigger; harder to tie to acquisition.

## Recommendation

**Option A (driver pushes NDArrays)** is the best fit:

1. Matches the existing pattern (PrvHst already pushes processed data as NDArray on address 5).
2. Reuses NDFileTIFF and NDFileHDF5; no new plugin.
3. Capture/trigger stays inside areaDetector (Capture or dedicated “Write processed” PV).
4. For **TIFF:** driver can push **average-per-frame** (sum / N_frames, or sum-of-N / N) as NDInt32/NDUInt32 so NDFileTIFF works without overflow and keeps a meaningful value; or NDFloat64. For **HDF5:** push NDInt64 to keep full range when exact sums are needed.

**Suggested implementation steps:**

1. Add two new NDArray addresses (e.g. 2 = running sum, 3 = sum-of-N) in the driver; keep `maxAddr=6` (addresses 0–5 already in use; 2 and 3 are free).
2. Add a trigger (e.g. `WriteProcessedImg` or “on Capture” when a mode is set) that:
   - Builds an NDArray from `imgRunningSum_` (and optionally one from sum-of-N buffer).
   - For **TIFF:** use **average = sum / N_frames** (and sum-of-N / N) as NDInt32/NDUInt32; handle N=0. For **HDF5:** use NDInt64 for full range. Optionally support a PV to choose “sum” vs “average” or output type.
   - Calls `doCallbacksGenericPointer(..., 2)` and/or `(..., 3)`.
3. Document that e.g. `TIFF2:NDArrayAddress=2` saves processed Img (as average when using divide-by-N); `HDF5_xxx:NDArrayAddress=2` can save full 64-bit sum. Use NDFileHDF5 when you need exact sums or to combine/merge runs.
4. (Optional) Add per-frame push of address 2/3 if “streaming” processed images to file is required (with performance in mind).

## Histogram (PrvHst) — Already Supported; Optional Extensions

- **PrvHstHistogramData** is already sent as an NDArray on **address 5** (running sum, NDInt64). Any file plugin with `NDArrayAddress=5` (e.g. NDFileHDF5) can save it.
- **PrvHstHistogramTimeMs** is the time axis for plotting; it is not sent as a separate NDArray. It could be stored as an attribute on the histogram NDArray or in the same HDF5 file as a separate dataset if the plugin or a small extension supports it.
- If you need **PrvHstHistogramSumNFrames** (sum of last N histogram frames) as a separate stream, the same pattern applies: push it on another free address (e.g. 4 or 6 would require increasing `maxAddr`) so a file plugin can subscribe to it.

## Summary

| Approach | Best for |
|----------|----------|
| **Driver pushes NDArrays (Option A)** | ImgImageData, ImgImageSumNFrames (and optionally PrvHst sum-of-N); reuse TIFF/HDF5 plugins; consistent with PrvHst. |
| **TIFF (average-per-frame)** | Divide sum by N_frames (or N for sum-of-N); write as NDInt32/NDUInt32 via NDFileTIFF; meaningful value, no overflow. |
| **HDF5 (full 64-bit)** | Push NDInt64 when exact sums, merging runs, or quantitative analysis is needed. |
| **Other types** | NDFloat64 (one type for both); 16-bit scaling; configurable “sum vs average” PV; store N_frames in attributes. |
| **New plugin (Option B)** | Only if you want to avoid any driver changes and are willing to maintain a plugin that reads waveform PVs. |
| **PrvHst** | Already writable via address 5; optional: add time axis in attributes or HDF5, and/or another address for sum-of-N histogram. |

Implementing Option A (driver-side NDArray push for addresses 2 and 3, with on-demand or per-frame trigger) is the most straightforward way to get TIFF/HDF5 writing of `ImgImageData` and `ImgImageSumNFrames`. For TIFF, use **average = sum / N_frames** (and sum-of-N / N) as 32-bit; for HDF5, use **NDInt64** when full range is needed.
