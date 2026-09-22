/*
 * ADTimePix3 - Serval DAC response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_dacs.h"

#include <limits>
#include <utility>

namespace ADTimePix3ServalDacs {

namespace {

bool epicsIntValue(const nlohmann::json& value, int& result)
{
    if (value.is_number_unsigned()) {
        const unsigned long long parsed = value.get<unsigned long long>();
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
            return false;
        }
        result = static_cast<int>(parsed);
        return true;
    }
    if (value.is_number_integer()) {
        const long long parsed = value.get<long long>();
        if (parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        result = static_cast<int>(parsed);
        return true;
    }
    return false;
}

}  // namespace

UpdateError prepareUpdate(const std::string& body, const std::string& dac,
                          int requestedValue, nlohmann::json& updatedDacs,
                          int& currentValue)
{
    updatedDacs = nlohmann::json::object();
    if (body.empty()) return UpdateError::EmptyBody;

    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return UpdateError::MalformedJson;
    if (!parsed.is_object()) return UpdateError::InvalidRoot;
    if (!parsed.contains(dac)) return UpdateError::MissingDac;

    int parsedCurrent = 0;
    if (!epicsIntValue(parsed[dac], parsedCurrent)) {
        return UpdateError::InvalidDacValue;
    }

    parsed[dac] = requestedValue;
    currentValue = parsedCurrent;
    updatedDacs = std::move(parsed);
    return UpdateError::None;
}

const char* updateErrorMessage(UpdateError error)
{
    switch (error) {
        case UpdateError::None:
            return "valid DAC response";
        case UpdateError::EmptyBody:
            return "empty DAC response";
        case UpdateError::MalformedJson:
            return "malformed DAC JSON";
        case UpdateError::InvalidRoot:
            return "DAC JSON root is not an object";
        case UpdateError::MissingDac:
            return "DAC response is missing the requested field";
        case UpdateError::InvalidDacValue:
            return "DAC response contains an invalid requested-field value";
    }
    return "unknown DAC response error";
}

bool putAccepted(long httpStatus)
{
    return httpStatus == 200;
}

}  // namespace ADTimePix3ServalDacs
