#ifndef ADTIMEPIX3_LOG_H
#define ADTIMEPIX3_LOG_H

#include <asynDriver.h>

/**
 * asynPrint helpers for ADTimePix:: implementation files (ADTimePix.cpp, histogram_io.cpp, etc.).
 * Requires: pasynUserSelf, and driverName in scope (asynPortDriver / ADTimePix).
 *
 * Log context: GCC/Clang default to __PRETTY_FUNCTION__ (class + signature). Define
 * ADTPX3_LOG_SHORT (e.g. USR_CPPFLAGS += -DADTPX3_LOG_SHORT) for short __func__ only.
 */
#if defined(ADTPX3_LOG_SHORT)
#  define ADTPX3_FUNC __func__
#elif defined(__GNUC__) || defined(__clang__)
#  define ADTPX3_FUNC __PRETTY_FUNCTION__
#else
#  define ADTPX3_FUNC __func__
#endif

/* WARN* asyn trace: default ASYN_TRACE_WARNING. Define ADTPX3_WARN_AS_ERROR (see Makefile)
   so WARN* uses ASYN_TRACE_ERROR and shows up when only ERROR-level trace is enabled. */
#if defined(ADTPX3_WARN_AS_ERROR)
#  define ADTPX3_WARN_TRACE ASYN_TRACE_ERROR
#else
#  define ADTPX3_WARN_TRACE ASYN_TRACE_WARNING
#endif

/* Error message formatters */
#define ERR(msg)                                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: %s\n", driverName, ADTPX3_FUNC, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: " fmt "\n", driverName, \
              ADTPX3_FUNC, ##__VA_ARGS__)

/* Warning message formatters */
#define WARN(msg) \
    asynPrint(pasynUserSelf, ADTPX3_WARN_TRACE, "WARN | %s::%s: %s\n", driverName, ADTPX3_FUNC, msg)

#define WARN_ARGS(fmt, ...)                                                            \
    asynPrint(pasynUserSelf, ADTPX3_WARN_TRACE, "WARN | %s::%s: " fmt "\n", driverName, \
              ADTPX3_FUNC, ##__VA_ARGS__)

/* Log message formatters */
#define LOG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: %s\n", driverName, ADTPX3_FUNC, msg)

#define LOG_ARGS(fmt, ...)                                                                       \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: " fmt "\n", driverName, ADTPX3_FUNC, \
              ##__VA_ARGS__)

/* Flow message formatters */
#define FLOW(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s: %s\n", driverName, ADTPX3_FUNC, msg)

#define FLOW_ARGS(fmt, ...) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s: " fmt "\n", driverName, ADTPX3_FUNC, \
              ##__VA_ARGS__)

#endif /* ADTIMEPIX3_LOG_H */
