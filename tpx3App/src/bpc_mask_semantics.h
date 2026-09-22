/*
 * Detector-family-specific BPC operator-mask semantics.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_BPC_MASK_SEMANTICS_H
#define ADTIMEPIX3_BPC_MASK_SEMANTICS_H

#include "detector_family.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ADTimePix3BpcMask {

constexpr std::uint8_t kTpx3DisabledPixel = 0x1f;

/** True only where the vendor mask-byte definition is known. */
bool operatorMaskSupported(DetectorFamily family);

/** Classify a calibration byte as a fully disabled pixel. */
bool isMasked(DetectorFamily family, std::uint8_t byte);

/** Count fully disabled pixels; returns zero for unsupported families. */
std::size_t countMasked(DetectorFamily family,
                        const std::vector<std::uint8_t>& bytes);

/** Replace one TPX3 calibration byte with the Accos fully-disabled pattern. */
bool applyOperatorMask(DetectorFamily family, std::uint8_t& byte);

}  // namespace ADTimePix3BpcMask

#endif
