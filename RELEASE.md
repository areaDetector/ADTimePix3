ADTimePix3 Releases
==================

The repository was transferred a while back to areaDetector organization.

* see https://github.com/areaDetector/ADTimePix3

Since Serval features differ the driver is specific to Serval version.
Different branches of ADTimePix3 support different Serval versions.
The master branch (under development) supports Serval 4.x.x and 3.x.x; latest tested is Serval 4.1.5-rc2 (requires TimePix3 Emulator 4.1.5-rc2). Serval 4.1.5 is recommended for 4.x (dual-image fix).

Driver depends on Serval versions, at this time. Current releases support Serval 4.1.5-rc2, 4.1.x, and 3.0.0-3.3.2.


R1-6-2 (TBD, 2026)
--------

* **IOC boot -- deployment-independent calibration paths**: `iocs/tpx3IOC/iocBoot/iocTimePix/init_detector.cmd` sets `BPCFilePath` and `DACSFilePath` using `$(ADTIMEPIX)/vendor/` (with `tpx3-demo.bpc` / `tpx3-demo.dacs`) instead of hard-coded site paths, so the same script works when the module is installed under different trees.

* **Robustness — HTTP error bodies vs JSON**: When SERVAL returns a non-success HTTP status (e.g. **409 Conflict**) or a body that is not JSON (plain text/HTML), the driver previously called `nlohmann::json::parse` on that body in several paths (**`writeDac`**, **`getHealth`**, **`initialServerCheckConnection`** dashboard handling, **`timePixCallback`** `/measurement` polling). That could throw an uncaught **`json::parse_error`** and **abort the IOC** (`SIGABRT`). The driver now checks HTTP status where appropriate, parses JSON only for successful responses, and catches parse exceptions so the IOC logs an error and continues. Operators should still fix SERVAL/detector connectivity; behavior is unchanged when responses are valid JSON.

* **Driver logging (asyn)**: Internal `ERR` / `WARN` / `LOG` / `FLOW` helpers use a single `ADTPX3_FUNC` macro: on GCC/Clang it defaults to `__PRETTY_FUNCTION__` (richer context); other compilers use `__func__`. For shorter log lines, build with `-DADTPX3_LOG_SHORT` (see commented line in `tpx3App/src/Makefile`). Variadic `*_ARGS` macros use `##__VA_ARGS__` so format-only calls (no extra printf args) compile. By default `WARN` / `WARN_ARGS` use `ASYN_TRACE_WARNING`; the port trace mask must include the warning bit to see them. Sites that only enable ERROR-level trace and need legacy visibility for warnings can build with `-DADTPX3_WARN_AS_ERROR` so `WARN*` is emitted at `ASYN_TRACE_ERROR` (see commented line in `tpx3App/src/Makefile`).

* **PixelConfig vs on-disk BPC (on-demand compare)**: Documents what is **running in SERVAL** (per-chip PixelConfig from GET `/detector/chips/<i>/PixelConfig`) versus the **binary file** the IOC points at (`BPCFilePath` + `BPCFileName`). A mismatch means the detector's active pixel configuration bytes differ from that file--for example after local mask edits, a failed upload, or config applied outside the path EPICS uses. Full operator-oriented explanation: **`documentation/PIXELCONFIG_BPC_DIFF.md`**.
  * **Trigger**: `RefreshPixelConfig` (`Dashboard.template`). Not merged into DAC refresh; not polled at 1 Hz.
  * **Per chip**: Base64 JSON string decoded to **65536 bytes** per chip; compared to the BPC file slice at **byte offset `chip x 65536`** (quad = 262144-byte file). Readbacks: `PixelConfigLen_RBV`, `PixelConfigMatchBPC_RBV` (-1 error, 0 !=, 1 =, 2 no file, 3 size), `PixelConfigMismatchBytes_RBV`, `PixelConfigStatus_RBV` (`Chips.template`).
  * **`PixelConfigStatus_RBV` OPI**: Implemented as a **CHAR waveform** (octet readback). In **`tpx3App/op/bob/Mask/PixelConfigMaskPanel.bob`**, each chip status **Text Update** uses **Format = String** (file value `<format>6</format>`) so Phoebus shows the message as text instead of a sequence of ASCII codes.
  * **Waveform `PixelConfigDiff`**: After each refresh, holds **|SERVAL byte - BPC byte|** at each image pixel **`(i,j)`** using **`k = pelIndex(i,j)`** (same mapping as mask write / `maskCircle`), **not** `bcp2ImgIndex`--those two were not equivalent for all quad orientations (e.g. **LEFT**), which broke alignment at chip boundaries. Same `NELEMENTS` as `BPC` / `MaskBPC` in `MaskBPC.template`. Phoebus: `Mask/Mask.bob` embeds `PixelConfigMaskPanel.bob` (refresh, table, 512x512 / 256x256 |Delta| heatmaps).
  * **`MaskBPC` read from `.bpc`**: Loading the mask image from disk uses **`pelIndex(i,j)`** and **`j*COLS+i`** (clears **`value[]`** first), matching mask **write** and **`PixelConfigDiff`**; **`bcp2ImgIndex`** is no longer used on that path.
  * **EPICS / asyn fixes (R1-6-2)**: `TPX3_PIXEL_CONFIG_DIFF` registered as **`asynParamInt32Array`**; `doCallbacksInt32Array` pushes the **full** diff buffer (not `PixCount`). `RefreshPixelConfig` **FLNK** -> `PixelConfigDiff.PROC` so the waveform **VAL** updates on process (same idea as reading `BPC` / `MaskBPC`). Compared length uses **`min(65536, bytes remaining in file)`** per chip so a 4x64 KiB file does not look like a single oversized "chip 0" slice.
  * **Mask mapping bug fix**: `bcp2ImgIndex` quad-chip index used **`(bpcIndex+1)/chipPelCount`**, which misclassified the last pel of each 64 KiB block and the final file byte (`chip == 4`), producing bogus **"detector larger than 2x2"** console messages and wrong boundary mapping. Now **`bcpIndex / chipPelCount`** (for remaining **`bcp2ImgIndex`** callers).

