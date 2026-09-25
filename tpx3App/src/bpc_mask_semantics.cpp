/*
 * Detector-family-specific BPC operator-mask semantics.
 *
 * SPDX-License-Identifier: MIT
 */

#include "bpc_mask_semantics.h"

namespace ADTimePix3BpcMask {

bool operatorMaskSupported(DetectorFamily family)
{
    return family == DetectorFamily::TPX3 || family == DetectorFamily::MPX3;
}

std::size_t bytesPerPixel(DetectorFamily family)
{
    switch (family) {
    case DetectorFamily::TPX3:
        return 1;
    case DetectorFamily::MPX3:
        return 2;
    default:
        return 0;
    }
}

bool pixelValue(DetectorFamily family,
                const std::vector<std::uint8_t>& bytes,
                std::size_t byteOffset,
                std::uint16_t& value)
{
    value = 0;
    const std::size_t width = bytesPerPixel(family);
    if (width == 0 || byteOffset > bytes.size() || width > bytes.size() - byteOffset) {
        return false;
    }
    if (width == 1) {
        value = bytes[byteOffset];
    } else {
        value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[byteOffset]) << 8) |
            static_cast<std::uint16_t>(bytes[byteOffset + 1]));
    }
    return true;
}

bool isMasked(DetectorFamily family,
              const std::vector<std::uint8_t>& bytes,
              std::size_t byteOffset)
{
    std::uint16_t value = 0;
    return pixelValue(family, bytes, byteOffset, value) && (value & kMaskBit) != 0;
}

std::size_t countMasked(DetectorFamily family,
                        const std::vector<std::uint8_t>& bytes)
{
    const std::size_t width = bytesPerPixel(family);
    if (width == 0) return 0;
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + width <= bytes.size(); offset += width) {
        if (isMasked(family, bytes, offset)) ++count;
    }
    return count;
}

bool setMasked(DetectorFamily family,
               std::vector<std::uint8_t>& bytes,
               std::size_t byteOffset,
               bool masked)
{
    const std::size_t width = bytesPerPixel(family);
    if (width == 0 || byteOffset > bytes.size() || width > bytes.size() - byteOffset) {
        return false;
    }
    const std::size_t maskByte = byteOffset + width - 1;
    if (masked) {
        bytes[maskByte] |= static_cast<std::uint8_t>(kMaskBit);
    } else {
        bytes[maskByte] &= static_cast<std::uint8_t>(~kMaskBit);
    }
    return true;
}

}  // namespace ADTimePix3BpcMask
