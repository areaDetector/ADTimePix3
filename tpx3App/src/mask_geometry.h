/*
 * ADTimePix3 - checked image-mask drawing helpers
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_MASK_GEOMETRY_H
#define ADTIMEPIX3_MASK_GEOMETRY_H

#include <cstddef>
#include <cstdint>

namespace ADTimePix3MaskGeometry {

enum class Status {
    Ok,
    InvalidArgument,
    BufferTooSmall
};

Status reset(std::int32_t* buffer, std::size_t elements,
             int width, int height, std::int32_t value);

Status rectangle(std::int32_t* buffer, std::size_t elements,
                 int width, int height, int x, int sizeX, int y, int sizeY,
                 bool masked);

Status circle(std::int32_t* buffer, std::size_t elements,
              int width, int height, int centerX, int centerY, int radius,
              bool masked);

const char* statusMessage(Status status);

}  // namespace ADTimePix3MaskGeometry

#endif
