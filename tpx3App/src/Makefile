TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11
# Shorter asyn log prefixes (__func__ only; avoids long __PRETTY_FUNCTION__ on GCC/Clang):
# USR_CPPFLAGS += -DADTPX3_LOG_SHORT
# Route WARN / WARN_ARGS through ASYN_TRACE_ERROR (visible when only ERROR trace is on):
# USR_CPPFLAGS += -DADTPX3_WARN_AS_ERROR

INC += ADTimePix.h
INC += ADTimePixLog.h

# GraphicsMagick removed - TCP streaming is used instead
# GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch

LIBRARY_IOC = ADTimePix
LIB_SRCS += ADTimePix.cpp
LIB_SRCS += mask_io.cpp
LIB_SRCS += img_accumulation.cpp
LIB_SRCS += histogram_io.cpp

LIB_SYS_LIBS += cpr curl z

DBD += tpx3Support.dbd

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
