TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++17
# Shorter asyn log prefixes (__func__ only; avoids long __PRETTY_FUNCTION__ on GCC/Clang):
# USR_CPPFLAGS += -DADTPX3_LOG_SHORT
# Route WARN / WARN_ARGS through ASYN_TRACE_ERROR (visible when only ERROR trace is on):
# USR_CPPFLAGS += -DADTPX3_WARN_AS_ERROR

INC += ADTimePix.h
INC += ADTimePixLog.h
INC += bpc_file_io.h
INC += detector_family.h
INC += network_client.h
INC += one_shot_action.h
INC += serval_http.h
INC += serval_config.h
INC += serval_dacs.h
INC += serval_detector.h
INC += serval_destination.h
INC += serval_measurement.h
INC += serval_stream_validation.h

# GraphicsMagick removed - TCP streaming is used instead
# GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch

LIBRARY_IOC = ADTimePix
LIB_SRCS += ADTimePix.cpp
LIB_SRCS += bpc_file_io.cpp
LIB_SRCS += detector_family.cpp
LIB_SRCS += mask_io.cpp
LIB_SRCS += img_accumulation.cpp
LIB_SRCS += histogram_io.cpp
LIB_SRCS += network_client.cpp
LIB_SRCS += serval_config.cpp
LIB_SRCS += serval_dacs.cpp
LIB_SRCS += serval_detector.cpp
LIB_SRCS += serval_destination.cpp
LIB_SRCS += serval_http_client.cpp
LIB_SRCS += serval_measurement.cpp
LIB_SRCS += serval_stream_validation.cpp
LIB_SRCS += serval_stream.cpp
LIB_SRCS += serval_http.cpp
LIB_SRCS += acquire.cpp

LIB_SYS_LIBS += cpr curl z

DBD += tpx3Support.dbd

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
