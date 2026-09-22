/*
 * Typed validation for Serval GET /detector/health responses.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_SERVAL_HEALTH_H
#define ADTIMEPIX3_SERVAL_HEALTH_H

#include <string>

namespace ADTimePix3ServalHealth {

enum class ResponseError {
    None,
    HttpFailure,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    EmptyHealthArray,
    InvalidHealthEntry,
    MissingHealthFields,
    InvalidMetric,
    InvalidArray,
};

struct Snapshot {
    bool hasLocalTemperature = false;
    double localTemperature = 0.0;
    bool hasFpgaTemperature = false;
    double fpgaTemperature = 0.0;
    bool hasFan1Speed = false;
    double fan1Speed = 0.0;
    bool hasFan2Speed = false;
    double fan2Speed = 0.0;
    bool hasBiasVoltage = false;
    double biasVoltage = 0.0;
    bool hasHumidity = false;
    int humidity = 0;
    bool hasChipTemperatures = false;
    std::string chipTemperatures;
    bool hasVdd = false;
    std::string vdd;
    bool hasAvdd = false;
    std::string avdd;
};

/** Validate all recognized health fields before returning a publishable snapshot. */
ResponseError parseResponse(long statusCode, const std::string& body, Snapshot& snapshot);

const char* responseErrorMessage(ResponseError error);

}  // namespace ADTimePix3ServalHealth

#endif
