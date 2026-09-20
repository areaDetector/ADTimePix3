/*
 * ADTimePix3 - Serval measurement response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_measurement.h"

#include <utility>

namespace ADTimePix3ServalMeasurement {

ParseError parseResponse(const std::string& body, nlohmann::json& measurement)
{
    measurement = nlohmann::json::object();
    if (body.empty()) {
        return ParseError::EmptyBody;
    }

    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) {
        return ParseError::MalformedJson;
    }
    if (!parsed.is_object()) {
        return ParseError::InvalidRoot;
    }

    measurement = std::move(parsed);
    return ParseError::None;
}

const char* parseErrorMessage(ParseError error)
{
    switch (error) {
        case ParseError::None:
            return "valid measurement response";
        case ParseError::EmptyBody:
            return "empty response body";
        case ParseError::MalformedJson:
            return "malformed JSON";
        case ParseError::InvalidRoot:
            return "measurement JSON root is not an object";
    }
    return "unknown measurement response error";
}

}  // namespace ADTimePix3ServalMeasurement
