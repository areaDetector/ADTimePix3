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

constexpr std::uint16_t kMaskBit = 0x0001;

/** True only where the vendor per-pixel mask-bit definition is known. */
bool operatorMaskSupported(DetectorFamily family);

/** Number of packed BPC bytes per pixel, or zero for an unsupported family. */
std::size_t bytesPerPixel(DetectorFamily family);

/** Decode one packed pixel value at byteOffset (MPX3 words are big-endian). */
bool pixelValue(DetectorFamily family,
                const std::vector<std::uint8_t>& bytes,
                std::size_t byteOffset,
                std::uint16_t& value);

/** Classify one packed pixel by its vendor-defined bit-0 mask. */
bool isMasked(DetectorFamily family,
              const std::vector<std::uint8_t>& bytes,
              std::size_t byteOffset);

/** Count masked packed pixels; returns zero for unsupported families. */
std::size_t countMasked(DetectorFamily family,
                        const std::vector<std::uint8_t>& bytes);

/** Set or clear only bit 0 of one packed pixel, preserving every other bit. */
bool setMasked(DetectorFamily family,
               std::vector<std::uint8_t>& bytes,
               std::size_t byteOffset,
               bool masked);

}  // namespace ADTimePix3BpcMask

#endif
