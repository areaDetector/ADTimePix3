# ADTimePix3

ADTimePix3 is an open-source EPICS areaDetector driver for TimePix3 pixel detectors from [Advanced Scientific Instruments (ASI)](https://www.amscins.com/). It is a **unified driver**: it provides both detector configuration and control via the Serval server and on-IOC data processing. The driver talks to Serval over an HTTP/JSON REST API and acquires data through high-rate TCP streams for raw events, images, and histograms. It supports real-time preview and **image accumulation** (running sum, sum of last N frames), **Time-of-Flight (ToF) histogram** processing and accumulation, pixel masking (Binary Pixel Configuration files, rectangular/circular masks, hot-pixel tools), health and status monitoring, and integration with standard areaDetector plugins for file saving and analysis. Support is included for real detectors (single-chip, 2×2 quad, and **eight-chip / dual-SPIDR** layouts with IOC and driver hooks described in [documentation/8chip-migration.md](documentation/8chip-migration.md)) and the TimePix3 emulator. The driver is developed for Linux 64-bit (tested on Ubuntu and RHEL) and is expected to evolve toward processing of TCP raw events as well. ADTimePix3 is published in [DOE CODE](https://www.osti.gov/doecode/biblio/176778) and can be cited from there.

Additional information:
* [Documentation](https://areadetector.github.io/areaDetector/ADTimePix3/ADTimePix3.html)
* [Release notes](RELEASE.md)
* **Eight-chip / dual SPIDR** (IOC `load_chips.cmd`, `MASK_BPC_NELEMENTS`, health, mask indexing): [documentation/8chip-migration.md](documentation/8chip-migration.md)
* **PixelConfig vs on-disk BPC** (SERVAL live config vs `.bpc` file, `PixelConfigDiff` / mask layout): [documentation/PIXELCONFIG_BPC_DIFF.md](documentation/PIXELCONFIG_BPC_DIFF.md)
* **Readout stack diagram** (48→64→SERVAL; **ADTimePix3** is a SERVAL-only client; **LUNA** is an optional parallel ASI path not used by this driver): [PNG](documentation/TimePix3_pipeline_48_64_96_caption.png), [SVG source](documentation/TimePix3_pipeline_48_64_96.svg) (regenerate PNG with Inkscape from the SVG if you edit the figure).

Notes:
------

* Depends on the [CPR](https://github.com/libcpr/cpr) version **1.14.2** (bundled under `tpx3Support`).
* Build baseline for bundled CPR is **C++17** (`-std=c++17` in `tpx3Support` and `tpx3App/src` Makefiles).
* Depends on the [json](https://github.com/nlohmann/json) version v3.11.2.
* Developed with ADCore R3-11 and ADSupport R1-10 or newer.
* **Preview Images**: Uses TCP streaming (jsonimage format) for preview images. GraphicsMagick HTTP method has been removed. For backward compatibility, the GraphicsMagick implementation is preserved in the `preserve/graphicsmagick-preview` branch.
* This has only been developed/tested on ubuntu 22.04, 20.04, 18.04, RHEL 7.9, RHEL 9.6 Linux 64-bit machines.
* Layout support in driver and OPI is most complete for **1 chip** and **2×2 quad**; **8-chip** (e.g. 2×4 mosaic, two SPIDR boards) has IOC/DB/driver support documented in [documentation/8chip-migration.md](documentation/8chip-migration.md)—validate BPC/mask mapping and screens on your hardware.
* **Serval versions**: The master branch supports both Serval 4.x.x and 3.x.x and is recommended (no need to use the 3.3.2 branch). The "dual image" issue was resolved in Serval 4.1.5; **Serval 4.1.5 is currently recommended** for 4.x. Serval 4.1.5-rc2 requires the same version of the TimePix3 Emulator (4.1.5-rc2). Data replay has been tested and is currently supported only with older Serval (3.3.2). Serval 2.x.y is in a separate branch and is not under active development.
* The driver has been developed using the TimePix3 Emulator and real detectors (quad-chip and single-chip).

Driver logging (asyn)
---------------------

Internal `ERR` / `WARN` / `LOG` / `FLOW` helpers in `tpx3App/src/ADTimePix.cpp` use **`ADTPX3_FUNC`** in the log prefix: on **GCC/Clang** it defaults to **`__PRETTY_FUNCTION__`** (signature-rich); on other compilers it uses **`__func__`**.

* **Shorter prefixes**: define **`ADTPX3_LOG_SHORT`** when building the driver library so prefixes use **`__func__` only** (see commented `USR_CPPFLAGS` line in `tpx3App/src/Makefile`).
* **`WARN` visibility**: by default **`WARN` / `WARN_ARGS`** use **`ASYN_TRACE_WARNING`**. The port’s asyn **trace mask** must include the **warning** bit for those lines to appear. If your site only enables **ERROR**-level trace and you need the old behavior, build with **`ADTPX3_WARN_AS_ERROR`** so `WARN*` is emitted at **`ASYN_TRACE_ERROR`** (commented example in the same `Makefile`).
* **Release detail**: see [RELEASE.md](RELEASE.md) (**R1-6-3** in development; **R1-6-2** tagged April 29, 2026).

TCP Image Streaming
--------------------

* **Preview Image Streaming (PrvImg)**: Preview images use TCP streaming with jsonimage format for real-time image delivery. Set `PrvImgFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8089`) and `PrvImgFileFmt` to `jsonimage` (format index 3).
* **Image Channel Streaming (Img)**: The Img channel also supports TCP jsonimage streaming for real-time 2D image delivery. Set `ImgFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8087`) and `ImgFileFmt` to `jsonimage` (format index 3).
* **Histogram Channel Streaming (PrvHst)**: The PrvHst channel supports TCP streaming with jsonhisto format for real-time 1D histogram delivery. Set `PrvHstFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8451`) and `PrvHstFileFmt` to `jsonhisto` (format index 4). With accumulation enabled, histogram data is pushed on **NDArray addresses 4–7** (sum-of-N, running sum, current frame, ToF bin centers in ms); see **Histogram Streaming** below. The driver processes histogram frames, accumulates running sums, and provides waveform PVs for plotting Time-of-Flight (ToF) histograms. The integration follows the same pattern as the standalone histogram IOC, providing unified histogram processing within the areaDetector framework.
* **Concurrent Operation**: PrvImg, Img, and PrvHst channels can stream concurrently without conflicts. Each logical stream uses its own NDArray address (PrvImg **0**, Img **1**, processed Img **2**/**3**, PrvHst **4**–**7** as documented below) to prevent conflicts.
* **Shutdown Behavior**: When stopping acquisition, the shutdown sequence properly handles TCP streaming channels. Serval measurement is stopped first, allowing Serval's TcpSender threads to stop sending data cleanly. Worker threads then detect "Connection closed by peer" as expected behavior. Connection closure messages are channel-specific ("PrvImg TCP connection closed by peer", "Img TCP connection closed by peer", and "PrvHst TCP connection closed by peer") to distinguish which channel is closing. This prevents "Broken pipe" errors even with rate mismatches (e.g., 60 Hz frame rate vs 5 Hz preview rate).
* **Debug Output**: Acquisition status debug messages show actual ADStatus values (0 = Idle, 1 = Acquire) with context including ADAcquire value, previous state, and ADStatus transitions for better troubleshooting.
* **GraphicsMagick**: The GraphicsMagick HTTP method for preview images has been removed from the master branch. For backward compatibility, the GraphicsMagick implementation is preserved in the `preserve/graphicsmagick-preview` branch.

Image Accumulation Features (Img Channel)
------------------------------------------

The Img channel (`TPX3-TEST:cam1:ImgFilePath`) supports advanced image accumulation and display features similar to the standalone `tpx3image` IOC:

* **Accumulation Enable Control (`ImgAccumulationEnable`)**: Controls whether the ADTimePix3 driver connects to the TCP port and performs accumulation processing. When enabled, the driver connects to the TCP port (e.g., port 8087) and processes images for accumulation. When disabled, the driver does not connect to the TCP port, allowing other clients to connect instead. This is useful when you want to write images to disk or have other clients connect to the image channel without the driver consuming the TCP connection. Note: `WriteImg` must still be enabled for Serval to configure the Img channel; `ImgAccumulationEnable` only controls whether the driver connects to the TCP stream.

* **Running Sum Accumulation**: Accumulates pixel values over all frames using 64-bit integers to prevent overflow. Access via `ImgImageData` PV (INT64 waveform array).
* **Current Frame Display**: Individual frame data available via `ImgImageFrame` PV (INT32 waveform array). This PV is **not** exposed as a separate NDArray address; file plugins that need one frame per callback should use **NDArrayAddress=1** (the Img stream from the TCP jsonimage path), which matches the same frame sequence as accumulation.
* **Sum of Last N Frames**: Calculates sum of the last N frames (configurable via `ImgFramesToSum` PV, default: 10). Access via `ImgImageSumNFrames` PV (INT64 waveform array). Update interval configurable via `ImgSumUpdateInterval` PV (default: 1 frame).
* **Performance Monitoring**: 
  - Acquisition rate: `ImgAcqRate_RBV` (Hz) - already available from TCP streaming metadata
  - Processing time: `ImgProcessingTime` (ms) - average processing time per frame
  - Memory usage: `ImgMemoryUsage` (MB) - estimated memory usage for accumulation buffers
* **Total Counts**: `ImgTotalCounts` (INT64) - total counts across all accumulated frames
* **Reset Control**: `ImgImageDataReset` (boolean output) - one-shot button to reset accumulated image data. Clears running sum, frame buffer, total counts, and processing time samples. The PV automatically resets to 0 after the reset action.
* **Phoebus Screen**: Use `ImgAccumulation.bob` screen (located in `Acquire/` folder) to visualize accumulated images, sum of N frames, and performance metrics. The screen includes controls for enabling/disabling accumulation, resetting accumulation, and configuring frame buffer parameters.

**Configuration**:
- Set `ImgFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8087`)
- Set `ImgFileFmt` to `jsonimage` (format index 3)
- Configure `ImgFramesToSum` (1-100000, default: 10) to control frame buffer size
- Configure `ImgSumUpdateInterval` (1-10000, default: 1) to control update frequency for sum of N frames

**File Saving**: Image data is available via NDArray callbacks for areaDetector file plugins (NDFileTIFF, NDFileHDF5, etc.). **NDArray addresses (Img-related):** 0 = PrvImg preview; **1** = each new Img frame (raw stream); **2** = running sum (same buffer as `ImgImageData`); **3** = sum of last N frames (same buffer as `ImgImageSumNFrames`, updated per `ImgSumUpdateInterval`). With **Img accumulation enabled**, the driver pushes NDArrays to addresses **2** every processed frame and to **3** whenever the sum-of-N buffer is updated (typically every frame if `ImgSumUpdateInterval` is 1). The waveform PVs `ImgImageData`, `ImgImageFrame`, and `ImgImageSumNFrames` are parallel views for displays and CA clients; only **1**, **2**, and **3** are NDArray streams for plugins. **`WriteProcessedImg`** (one-shot) triggers an **additional** push to addresses 2 and 3 built by `pushProcessedImgToPlugins()`: use **`ProcessedImgOutputType`** = Sum (0, NDInt64, good for HDF5) or Average (1, NDInt32, better for NDFileTIFF). **Note:** Changing a single plugin’s `NDArrayAddress` at runtime often has no effect (plugins usually register at init). To save different streams at once, use **separate** plugin instances (e.g. HDF5 on 1, 2, and 3) with each address set at startup. **iocsh:** keep each `dbLoadRecords("NDFileHDF5.template", "...")` on **one line**; a second line starting with the macro string is parsed as a new command and breaks macros (see `st_base.cmd` and ADCore `commonPlugins.cmd`).
See `documentation/PROCESSED_IMAGE_FILE_SAVING.md`, section **Runtime NDArrayAddress switching validation (single-plugin test)**, for reproducible HDF5/TIFF runtime-switch verification results and testing notes.

**NeXus-style HDF5 and NDFileNexus (two different XML formats)**  
ADCore uses **different** XML layouts for **`NDFileHDF5`** vs **`NDFileNexus`**; do not point both plugins at the same file.

- **`NDFileHDF5`** (`XMLFileName`): uses `<hdf5_layout>` with `<group>`, `<dataset source="detector">`, etc. Example in this IOC: `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_minimal.xml` (NeXus-like `NX_class` attributes on groups/datasets).
- **`NDFileNexus`** (`TemplateFilePath` + `TemplateFileName`): root must be `<NXroot>`; groups are elements such as `<NXentry name="entry">`, `<NXdata name="data">`; the image is `<data type="pArray">` (tag name = dataset name). Example: `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_plugin_template.xml`. Validate with `xmllint` and ADCore `XML_schema/template.sch` (see `ADCore/docs/ADCore/NDFileNexus.rst`). **Do not put `--` inside XML comments** (invalid XML; libxml will reject the template). Set `TemplateFilePath` with a **trailing slash**; path and file name are concatenated. Check `TemplateFileValid_RBV` before writing. The **`signal`** attribute in the template uses a small integer type; **pixel dtype** still follows the NDArray (e.g. addr **1** often `uint16`, addr **2**/`3` often `uint64` for processed sums).

Histogram Streaming (PrvHst)
------------------------------------------

The PrvHst channel (`TPX3-TEST:cam1:PrvHstFilePath`) supports real-time 1D histogram streaming and accumulation with Time-of-Flight (ToF) plotting capabilities. This feature integrates the jsonhisto TCP streaming functionality from the standalone histogram IOC into the ADTimePix3 driver, providing unified histogram processing within the areaDetector framework.

* **Accumulation Enable Control (`PrvHstAccumulationEnable`)**: Controls whether the ADTimePix3 driver connects to the TCP port and performs histogram accumulation processing. When enabled, the driver connects to the TCP port (e.g., port 8451) and processes histogram frames for accumulation. When disabled, the driver does not connect to the TCP port, allowing other clients to connect instead. This is useful when you want external clients to process jsonhisto data without the driver consuming the TCP connection. Note: `WritePrvHst` must still be enabled for Serval to configure the PrvHst channel; `PrvHstAccumulationEnable` only controls whether the driver connects to the TCP stream.

* **Running Sum Accumulation**: Accumulates histogram bin values over all frames using 64-bit integers to prevent overflow. Access via `PrvHstHistogramData` PV (INT64 waveform array). The running sum is continuously updated as new frames arrive.

* **Current Frame Display**: Individual frame histogram data available via `PrvHstHistogramFrame` PV (INT32 waveform array). Each frame shows the histogram bin values for the most recently received frame.

* **Sum of Last N Frames**: Calculates sum of the last N frames (configurable via `PrvHstFramesToSum` PV, default: 10). Access via `PrvHstHistogramSumNFrames` PV (INT64 waveform array). Update interval configurable via `PrvHstSumUpdateInterval` PV (default: 1 frame).

* **Time-of-Flight Axis**: Time axis in milliseconds for plotting histograms vs ToF. Access via `PrvHstHistogramTimeMs` PV (DOUBLE waveform array). Bin centers are calculated from bin edges (using `binWidth` and `binOffset` from jsonhisto metadata) and converted to milliseconds using the TimePix3 TDC clock period.

* **NDArray callbacks (file plugins)**: With **PrvHst accumulation** enabled, each processed histogram frame pushes **1D** NDArrays on multiple addresses: **4** = sum of last N frames (`PrvHstHistogramSumNFrames`, NDInt64) when that buffer updates; **5** = running sum (`PrvHstHistogramData`, NDInt64); **6** = current frame (`PrvHstHistogramFrame`, NDInt32); **7** = ToF bin centers in ms (same axis as `PrvHstHistogramTimeMs`, NDFloat64). Arrays **4** and **5** include NDAttributes `PrvHstTimeBin0Ms`, `PrvHstTimeBinStepMs`, and `PrvHstNumBins` for a uniform time axis. Use **separate** NDFileHDF5 (or TIFF) instances with **`NDArrayAddress` set at IOC startup** for each stream you want to save. **`WriteProcessedHst`** (one-shot) pushes **4**, **5**, **6**, and **7** again with type selected by **`ProcessedHstOutputType`**: **Sum** (NDInt64 counts) or **Average** (NDInt32, divide running sum by frame count; sum-of-N by buffer length) for TIFF-friendly ranges—mirrors **`WriteProcessedImg`** / **`ProcessedImgOutputType`** for images.

* **Metadata and Statistics**: The driver provides additional metadata PVs:
  - `PrvHstTimeAtFrame`: Timestamp at frame (nanoseconds from jsonhisto)
  - `PrvHstFrameBinSize`: Number of bins in current frame
  - `PrvHstFrameBinWidth`: Bin width parameter (TDC clock ticks)
  - `PrvHstFrameBinOffset`: Bin offset parameter (TDC clock ticks)
  - `PrvHstFrameCount`: Total number of frames processed
  - `PrvHstTotalCounts`: Total counts across all accumulated frames (INT64)
  - `PrvHstAcqRate`: Acquisition rate (Hz)
  - `PrvHstProcessingTime`: Average processing time per frame (ms)
  - `PrvHstMemoryUsage`: Estimated memory usage for accumulation buffers (MB)

* **Phoebus Screen**: Use `PrvHstHistogram.bob` screen (located in `Acquire/` folder) to visualize histogram data with three XY plots:
  - **Accumulated Histogram**: Running sum vs ToF [ms]
  - **Current Frame**: Individual frame histogram vs ToF [ms]
  - **Sum of Last N Frames**: Sum of last N frames vs ToF [ms]
  The screen includes controls for enabling/disabling accumulation and histogram configuration parameters.

**Configuration**:
- Set `PrvHstFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8451`)
- Set `PrvHstFileFmt` to `jsonhisto` (format index 4)
- Set `PrvHstFileMode` to `tof` (format index 3) for Time-of-Flight histograms
- Configure `PrvHstNumBins`, `PrvHstBinWidth`, and `PrvHstOffset` for histogram binning parameters
- Set `PrvHstAccumulationEnable` to `Enable` (1) to enable histogram accumulation processing
- Configure `PrvHstFramesToSum` (1-100000, default: 10) to control frame buffer size
- Configure `PrvHstSumUpdateInterval` (1-10000, default: 1) to control update frequency for sum of N frames

