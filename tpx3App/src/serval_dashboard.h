/*
 * Typed validation for Serval GET /dashboard responses.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_SERVAL_DASHBOARD_H
#define ADTIMEPIX3_SERVAL_DASHBOARD_H

#include <string>

#include <json.hpp>

namespace ADTimePix3ServalDashboard {

enum class ResponseError {
    None,
    HttpFailure,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    MissingServer,
    InvalidServer,
    MissingDetector,
    InvalidDetector,
    InvalidDetectorType,
    InvalidMeasurement,
    InvalidSoftwareVersion,
    InvalidSoftwareTimestamp,
    InvalidDiskSpace,
    InvalidDiskEntry,
    InvalidDiskValue,
};

struct Snapshot {
    nlohmann::json response = nlohmann::json::object();
    bool detectorConnected = false;
    std::string detectorType;
};

/** Validate the complete dashboard response before any driver state is changed. */
ResponseError parseResponse(long statusCode, const std::string& body, Snapshot& snapshot);

const char* responseErrorMessage(ResponseError error);

}  // namespace ADTimePix3ServalDashboard

#endif
