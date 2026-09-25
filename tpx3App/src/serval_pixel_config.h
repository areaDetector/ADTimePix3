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

/** Return the packed per-chip byte count, or zero for invalid/overflowing geometry. */
std::size_t bytesPerChip(std::size_t pixelsPerChip, int bytesPerPixel,
                         int thresholdSlices);

/**
 * Convert pelIndex()'s logical pixel index to the first byte of its packed value
 * in a selected threshold slice of a family-aware BPC/PixelConfig buffer.
 */
bool selectedSliceIndex(std::size_t logicalIndex, std::size_t pixelsPerChip,
                        int bytesPerPixel, int thresholdSlices,
                        int selectedSlice, std::size_t& physicalIndex);

/** Compare one 1- or 2-byte unsigned big-endian pixel value at byteOffset. */
bool absolutePackedDifference(const std::vector<std::uint8_t>& first,
                              const std::vector<std::uint8_t>& second,
                              std::size_t byteOffset, int bytesPerPixel,
                              std::uint32_t& difference);

/**
 * Map one MPX3 BPC-local pixel through a Serval Layout chip entry.
 *
 * The eight orientation names are the values reported in Serval
 * Layout.Rotated.Chips for quad detector rotations and reflections. Serval's
 * tile Y origin is bottom-left; imageHeight converts it to the top-left,
 * Y-down areaDetector image convention. Unknown names fail closed so a diff
 * is not displayed at a misleading coordinate.
 */
bool mpx3LayoutCoordinates(int localX, int localY, int chipWidth,
                           int originX, int originY, int imageHeight,
                           const std::string& orientation,
                           int& imageX, int& imageY);

const char* responseErrorMessage(ResponseError error);

}  // namespace ADTimePix3ServalPixelConfig

#endif