**File Saving**: Histogram data is available via NDArray callbacks on **addresses 4–7** (see **NDArray callbacks** under Histogram Streaming) for areaDetector file plugins. The same data is mirrored on waveform PVs (`PrvHstHistogramData`, `PrvHstHistogramFrame`, `PrvHstHistogramSumNFrames`, `PrvHstHistogramTimeMs`) for displays and CA clients. Use **`WriteProcessedHst`** / **`ProcessedHstOutputType`** for an on-demand typed push (Sum vs Average), similar to processed images. For HDF5 layout examples, see `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_minimal.xml` and `iocs/tpx3IOC/iocBoot/iocTimePix/nexus_prvhst_histogram.xml` (counts + uniform ToF-axis metadata via NDAttributes).

**Callback control PV name**: The EPICS PV is `$(P)$(R)ArrayCallbacks` (and `_RBV`), while `NDArrayCallbacks` is the internal driver/asyn parameter name used in code and logs.

**Integration Notes**:
- The jsonhisto integration follows the same pattern as the standalone histogram IOC (`/epics/iocs/histogram/tpx3HistogramApp/src/tpx3HistogramDriver.cpp`)
- Histogram data is processed in a dedicated worker thread that handles TCP connection, JSON parsing, binary data reading, and frame accumulation
- The driver uses `doCallbacksFloat64Array`, `doCallbacksInt64Array`, and `doCallbacksInt32Array` to push data directly to waveform PVs (no `readFloat64Array` implementation, matching the histogram IOC)
- Thread synchronization uses `epicsMutex` to protect shared data structures
- The worker thread automatically clears its thread ID before exiting to allow clean shutdown