* **Eight-chip / dual SPIDR (IOC, DB, driver, mask indexing)**:
  * **IOC**: `iocs/tpx3IOC/iocBoot/iocTimePix/load_chips.cmd` loads **`Chips.template`** for **`CHIP0`…`CHIP7`** (asyn **`ADDR` 0–7**); `st_base.cmd` sources it. **`OperatingVoltage.template`** extended with **`Pwr3`…`Pwr5`** at **`ADDR` 3–5** for the second board’s three **`VDD`/`AVDD`** rails (see driver).
  * **`MASK_BPC_NELEMENTS`**: **`MaskBPC.template`** requires macro **`MASK_BPC_NELEMENTS`** (must be **≥ PixCount**). It is set in **`unique.cmd`** with documented choices **65536** (1 chip), **262144** (4 chip), **524288** (8 chip). If undefined, iocsh fails **`dbLoadRecords`** for **`MaskBPC.template`** and mask PVs stay disconnected. **`envPaths`** only notes that **`unique.cmd`** must define it.
  * **Driver (`getDetector`, Serval 4)**: Merges all **`Health[].ChipTemperatures`** into one flat JSON array for **`ChipTemps_RBV`**; with two **`Health`** blocks, **`VDD_RBV`/`AVDD_RBV`** strings become a JSON array of per-board rail arrays; fills **`TPX3_CHIP_VDD`/`TPX3_CHIP_AVDD`** at **`ADDR` 3–5** from **`Health[1]`** (zeros when single board). **`Info.Boards[1]`** → **`TPX3_BOARDS2_ID`**, **`TPX3_BOARDS2_IP`**, **`TPX3_BOARDS_CH5`…`CH8`** (second SPIDR chip JSON). Safer parsing when **`Boards`** or **`Chips`** arrays are short.
  * **Database**: **`ADTimePix3.template`** — **`BChBoard2Id_RBV`**, **`IpAddr2_RBV`**, **`Chip5_RBV`…`Chip8_RBV`**. New asyn parameter names in **`ADTimePix.h`** / **`createParam`**.
  * **Mask / BPC (`mask_io.cpp`)**: For **`numChips == 8`** and **DetectorOrientation UP (0)** only, **`pelIndex`** and **`bcp2ImgIndex`** use a rectangular mosaic with **`chip = Y_CHIP * xChips + X_CHIP`** from **`rowsCols()`**; intra-chip order matches single-chip **UP**. Other orientations for eight tiles remain TODO (verify on hardware vs Serval **`Layout`**).
  * **Documentation**: **`documentation/8chip-migration.md`**; README and **`documentation/MIGRATION_SUPPORT2.md`** updated (**`MASK_BPC_NELEMENTS`** checklist, links).

* **Phoebus histogram InfoDisplay layout tweak**: Manually adjusted the layout in `tpx3App/op/bob/Acquire/PrvHstHistogram.bob` so InfoDisplay text fits properly (increased container/text height).


R1-6-1 (March 31, 2026)
--------

* **Processed Img file saving (Option A)**: The driver can push the current image accumulation (running sum and sum-of-N) as NDArrays to file plugins so that `ImgImageData` and `ImgImageSumNFrames` can be saved as TIFF or HDF5.
  * **WriteProcessedImg** (boolean PV): Writing 1 pushes the current running sum to NDArray address 2 and the current sum-of-N (if available) to address 3. File plugins (NDFileTIFF, NDFileHDF5) with `NDArrayAddress=2` or `3` receive and can save them. One-shot; driver resets the PV to 0 after the push.
  * **ProcessedImgOutputType** (mbbo): 0 = Sum (NDInt64, for HDF5); 1 = Average (NDInt32, sum divided by number of frames, for NDFileTIFF). Enables TIFF writing without overflow by storing average counts per frame.
  * **Driver**: New `pushProcessedImgToPlugins()` builds NDArrays from `imgRunningSum_` and the sum-of-N buffer, optionally converts to average (divide by `imgAccumulatedFrameCount_` or by N for sum-of-N), and calls `doCallbacksGenericPointer(..., 2)` and `(..., 3)`. Shared params (ADSizeX/Y, etc.) are saved/restored so image channels are unchanged; NDArrayCounter is restored so the main counter is not affected. New `imgAccumulatedFrameCount_` tracks frames in the running sum for the average calculation.
  * **Database**: New records in `Server.template` for `WriteProcessedImg` and `ProcessedImgOutputType` (DESC fields kept <=40 characters).
  * **Phoebus**: `ImgAccumulation.bob` - added "Push to file (addr 2,3)" button and "Output type" combo (Sum / Average). Uses `combo` widget type for compatibility.
  * **Documentation**: `documentation/PROCESSED_IMAGE_FILE_SAVING.md` - Status section added; README "File Saving" under Img accumulation updated with WriteProcessedImg and NDArrayAddress 2/3 usage. **Clarified (doc fix):** addresses 2 and 3 also receive **per-frame** NDArrays from `processImgFrame` when Img accumulation is enabled; `WriteProcessedImg` is an additional on-demand push with Sum/Average typing. Documented that `ImgImageFrame` has no NDArray address (use address 1 for single-frame file saving), optional HDF5 `dbLoadRecords` one-line iocsh note, and a short verification tip (mean ratio addr 3 vs 1 ~ N).
  * **Validation procedure/results**: Added a reproducible single-plugin runtime `NDArrayAddress` verification section (HDF5 and TIFF) in `documentation/PROCESSED_IMAGE_FILE_SAVING.md`, including numeric results and viewer-compatibility notes for 64-bit TIFF.
  * **NeXus-style file output**: IOC boot examples `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_minimal.xml` (NDFileHDF5 `hdf5_layout`) and `nexus_plugin_template.xml` (NDFileNexus `NXroot` template, validated with ADCore `XML_schema/template.sch`). README documents that HDF5 and Nexus plugins use different XML dialects, template PVs, and `TemplateFilePath` concatenation rules.
  * **Readout stack diagram**: `documentation/TimePix3_pipeline_48_64_96_caption.png` (PNG) and `documentation/TimePix3_pipeline_48_64_96.svg` (editable source). Shows chip -> readout -> SERVAL and the **ADTimePix3** / EPICS path vs optional **LUNA** (96-bit) path not used by the driver; README links to both files under *Additional information*.
  * **PrvHst NDArray file streams (addresses 4-7)**: With PrvHst accumulation enabled, `processPrvHstFrame` pushes **1D** NDArrays each frame: **4** = sum of last N (`NDInt64`, when the sum-of-N buffer updates), **5** = running sum (`NDInt64`), **6** = current frame (`NDInt32`), **7** = ToF bin centers in ms (`NDFloat64`, parallel to `PrvHstHistogramTimeMs`). Arrays on **4** and **5** add NDAttributes `PrvHstTimeBin0Ms`, `PrvHstTimeBinStepMs`, `PrvHstNumBins` (uniform axis metadata). **`WriteProcessedHst`** / **`ProcessedHstOutputType`** mirror processed Img: one-shot push with Sum (`NDInt64`) vs Average (`NDInt32`, divide by frame count or sum-of-N buffer length). **`pushProcessedHstToPlugins()`** in `ADTimePix.cpp`; **`ADDriver(..., maxAddr=8, ...)`** so lists **0-7** exist. Records in `Server.template`. See `documentation/PROCESSED_IMAGE_FILE_SAVING.md` and README Histogram sections.
