TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

#UVC_HOME = /epics/support/areaDetector-3-2/ADUVC
#USR_INCLUDES += -I$(UVC_HOME)/include/libuvc

INC += ADTimePix.h

USR_INCLUDES += -I$(TOP)/tpx3Support/frozenSrc

LIBRARY_IOC = ADTimePix
LIB_SRCS += ADTimePix.cpp
LIB_SRCS += restApi.cpp tpx3Param.cpp

LIB_LIBS += frozen

DBD += tpx3Support.dbd


#include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE