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
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>

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

    return writeAtomic(path, data.data(), data.size());
}

Status writeAtomic(const std::string& path, const void* data, std::size_t size)
{
    if ((data == nullptr && size != 0) || path.empty()) {
        return Status::InvalidExpectedSize;
    }

    namespace fs = std::filesystem;
    const fs::path target(path);
    const fs::path parent = target.parent_path();
    if (parent.empty() || target.filename().empty()) return Status::OpenFailed;

    std::string temporary =
        (parent / (std::string(".") + target.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> temporaryName(temporary.begin(), temporary.end());
    temporaryName.push_back('\0');

    struct stat existing{};
    const mode_t outputMode = stat(path.c_str(), &existing) == 0
        ? static_cast<mode_t>(existing.st_mode & 0777)
        : static_cast<mode_t>(0640);

    const int descriptor = mkstemp(temporaryName.data());
    if (descriptor < 0) return Status::OpenFailed;
    if (fchmod(descriptor, outputMode) != 0) {
        close(descriptor);
        unlink(temporaryName.data());
        return Status::OpenFailed;
    }

    Status status = Status::Ok;
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t written = 0;
    while (written < size) {
        const ssize_t count = ::write(descriptor, bytes + written, size - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            status = Status::WriteFailed;
            break;
        }
        written += static_cast<std::size_t>(count);
    }
    if (status == Status::Ok && fsync(descriptor) != 0) status = Status::SyncFailed;
    if (close(descriptor) != 0 && status == Status::Ok) status = Status::WriteFailed;

    const std::string staged(temporaryName.data());
    if (status == Status::Ok && rename(staged.c_str(), path.c_str()) != 0) {
        status = Status::RenameFailed;
    }
    if (status != Status::Ok) unlink(staged.c_str());
    return status;
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
    case Status::SyncFailed:
        return "unable to synchronize staged file";
    case Status::RenameFailed:
        return "unable to atomically replace target file";
    }
    return "unknown BPC file error";
}

}  // namespace ADTimePix3BpcFile
