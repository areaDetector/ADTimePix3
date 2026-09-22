/*
 * ADTimePix3 - Serval DAC response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_SERVAL_DACS_H
#define ADTIMEPIX_SERVAL_DACS_H

#include <string>

#include <json.hpp>

namespace ADTimePix3ServalDacs {

enum class UpdateError {
    None,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    MissingDac,
    InvalidDacValue
};

/**
 * Validate a GET /detector/chips/<chip>/dacs/ response, retain the current
 * target value, and build the complete object used for the atomic PUT.
 */
UpdateError prepareUpdate(const std::string& body, const std::string& dac,
                          int requestedValue, nlohmann::json& updatedDacs,
                          int& currentValue);

const char* updateErrorMessage(UpdateError error);

/** Serval documents HTTP 200 as the successful DAC PUT response. */
bool putAccepted(long httpStatus);

}  // namespace ADTimePix3ServalDacs

#endif  // ADTIMEPIX_SERVAL_DACS_H
