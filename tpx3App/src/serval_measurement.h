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

/** Parse a Serval /measurement response and require a JSON object root. */
ParseError parseResponse(const std::string& body, nlohmann::json& measurement);

const char* parseErrorMessage(ParseError error);

/** Validate the GET /measurement/config response used as the base of a merge. */
ConfigResponseError parseConfigResponse(long statusCode, const std::string& body,
                                        nlohmann::json& config);

const char* configResponseErrorMessage(ConfigResponseError error);

}  // namespace ADTimePix3ServalMeasurement

#endif  // ADTIMEPIX_SERVAL_MEASUREMENT_H