* **PrvHst NeXus/HDF5 1D ToF example layout**: Added `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_prvhst_histogram.xml` (`hdf5_layout`) for NDFileHDF5. The main detector dataset stores counts (address 5 by default), with uniform ToF axis metadata from NDAttributes (`PrvHstTimeBin0Ms`, `PrvHstTimeBinStepMs`, `PrvHstNumBins`) so clients can reconstruct bin centers without requiring a second full axis dataset.
* **HDF5 write troubleshooting (PrvHst)**: Documented and validated the `NDPluginFile::writeInt32: ERROR, no valid array to write` path in `documentation/PROCESSED_IMAGE_FILE_SAVING.md`. Clarifies that `NDWriteFile` requires at least one latched NDArray on the selected plugin address, and adds a minimal check sequence for `ArrayCallbacks`, `HDF1:NDArrayAddress`, and `WriteProcessedHst`.
* **PV naming clarification for callbacks**: Updated docs to distinguish EPICS PV `$(P)$(R)ArrayCallbacks` / `_RBV` from internal driver/asyn parameter `NDArrayCallbacks`, avoiding "channel not found" confusion when testing from CA clients.
* **Phoebus histogram plot artifact fix**: Updated `tpx3App/op/bob/Acquire/PrvHstHistogram.bob` XY traces to points-only rendering (no line interpolation) to avoid line-to-origin artifacts when waveform `NELM` exceeds valid `NORD`.


R1-6 (March 4, 2026)
--------

* **CONNECT/DISCONNECT (graceful reconnection without IOC restart)**: The driver now supports connection status monitoring and optional config refresh when SERVAL or the detector reconnects, so the IOC does not need to be restarted when the detector/emulator or SERVAL restarts.
  * **Bug fix - DetConnected inversion**: Corrected `ADTimePixDetConnected` logic in `initialServerCheckConnection()`: detector present is now reported as 1 (connected), detector absent as 0 (disconnected). Previously the values were inverted.
  * **Lightweight connection check**: New `checkConnection()` uses GET `/dashboard` as the single source of truth and updates only `ServalConnected_RBV`, `DetConnected_RBV`, `ADStatusMessage`, and (when connected) `DetType`/`ADModel`. ServalConnected is 1 when GET `/dashboard` returns 200 (SERVAL reachable even when the root URL returns 404/302). DetConnected is 1 only when the dashboard JSON has `Detector` non-null. It does not call `getServer()` or full `getDashboard()`.
  * **Consistent connection status in getDashboard()**: `getDashboard()` (Health PV path) now sets `ServalConnected_RBV` and `DetConnected_RBV` the same way as `checkConnection()`: ServalConnected=1 when GET `/dashboard` returns 200; DetConnected=1 only when the dashboard JSON has `Detector` non-null (and DetType/ADModel set); both 0 on non-200 or parse error. This avoids inconsistent states (e.g. ServalConnected=false, DetConnected=true) when Health is triggered.
  * **On-demand refresh**: New PV `RefreshConnection` (boolean output): writing 1 runs `checkConnection()` and updates connection status. Record in `Dashboard.template`.
  * **Periodic connection poll**: A low-priority thread runs every 5 seconds (default), calls `checkConnection()`, and on transition from disconnected to connected optionally calls `getServer()` once to refresh channel config. No `acquireStop()` or `initCamera()` is invoked from the poll thread. Poll is started in the constructor and stopped in the destructor.
  * **Behavior on disconnect**: Status PVs are set to disconnected and `ADStatusMessage` is set to "SERVAL or detector disconnected".
  * **Behavior on reconnect**: Status PVs are set to connected, `ADStatusMessage` is cleared to "OK", and channel config is re-applied by calling `fileWriter()` (push current PV config to SERVAL) then `getServer()` (refresh PVs from SERVAL). Detector initialization (BPC/DACS load, `initCamera()`) is not run on reconnect.
