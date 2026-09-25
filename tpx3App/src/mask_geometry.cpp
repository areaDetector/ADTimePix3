/*
 * ADTimePix3 - checked image-mask drawing helpers
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "mask_geometry.h"

#include <algorithm>
#include <limits>

namespace ADTimePix3MaskGeometry {
namespace {

Status validate(std::int32_t* buffer, std::size_t elements,
                int width, int height, std::size_t& required)
{
    required = 0;
    if (!buffer || width <= 0 || height <= 0) {
        return Status::InvalidArgument;
    }
    const std::size_t widthSize = static_cast<std::size_t>(width);
    const std::size_t heightSize = static_cast<std::size_t>(height);
    if (widthSize > std::numeric_limits<std::size_t>::max() / heightSize) {
        return Status::InvalidArgument;
    }
    required = widthSize * heightSize;
    return elements < required ? Status::BufferTooSmall : Status::Ok;
}

void setMaskBit(std::int32_t& value, bool masked)
{
    if (masked) {
        value |= 1;
    } else {
        value &= ~1;
    }
}

}  // namespace

Status reset(std::int32_t* buffer, std::size_t elements,
             int width, int height, std::int32_t value)
{
    std::size_t required = 0;
    const Status status = validate(buffer, elements, width, height, required);
    if (status != Status::Ok) return status;
    std::fill(buffer, buffer + required, value);
    return Status::Ok;
}

Status rectangle(std::int32_t* buffer, std::size_t elements,
                 int width, int height, int x, int sizeX, int y, int sizeY,
                 bool masked)
{
    std::size_t required = 0;
    const Status status = validate(buffer, elements, width, height, required);
    if (status != Status::Ok) return status;
    if (sizeX < 0 || sizeY < 0) return Status::InvalidArgument;
    if (sizeX == 0 || sizeY == 0) return Status::Ok;

    const std::int64_t xBegin = std::max<std::int64_t>(0, x);
    const std::int64_t yBegin = std::max<std::int64_t>(0, y);
    const std::int64_t xEnd = std::min<std::int64_t>(
        width, static_cast<std::int64_t>(x) + sizeX);
    const std::int64_t yEnd = std::min<std::int64_t>(
        height, static_cast<std::int64_t>(y) + sizeY);

    for (std::int64_t imageY = yBegin; imageY < yEnd; ++imageY) {
        for (std::int64_t imageX = xBegin; imageX < xEnd; ++imageX) {
            const std::size_t index = static_cast<std::size_t>(imageY) *
                static_cast<std::size_t>(width) + static_cast<std::size_t>(imageX);
            setMaskBit(buffer[index], masked);
        }
    }
    return Status::Ok;
}

Status circle(std::int32_t* buffer, std::size_t elements,
              int width, int height, int centerX, int centerY, int radius,
              bool masked)
{
    std::size_t required = 0;
    const Status status = validate(buffer, elements, width, height, required);
    if (status != Status::Ok) return status;
    if (radius < 0) return Status::InvalidArgument;

    const std::int64_t xBegin = std::max<std::int64_t>(
        0, static_cast<std::int64_t>(centerX) - radius);
    const std::int64_t yBegin = std::max<std::int64_t>(
        0, static_cast<std::int64_t>(centerY) - radius);
    const std::int64_t xEnd = std::min<std::int64_t>(
        static_cast<std::int64_t>(width) - 1,
        static_cast<std::int64_t>(centerX) + radius);
    const std::int64_t yEnd = std::min<std::int64_t>(
        static_cast<std::int64_t>(height) - 1,
        static_cast<std::int64_t>(centerY) + radius);
    const long double radiusSquared =
        static_cast<long double>(radius) * static_cast<long double>(radius);

    for (std::int64_t imageY = yBegin; imageY <= yEnd; ++imageY) {
        for (std::int64_t imageX = xBegin; imageX <= xEnd; ++imageX) {
            const long double dx = static_cast<long double>(imageX) - centerX;
            const long double dy = static_cast<long double>(imageY) - centerY;
            if (dx * dx + dy * dy <= radiusSquared) {
                const std::size_t index = static_cast<std::size_t>(imageY) *
                    static_cast<std::size_t>(width) + static_cast<std::size_t>(imageX);
                setMaskBit(buffer[index], masked);
            }
        }
    }
    return Status::Ok;
}

const char* statusMessage(Status status)
{
    switch (status) {
    case Status::Ok:
        return "ok";
    case Status::InvalidArgument:
        return "invalid mask geometry or operation";
    case Status::BufferTooSmall:
        return "mask waveform is smaller than detector geometry";
    }
    return "unknown mask geometry error";
}

}  // namespace ADTimePix3MaskGeometry
