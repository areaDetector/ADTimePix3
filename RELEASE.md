ADTimePix3 Releases
==================

The latest untagged master branch can be obtained at
https://github.com/kgofron/ADTimePix3.

Tagged source code can be obtained at
https://github.com/kgofron/ADTimePix3/tags

There are currently only pre-releases for ADTimePix3
Since Serval features differ the driver is specific to Serval version. 
Different branches of ADTimePix3 support differnet Serval versions.
The master branch (under development) is for Serval 3.0.0


Driver depends on Serval versions, at this time.

R0-7 (June 26, 2023)
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

R0-6 (March 30, 2023)
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
