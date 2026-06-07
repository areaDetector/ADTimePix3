/*
 * ADTimePix3 - Detector family identification (Timepix3 vs Medipix3)
 *
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "detector_family.h"

#include <algorithm>
#include <cctype>

namespace {

std::string upperAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool chipboardPrefixIs(const std::string& chipboardId, char digit) {
    return !chipboardId.empty() && chipboardId[0] == digit;
}

}  // namespace

DetectorFamily detectDetectorFamily(int mpxType, const std::string& chipType,
                                    const std::string& chipboardId) {
    const std::string chip = upperAscii(chipType);

    if (chip == "MPX3") return DetectorFamily::MPX3;
    if (chip == "TPX3") return DetectorFamily::TPX3;

    if (mpxType == 5) return DetectorFamily::MPX3;
    if (mpxType == 6) return DetectorFamily::TPX3;

    if (chipboardPrefixIs(chipboardId, '5')) return DetectorFamily::MPX3;
    if (chipboardPrefixIs(chipboardId, '4')) return DetectorFamily::TPX3;

    return DetectorFamily::Unknown;
}

DetectorCapabilities capabilitiesForFamily(DetectorFamily family) {
    DetectorCapabilities caps;
    switch (family) {
    case DetectorFamily::MPX3:
        caps.supportsTdc = false;
        caps.supportsTofHistogram = false;
        caps.supportsDualPreview = true;
        caps.supportsImageThresholds = true;
        caps.previewLayerCount = 2;
        break;
    case DetectorFamily::TPX3:
        caps.supportsTdc = true;
        caps.supportsTofHistogram = true;
        caps.supportsDualPreview = true;
        caps.supportsImageThresholds = false;
        caps.previewLayerCount = 2;
        break;
    default:
        break;
    }
    return caps;
}

const char* detectorFamilyName(DetectorFamily family) {
    switch (family) {
    case DetectorFamily::TPX3:
        return "TPX3";
    case DetectorFamily::MPX3:
        return "MPX3";
    default:
        return "UNKNOWN";
    }
}
