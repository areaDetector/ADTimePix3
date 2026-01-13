ADTimePix3 Releases
==================

The repository was transferred a while back to areaDetector organization.

* see https://github.com/areaDetector/ADTimePix3

Since Serval features differ the driver is specific to Serval version.
Different branches of ADTimePix3 support different Serval versions.
The master branch (under development) is tested with Serval 4.1.1 and 3.3.2

Driver depends on Serval versions, at this time. The current releases support Serval 4.1.1 and 3.0.0-3.3.2


R1-5 (XXX, 2026)
--------

* **TCP Streaming for Preview Images**: Replaced GraphicsMagick HTTP method with TCP jsonimage streaming for PrvImg channel. Preview images now use configurable TCP streaming (tcp://listen@hostname:port format) for lower latency and better performance. GraphicsMagick implementation preserved in `preserve/graphicsmagick-preview` branch for backward compatibility.
* **TCP Streaming for Image Channel**: Added TCP jsonimage streaming support for Img channel, enabling real-time 2D image streaming similar to PrvImg channel. Both PrvImg and Img channels can now stream concurrently using separate array slots (pArrays[0] for PrvImg, pArrays[1] for Img) to prevent reference count conflicts.
* **Concurrent Channel Support**: Fixed NDArray reference count conflicts when both PrvImg and Img channels are configured for TCP streaming simultaneously. Each channel now uses its own array slot, allowing stable concurrent operation.
* **Clean Shutdown for TCP Streaming**: Fixed "Broken pipe" errors when stopping acquisition with preview and/or image channels enabled. Shutdown sequence now properly stops Serval measurement first, waits for Serval's TcpSender threads to stop sending (300ms delay), then signals worker threads to exit. This ensures clean shutdown even with rate mismatches (e.g., 60 Hz frame rate vs 5 Hz preview rate). Worker threads now detect "Connection closed by peer" as expected behavior during shutdown. Connection closure messages are channel-specific ("PrvImg TCP connection closed by peer" and "Img TCP connection closed by peer") to distinguish which channel is closing.
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
* Serval 4.1.1/4.1.0 requires changes to the driver.
  * Server->Detector->Health is an array of kv pairs in Serval v4.1.0, but was kv pairs in Serval v3.3.2.
* Additional controls in Serval 4.1.1
  * Server->Measurement->Stem
    * Scan
    * Virtual Detector
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
