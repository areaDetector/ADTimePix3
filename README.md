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
* **Concurrent Operation**: Both PrvImg and Img channels can stream concurrently without conflicts. Each channel uses its own array slot (PrvImg uses pArrays[0], Img uses pArrays[1]) to prevent NDArray reference count issues.
* **Shutdown Behavior**: When stopping acquisition, the shutdown sequence properly handles TCP streaming channels. Serval measurement is stopped first, allowing Serval's TcpSender threads to stop sending data cleanly. Worker threads then detect "Connection closed by peer" as expected behavior. Connection closure messages are channel-specific ("PrvImg TCP connection closed by peer" and "Img TCP connection closed by peer") to distinguish which channel is closing. This prevents "Broken pipe" errors even with rate mismatches (e.g., 60 Hz frame rate vs 5 Hz preview rate).
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

The ADTimePix3 driver provides a comprehensive, production-ready interface for TimePix3 detectors in EPICS environments, with extensive monitoring, control, and data acquisition capabilities.
