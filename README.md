# ADTimePix3

An EPICS areaDetector driver for TimePix3 detector from [ASC](https://www.amscins.com/).

Additional information:
* [Documentation](https://areadetector.github.io/areaDetector/ADTimePix3/ADTimePix3.html)
* [Release notes](RELEASE.md)

Notes:
------

* Depends on the [CPR](https://github.com/libcpr/cpr) verison 1.9.1.
* Depends on the [json](https://github.com/nlohmann/json) version v3.11.2.
* Developed with ADCore R3-11 and ADSupport R1-10 or newer.
* **Preview Images**: Uses TCP streaming (jsonimage format) for preview images. GraphicsMagick HTTP method has been removed. For backward compatibility, the GraphicsMagick implementation is preserved in the `preserve/graphicsmagick-preview` branch.
* This has only been developed/tested on ubuntu 22.04, 20.04, 18.04, RHEL 7.9, RHEL 9.6 Linux 64-bit machines.
* This has only been developed for 2 x 2 chips layout and 1 chip tpx3CAM, since that is what I have access to now.
* This has been tested with serval version 4.1.1, 4.1.0, 3.3.2, 3.2.0, 3.1.0 and 3.0.0 extensively. Only most recent serval version(s) are tested extensively. However, the master branch is compatible with serval 4.x.x, and attempts are being made to make it compatible with Serval 3.x.x. In the meantime, please use the 3.3.2 branch for serval 3.x.x. The Serval 4.x.x has additional features, which are not yet supported.
* Driver is specific to Serval version, since Rust features differ. Driver for Serval 2.x.y is in separate branch, and is not under current development. The branch 3.3.2 is compatible with serval 3.x.x only, and will likely not be developed further.
* The driver has been developed using TimePix3 Emulator, and real detectors. Real detectors are quad-chip, and single chip.

TCP Image Streaming
--------------------

* **Preview Image Streaming (PrvImg)**: Preview images use TCP streaming with jsonimage format for real-time image delivery. Set `PrvImgFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8089`) and `PrvImgFileFmt` to `jsonimage` (format index 3).
* **Image Channel Streaming (Img)**: The Img channel also supports TCP jsonimage streaming for real-time 2D image delivery. Set `ImgFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8087`) and `ImgFileFmt` to `jsonimage` (format index 3).
* **Histogram Channel Streaming (PrvHst)**: The PrvHst channel supports TCP streaming with jsonhisto format for real-time 1D histogram delivery. Set `PrvHstFilePath` to `tcp://listen@hostname:port` (e.g., `tcp://listen@localhost:8451`) and `PrvHstFileFmt` to `jsonhisto` (format index 4). Histogram data is available via NDArray callbacks (NDArrayAddress=5) for use with areaDetector plugins. The driver processes histogram frames, accumulates running sums, and provides waveform PVs for plotting Time-of-Flight (ToF) histograms. The integration follows the same pattern as the standalone histogram IOC, providing unified histogram processing within the areaDetector framework.
* **Concurrent Operation**: PrvImg, Img, and PrvHst channels can stream concurrently without conflicts. Each channel uses its own NDArray address (PrvImg uses address 0, Img uses address 1, PrvHst uses address 5) to prevent conflicts.
* **Shutdown Behavior**: When stopping acquisition, the shutdown sequence properly handles TCP streaming channels. Serval measurement is stopped first, allowing Serval's TcpSender threads to stop sending data cleanly. Worker threads then detect "Connection closed by peer" as expected behavior. Connection closure messages are channel-specific ("PrvImg TCP connection closed by peer", "Img TCP connection closed by peer", and "PrvHst TCP connection closed by peer") to distinguish which channel is closing. This prevents "Broken pipe" errors even with rate mismatches (e.g., 60 Hz frame rate vs 5 Hz preview rate).
* **Debug Output**: Acquisition status debug messages show actual ADStatus values (0 = Idle, 1 = Acquire) with context including ADAcquire value, previous state, and ADStatus transitions for better troubleshooting.
* **GraphicsMagick**: The GraphicsMagick HTTP method for preview images has been removed from the master branch. For backward compatibility, the GraphicsMagick implementation is preserved in the `preserve/graphicsmagick-preview` branch.

Image Accumulation Features (Img Channel)
------------------------------------------

The Img channel (`TPX3-TEST:cam1:ImgFilePath`) supports advanced image accumulation and display features similar to the standalone `tpx3image` IOC:

* **Accumulation Enable Control (`ImgAccumulationEnable`)**: Controls whether the ADTimePix3 driver connects to the TCP port and performs accumulation processing. When enabled, the driver connects to the TCP port (e.g., port 8087) and processes images for accumulation. When disabled, the driver does not connect to the TCP port, allowing other clients to connect instead. This is useful when you want to write images to disk or have other clients connect to the image channel without the driver consuming the TCP connection. Note: `WriteImg` must still be enabled for Serval to configure the Img channel; `ImgAccumulationEnable` only controls whether the driver connects to the TCP stream.

* **Running Sum Accumulation**: Accumulates pixel values over all frames using 64-bit integers to prevent overflow. Access via `ImgImageData` PV (INT64 waveform array).
* **Current Frame Display**: Individual frame data available via `ImgImageFrame` PV (INT32 waveform array).
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

**File Saving**: Image data is available via NDArray callbacks for use with areaDetector file plugins (NDFileTIFF, NDFileHDF5, etc.). The accumulated image data arrays (`ImgImageData`, `ImgImageFrame`, `ImgImageSumNFrames`) are also available as EPICS waveform arrays for direct access.

Histogram Streaming (PrvHst)
------------------------------------------

The PrvHst channel (`TPX3-TEST:cam1:PrvHstFilePath`) supports real-time 1D histogram streaming and accumulation with Time-of-Flight (ToF) plotting capabilities. This feature integrates the jsonhisto TCP streaming functionality from the standalone histogram IOC into the ADTimePix3 driver, providing unified histogram processing within the areaDetector framework.

* **Accumulation Enable Control (`PrvHstAccumulationEnable`)**: Controls whether the ADTimePix3 driver connects to the TCP port and performs histogram accumulation processing. When enabled, the driver connects to the TCP port (e.g., port 8451) and processes histogram frames for accumulation. When disabled, the driver does not connect to the TCP port, allowing other clients to connect instead. This is useful when you want external clients to process jsonhisto data without the driver consuming the TCP connection. Note: `WritePrvHst` must still be enabled for Serval to configure the PrvHst channel; `PrvHstAccumulationEnable` only controls whether the driver connects to the TCP stream.

* **Running Sum Accumulation**: Accumulates histogram bin values over all frames using 64-bit integers to prevent overflow. Access via `PrvHstHistogramData` PV (INT64 waveform array). The running sum is continuously updated as new frames arrive.

* **Current Frame Display**: Individual frame histogram data available via `PrvHstHistogramFrame` PV (INT32 waveform array). Each frame shows the histogram bin values for the most recently received frame.

* **Sum of Last N Frames**: Calculates sum of the last N frames (configurable via `PrvHstFramesToSum` PV, default: 10). Access via `PrvHstHistogramSumNFrames` PV (INT64 waveform array). Update interval configurable via `PrvHstSumUpdateInterval` PV (default: 1 frame).

* **Time-of-Flight Axis**: Time axis in milliseconds for plotting histograms vs ToF. Access via `PrvHstHistogramTimeMs` PV (DOUBLE waveform array). Bin centers are calculated from bin edges (using `binWidth` and `binOffset` from jsonhisto metadata) and converted to milliseconds using the TimePix3 TDC clock period.

* **NDArray Callbacks**: Histogram data is available as 1D NDArray via callbacks (NDArrayAddress=5) for use with areaDetector plugins (NDFileTIFF, NDFileHDF5, etc.). The NDArray contains the accumulated histogram data (64-bit integers) and can be saved to disk or processed by other areaDetector plugins.

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

**File Saving**: Histogram data is available via NDArray callbacks (NDArrayAddress=5) for use with areaDetector file plugins (NDFileTIFF, NDFileHDF5, etc.). The histogram waveform arrays (`PrvHstHistogramData`, `PrvHstHistogramFrame`, `PrvHstHistogramSumNFrames`, `PrvHstHistogramTimeMs`) are also available as EPICS waveform arrays for direct access and plotting.

**Integration Notes**:
- The jsonhisto integration follows the same pattern as the standalone histogram IOC (`/epics/iocs/histogram/tpx3HistogramApp/src/tpx3HistogramDriver.cpp`)
- Histogram data is processed in a dedicated worker thread that handles TCP connection, JSON parsing, binary data reading, and frame accumulation
- The driver uses `doCallbacksFloat64Array`, `doCallbacksInt64Array`, and `doCallbacksInt32Array` to push data directly to waveform PVs (no `readFloat64Array` implementation, matching the histogram IOC)
- Thread synchronization uses `epicsMutex` to protect shared data structures
- The worker thread automatically clears its thread ID before exiting to allow clean shutdown

**Note on asyn and histogram**: The driver uses NDArray address 5 for the histogram channel (PrvImg=0, Img=1, PrvHst=5). The driver is constructed with `maxAddr=6` so that address 5 is valid; this prevents "parameter 51 in list 5, invalid list" warnings that occurred when `maxAddr` was 4. The driver also handles the `NDInt64` data type used for histogram data without causing "Illegal Value" status in `DataType_RBV`, and preserves shared size parameters (`SizeX_RBV`, `SizeY_RBV`) so they correctly reflect image dimensions even when histogram streaming is active. See the Troubleshooting section for more details.

How to run:
-----------

* Under `ADTimePix3/iocs/tpx3IOC/iocBoot/iocTimePix` there is already a ready to use IOC for the TimePix3
  - run serval
  - Change the IP address in `st.cmd` or `st_base.cmd`.
  - Run `./st.cmd`.
* There are CSS-Boy, screens under `ADTimePix3/tpx3App/op/`
  - Phoebus .bob screens have been converted/created but not yet included in the driver.

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

-   Binary Pixel Configuration (BPC) file support
-   Real-time mask generation (rectangular, circular)
-   Positive/negative masking modes
-   Hot pixel detection and correction

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

* **`NumImages` showing INVALID status**: Fixed in R1-5. The `NumImages` PV was showing INVALID status with value 100000000 (100 million) at IOC startup. The ADDriver base class was initializing `NumImages` to a very large default value, causing INVALID status. The driver now initializes `NumImages` to 0 (unlimited) in the constructor, which is appropriate for continuous mode and prevents the INVALID status. For single mode (ImageMode = 0), the driver automatically sets `NumImages = 1` when switching to single mode. **No action required** - this has been resolved in R1-5. If you see this issue before recompiling, you can manually set `NumImages` to 0: `caput TPX3-TEST:cam1:NumImages 0`.

* **`ArrayCounter_RBV` showing double count when histogram channel enabled**: Fixed in R1-5. When jsonhisto streaming was enabled, `ArrayCounter_RBV` was approximately twice `NumImagesCounter_RBV` (e.g., 46 vs 22). The histogram channel was calling `doCallbacksGenericPointer()`, which automatically increments the shared `NDArrayCounter` parameter. Since histogram uses NDArray address 5 (separate from image addresses 0 and 1) and should not affect the image channel counter, the driver now saves and restores `NDArrayCounter` around histogram callbacks to prevent histogram from incrementing it. `ArrayCounter_RBV` now correctly reflects only image frame counts, matching `NumImagesCounter_RBV` when histogram is enabled. **No action required** - this has been resolved in R1-5.

* **`asynPortDriver:getParamStatus: port=TPX3 error setting parameter 51 in list 5, invalid list`**: Fixed in R1-5. The driver was created with `maxAddr=4` (addresses 0–3 only), but the histogram channel uses NDArray address 5. When asyn or plugins accessed parameter 51 (ARRAY_DATA) at address 5, asyn reported "invalid list" because that address did not exist. The driver now uses `maxAddr=6` so that PrvImg=0, Img=1, and PrvHst=5 are all valid. The warning no longer appears when the histogram channel is enabled. **No action required** after recompile and IOC restart.

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
