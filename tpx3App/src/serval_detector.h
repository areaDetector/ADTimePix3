/*
 * ADTimePix3 - Serval detector response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_SERVAL_DETECTOR_H
#define ADTIMEPIX_SERVAL_DETECTOR_H

#include <string>

#include <json.hpp>

namespace ADTimePix3ServalDetector {

enum class ParseError {
    None,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    MissingInfo,
    MissingConfig,
    InvalidGeometry
};

struct Snapshot {
    nlohmann::json response;
    int pixelCount = 0;
    int rowLength = 0;
    int numberOfChips = 0;
    int numberOfRows = 0;
    int mpxType = 0;
};

/** Parse GET /detector and validate the fields required for detector geometry. */
ParseError parseResponse(const std::string& body, Snapshot& snapshot);

const char* parseErrorMessage(ParseError error);

}  // namespace ADTimePix3ServalDetector

#endif  // ADTIMEPIX_SERVAL_DETECTOR_H
