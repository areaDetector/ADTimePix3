ADTimePix3 Releases
==================

The repository was transferred a while back to areaDetector organization.

* see https://github.com/areaDetector/ADTimePix3

There are currently only pre-releases for ADTimePix3
Since Serval features differ the driver is specific to Serval version.
Different branches of ADTimePix3 support differnet Serval versions.
The master branch (under development) is for Serval 3.3.0

Driver depends on Serval versions, at this time. The current releases support Serval 3.0.0-3.3.2

R1-3 (xxx, 2025)
--------

* TDC1/TDC2 reporting for serval 4, and Serval 3.
* The Serval 4.x.y requires {"Content-Type", "application/json"}, instead of "text/plain", as "Content-Type" when using cpr library.
  * Log Level, ChainMode, Polarity, PeriphClk80, ExternalReferenceClock, etc.
* Serval 4.1.1/4.1.0 requires changes to the driver.
  * Server->Detector->Health is an array of kv pairs in Serval v4.1.0, but was kv pairs in Serval v3.3.2.
* Additional controls
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
* Mask generation for one chip, and 2x2 quad chip detectors for all 8 detector orientaitons.
* AD mask image placed into BPC calibration mask.bpc file.
* Masked pixels from BPC read into AD mask image.
* Masked pels in 2nd octet of BPC waveform PV
* Connection status PVs
  + Serval connected (URL web interface is up)
  + Detector is connnected to Serval
* Report number of masked pixels in bpc calibration
* Manipulation of the Binary Pixel Configuration (BPC). Allows creation of positive or negative image mask.
  + Create image rectangular mask
  + Create image circular mask
  + Image mask can be positive or negative
  + Individual image mask is additive
  + Number of image masks is not limited
* Rotate image into all 8 posible arrangement.
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
* Driver ensures that for Continous mode ExposureTime (AcquireTime) propagates to TriggerPeriod (AcquirePeriod), must be equal to each other
* Default reduce readout of preview images to not more than 1 per second
* Remove disgnostic messages
* Dashboard DiskSpace is empty until raw file writing enabled, and acquisition starts
* Dashboard work updated
* Startup update with description of channels
* Allow streamling of raw .tpx3 events, saving of prv images not required, more consistent default chan

R0-7 (June 26, 2023)
--------

* Driver works for Serval 3.2 and Emulator 3.2
  * Separate TDC1 and TDC2 EventRate PVs in Serval 3.2
* Performance issue resolved for reading preview images. Mulitple connections were opening in callback while loop.
  * Used cpr library object called a Session is now used. Session is the only statful piece of the cpr library.
  * Session object of cpr library is useful to hold on to state while reading preview images.
* The priview images do not need to be written to disk, and are read from from PrvImg Base [0] channel socket channel.
  * The preview is on Preview [1] channel, which does not have to enabled
* Allow streaming of raw .tpx3 data.
  * Streamling: tcp://localhost:8085
  * Writing to .tpx3 file: file:/media/nvme/raw
* More consistent settings for file writing and streaming channels in st_base.cmd
* opi screens for fileWriter streaming enhancement
* Continous mode: ExposureTime (AcquireTime) used and propagates to TriggerPeriod (AcquirePeriod)
* Preveiw[0] does not need to be enabled. Preview images are then not displayed.

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
