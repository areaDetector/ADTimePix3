/*
 * ADTimePix3 - Detector family identification (Timepix3 vs Medipix3)
 *
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_DETECTOR_FAMILY_H
#define ADTIMEPIX_DETECTOR_FAMILY_H

#include <string>

enum class DetectorFamily {
    Unknown = 0,
    TPX3 = 1,
    MPX3 = 2,
};

struct DetectorCapabilities {
    bool supportsTdc = false;
    bool supportsTofHistogram = false;
    bool supportsDualPreview = false;
    bool supportsImageThresholds = false;
    int previewLayerCount = 0;
    /** Bytes in one pixel value within a threshold slice. See PIXELCONFIG_BPC_DIFF.md. */
    int bpcBytesPerPel = 1;
    /** Concatenated threshold slices per chip. TPX3 and packed-word MPX3 both use one. */
    int bpcThresholdSlices = 1;
};

DetectorFamily detectDetectorFamily(int mpxType, const std::string& chipType,
                                    const std::string& chipboardId);

DetectorCapabilities capabilitiesForFamily(DetectorFamily family);

const char* detectorFamilyName(DetectorFamily family);

#endif
