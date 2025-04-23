TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = tpx3App
PROD_IOC += $(PROD_NAME)

USR_CXXFLAGS += -std=c++11
USR_CXXFLAGS += -O3 -march=native -fopenmp -Wall -Wextra

USR_LDFLAGS += -fopenmp -std=c++11

# <name>.dbd will be created from <name>Include.dbd
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += tpx3Support.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp tpx3Main.cpp

# Add locally compiled object code
$(PROD_NAME)_LIBS += ADTimePix

$(PROD_NAME)_SYS_LIBS += cpr curl-d z 
$(PROD_NAME)_SYS_LIBS += json-c

#LIB_LIBS_Linux += boost_system-mt
include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
