/*
 * ADTimePix3 - TPX3 two-quad 4x2 BPC/image mapping
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "tpx3_dual_quad_mapping.h"

#include <cstdint>

namespace ADTimePix3Tpx3DualQuad {
namespace {

bool quadBpcToImage(int chip, int localX, int localY, int width,
                    int& imageX, int& imageY)
{
    switch (chip) {
    case 0:
        imageX = width + localX;
        imageY = 2 * width - 1 - localY;
        return true;
    case 1:
        imageX = 2 * width - 1 - localX;
        imageY = localY;
        return true;
    case 2:
        imageX = width - 1 - localX;
        imageY = localY;
        return true;
    case 3:
        imageX = localX;
        imageY = 2 * width - 1 - localY;
        return true;
    default:
        return false;
    }
}

bool quadImageToBpc(int imageX, int imageY, int width, int& localIndex)
{
    if (imageX < 0 || imageX >= 2 * width ||
        imageY < 0 || imageY >= 2 * width) {
        return false;
    }

    int chip = 0;
    int localX = 0;
    int localY = 0;
    if (imageX >= width && imageY >= width) {
        chip = 0;
        localX = imageX - width;
        localY = 2 * width - 1 - imageY;
    } else if (imageX >= width) {
        chip = 1;
        localX = 2 * width - 1 - imageX;
        localY = imageY;
    } else if (imageY < width) {
        chip = 2;
        localX = width - 1 - imageX;
        localY = imageY;
    } else {
        chip = 3;
        localX = imageX;
        localY = 2 * width - 1 - imageY;
    }
    localIndex = chip * width * width + localX + localY * width;
    return true;
}

}  // namespace

bool bpcToImage(int bpcIndex, int chipWidth, int& imageX, int& imageY)
{
    if (bpcIndex < 0 || chipWidth <= 0) return false;
    const std::int64_t chipPixels =
        static_cast<std::int64_t>(chipWidth) * chipWidth;
    const std::int64_t totalPixels = 8 * chipPixels;
    if (bpcIndex >= totalPixels) return false;

    const int chip = static_cast<int>(bpcIndex / chipPixels);
    const int local = static_cast<int>(bpcIndex % chipPixels);
    const int localX = local % chipWidth;
    const int localY = local / chipWidth;
    int quadX = 0;
    int quadY = 0;
    if (!quadBpcToImage(chip % 4, localX, localY,
                        chipWidth, quadX, quadY)) {
        return false;
    }

    if (chip < 4) {
        imageX = quadX;
        imageY = quadY;
    } else {
        imageX = 4 * chipWidth - 1 - quadX;
        imageY = 2 * chipWidth - 1 - quadY;
    }
    return true;
}

bool imageToBpc(int imageX, int imageY, int chipWidth, int& bpcIndex)
{
    if (chipWidth <= 0 || imageX < 0 || imageX >= 4 * chipWidth ||
        imageY < 0 || imageY >= 2 * chipWidth) {
        return false;
    }

    int quadX = imageX;
    int quadY = imageY;
    int chipOffset = 0;
    if (imageX >= 2 * chipWidth) {
        quadX = 4 * chipWidth - 1 - imageX;
        quadY = 2 * chipWidth - 1 - imageY;
        chipOffset = 4 * chipWidth * chipWidth;
    }

    int localIndex = 0;
    if (!quadImageToBpc(quadX, quadY, chipWidth, localIndex)) return false;
    bpcIndex = chipOffset + localIndex;
    return true;
}

}  // namespace ADTimePix3Tpx3DualQuad
