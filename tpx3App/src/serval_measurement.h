/*
 * ADTimePix3 - Serval measurement response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_SERVAL_MEASUREMENT_H
#define ADTIMEPIX_SERVAL_MEASUREMENT_H

#include <string>

#include <json.hpp>

namespace ADTimePix3ServalMeasurement {

enum class ParseError {
    None,
    EmptyBody,
    MalformedJson,
    InvalidRoot
};

enum class ConfigResponseError {
    None,
    HttpFailure,
    EmptyBody,
    MalformedJson,
    InvalidRoot
};

enum class StatusResponseError {
    None,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    InvalidInfo,
    InvalidStatus,
    InvalidMetric
};

struct StatusSnapshot {
    bool hasPixelEventRate = false;
    int pixelEventRate = 0;
    bool hasTdc1EventRate = false;
    int tdc1EventRate = 0;
    bool hasTdc2EventRate = false;
    int tdc2EventRate = 0;
    bool hasStartDateTime = false;
    long long startDateTime = 0;
    bool hasElapsedTime = false;
    double elapsedTime = 0.0;
    bool hasTimeLeft = false;
    double timeLeft = 0.0;
    bool hasFrameCount = false;
    int frameCount = 0;
    bool hasDroppedFrames = false;
    int droppedFrames = 0;
    bool hasStatus = false;
    std::string status;
};

/** Parse a Serval /measurement response and require a JSON object root. */
ParseError parseResponse(const std::string& body, nlohmann::json& measurement);

const char* parseErrorMessage(ParseError error);

/** Parse and validate the fields consumed from GET /measurement. */
StatusResponseError parseStatusResponse(const std::string& body, StatusSnapshot& snapshot);

const char* statusResponseErrorMessage(StatusResponseError error);

/** Validate the GET /measurement/config response used as the base of a merge. */
ConfigResponseError parseConfigResponse(long statusCode, const std::string& body,
                                        nlohmann::json& config);

const char* configResponseErrorMessage(ConfigResponseError error);

}  // namespace ADTimePix3ServalMeasurement

#endif  // ADTIMEPIX_SERVAL_MEASUREMENT_H
