/*
 * ADTimePix3 - TPX3 two-quad 4x2 BPC/image mapping
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_TPX3_DUAL_QUAD_MAPPING_H
#define ADTIMEPIX3_TPX3_DUAL_QUAD_MAPPING_H

namespace ADTimePix3Tpx3DualQuad {

/*
 * Map two adjacent TPX3 2x2 quads into a four-across, two-down image:
 *
 *   2 1 | 4 7
 *   3 0 | 5 6
 *
 * The right-hand quad is rotated 180 degrees relative to the left-hand quad.
 */
bool bpcToImage(int bpcIndex, int chipWidth, int& imageX, int& imageY);
bool imageToBpc(int imageX, int imageY, int chipWidth, int& bpcIndex);

}  // namespace ADTimePix3Tpx3DualQuad

#endif
