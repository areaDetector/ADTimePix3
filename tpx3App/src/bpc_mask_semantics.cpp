/*
 * Detector-family-specific BPC operator-mask semantics.
 *
 * SPDX-License-Identifier: MIT
 */

#include "bpc_mask_semantics.h"

namespace ADTimePix3BpcMask {

bool operatorMaskSupported(DetectorFamily family)
{
    return family == DetectorFamily::TPX3;
}

bool isMasked(DetectorFamily family, std::uint8_t byte)
{
    return family == DetectorFamily::TPX3 && byte == kTpx3DisabledPixel;
}

std::size_t countMasked(DetectorFamily family,
                        const std::vector<std::uint8_t>& bytes)
{
    if (!operatorMaskSupported(family)) return 0;
    std::size_t count = 0;
    for (std::uint8_t byte : bytes) {
        if (isMasked(family, byte)) ++count;
    }
    return count;
}

bool applyOperatorMask(DetectorFamily family, std::uint8_t& byte)
{
    if (!operatorMaskSupported(family)) return false;
    byte = kTpx3DisabledPixel;
    return true;
}

}  // namespace ADTimePix3BpcMask