**Note on asyn and NDArray addresses**: The driver is constructed with **`maxAddr=8`** (valid asyn **address lists 0–7**): **0** = PrvImg, **1** = Img frame, **2** = Img running sum, **3** = Img sum-of-N, **4** = PrvHst sum-of-N, **5** = PrvHst running sum, **6** = PrvHst current frame, **7** = PrvHst ToF axis (ms). Earlier releases used `maxAddr=6` for PrvHst on address 5 only; the extra lists support processed histogram file saving. The driver preserves shared size parameters (`SizeX_RBV`, `SizeY_RBV`) for image channels when pushing histogram NDArrays. See the Troubleshooting section for historical context on "parameter … in list 5".

CONNECT/DISCONNECT (reconnection without IOC restart)
-----------------------------------------------------

The driver monitors SERVAL and detector connection status and can recover when SERVAL or the detector reconnects without restarting the IOC.

* **ServalConnected_RBV / DetConnected_RBV**: Read-only status PVs (in `Dashboard.template`) indicate whether SERVAL is reachable and whether a detector is reported by SERVAL. They are updated by the initial connection check at startup, by the periodic connection poll, and when `RefreshConnection` or Health is triggered.
* **RefreshConnection**: Boolean output PV. Writing 1 runs a lightweight connection check and updates `ServalConnected_RBV`, `DetConnected_RBV`, and `ADStatusMessage`. Use this to refresh connection status on demand without running the full Health (getDashboard/getDetector) sequence.
* **Periodic connection poll**: A background thread (default period 5 s) runs a lightweight connection check. On transition from disconnected to connected, the driver calls `fileWriter()` (push channel config), `initAcquisition()` (push detector config: TriggerMode, NumImages, etc., from PVs to SERVAL so they stay e.g. CONTINUOUS and desired NumImages after restart), then `getServer()` (refresh PVs from SERVAL); it does not call `initCamera()` or stop acquisition. On disconnect, status PVs and `ADStatusMessage` are updated; acquisition is not automatically stopped.
* **Detector initialization**: Customization of detector initialization (e.g. file paths, BPC/DACS, WriteData) is done at startup (e.g. via `init_detector.cmd` or `st_base.cmd`). On reconnect, the current PV config is re-pushed to SERVAL via `fileWriter()` then refreshed via `getServer()`; full initialization (BPC/DACS load) is not re-run.

