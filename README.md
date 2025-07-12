# ADTimePix3

An EPICS areaDetector driver for TimePix3 detector from [ASC](https://www.amscins.com/).

Additional information:
* [Documentation](https://areadetector.github.io/areaDetector/ADTimePix3/ADTimePix3.html)
* [Release notes](RELEASE.md)

Notes:
------

* Depends on the [CPR](https://github.com/libcpr/cpr) verson 1.9.1.
* Depends on the [json](https://github.com/nlohmann/json) version v3.11.2.
* Developed with ADCore R3-11 and ADSupport R1-10 or newer.
* This has only been developed/tested on ubuntu 22.04, 20.04, 18.04, RHEL 7.9, RHEL 9.6 Linux 64-bit machines.
* This has only been developed for 2 x 2 chips layout and 1 chip tpx3CAM, since that is what I have access to now.
* This has been tested with serval version 4.1.1, 4.1.0, 3.3.2, 3.2.0, 3.1.0 and 3.0.0 extensivly. Only most recent serval version(s) are tested extensivly. However, the master branch is compatible with serval 4.x.x, and attempts are being made to make it compatible with Serval 3.x.x. In the meantime, please use the 3.3.2 branch for serval 3.x.x. The Serval 4.x.x has additional features, which are not yet supported.
* Driver is specific to Serval version, since RUST features differ. Driver for Serval 2.x.y is in separate branch, and is not under current development. The branch 3.3.2 is compatible with serval 3.x.x only, and will likely not be developed further.
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

  