* **Detector initialization single source of truth**: Detector initialization (channel paths, formats, modes, BPC/DACS paths, WriteData, etc.) uses EPICS PVs as the single source of truth: the init script sets PVs, and the driver applies them to SERVAL via `fileWriter()` (triggered by `WriteData=1` or `ApplyConfig`).
  * **init_detector.cmd**: The detector init block (formerly inline in `st_base.cmd`) is now in `iocs/tpx3IOC/iocBoot/iocTimePix/init_detector.cmd`. `st_base.cmd` sources it after `iocInit()`. The same file can be re-run from iocsh after reconnect (e.g. `< init_detector.cmd`) to re-apply the same init.
  * **ApplyConfig PV**: New PV `ApplyConfig` (boolean output): writing 1 runs `fileWriter()` + `getServer()` (same effect as `WriteData=1`), so operators can re-apply current PV config to SERVAL without toggling WriteData or re-running a script. Record in `Dashboard.template`.
  * **Reconnect**: On transition from disconnected to connected, the connection poll now calls `fileWriter()` (push channel config), `initAcquisition()` (push detector config: TriggerMode, NumImages, etc., from PVs to SERVAL so they stay CONTINUOUS / desired values after restart), then `getServer()` (refresh channel state from SERVAL).
  * **Documentation**: README and this release note state that detector initialization is defined by the PV values set at startup (e.g. from `init_detector.cmd` or `st_base.cmd`); the driver applies those PVs to SERVAL when `WriteData=1` or when `ApplyConfig` is triggered.
* **Code Organization - BPC/mask logic in mask_io.cpp**: Moved `checkBPCPath()` and `uploadBPC()` from `ADTimePix.cpp` to `mask_io.cpp` to shorten the main driver file and keep all BPC/mask path and upload logic in one place. `mask_io.cpp` already contained `readBPCfile()`, `writeBPCfile()`, and mask helpers; it now also implements BPC path validation and BPC upload to SERVAL. No functional or PV changes.
* **Measurement.Config PVs (SERVAL 4.1.x - 4D-STEM and Time-of-Flight)**: Added read/write support for SERVAL Measurement.Config via GET/PUT `/measurement/config`. Previously the driver used only Measurement.Info (status, rates, frame count); Measurement.Config was not exposed. New asyn parameters and EPICS records:
  * **Stem (4D-STEM)**: `StemScanWidth`, `StemScanHeight`, `StemDwellTime` (Scan); `StemRadiusOuter`, `StemRadiusInner` (VirtualDetector). Read/write; values are sent to SERVAL on write and refreshed from SERVAL when "Scan detector/health" (Health PV) is triggered.
  * **TimeOfFlight**: `TofTdcReference` (comma-separated string, e.g. `PN0123,PN0123`), `TofMin`, `TofMax`. Same read/write and refresh behavior.
* **Driver**: `getMeasurementConfig()` (GET `/measurement/config`, parse Stem and TimeOfFlight into PVs), `sendMeasurementConfig()` (merge PVs into current config, PUT `/measurement/config`). Called when Health is written (with getDetector) and when any of the new PVs are written. Corrections and Processing branches are preserved on PUT (merge with existing config).
* **Database**: New records in `Measurement.template` for all eight parameters (setpoint + _RBV readback where applicable).
* **Phoebus screen**: New `Measurement/MeasurementConfig.bob` screen (4D-STEM and ToF sections) with spinners/text entries and readbacks. Embedded in `TimePix3Detector.bob` below Measurement Info so Measurement Config is available from the Detector Status tab. Layout may be adjusted manually for best fit.
* **Screens (new and updated)**:
  * **New**: `Measurement/MeasurementConfig.bob` - Measurement.Config (4D-STEM Stem scan/virtual detector, Time-of-Flight). Opened from Detector Status tab (TimePix3Detector) below Measurement Info.
  * **Updated**: `TimePix3Detector.bob` - embeds MeasurementConfig; size/layout may be adjusted manually.
  * **Updated**: `tpx3emulator.bob` - title and Replay File label now state replay of previously collected raw .tpx3 data; tooltip added on Replay File. Emulator screen supports replay of raw .tpx3 files (single, repeat N, forever, file list). Opened from Detector Config tab.
* **Bug fix - Memory leaks**:
  * **BPC buffer** (`mask_io.cpp`): `readBPCfile()` allocates a buffer with `malloc()` and passes it to the caller. The buffer was never freed in `readInt32Array()`, leaking memory on every mask/BPC read or write. The buffer is now freed before `readInt32Array()` returns (and `bufBPC` is initialized to `NULL`).
  * **Histogram NDArray** (`histogram_io.cpp`): When `pNDArrayPool->alloc()` returned a non-null array with null `pData`, the array was not released, leaking a pool reference. The failure path now calls `pHistArray->release()` when `pHistArray` is non-null.
* **Database**: `Measurement.template` - `TofTdcReference` stringout record uses device support `asynOctetWrite` (not `asynOctet`) so `dbLoadRecords` does not report "no such device support for 'stringout' record type".
* **ADCore** `iocBoot/commonPlugins.cmd`: request file paths for support modules must use each module's **Db** folder (e.g. `$(CALC)/calcApp/Db`, `$(SSCAN)/sscanApp/Db`), not the lowercase `db` folder, so save_restore finds the `.req` files (e.g. `sseq_settings.req`, `sseqRecord_settings.req`).

R1-5 (January 27, 2026)
--------

