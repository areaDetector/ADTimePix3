/*
 * ADTimePix3 - bounded BPC file I/O
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "bpc_file_io.h"

#include <fstream>
#include <limits>

namespace ADTimePix3BpcFile {
namespace {

bool checkedMultiply(std::size_t left, std::size_t right, std::size_t& product)
{
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    product = left * right;
    return true;
}

}  // namespace

bool expectedSize(int pixelCount, int bytesPerPixel, int thresholdSlices,
                  std::size_t& size)
{
    size = 0;
    if (pixelCount <= 0 || bytesPerPixel <= 0 || thresholdSlices <= 0) {
        return false;
    }

    std::size_t bytes = 0;
    if (!checkedMultiply(static_cast<std::size_t>(pixelCount),
                         static_cast<std::size_t>(bytesPerPixel), bytes) ||
        !checkedMultiply(bytes, static_cast<std::size_t>(thresholdSlices), size)) {
        size = 0;
        return false;
    }
    return true;
}

Status readExact(const std::string& path, std::size_t expectedSize,
                 std::vector<std::uint8_t>& data)
{
    data.clear();
    if (expectedSize == 0 ||
        expectedSize > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return Status::InvalidExpectedSize;
    }

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return Status::OpenFailed;
    }
    const std::streampos end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) != expectedSize) {
        return Status::SizeMismatch;
    }

    data.resize(expectedSize);
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char*>(data.data()),
               static_cast<std::streamsize>(expectedSize));
    if (!input || input.gcount() != static_cast<std::streamsize>(expectedSize)) {
        data.clear();
        return Status::ReadFailed;
    }
    return Status::Ok;
}

Status writeExact(const std::string& path, const std::vector<std::uint8_t>& data,
                  std::size_t expectedSize)
{
    if (expectedSize == 0 || data.size() != expectedSize ||
        expectedSize > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return Status::InvalidExpectedSize;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return Status::OpenFailed;
    }
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    output.close();
    return output ? Status::Ok : Status::WriteFailed;
}

const char* statusMessage(Status status)
{
    switch (status) {
    case Status::Ok:
        return "ok";
    case Status::InvalidExpectedSize:
        return "invalid expected BPC size";
    case Status::OpenFailed:
        return "unable to open BPC file";
    case Status::SizeMismatch:
        return "BPC file size does not match detector geometry";
    case Status::ReadFailed:
        return "incomplete BPC file read";
    case Status::WriteFailed:
        return "incomplete BPC file write";
    }
    return "unknown BPC file error";
}

}  // namespace ADTimePix3BpcFile
