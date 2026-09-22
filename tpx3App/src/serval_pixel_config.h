/*
 * Typed validation for Serval GET /detector/chips/<chip>/PixelConfig responses.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_SERVAL_PIXEL_CONFIG_H
#define ADTIMEPIX3_SERVAL_PIXEL_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ADTimePix3ServalPixelConfig {

enum class ResponseError {
    None,
    HttpFailure,
    EmptyBody,
    MalformedJson,
    InvalidRoot,
    InvalidBase64,
    LengthMismatch,
};

/** Decode one JSON-string PixelConfig response and require its exact family-specific size. */
ResponseError parseResponse(long statusCode, const std::string& body,
                            std::size_t expectedBytes,
                            std::vector<std::uint8_t>& decoded);

/** Return the concatenated per-chip byte count, or zero for invalid/overflowing geometry. */
std::size_t bytesPerChip(std::size_t pixelsPerChip, int bytesPerPixel,
                         int thresholdSlices);

/**
 * Convert pelIndex()'s one-byte/one-slice logical index to a selected threshold
 * slice in a family-aware BPC/PixelConfig buffer.
 */
bool selectedSliceIndex(std::size_t logicalIndex, std::size_t pixelsPerChip,
                        int bytesPerPixel, int thresholdSlices,
                        int selectedSlice, std::size_t& physicalIndex);

const char* responseErrorMessage(ResponseError error);

}  // namespace ADTimePix3ServalPixelConfig

#endif
