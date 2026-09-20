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

/** Set a JSON boolean from an EPICS bo value. Returns false unless value is 0 or 1. */
bool setBoolean(nlohmann::json& config, const char* field, int value);

/** Set BiasEnabled from its EPICS bo value. Returns false unless value is 0 or 1. */
bool setBiasEnabled(nlohmann::json& config, int value);

/** Set the Serval ChainMode string from its EPICS mbbo value. */
bool setChainMode(nlohmann::json& config, int value);

/** Set the Serval Polarity string from its EPICS bo value. */
bool setPolarity(nlohmann::json& config, int value);

/** Set both Serval Tdc strings from their EPICS mbbo values. */
bool setTdc(nlohmann::json& config, int first, int second);

/** Decode an exact two-entry Serval Tdc array into EPICS mbbo values. */
bool parseTdc(const nlohmann::json& value, int& first, int& second);

/** Format a valid Serval Tdc array for the aggregate string readback. */
std::string formatTdc(const nlohmann::json& value);

}  // namespace ADTimePix3ServalConfig

#endif
