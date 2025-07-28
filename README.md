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
* This has only been developed/tested on ubuntu 22.04, 20.04, 18.04, RHEL 7.9, RHEL 9.6 Linux 64-bit machines.
* This has only been developed for 2 x 2 chips layout and 1 chip tpx3CAM, since that is what I have access to now.
* This has been tested with serval version 4.1.1, 4.1.0, 3.3.2, 3.2.0, 3.1.0 and 3.0.0 extensively. Only most recent serval version(s) are tested extensively. However, the master branch is compatible with serval 4.x.x, and attempts are being made to make it compatible with Serval 3.x.x. In the meantime, please use the 3.3.2 branch for serval 3.x.x. The Serval 4.x.x has additional features, which are not yet supported.
* Driver is specific to Serval version, since Rust features differ. Driver for Serval 2.x.y is in separate branch, and is not under current development. The branch 3.3.2 is compatible with serval 3.x.x only, and will likely not be developed further.
* The driver has been developed using TimePix3 Emulator, and real detectors. Real detectors are quad-chip, and single chip.

Before compiling:
-----------------

* Compile cpr
* Clone json

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
  * Magick file saving plugin
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

#### 1\. Driver Architecture

-   Main Class: ADTimePix inherits from ADDriver (areaDetector base class)
-   Communication: HTTP REST API using the cpr (C++ Requests) library
-   Data Format: JSON using nlohmann/json library
-   Image Processing: GraphicsMagick for image handling

#### 2. Core Functionality

Detector Communication:

-   Connects to Serval server via HTTP REST API (default: http://localhost:8081)
-   Supports multiple Serval versions (3.0.0-4.x.x) with version-specific features
-   Handles both real detectors and emulator mode

Acquisition Modes:

-   Raw Mode: .tpx3 file output or TCP streaming
-   Image Mode: Processed images (TIFF, PNG, PGM formats)
-   Preview Mode: Real-time preview images and histograms
-   Multiple Streams: Support for dual raw streams (Serval 3.3.0+)

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

* Image Acquisition:
  -   Asynchronous callback thread for continuous acquisition
  -   Real-time measurement status updates
  -   Support for multiple data formats and streaming

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