Detector initialization (single source of truth)
------------------------------------------------

Detector initialization uses **EPICS PV values** as the single source of truth: the init script sets PVs, and the driver applies them to SERVAL when `WriteData=1` or when `ApplyConfig` is triggered.

* **init_detector.cmd**: The detector init block (channel paths, formats, modes, BPC/DACS paths, WriteData, ImageMode, TriggerMode, etc.) lives in `iocs/tpx3IOC/iocBoot/iocTimePix/init_detector.cmd`. `st_base.cmd` sources it after `iocInit()`. You can re-run the same init from iocsh after a reconnect: `< init_detector.cmd`.
* **Chip records and mask DB size:** Per-chip PVs are loaded via `load_chips.cmd` (eight instances of `Chips.template`, `CHIP0`…`CHIP7`). **`MASK_BPC_NELEMENTS`** in `unique.cmd` must be at least the detector **PixCount** (65536 / 262144 / 524288 for typical 1 / 4 / 8 chip sizes) or `MaskBPC.template` fails to load and mask PVs stay disconnected. See the table in `unique.cmd` and [documentation/8chip-migration.md](documentation/8chip-migration.md).
* **ApplyConfig**: Boolean output PV. Writing 1 runs `fileWriter()` + `getServer()` (same effect as `WriteData=1`), so you can re-apply the current PV config to SERVAL without toggling WriteData or re-running a script. Use after reconnect or after changing PVs from the OPI.
* **Driver**: The driver does not use a separate config file; it always builds config from current PVs in `fileWriter()` and sends it to SERVAL via `sendConfiguration()`. So both the init script and the driver use the same source of truth: the EPICS PVs.

