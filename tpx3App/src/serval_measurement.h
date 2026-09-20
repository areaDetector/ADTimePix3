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

/** Parse a Serval /measurement response and require a JSON object root. */
ParseError parseResponse(const std::string& body, nlohmann::json& measurement);

const char* parseErrorMessage(ParseError error);

}  // namespace ADTimePix3ServalMeasurement

#endif  // ADTIMEPIX_SERVAL_MEASUREMENT_H
