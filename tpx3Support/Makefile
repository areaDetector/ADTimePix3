TOP=..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

SRC_DIRS += ../frozenSrc
SRC_DIRS += ../cprSrc
#SRC_DIRS += ../cprSrc/cpr/build/cpr_generated_includes
SRC_DIRS += ../cprSrc/cpr/include


LIBRARY=frozen

INC += frozen.h
INC += cpr/cprver.h

INC += cpr/accept_encoding.h
INC += cpr/api.h
INC += cpr/async.h
INC += cpr/auth.h
INC += cpr/bearer.h
INC += cpr/body.h
INC += cpr/callback.h
INC += cpr/connect_timeout.h
INC += cpr/cookies.h
INC += cpr/cpr.h
INC += cpr/cprtypes.h
INC += cpr/curl_container.h
INC += cpr/curlholder.h
INC += cpr/error.h
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



frozen_SRCS += frozen.c

LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libcpr.so
LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libcurl-d.so
LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libz.so

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


