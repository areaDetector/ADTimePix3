TOP=..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

SRC_DIRS += ../json

INC += json.hpp
INC += json_fwd.hpp

SRC_DIRS += ../cpr

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

LIBRARY_IOC = cpr

INC += cpr/accept_encoding.h
INC += cpr/api.h
INC += cpr/async.h
INC += cpr/auth.h
INC += cpr/bearer.h
INC += cpr/body.h
INC += cpr/buffer.h
INC += cpr/callback.h
INC += cpr/cert_info.h
INC += cpr/connect_timeout.h
INC += cpr/cookies.h
INC += cpr/cpr.h
INC += cpr/cprtypes.h
INC += cpr/cprver.h
INC += cpr/curl_container.h
INC += cpr/curlholder.h
INC += cpr/error.h
INC += cpr/file.h
INC += cpr/http_version.h
INC += cpr/interceptor.h
INC += cpr/interface.h
INC += cpr/limit_rate.h
INC += cpr/local_port.h
INC += cpr/local_port_range.h
INC += cpr/low_speed.h
INC += cpr/multipart.h
INC += cpr/parameters.h
INC += cpr/payload.h
INC += cpr/proxies.h
INC += cpr/proxyauth.h
INC += cpr/range.h
INC += cpr/redirect.h
INC += cpr/reserve_size.h
INC += cpr/response.h
INC += cpr/session.h
INC += cpr/singleton.h
INC += cpr/ssl_ctx.h
INC += cpr/ssl_options.h
INC += cpr/status_codes.h
INC += cpr/threadpool.h
INC += cpr/timeout.h
INC += cpr/unix_socket.h
INC += cpr/user_agent.h
INC += cpr/util.h
INC += cpr/verbose.h

LIB_SRCS += accept_encoding.cpp
LIB_SRCS += async.cpp
LIB_SRCS += auth.cpp
LIB_SRCS += bearer.cpp
LIB_SRCS += cert_info.cpp
LIB_SRCS += cookies.cpp
LIB_SRCS += cprtypes.cpp
LIB_SRCS += curl_container.cpp
LIB_SRCS += curlholder.cpp
LIB_SRCS += error.cpp
LIB_SRCS += interceptor.cpp
LIB_SRCS += multipart.cpp
LIB_SRCS += parameters.cpp
LIB_SRCS += payload.cpp
LIB_SRCS += proxies.cpp
LIB_SRCS += proxyauth.cpp
LIB_SRCS += redirect.cpp
LIB_SRCS += response.cpp
LIB_SRCS += session.cpp
LIB_SRCS += ssl_ctx.cpp
LIB_SRCS += threadpool.cpp
LIB_SRCS += timeout.cpp
LIB_SRCS += unix_socket.cpp
LIB_SRCS += util.cpp

LIB_SYS_LIBS += curl z

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
