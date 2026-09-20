/*
 * ADTimePix3 - bounded BPC file I/O
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BPC_FILE_IO_H
#define BPC_FILE_IO_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ADTimePix3BpcFile {

enum class Status {
    Ok,
    InvalidExpectedSize,
    OpenFailed,
    SizeMismatch,
    ReadFailed,
    WriteFailed
};

bool expectedSize(int pixelCount, int bytesPerPixel, int thresholdSlices,
                  std::size_t& size);

Status readExact(const std::string& path, std::size_t expectedSize,
                 std::vector<std::uint8_t>& data);

Status writeExact(const std::string& path, const std::vector<std::uint8_t>& data,
                  std::size_t expectedSize);

const char* statusMessage(Status status);

}  // namespace ADTimePix3BpcFile

#endif