* **TCP Streaming for Preview Images**: Replaced GraphicsMagick HTTP method with TCP jsonimage streaming for PrvImg channel. Preview images now use configurable TCP streaming (tcp://listen@hostname:port format) for lower latency and better performance. GraphicsMagick implementation preserved in `preserve/graphicsmagick-preview` branch for backward compatibility.
* **TCP Streaming for Image Channel**: Added TCP jsonimage streaming support for Img channel, enabling real-time 2D image streaming similar to PrvImg channel. Both PrvImg and Img channels can now stream concurrently using separate array slots (pArrays[0] for PrvImg, pArrays[1] for Img) to prevent reference count conflicts.
* **TCP Streaming for Histogram Channel**: Added TCP jsonhisto streaming support for PrvHst channel, enabling real-time 1D histogram streaming. Histogram data is processed and accumulated (running sum) with support for sum of last N frames. Histogram NDArrays are sent via callbacks with NDArrayAddress=5 for use with areaDetector plugins. PrvHst channel can stream concurrently with PrvImg and Img channels without conflicts.
* **Histogram Accumulation Enable Control**: Added `PrvHstAccumulationEnable` control (similar to `ImgAccumulationEnable`) to enable/disable histogram accumulation processing. When disabled, the driver does not connect to the TCP port, allowing other clients to process jsonhisto data. When enabled, the driver connects and processes histogram frames for accumulation and display.
* **Histogram Waveform PVs for ToF Plotting**: Added waveform PVs for histogram data visualization:
  - `PrvHstHistogramData` (INT64): Accumulated histogram data (running sum)
  - `PrvHstHistogramFrame` (INT32): Current frame histogram data
  - `PrvHstHistogramSumNFrames` (INT64): Sum of last N frames
  - `PrvHstHistogramTimeMs` (DOUBLE): Time-of-Flight axis in milliseconds (bin centers)
* **Phoebus Screen for Histogram Plotting**: Updated `PrvHstHistogram.bob` screen with three XY plots for Time-of-Flight histogram visualization:
  - Accumulated histogram vs ToF [ms]
  - Current frame histogram vs ToF [ms]
  - Sum of last N frames vs ToF [ms]
  The screen includes controls for enabling/disabling accumulation and histogram configuration parameters.
* **Concurrent Channel Support**: Fixed NDArray reference count conflicts when multiple channels are configured for TCP streaming simultaneously. Each channel now uses its own NDArray address (PrvImg: 0, Img: 1, PrvHst: 5), allowing stable concurrent operation of all three channels.
* **Clean Shutdown for TCP Streaming**: Fixed "Broken pipe" errors when stopping acquisition with preview, image, and/or histogram channels enabled. Shutdown sequence now properly stops Serval measurement first, signals all worker threads (PrvImg, Img, PrvHst) to stop concurrently, then waits for all threads to exit before disconnecting. This ensures clean shutdown even with rate mismatches (e.g., 60 Hz frame rate vs 5 Hz preview rate). Worker threads now detect "Connection closed by peer" as expected behavior during shutdown. Connection closure messages are channel-specific ("PrvImg TCP connection closed by peer", "Img TCP connection closed by peer", and "PrvHst TCP connection closed by peer") to distinguish which channel is closing. This unified shutdown sequence prevents `java.lang.InterruptedException` errors from Serval's TcpSender threads.
* **Optimized TCP Connection Delays**: Reduced delay times to minimum safe values for faster acquisition startup/shutdown: port release delay (100ms), port binding delays (200ms for PrvImg and Img channels), and TcpSender stop delay (300ms). These optimized delays reduce total acquisition cycle time by ~900ms while maintaining reliability.
* **Improved Debug Messages**: Enhanced acquisition status debug messages to show actual ADStatus values (0 = Idle, 1 = Acquire) instead of asynStatus return codes. Messages now display ADAcquire value, previous state, and ADStatus before and after acquireStart()/acquireStop() operations for better debugging.
* **Phoebus .bob Screen Updates**: Updated multiple Phoebus display builder (.bob) screens throughout the user interface to reflect new features and improvements, including TCP streaming support, Img1 channel configuration, PrvImg monitoring, and enhanced metadata display. Screens updated include acquisition controls, detector configuration, file writing, mask management, and status monitoring.
* **Image Accumulation Features for Img Channel**: Integrated functionality from the standalone `tpx3image` IOC into the ADTimePix3 driver's Img channel. New features include:
  - **Running Sum Accumulation**: 64-bit accumulation of pixel values over all frames via `ImgImageData` PV (INT64 waveform array)
  - **Current Frame Display**: Individual frame data via `ImgImageFrame` PV (INT32 waveform array)
  - **Sum of Last N Frames**: Configurable sum of last N frames (default: 10) via `ImgImageSumNFrames` PV (INT64 waveform array), with configurable update interval
  - **Performance Monitoring**: Processing time (`ImgProcessingTime`, ms) and memory usage (`ImgMemoryUsage`, MB) tracking
  - **Total Counts**: Total counts across all accumulated frames via `ImgTotalCounts` PV (INT64)
  - **Reset Control**: `ImgImageDataReset` (boolean output) - one-shot button to reset accumulated image data, clearing running sum, frame buffer, total counts, and processing time samples
  - **Configuration PVs**: `ImgFramesToSum` (1-100000, default: 10) and `ImgSumUpdateInterval` (1-10000, default: 1 frame)
  - **Phoebus Screen**: Updated `ImgAccumulation.bob` screen (located in `Acquire/` folder) adapted for ADTimePix3 to visualize accumulated images, sum of N frames, and performance metrics, with controls for enabling/disabling accumulation and resetting accumulation
  - **Code Organization**: ImageData class and accumulation logic implemented in separate files (`img_accumulation.h`/`.cpp`) following existing pattern (`mask_io.cpp`) for better maintainability
* **Bug Fix - Image Accumulation Parameter Update**: Fixed critical bug in `writeInt32()` handler where condition for bias/trigger parameters (line 2795) was missing `function ==` comparisons for subsequent terms, causing all writeInt32 calls to be intercepted and preventing `ImgFramesToSum` parameter updates from being processed. The buffer size now correctly updates when `ImgFramesToSum` PV is changed, and memory usage properly reflects the configured buffer size.
* **Bug Fix - DataType_RBV "Illegal Value" with Histogram Channel**: Fixed issue where histogram channel was setting `NDDataType` to `NDInt64` (value 8), but the `DataType_RBV` database record enum only supports values 0-5, causing "Illegal Value" status. The driver now preserves the previous `NDDataType` value and does not set it for histogram data. The NDArray itself still correctly uses `NDInt64` data type internally.
* **Bug Fix - SizeX_RBV/SizeY_RBV Incorrect Values with Histogram Channel**: Fixed issue where histogram channel was overwriting shared size parameters (`ADSizeX`, `NDArraySizeX`, `ADSizeY`, `NDArraySizeY`) used by image channels, causing histogram bin sizes (e.g., 16000) to overwrite image dimensions (e.g., 512). The driver now preserves these shared parameters and does not set them for histogram data, since histogram uses NDArray address 5 (separate from image addresses) and the NDArray contains size information in its attributes.
* **Bug Fix - ArrayRate_RBV Showing Double Rate**: Fixed issue where `ArrayRate_RBV` was showing double the actual rate (e.g., 10 Hz instead of 5 Hz) when preview image channel (PrvImg) was active. Both PrvImg and Img channels were incrementing the shared `NDArrayCounter`, causing double counting. The driver now only increments `NDArrayCounter` in the Img channel (main acquisition), not in PrvImg channel (preview). `ArrayRate_RBV` now correctly reflects the main Img channel rate. Preview channel rate is available via `PrvImgAcqRate_RBV` PV. Also fixed NDArray address assignment: PrvImg uses address 0, Img uses address 1 (was incorrectly using 0 for both).
* **Bug Fix - PVA Image Flickering with Histogram Channel**: Fixed issue where PVA image display (`pva://TPX3-TEST:Pva1:Image`) flickered when histogram channel was enabled. The flickering occurs in the **Preview image channel** (`TPX3-TEST:cam1:PrvImgFilePath`, using `tcp://listen@localhost:8089` and `http://localhost`), **not** in the actual image channel (`TPX3-TEST:cam1:ImgFilePath`). The histogram channel was calling `callParamCallbacks()` without arguments, which updated all parameters including shared ones (ADSizeX, ADSizeY, etc.) used by PVA plugin, triggering unnecessary redraws. The driver now uses selective parameter callbacks, only updating histogram-specific parameters. This prevents parameter updates from affecting image channel displays. **Note**: Investigation confirmed there are no duplicate PVs between ADTimePix3 and the standalone histogram IOC that could cause conflicts. The flickering that persists after this fix is likely related to the TimePix3 emulator (`tpx3Emulator-4.1.1-rc1.jar`) behavior when histogram channel is enabled in Serval configuration, affecting preview image data timing, network bandwidth allocation, and frame synchronization. This may be emulator-specific and might not occur with physical detectors. This is a known emulator limitation, not a driver bug. See `documentation/PVA_FLICKERING_DIAGNOSIS.md` for detailed analysis and diagnostic steps.
* **Optimization - Histogram Processing When Accumulation Disabled**: Optimized histogram data processing to skip all processing (including rate calculation) when `PrvHstAccumulationEnable` is disabled. Previously, the worker thread would still parse and process histogram data even when accumulation was disabled, only skipping the accumulation step. The driver now returns early from `processPrvHstDataLine()` when accumulation is disabled, avoiding unnecessary CPU usage and parameter updates.
* **Bug Fix - Histogram Log Message Spam When Accumulation Disabled**: Fixed issue where repeated "PrvHst TCP buffer full without finding newline, resetting" messages were filling log files when `PrvHstAccumulationEnable` was disabled. The worker thread was still reading data from the TCP socket even when accumulation was disabled, causing the buffer to fill without processing. The driver now skips data reading when accumulation is disabled, preventing buffer overflow and suppressing warning messages. The worker thread remains connected (so Serval doesn't see a disconnect) but doesn't consume data, significantly reducing log file spam. These messages are warnings (not errors) indicating buffer overflow, which is expected behavior when accumulation is disabled since data isn't being processed.
* **Feature - Histogram Data Reset Control**: Added `PrvHstDataReset` PV (boolean output) to allow manual reset of accumulated histogram data. This one-shot button resets `PrvHstFrameCount_RBV`, `PrvHstTotalCounts_RBV`, clears `PrvHstHistogramData` and `PrvHstHistogramSumNFrames` waveform arrays, and resets processing time and memory usage. Histogram counters no longer automatically reset when acquisition stops, allowing users to preserve accumulated data between acquisitions and reset manually when needed. A reset button has been added to the `PrvHstHistogram.bob` screen in the ConfigurationControl group, matching the arrangement in the image accumulation screen. This provides consistent user experience between image and histogram accumulation features.
* **Feature - Histogram Millisecond PVs for Compatibility**: Added calculated PVs `PrvHstBinWidthMs_RBV`, `PrvHstBinOffsetMs_RBV`, and `PrvHstTotalTimeMs_RBV` that provide histogram bin parameters in milliseconds (calculated from frame parameters using TDC clock period constant). These PVs are compatible with the standalone histogram IOC screen format and are automatically updated when frame parameters change. The PVs use calc records with `CP` (Change Process) flags to update when source parameters change.
* **Feature - StatusMessage_RBV Updates**: Added status message updates to `ADStatusMessage` (standard areaDetector parameter) to provide user feedback during acquisition operations. Status messages include "Starting acquisition...", "Acquisition running", "Acquisition stopped", and error messages for failed start/stop operations. The status message updates automatically as acquisition state changes, providing clear feedback in the user interface.
* **Bug Fix - NumImages INVALID Status**: Fixed issue where `NumImages` PV showed INVALID status with value 100000000 (100 million) at IOC startup. The ADDriver base class was initializing `NumImages` to a very large default value, causing INVALID status. The driver now initializes `NumImages` to 0 (unlimited) in the constructor, which is appropriate for continuous mode and prevents the INVALID status. For single mode (ImageMode = 0), the driver automatically sets `NumImages = 1` when switching to single mode. **Bug Fix - NumImages WRITE INVALID in single mode**: When the database or a client wrote a value other than 1 to `NumImages` while already in single mode, the driver returned an error ("Cannot set numImages in single mode"), causing WRITE INVALID on the record and log spam. The driver now accepts the write and clamps the value to 1 internally, so the record no longer goes WRITE INVALID. **No action required** - both issues have been resolved in R1-5.
* **Bug Fix - ArrayCounter_RBV Double Count with Histogram Channel**: Fixed issue where `ArrayCounter_RBV` was approximately twice `NumImagesCounter_RBV` when jsonhisto streaming was enabled. The histogram channel (PrvHst, address 5) was calling `doCallbacksGenericPointer()`, which automatically increments the shared `NDArrayCounter` parameter. Since histogram uses a separate NDArray address (5) and should not affect the image channel counter, the driver now saves and restores `NDArrayCounter` around histogram callbacks to prevent histogram from incrementing it. `ArrayCounter_RBV` now correctly reflects only image frame counts, matching `NumImagesCounter_RBV` when histogram is enabled. **No action required** - this has been resolved in R1-5.
* **Bug Fix - "parameter 51 in list 5, invalid list" asyn Warnings**: The driver was created with `maxAddr=4` (addresses 0-3 only), but the histogram channel uses NDArray address 5. When asyn or plugins accessed parameter 51 (ARRAY_DATA) at address 5, asyn reported "invalid list" because that address did not exist. The driver now uses `maxAddr=6` so that PrvImg=0, Img=1, and PrvHst=5 are all valid. The "parameter 51 in list 5, invalid list" (and similar) messages no longer appear when the histogram channel is enabled. **Root cause (from asynReport)**: "Parameter 51 is undefined, name=ARRAY_DATA" in "list 5" indicated that address (list) 5 was invalid on the TPX3 port, not that parameter 51 was undefined.
* **Known Issue - RESNIC Pattern Distortion in Phoebus**: Some images displayed in Phoebus show distorted RESNIC patterns (expected checkerboard pattern appears as a different structure). This is an intermittent issue that affects only some frames, not all. The correct checkerboard pattern is visible in some images, while others show a distorted pattern with vertical stripes or different geometric structures. **This may be a SERVAL bug specific to TCP streaming channels when histogram (jsonhisto) is enabled.** The byte swapping code in the driver appears correct (using `__builtin_bswap16`/`__builtin_bswap32` for network byte order conversion). The issue may be related to:
  - SERVAL TCP streaming implementation when multiple channels (image + histogram) are active
  - Data corruption or frame synchronization issues in SERVAL's TCP sender threads
  - Network bandwidth allocation or timing issues when histogram channel competes with image channel
  - Emulator-specific behavior (if using TimePix3 emulator)

  Further investigation is needed to determine if this is a SERVAL bug, driver issue, network transfer issue, or display issue. If you encounter this issue, please report it with details about which channel (PrvImg or Img), frame numbers, whether it occurs consistently or intermittently, and whether it only occurs when histogram channel is enabled.
* **Note**: GraphicsMagick was only used for preview image reading via HTTP. File saving is handled directly by Serval, which writes files to disk without using GraphicsMagick.

R1-4 (January 8, 2026)
--------

* For img channel, rename $(P)$(R)ImgStpOnDskLim, $(P)$(R)ImgStpOnDskLim_RBV for consistancy. 
* Refresh the FileWriter Phoebus screens for additional img1 channel.
* Add additional image (img1) channel, to allow concurrent tcp streming of jsonimag, and TIFF file writing.
* cpr (1.9.1) and nlohmann/json (3.12.0) sources embeded in this ioc (Jakub Wlodek)
* documentation folder created

R1-3 (September 7, 2025)
--------

* Preview readout of tiff (50 fsp) is faster than png (37 fps).
* Resolved "Warning: timePixCallback thread self-join of unjoinable"
* The `fileWriter()` function in the ADTimePix3 driver has been successfully optimized to improve maintainability, performance, and reliability. The original 200+ line monolithic function has been refactored into a modular, well-structured implementation.
* Serval 4.x.x: Rotations/mirror operation strict checking, and requires 'reset=true', DOWN->half instead of 180.
* TDC1/TDC2 reporting for serval 4, and Serval 3.
* The Serval 4.x.y requires {"Content-Type", "application/json"}, instead of "text/plain", as "Content-Type" when using cpr library.
  * Log Level, ChainMode, Polarity, PeriphClk80, ExternalReferenceClock, etc.
* Serval 4.1.x requires changes to the driver.
  * Server->Detector->Health is an array of kv pairs in Serval 4.1.x, but was kv pairs in Serval v3.3.2.

  * Server->Detector->Health->Pressure
  * Server->Detector->Info->ChipType

R1-2 (June 24, 2025)
--------

* Alarms added: TDC not present, NO detector counts.
* ResetHotPixels PV loads negative then positive mask. Neutron detectors radiation issues.
* Alarms: stream channels, serval/detector connection, hot pixels detection.
* Read back configured channels from Serval
* Read VDD and AVDD and set monitor. {Voltage [V], Current [A], Power [W]}
* Compute masked pixels as areaDetector 1D array index (replaces bpc vector index).
* Links to manuals from DetectorConfig.opi screen.
* The ioc starts even without detector or Serval: detector and serval API status PVs are updated.
* Individual chip temperature, and archiver monitoring
* Mask generation for one chip, and 2x2 quad chip detectors for all 8 detector orientations.
* AD mask image placed into BPC calibration mask.bpc file.
* Masked pixels from BPC read into AD mask image.
* Masked pels in 2nd octet of BPC waveform PV
* Connection status PVs
  + Serval connected (URL web interface is up)
  + Detector is connected to Serval
* Report number of masked pixels in bpc calibration
* Manipulation of the Binary Pixel Configuration (BPC). Allows creation of positive or negative image mask.
  + Create image rectangular mask
  + Create image circular mask
  + Image mask can be positive or negative
  + Individual image mask is additive
  + Number of image masks is not limited
* Rotate image into all 8 possible arrangement.
  + Update readback PV to report orientation correctly
* Data collection control added
  + Raw modes: Queue size
  + Image mode: Integration Size, Queue Size, Stop Measurement on Disk Limit.
  + Preview modes: Integration Size, Queue Size, Stop Measurement on Disk Limit.
* Serval 3.3.2-SNAPSHOT: ORNL assembled detectors do not have voltage bias sensors in detection layer. Detection layer is build by ORNL, and not provided by ASI. The 3.3.2-SNAPSHOT minimizes spam TimePix3 related to missing bias from log file. The log file size is manageable with this change in Serval.
* Bug fix: Image Integration mode connected to correct PV.
* Operational ioc: st.cmd delay before writing the preview images.
* Updated driver Revision version in file.

R1-1 (August 18, 2024)
--------

* Updated documentaiton
* Attempt at rotating images using DetectorOrientation in Layout

R1-0 (August 17, 2024)
--------

* Chips DACs voltages settings use atomic function. The DACs voltages must be written at the same time (serval constraint).
  * Two chips DACs voltages have PVs for setting values, but support for changing any of the 18 voltages can be added with new `writeDac()` rewrite.
    * Vthreshold_coarse
    * Vthreshold_fine
* Another rewrite of the layout image rotation [not working, checking with ASI]
  * There are 8 possible image rotation modes
* The Phoebus .bob screens are converted/created, but not yet included in the driver.

R0-9 (February 25, 2024)
--------

* Multiple raw streams (Serval 3.3.0 support)
  * raw can be .tpx3 file
  * raw1 can be a socket

R0-8 (February 25, 2024)
--------

* count_fb mode support added which was added originally in Serval 3.1
* Chip loop update starts at chip1, since chip0
* Initial work on Chip orientation control in FPGA
* Measurement status values updated for interrupted acquisition
* Serval 3.3.0 both TDC update during image readout
* Serval 3.3.0 uses two TDC with separate computations
* Updated BPC / DACS to have default shorter path
* Chip Voltage threshold coarse/fine setting opi and control
  * Douglas Araujo from ESS PR
  * Refactor DACs code to use asyn multidevice mechanism, using different asyn address for the chips
  * Initialize GraphicsMagick on class constructor
  * Fix RawStream PV type
* Destructor is now static const
* TDC signals for one-chip TPX3 detector
* Raw file streaming to tcp://connect@localhost:8085 enabled
* ADCore commonPlugins.cmd settings
* Preview image loop speed up
* Preview[0] channel does not need to be enabled; however, prv images are not displayed
* Driver ensures that for Continuous mode ExposureTime (AcquireTime) propagates to TriggerPeriod (AcquirePeriod), must be equal to each other
* Default reduce readout of preview images to not more than 1 per second
* Remove diagnostic messages
* Dashboard DiskSpace is empty until raw file writing enabled, and acquisition starts
* Dashboard work updated
* Startup update with description of channels
* Allow streaming of raw .tpx3 events, saving of prv images not required, more consistent default channel setting.

R0-7 (June 26, 2023)
--------

* Driver works for Serval 3.2 and Emulator 3.2
  * Separate TDC1 and TDC2 EventRate PVs in Serval 3.2
* Performance issue resolved for reading preview images. Multiple connections were opening in callback while loop.
  * Used cpr library object called a Session is now used. Session is the only stateful piece of the cpr library.
  * Session object of cpr library is useful to hold on to state while reading preview images.
* The preview images do not need to be written to disk, and are read from PrvImg Base [0] channel socket channel.
  * The preview is on Preview [1] channel, which does not have to enabled
* Allow streaming of raw .tpx3 data.
  * Streaming: tcp://localhost:8085
  * Writing to .tpx3 file: file:/media/nvme/raw
* More consistent settings for file writing and streaming channels in st_base.cmd
* opi screens for fileWriter streaming enhancement
* Continuous mode: ExposureTime (AcquireTime) used and propagates to TriggerPeriod (AcquirePeriod)
* Preview[0] does not need to be enabled. Preview images are then not displayed.

R0-6 (March 30, 2023)
--------

* Driver works for one chip, in addition to four chips, and any number of chips
  * Status (temperature, ...) PVs exists for up to four chips
  * txp3Cam optical detector (one chip)
* Changed PVs related to serval 3.0.0
  * Removed Detector chip Network Port, which does not exist in R3.0.0
  * ChipID is not part of the Detector Info Boards in R3.0.0
  * Humidity added
  * Detector orientation is part of Detector Layout
* Prepopulate BPC and DACS calibration paths
* Changed default port from 8080 to 8081 due to conflict with thinlinc
* Tested with Serval 3.1.1

R0-4 (August 10, 2022)
--------

* Serval 2.3.6 only.
* prepare for Serval 3.0.0

R0-3 (August 10, 2022)
----

* Serval 2.3.6 only.
* Removed restApi
* Removed Param
* Removed frozen
* Uses nlohmann json
* Uses cpr (Curl for humans)

R0-2 (August 3, 2022)
----

* Serval 2.3.6 only.
* frozen sources removed
* Enhancements

R0-1 (July 26, 2022)
--------

* First release.
* Serval 2.3.6 only
* Additional controls in Serval 4.1.x (Stem/Measurement.Config implemented in R1-6)
  * Server->Measurement->Stem (Scan, Virtual Detector) - PVs and MeasurementConfig.bob in R1-6