How to run:
-----------

* Under `ADTimePix3/iocs/tpx3IOC/iocBoot/iocTimePix` there is already a ready to use IOC for the TimePix3
  - run serval
  - Change the IP address in `st.cmd` or `st_base.cmd`.
  - Run `./st.cmd`.
* There are CSS-Boy and Phoebus screens under `ADTimePix3/tpx3App/op/`
  - **Emulator** (`tpx3emulator.bob`): TPX3 Emulator IOC control panel; supports replay of previously collected raw .tpx3 data (Replay File, Replay Mode, file list, etc.). Opened from Detector Config tab.
  - **Measurement Config** (`Measurement/MeasurementConfig.bob`): SERVAL 4.1.x Measurement.Config — 4D-STEM (Stem scan, virtual detector) and Time-of-Flight (TdcReference, Min, Max). Embedded in TimePix3Detector below Measurement Info.
  - Phoebus .bob screens have been converted/created; layout may be adjusted manually for best fit.

Adjust chip thresholds
----------------

* Optimize/equalize chips in the 'count' mode (not ToT)
* Threshold fine increase is closer to noise (you get more counts)
* Threshold fine decrease is away from noise, higher threshold in keV. (you get less counts)
* These threshold settings can depend on X-ray energy.
* After changing the thresholds take a background image to check that you do not get extra noise pixels.

areaDetector 
------------

Uncomment following lines in ADCore/iocBoot

