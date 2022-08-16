# ADTimePix

An EPICS areaDetector driver for TimePix3 detector from [ASC](https://www.amscins.com/).

Additional information:
* [Documentation](https://areadetector.github.io/master/ADTimePix/timepix.html)
* [Release notes](RELEASE.md)

Notes:
------

* Depends on the [CPR](https://github.com/libcpr/cpr) verson 1.9.1.
* Depends on the [json](https://github.com/nlohmann/json) version v3.11.2.
* Developed with ADCore R3-11 and ADSupport R1-10 or newer.
* This has only been tested on ubuntu 18.04 and 20.04 Linux 64-bit machines.
* This has only been developed for 2 x 2 chips layout, since that is what I have access to now
* This has only been tested with serval version 2.3.6

Before compiling:
-----------------

* Compile cpr
* Clone json

How to run:
-----------

* Under `ADTimePix/iocs/tpx3IOC/iocBoot/iocTimePix` there is already a ready to use IOC for the TimePix3
  - run serval
  - Change the IP address in `st.cmd`.
  - Run `./st.cmd`.
* There are CSS-Boy, screens under `areaDetector/tpx3App/op/` [TODO - copy from main CSS repo].


