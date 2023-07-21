# ADTimePix3

An EPICS areaDetector driver for TimePix3 detector from [ASC](https://www.amscins.com/).

Additional information:
* [Documentation](https://areadetector.github.io/master/ADTimePix3/timepix3.html)
* [Release notes](RELEASE.md)

Notes:
------

* Depends on the [CPR](https://github.com/libcpr/cpr) verson 1.9.1.
* Depends on the [json](https://github.com/nlohmann/json) version v3.11.2.
* Developed with ADCore R3-11 and ADSupport R1-10 or newer.
* This has only been tested on ubuntu 18.04 and 20.04 Linux 64-bit machines.
* This has only been developed for 2 x 2 chips layout, since that is what I have access to now
* This has only been tested with serval version 3.0.0. 
* Driver is specific to Serval version, since features differ.

Before compiling:
-----------------

* Compile cpr
* Clone json

How to run:
-----------

* Under `ADTimePix3/iocs/tpx3IOC/iocBoot/iocTimePix` there is already a ready to use IOC for the TimePix3
  - run serval
  - Change the IP address in `st.cmd`.
  - Run `./st.cmd`.
* There are CSS-Boy, screens under `areaDetector/tpx3App/op/` [TODO - update for serval 3.0.0].

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

  