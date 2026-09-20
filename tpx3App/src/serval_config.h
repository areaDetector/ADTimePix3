/*
 * ADTimePix3 - Serval configuration serialization helpers
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_SERVAL_CONFIG_H
#define ADTIMEPIX_SERVAL_CONFIG_H

#include <string>

#include <json.hpp>

namespace ADTimePix3ServalConfig {

enum class ParseError {
    None,
    EmptyBody,
    MalformedJson,
    InvalidRoot
};

/** Parse a Serval /detector/config response and require a JSON object root. */
ParseError parseResponse(const std::string& body, nlohmann::json& config);

const char* parseErrorMessage(ParseError error);

bool isBiasEnabledValue(int value);

/** Set BiasEnabled from its EPICS bo value. Returns false unless value is 0 or 1. */
bool setBiasEnabled(nlohmann::json& config, int value);

}  // namespace ADTimePix3ServalConfig

#endif