* ADCore/iocBoot/commonPlugins.cmd
  * Magick file saving plugin (optional, for file saving only - preview images use TCP streaming)
    * NDFileMagickConfigure("FileMagick1", $(QSIZE), 0, "$(PORT)", 0)
    * dbLoadRecords("NDFileMagick.template","P=$(PREFIX),R=Magick1:,PORT=FileMagick1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
  * load NDPluginPva plugin
    * NDPvaConfigure("PVA1", $(QSIZE), 0, "$(PORT)", 0, $(PREFIX)Pva1:Image, 0, 0, 0)
    * dbLoadRecords("NDPva.template",  "P=$(PREFIX),R=Pva1:, PORT=PVA1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
    * startPVAServer
  * load sseq record for acquisition sequence
    * dbLoadRecords("$(CALC)/calcApp/Db/sseqRecord.db", "P=$(PREFIX), S=AcquireSequence")
    * set_requestfile_path("$(CALC)/calcApp/Db")
  * load devIocStats records
    * dbLoadRecords("$(DEVIOCSTATS)/db/iocAdminSoft.db", "IOC=$(PREFIX)")

* ADCore/iocBoot/commonPlugin_settings.req
  * file "NDFileMagick_settings.req",   P=$(P),  R=Magick1:
  * file "NDPva_settings.req",          P=$(P),  R=Pva1:

  
EPICS ADTimePix3 Driver Analysis
--------------------------------

### Overview

The ADTimePix3 is an EPICS areaDetector driver for TimePix3 detectors from Advanced Scientific Instruments (ASI). It provides a complete interface between EPICS and TimePix3 detectors through the Serval server software.

### Key Components

#### 1\. Driver Architecture

-   Main Class: ADTimePix inherits from ADDriver (areaDetector base class)
-   Communication: HTTP REST API using the cpr (C++ Requests) library
-   Data Format: JSON using nlohmann/json library
-   Preview Images: TCP streaming (jsonimage format) for real-time preview images
-   Image Processing: TCP streaming replaces GraphicsMagick HTTP method (GraphicsMagick preserved in `preserve/graphicsmagick-preview` branch)

#### 2. Core Functionality

Detector Communication:

-   Connects to Serval server via HTTP REST API (default: http://localhost:8081)
-   Supports multiple Serval versions (3.0.0-4.x.x) with version-specific features
-   Handles both real detectors and emulator mode

Acquisition Modes:

-   Raw Mode: .tpx3 file output or TCP streaming
-   Image Mode: Processed images (TIFF, PNG, PGM formats) or TCP jsonimage streaming
-   Preview Mode: Real-time preview images and histograms via TCP jsonimage streaming
-   Multiple Streams: Support for dual raw streams (Serval 3.3.0+)
-   Concurrent Channels: PrvImg and Img channels can stream simultaneously via TCP without conflicts

Trigger Modes:

-   External trigger (PEX/NEX start/stop)
-   Timer-based acquisition
-   Continuous mode
-   Software trigger

#### 3\. Key Features

Multi-Chip Support:

-   Supports 1-4 chip configurations (single chip to 2x2 quad)
-   Individual chip control via asyn multi-device mechanism
-   Chip-specific DAC settings and temperature monitoring

Pixel Masking:

Masks apply in the IOC on image-domain data from the same **SERVAL** TCP/HTTP path as **Img** / **PrvImg** (see **readout stack diagram** under *Additional information*).

-   Binary Pixel Configuration (BPC) file support
-   Real-time mask generation (rectangular, circular)
-   Positive/negative masking modes
-   Hot pixel detection and correction
-   **SERVAL vs file check**: `RefreshPixelConfig` compares per-chip PixelConfig from SERVAL to the on-disk BPC; **`PixelConfigDiff`** shows |Δ| in **image** order (same mapping as **`MaskBPC`**). See [documentation/PIXELCONFIG_BPC_DIFF.md](documentation/PIXELCONFIG_BPC_DIFF.md).

Detector Health Monitoring:

-   Temperature monitoring (local, FPGA, individual chips)
-   Fan speed control
-   Bias voltage monitoring
-   Humidity and environmental sensors
-   VDD/AVDD power monitoring

Data Output:

-   Multiple file formats (TIFF, PNG, PGM, JSON)
-   TCP streaming capabilities
-   Configurable file naming patterns
-   Disk space monitoring and limits

#### 4\. Process Variables (PVs)

The driver exposes extensive PVs organized into categories:

Server/Connection:

-   TPX3_SERVAL_CONNECTED: Serval server connection status
-   TPX3_DETECTOR_CONNECTED: Detector connection status
-   TPX3_HTTP_CODE: HTTP response codes

Detector Health:

-   TPX3_LOCAL_TEMP, TPX3_FPGA_TEMP: Temperature readings
-   TPX3_FAN1_SPEED, TPX3_FAN2_SPEED: Fan speeds
-   TPX3_BIAS_VOLT_H: Bias voltage
-   TPX3_CHIP_TEMPS: Individual chip temperatures

Acquisition Control:

-   TPX3_EXPOSURE_TIME, TPX3_TRIGGER_PERIOD: Timing parameters
-   TPX3_TRIGGER_MODE: Trigger configuration
-   TPX3_PEL_RATE, TPX3_TDC1_RATE: Event rates

File Output:

-   TPX3_RAW_STREAM: Raw data streaming control
-   TPX3_IMG_FORMAT, TPX3_IMG_MODE: Image format settings
-   TPX3_PRV_PERIOD: Preview update rate

Masking:

-   TPX3_MASK_ARRAY_BPC: BPC mask array
-   TPX3_MASK_RECTANGLE, TPX3_MASK_CIRCLE: Mask generation
-   TPX3_MASK_RESET: Mask reset control

#### 5. Database Templates

The driver uses multiple EPICS database templates:

-   TimePix3Base.template: Basic areaDetector records
-   ADTimePix3.template: Main driver PVs
-   Chips.template: Multi-chip support
-   Server.template: Server configuration
-   File.template: File output settings
-   Measurement.template: Acquisition parameters
-   MaskBPC.template: Pixel masking
-   Dashboard.template: Status monitoring

#### 6. User Interface

OPI Screens:

-   TimePix3.opi: Main control interface
-   TimePix3Detector.opi: Detector configuration
-   TimePix3Status.opi: Status monitoring
-   TimePix3Alarm.opi: Alarm management
-   TimePix3API.opi: API information

#### 7\. Dependencies

External Libraries:

-   CPR (v1.9.1): HTTP client library
-   nlohmann/json (v3.11.2): JSON parsing
-   GraphicsMagick: Image processing
-   ADCore (R3-11+): areaDetector core
-   ADSupport (R1-10+): areaDetector support

#### 8. Supported Platforms

-   Ubuntu 22.04, 20.04, 18.04
-   RHEL 7.9, RHEL 9.6
-   Linux 64-bit only

#### 9. Key Implementation Details

* HTTP Communication:

  *  Example REST API call

```
cpr::Response r = cpr::Get(cpr::Url{serverURL + "/dashboard"},
                          cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
```

* Multi-Chip Support:

* Uses asyn multi-device mechanism

```
ADDriver(portName, 4, NUM_TIMEPIX_PARAMS, maxBuffers, maxMemory,
         asynInt64Mask | asynEnumMask,
         asynInt64Mask | asynEnumMask,
         ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, priority, stackSize)
```

* Image Acquisition:
  -   Asynchronous callback thread for continuous acquisition
  -   Real-time measurement status updates
  -   Support for multiple data formats and streaming
  -   TCP streaming for preview images (PrvImg) and image channel (Img) using jsonimage format with configurable ports
  -   Concurrent TCP streaming support: PrvImg and Img channels can operate simultaneously using separate array slots

#### 10. Recent Developments

Version R1-3 (Latest):

-   Serval 4.x.x compatibility improvements
-   Enhanced TDC1/TDC2 reporting
-   Additional detector health monitoring
-   Improved rotation/mirror operations

Version R1-2:

-   Advanced alarm system
-   Enhanced mask generation for all 8 detector orientations
-   Individual chip monitoring
-   Connection status improvements

### Usage

Basic Setup:

1.  Start Serval server
1.  Configure IP address in st.cmd
1.  Run IOC: ./st.cmd
1.  Use CSS-Boy or Phoebus screens for control

Configuration:

-   Set file paths for raw data, images, and preview
-   Configure trigger modes and timing
-   Set up pixel masking if needed
-   Adjust chip thresholds for optimal performance

The ADTimePix3 driver provides a comprehensive, production-ready interface for TimePix3 detectors in EPICS environments, with extensive monitoring, control, and data acquisition capabilities.

Troubleshooting
---------------

**SERVAL HTTP errors and IOC stability (R1-6-2+)**: If the detector is not connected or SERVAL returns an error status (e.g. HTTP **409**), the response body may be **plain text** (not JSON). Older builds could crash the IOC with a **`json::parse_error`** / **`SIGABRT`** after a line like **`Request failed with status code: 409`**. Current code treats bad status or non-JSON bodies as driver errors and **does not abort** the IOC; check **`ADStatusMessage`**, **`TPX3_HTTP_CODE`**, SERVAL logs, and wiring. Normal acquisition still requires a healthy detector and SERVAL.

**Known Warnings (Harmless)**:

* **`asynPortDriver:getParamStatus: port=TPX3 error setting parameter 51 in list 5, invalid list`** (or parameter 52, 50, etc.): **Fixed in R1-5.** The message means asyn was asked for parameter 51 (ARRAY_DATA) at "list 5" (address 5) on port TPX3, but address 5 did not exist. The driver was built with `maxAddr=4` (valid addresses 0–3), while the histogram channel uses NDArray address 5. In R1-5 the driver was changed to use `maxAddr=6`, so address 5 is valid and this warning no longer appears when the histogram channel is enabled. **Root cause (from asynReport):** "Parameter 51 is undefined, name=ARRAY_DATA" in "list 5" referred to the *address* (list 5 = address 5) being invalid on the TPX3 port, not to parameter 51 itself. **Why did the parameter number vary (51 vs 52)?** Different builds (e.g. Ubuntu 22.04 vs Red Hat 9) can have different ADCore/asyn parameter order, so the reported parameter index can differ; the underlying issue was always the invalid address. **No action required** if you are on R1-5 or later.

**Suppressing these messages in log files:**

Since these warnings can fill log files, here are several methods to suppress them. **Important**: Do NOT filter stderr/stdout at IOC startup as this breaks the interactive EPICS prompt and procServer integration.

**Recommended methods (post-processing):**

1. **Filter existing log files** (simplest, works with any setup):
   ```bash
   # Filter a log file
   grep -vE "(parameter [0-9]+ in list 5|getParamStatus.*list 5|getParamAlarmStatus.*list 5|getParamAlarmSeverity.*list 5)" ioc.log > ioc_filtered.log
   
   # Or filter in place (backup first!)
   cp ioc.log ioc.log.backup
   grep -vE "(parameter [0-9]+ in list 5|getParamStatus.*list 5|getParamAlarmStatus.*list 5|getParamAlarmSeverity.*list 5)" ioc.log.backup > ioc.log
   ```

2. **Use logrotate with post-rotate filtering** (for automated log management):
   Create a `/etc/logrotate.d/epics-ioc` configuration:
   ```
   /path/to/ioc.log {
       daily
       rotate 7
       compress
       delaycompress
       missingok
       notifempty
       postrotate
           # Filter warnings after rotation (if log file exists)
           if [ -f /path/to/ioc.log.1 ]; then
               grep -vE "(parameter [0-9]+ in list 5|getParamStatus.*list 5|getParamAlarmStatus.*list 5|getParamAlarmSeverity.*list 5)" /path/to/ioc.log.1 > /path/to/ioc.log.1.filtered
               mv /path/to/ioc.log.1.filtered /path/to/ioc.log.1
           fi
       endscript
   }
   ```

3. **For procServer users** (recommended approaches):
   - **Filter in log aggregation system**: If using ELK, Splunk, Grafana Loki, etc., add filters to exclude these patterns
   - **Use procServer log rotation with post-processing**: Configure procServer to run a post-rotation script that filters the rotated log
   - **Filter in log viewer**: Configure your log viewing tool (e.g., `less`, `tail -f`, log analysis tools) to filter these patterns
   - **Periodic log cleanup script**: Run a cron job that periodically filters the active log file:
     ```bash
     # Add to crontab (runs every hour)
     0 * * * * /path/to/filter_ioc_logs.sh
     ```
     Where `filter_ioc_logs.sh` contains:
     ```bash
     #!/bin/bash
     LOGFILE="/path/to/ioc.log"
     if [ -f "$LOGFILE" ]; then
         # Create filtered version, then replace (atomic operation)
         grep -vE "(parameter [0-9]+ in list 5|getParamStatus.*list 5|getParamAlarmStatus.*list 5|getParamAlarmSeverity.*list 5)" "$LOGFILE" > "${LOGFILE}.filtered"
         mv "${LOGFILE}.filtered" "$LOGFILE"
     fi
     ```

4. **Use systemd/journald filtering** (if using systemd):
   ```bash
   # View logs without these warnings
   journalctl -u ioc-service | grep -vE "(parameter [0-9]+ in list 5|getParamStatus.*list 5|getParamAlarmStatus.*list 5|getParamAlarmSeverity.*list 5)"
   
   # Or configure journald to filter (requires journald configuration)
   ```

5. **Use rsyslog filtering** (if using rsyslog):
   Add to `/etc/rsyslog.d/epics-ioc.conf`:
   ```
   :msg, contains, "parameter" & msg, contains, "in list 5" ~
   :msg, contains, "getParamStatus" & msg, contains, "list 5" ~
   :msg, contains, "getParamAlarmStatus" & msg, contains, "list 5" ~
   :msg, contains, "getParamAlarmSeverity" & msg, contains, "list 5" ~
   ```

**NOT recommended:**
- Filtering stdout/stderr with pipes at startup (breaks interactive prompt and procServer)
- Using wrapper scripts that pipe all output (prevents procServer from controlling the IOC)
- Modifying IOC startup scripts to filter output (breaks procServer integration)

**Note**: These warnings are printed by asyn library code, not the ADTimePix3 driver, so they cannot be suppressed at the driver level. Post-processing log files or using log rotation/monitoring tools with filtering is the recommended approach that works with interactive prompts and procServer.

**Fixed Issues (R1-5)**:

* **`DataType_RBV` showing "Illegal Value" when histogram channel is enabled**: Fixed in R1-5. The histogram channel was setting `NDDataType` to `NDInt64` (value 8), but the `DataType_RBV` database record enum only supports values 0-5, causing "Illegal Value" status. The driver now preserves the previous `NDDataType` value and does not set it for histogram data. The NDArray itself still correctly uses `NDInt64` data type internally. **No action required** - this has been resolved in R1-5.

* **`SizeX_RBV` and `SizeY_RBV` showing incorrect values when histogram channel is enabled**: Fixed in R1-5. The histogram channel was setting shared size parameters (`ADSizeX`, `NDArraySizeX`, `ADSizeY`, `NDArraySizeY`) that are also used by image channels, causing histogram bin sizes (e.g., 16000) to overwrite image dimensions (e.g., 512). The driver now preserves these shared parameters and does not set them for histogram data. Since histogram uses NDArray address 5 (separate from image addresses 0 and 1), the NDArray itself contains all necessary size information in its attributes, which plugins can access directly. **No action required** - this has been resolved in R1-5.

* **`NumImages` showing INVALID status**: Fixed in R1-5. The `NumImages` PV was showing INVALID status with value 100000000 (100 million) at IOC startup. The ADDriver base class was initializing `NumImages` to a very large default value, causing INVALID status. The driver now initializes `NumImages` to 0 (unlimited) in the constructor, which is appropriate for continuous mode and prevents the INVALID status. For single mode (ImageMode = 0), the driver automatically sets `NumImages = 1` when switching to single mode. In single mode, writes of values other than 1 (e.g. from database initialisation) are now accepted and clamped to 1, avoiding WRITE INVALID and the "Cannot set numImages in single mode" error. **No action required** - this has been resolved in R1-5. If you see this issue before recompiling, you can manually set `NumImages` to 0: `caput TPX3-TEST:cam1:NumImages 0`.

* **`ArrayCounter_RBV` showing double count when histogram channel enabled**: Fixed in R1-5. When jsonhisto streaming was enabled, `ArrayCounter_RBV` was approximately twice `NumImagesCounter_RBV` (e.g., 46 vs 22). The histogram channel was calling `doCallbacksGenericPointer()`, which automatically increments the shared `NDArrayCounter` parameter. Since histogram uses NDArray address 5 (separate from image addresses 0 and 1) and should not affect the image channel counter, the driver now saves and restores `NDArrayCounter` around histogram callbacks to prevent histogram from incrementing it. `ArrayCounter_RBV` now correctly reflects only image frame counts, matching `NumImagesCounter_RBV` when histogram is enabled. **No action required** - this has been resolved in R1-5.

* **`asynPortDriver:getParamStatus: port=TPX3 error setting parameter 51 in list 5, invalid list`**: Fixed in R1-5. The driver was created with `maxAddr=4` (addresses 0–3 only), but the histogram channel uses NDArray address 5. When asyn or plugins accessed parameter 51 (ARRAY_DATA) at address 5, asyn reported "invalid list" because that address did not exist. The driver now uses **`maxAddr=8`** (R1-7; was `maxAddr=6` from R1-5 through R1-6) so lists **0–7** are valid, including PrvHst **4–7**. The warning no longer appears when the histogram channel is enabled. **No action required** after recompile and IOC restart.

* **Histogram appears connected but does not accumulate (timing reference required)**: If `PrvHst` frame metadata updates but histogram bins stay near zero, verify the ToF timing reference path. For the TimePix3 emulator, start Java with **`-Dtdc=0`** (TDC selection), which was required in local tests for `jsonhisto` accumulation to populate correctly. For physical detectors, ensure **TDC1 and/or TDC2** receive valid time reference pulses; without those reference pulses, ToF bins can remain near zero even though streaming and metadata PVs update.

**SIGSEGV on IOC exit (after acquisition)**:

If the IOC segfaults when you type `exit` after running acquisition (e.g. "Segmentation fault" in `NDArrayPool::release()` or similar), the cause is pvAccess (PVA) still holding NDArrays after the driver and its NDArray pool have been destroyed. Two complementary approaches exist:

1. **ADTimePix3: rely on asyn for teardown (no manual exit handler)**  
   The driver does **not** register `epicsAtExit`; teardown on IOC exit is performed by asyn when the driver is created with `ASYN_DESTRUCTIBLE` (as recommended in [ADCore PR 570](https://github.com/areaDetector/ADCore/pull/570#issuecomment-4005838864) and [PR 572](https://github.com/areaDetector/ADCore/pull/572)).
   - Build ADTimePix3 with **asyn R4-45 or later** and an **ADCore** that supports destructible drivers (see [ADCore PR 572](https://github.com/areaDetector/ADCore/pull/572) and [destructible.rst](https://github.com/areaDetector/ADCore/blob/master/docs/ADCore/destructible.rst)).
   - **`ADTimePixConfig`** (6 arguments): when built with `ASYN_DESTRUCTIBLE` defined, it passes that flag to the driver so asyn performs shutdown (`shutdownPortDriver()` then delete). Use this in your IOC startup; no 7th argument needed.
   - **`ADTimePixConfigWithFlags`** (7 arguments): use when you need to pass other asyn flags or override the default (e.g. pass 0 for asynFlags to avoid destructible behavior if needed).

2. **ADCore: safe release when pool is destroyed**  
   [ADCore PR 570](https://github.com/areaDetector/ADCore/pull/570) makes `NDArray::release()` safe when the pool has already been destroyed (e.g. PVA still holds arrays after the driver is gone). Using ADCore with that fix (or equivalent) is recommended as a **safety net** regardless of shutdown order.

**Summary**: Use **asyn R4-45+** and **ADCore with destructible support (PR 572)**; the driver is created destructible by default and asyn performs teardown. Still recommend **ADCore PR 570** (or equivalent) so that any late `NDArray::release()` calls do not dereference a destroyed pool.

**Known Issues**:

* **RESNIC Pattern Distortion in Phoebus**: Some images displayed in Phoebus show distorted RESNIC patterns. The expected checkerboard pattern sometimes appears as a different structure (vertical stripes or different geometric patterns). This is an intermittent issue that affects only some frames, not all. The correct checkerboard pattern is visible in some images, while others show distortion. **This may be a SERVAL bug specific to TCP streaming channels when histogram (jsonhisto) is enabled.** The issue may be related to:
  - SERVAL TCP streaming implementation when multiple channels (image + histogram) are active simultaneously
  - Data corruption or frame synchronization issues in SERVAL's TCP sender threads when histogram channel competes with image channel
  - Network bandwidth allocation or timing issues in SERVAL when both channels stream concurrently
  - Emulator-specific behavior (if using TimePix3 emulator)

  The byte swapping code in the driver appears correct (using `__builtin_bswap16`/`__builtin_bswap32` for network byte order conversion). If you encounter this issue, please report it with:
  - Which channel (PrvImg or Img) shows distortion
  - Frame numbers affected
  - Whether it occurs consistently or intermittently
  - Whether it only occurs when histogram channel is enabled
  - Whether you're using emulator or physical detector
  - SERVAL version and configuration
