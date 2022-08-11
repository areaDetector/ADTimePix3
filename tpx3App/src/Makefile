TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

USR_CXXFLAGS += -DPREFIX_MAGICK_SYMBOLS

INC += ADTimePix.h

USR_INCLUDES += $(addprefix -I, $(GRAPHICSMAGICK_INCLUDE))

LIBRARY_IOC = ADTimePix
LIB_SRCS += ADTimePix.cpp

LIB_SYS_LIBS += cpr curl-d z

DBD += tpx3Support.dbd

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
