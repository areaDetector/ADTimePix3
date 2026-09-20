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

namespace {

const char* const kTdcValues[] = {"P0123", "N0123", "PN0123", "P0", "N0", "PN0"};

int tdcIndex(const std::string& value)
{
    for (int index = 0;
         index < static_cast<int>(sizeof(kTdcValues) / sizeof(kTdcValues[0])); ++index) {
        if (value == kTdcValues[index]) {
            return index;
        }
    }
    return -1;
}

}  // namespace

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

bool setBoolean(nlohmann::json& config, const char* field, int value)
{
    if (value != 0 && value != 1) {
        return false;
    }

    config[field] = (value != 0);
    return true;
}

bool setBiasEnabled(nlohmann::json& config, int value)
{
    return setBoolean(config, "BiasEnabled", value);
}

bool setChainMode(nlohmann::json& config, int value)
{
    static const char* values[] = {"NONE", "LEADER", "FOLLOWER"};
    if (value < 0 || value >= static_cast<int>(sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    config["ChainMode"] = values[value];
    return true;
}

bool setPolarity(nlohmann::json& config, int value)
{
    static const char* values[] = {"Positive", "Negative"};
    if (value < 0 || value >= static_cast<int>(sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    config["Polarity"] = values[value];
    return true;
}

bool setTdc(nlohmann::json& config, int first, int second)
{
    const int count = static_cast<int>(sizeof(kTdcValues) / sizeof(kTdcValues[0]));
    if (first < 0 || first >= count || second < 0 || second >= count) {
        return false;
    }
    config["Tdc"] = nlohmann::json::array({kTdcValues[first], kTdcValues[second]});
    return true;
}

bool parseTdc(const nlohmann::json& value, int& first, int& second)
{
    if (!value.is_array() || value.size() != 2 ||
        !value[0].is_string() || !value[1].is_string()) {
        return false;
    }

    const int parsedFirst = tdcIndex(value[0].get<std::string>());
    const int parsedSecond = tdcIndex(value[1].get<std::string>());
    if (parsedFirst < 0 || parsedSecond < 0) {
        return false;
    }

    first = parsedFirst;
    second = parsedSecond;
    return true;
}

std::string formatTdc(const nlohmann::json& value)
{
    int first = 0;
    int second = 0;
    return parseTdc(value, first, second) ? value.dump() : std::string();
}

}  // namespace ADTimePix3ServalConfig
