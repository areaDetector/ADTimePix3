/*
 * ADTimePix3 - bounded rolling sum helper
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ROLLING_WINDOW_SUM_H
#define ROLLING_WINDOW_SUM_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace ADTimePix3Accumulation {

enum class RollingSumStatus {
    Ok,
    InvalidArgument,
    SizeOverflow,
    BudgetTooSmall,
    AllocationFailure,
    FrameSizeMismatch,
    NumericOverflow
};

const char* rollingSumStatusName(RollingSumStatus status);

struct RollingSumConfiguration {
    RollingSumStatus status = RollingSumStatus::InvalidArgument;
    std::size_t requestedFrames = 0;
    std::size_t effectiveFrames = 0;
    std::size_t elementCount = 0;
    std::size_t budgetBytes = 0;
    std::size_t requiredBytes = 0;
    bool limited = false;
};

/**
 * Maintains the exact sum of the most recent N unsigned frames.
 *
 * Frames are normalized to uint32_t so one implementation can accept Img
 * uint16/uint32 pixels and PrvHst uint32 bins.  The configured byte budget
 * covers the retained frames and the uint64 rolling sum.  Updating the sum is
 * O(elementCount), independent of N.
 */
class RollingWindowSum {
public:
    RollingSumConfiguration configure(std::size_t elementCount,
                                      std::size_t requestedFrames,
                                      std::size_t budgetBytes);

    RollingSumStatus push(const std::uint16_t* values, std::size_t count);
    RollingSumStatus push(const std::uint32_t* values, std::size_t count);

    /** Clear accumulated frames and values while retaining the configuration. */
    void reset();

    /** Clear accumulated data and geometry. */
    void clear();

    const std::vector<std::uint64_t>& sum() const { return sum_; }
    std::size_t elementCount() const { return elementCount_; }
    std::size_t frameCount() const { return frames_.size(); }
    std::size_t requestedFrames() const { return requestedFrames_; }
    std::size_t effectiveFrames() const { return effectiveFrames_; }
    std::size_t budgetBytes() const { return budgetBytes_; }
    std::size_t memoryBytes() const;
    bool configured() const { return effectiveFrames_ > 0 && !sum_.empty(); }
    bool limited() const { return effectiveFrames_ < requestedFrames_; }

private:
    template <typename InputType>
    RollingSumStatus pushImpl(const InputType* values, std::size_t count);

    std::size_t elementCount_ = 0;
    std::size_t requestedFrames_ = 0;
    std::size_t effectiveFrames_ = 0;
    std::size_t budgetBytes_ = 0;
    std::deque<std::vector<std::uint32_t>> frames_;
    std::vector<std::uint64_t> sum_;
};

}  // namespace ADTimePix3Accumulation

#endif  // ROLLING_WINDOW_SUM_H
