/*
 * ADTimePix3 - Serval stream metadata validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SERVAL_STREAM_VALIDATION_H
#define SERVAL_STREAM_VALIDATION_H

#include <cstddef>

#include <json.hpp>

namespace ADTimePix3Stream {

enum class PixelFormat {
    UInt8,
    UInt16,
    UInt32
};

enum class ImageHeaderError {
    None,
    MissingField,
    InvalidFieldType,
    InvalidDimension,
    UnsupportedPixelFormat,
    InconsistentPixelMetadata,
    PixelLimitExceeded,
    PayloadLimitExceeded
};

struct ImageFrameLimits {
    std::size_t maxDimension;
    std::size_t maxPixels;
    std::size_t maxPayloadBytes;
};

struct ImageFrameLayout {
    int width;
    int height;
    PixelFormat pixelFormat;
    std::size_t pixelCount;
    std::size_t bytesPerPixel;
    std::size_t payloadBytes;
};

ImageFrameLimits detectorImageFrameLimits(int maxSizeX, int maxSizeY, int pixelCount);

ImageHeaderError validateJsonImageHeader(const nlohmann::json& header,
                                         const ImageFrameLimits& limits,
                                         ImageFrameLayout& layout);

const char* imageHeaderErrorMessage(ImageHeaderError error);

const char* pixelFormatName(PixelFormat format);

}  // namespace ADTimePix3Stream

#endif
