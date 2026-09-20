/*
 * ADTimePix3 - Serval configuration serialization helpers
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_config.h"

#include <utility>

namespace ADTimePix3ServalConfig {

ParseError parseResponse(const std::string& body, nlohmann::json& config)
{
    config = nlohmann::json::object();
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

    config = std::move(parsed);
    return ParseError::None;
}

const char* parseErrorMessage(ParseError error)
{
    switch (error) {
        case ParseError::None:
            return "valid detector configuration";
        case ParseError::EmptyBody:
            return "empty response body";
        case ParseError::MalformedJson:
            return "malformed JSON";
        case ParseError::InvalidRoot:
            return "detector configuration JSON root is not an object";
    }
    return "unknown detector configuration response error";
}

bool isBiasEnabledValue(int value)
{
    return value == 0 || value == 1;
}

bool setBiasEnabled(nlohmann::json& config, int value)
{
    if (!isBiasEnabledValue(value)) {
        return false;
    }

    config["BiasEnabled"] = (value != 0);
    return true;
}

}  // namespace ADTimePix3ServalConfig
