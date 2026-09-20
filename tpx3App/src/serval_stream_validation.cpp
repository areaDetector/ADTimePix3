/*
 * ADTimePix3 - Serval stream metadata validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_stream_validation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace ADTimePix3Stream {
namespace {

// The driver supports layouts of at most eight 256x256 chips. A linear
// eight-chip layout determines the per-axis ceiling; all layouts share the
// total-pixel ceiling.
constexpr std::size_t kSupportedChipPixels = 256U * 256U;
constexpr std::size_t kMaxSupportedChips = 8U;
constexpr std::size_t kMaxSupportedPixels = kMaxSupportedChips * kSupportedChipPixels;
constexpr std::size_t kMaxSupportedDimension = kMaxSupportedChips * 256U;
constexpr std::size_t kMaxBytesPerPixel = sizeof(std::uint32_t);

bool checkedMultiply(std::size_t left, std::size_t right, std::size_t& product)
{
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    product = left * right;
    return true;
}

}  // namespace

ImageFrameLimits detectorImageFrameLimits(int maxSizeX, int maxSizeY, int pixelCount)
{
    ImageFrameLimits limits{
        kMaxSupportedDimension,
        kMaxSupportedPixels,
        kMaxSupportedPixels * kMaxBytesPerPixel
    };

    if (maxSizeX > 0 && maxSizeY > 0 && pixelCount > 0) {
        // Detector metadata is peer supplied, so it may tighten but never
        // enlarge the driver's support limits.
        limits.maxDimension = std::min(
            static_cast<std::size_t>(std::max(maxSizeX, maxSizeY)),
            kMaxSupportedDimension);
        limits.maxPixels = std::min(static_cast<std::size_t>(pixelCount),
                                    kMaxSupportedPixels);
        if (!checkedMultiply(limits.maxPixels, kMaxBytesPerPixel,
                             limits.maxPayloadBytes)) {
            limits.maxPayloadBytes = 0;
        }
    }
    return limits;
}

ImageHeaderError validateJsonImageHeader(const nlohmann::json& header,
                                         const ImageFrameLimits& limits,
                                         ImageFrameLayout& layout)
{
    layout = ImageFrameLayout{0, 0, PixelFormat::UInt16, 0, 0, 0};

    if (!header.contains("width") || !header.contains("height")) {
        return ImageHeaderError::MissingField;
    }
    if (!header["width"].is_number_integer() ||
        !header["height"].is_number_integer() ||
        (header.contains("pixelFormat") && !header["pixelFormat"].is_string())) {
        return ImageHeaderError::InvalidFieldType;
    }

    std::int64_t width = 0;
    std::int64_t height = 0;
    // Serval 4.1.6 omits pixelFormat for its normal uint16 jsonimage stream.
    // Preserve that wire-compatible default, but validate any explicit value.
    std::string pixelFormat = "uint16";
    try {
        width = header["width"].get<std::int64_t>();
        height = header["height"].get<std::int64_t>();
        if (header.contains("pixelFormat")) {
            pixelFormat = header["pixelFormat"].get<std::string>();
        }
    } catch (const nlohmann::json::exception&) {
        return ImageHeaderError::InvalidFieldType;
    }

    if (width <= 0 || height <= 0 ||
        static_cast<std::uint64_t>(width) > limits.maxDimension ||
        static_cast<std::uint64_t>(height) > limits.maxDimension ||
        width > std::numeric_limits<int>::max() ||
        height > std::numeric_limits<int>::max()) {
        return ImageHeaderError::InvalidDimension;
    }

    PixelFormat format;
    std::size_t bytesPerPixel = 0;
    if (pixelFormat == "uint16" || pixelFormat == "UINT16") {
        format = PixelFormat::UInt16;
        bytesPerPixel = sizeof(std::uint16_t);
    } else if (pixelFormat == "uint32" || pixelFormat == "UINT32") {
        format = PixelFormat::UInt32;
        bytesPerPixel = sizeof(std::uint32_t);
    } else {
        return ImageHeaderError::UnsupportedPixelFormat;
    }

    const std::size_t checkedWidth = static_cast<std::size_t>(width);
    const std::size_t checkedHeight = static_cast<std::size_t>(height);
    std::size_t pixelCount = 0;
    if (!checkedMultiply(checkedWidth, checkedHeight, pixelCount) ||
        pixelCount > limits.maxPixels) {
        return ImageHeaderError::PixelLimitExceeded;
    }

    std::size_t payloadBytes = 0;
    if (!checkedMultiply(pixelCount, bytesPerPixel, payloadBytes) ||
        payloadBytes > limits.maxPayloadBytes) {
        return ImageHeaderError::PayloadLimitExceeded;
    }

    layout.width = static_cast<int>(width);
    layout.height = static_cast<int>(height);
    layout.pixelFormat = format;
    layout.pixelCount = pixelCount;
    layout.bytesPerPixel = bytesPerPixel;
    layout.payloadBytes = payloadBytes;
    return ImageHeaderError::None;
}

const char* imageHeaderErrorMessage(ImageHeaderError error)
{
    switch (error) {
    case ImageHeaderError::None:
        return "valid image header";
    case ImageHeaderError::MissingField:
        return "missing width or height";
    case ImageHeaderError::InvalidFieldType:
        return "width, height, or pixelFormat has the wrong JSON type";
    case ImageHeaderError::InvalidDimension:
        return "image dimensions are non-positive or exceed detector limits";
    case ImageHeaderError::UnsupportedPixelFormat:
        return "unsupported pixelFormat";
    case ImageHeaderError::PixelLimitExceeded:
        return "image pixel count exceeds detector limits";
    case ImageHeaderError::PayloadLimitExceeded:
        return "image payload size exceeds detector limits";
    }
    return "unknown image header error";
}

const char* pixelFormatName(PixelFormat format)
{
    return format == PixelFormat::UInt32 ? "uint32" : "uint16";
}

}  // namespace ADTimePix3Stream
