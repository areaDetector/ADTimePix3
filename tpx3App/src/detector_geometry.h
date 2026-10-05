/*
 * ADTimePix3 - validated detector raster and chip-grid geometry
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_DETECTOR_GEOMETRY_H
#define ADTIMEPIX3_DETECTOR_GEOMETRY_H

#include <string>

namespace ADTimePix3DetectorGeometry {

struct Geometry {
    int rows;
    int cols;
    int xChips;
    int yChips;
    int chipWidth;
};

enum class Status {
    Ok,
    InvalidArgument,
    PixelCountMismatch,
    NonSquareChip,
    NonIntegralChipGrid
};

/*
 * TPX3 Serval Info.RowLen is the number of chips across the detector and
 * Info.NumberOfRows is the assembled image height in pixels. Derive the
 * square-chip width and assembled raster, rejecting inconsistent metadata.
 */
Status derive(int pixelCount, int rowLength, int numberOfChips,
              int numberOfRows, Geometry& geometry);

/* The qualified eight-chip mask layout is two distinct TPX3 quad boards.
 * Single-board SpidrTurbo detectors use a different chip placement. */
bool isTwoQuadLayout(const std::string& firstChipboardId,
                     const std::string& secondChipboardId);

const char* statusMessage(Status status);

}  // namespace ADTimePix3DetectorGeometry

#endif
