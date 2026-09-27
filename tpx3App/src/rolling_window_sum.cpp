/*
 * ADTimePix3 - bounded rolling sum helper
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "rolling_window_sum.h"

#include <algorithm>
#include <limits>
#include <new>

namespace ADTimePix3Accumulation {
namespace {

bool checkedMultiply(std::size_t left, std::size_t right, std::size_t& result)
{
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
}

}  // namespace

const char* rollingSumStatusName(RollingSumStatus status)
{
    switch (status) {
    case RollingSumStatus::Ok: return "ok";
    case RollingSumStatus::InvalidArgument: return "invalid argument";
    case RollingSumStatus::SizeOverflow: return "size overflow";
    case RollingSumStatus::BudgetTooSmall: return "budget too small";
    case RollingSumStatus::AllocationFailure: return "allocation failure";
    case RollingSumStatus::FrameSizeMismatch: return "frame size mismatch";
    case RollingSumStatus::NumericOverflow: return "numeric overflow";
    }
    return "unknown";
}

RollingSumConfiguration RollingWindowSum::configure(std::size_t elementCount,
                                                    std::size_t requestedFrames,
                                                    std::size_t budgetBytes)
{
    RollingSumConfiguration result;
    result.requestedFrames = requestedFrames;
    result.elementCount = elementCount;
    result.budgetBytes = budgetBytes;

    if (elementCount == 0 || requestedFrames == 0 || budgetBytes == 0) {
        clear();
        result.status = RollingSumStatus::InvalidArgument;
        return result;
    }

    std::size_t sumBytes = 0;
    std::size_t frameBytes = 0;
    if (!checkedMultiply(elementCount, sizeof(std::uint64_t), sumBytes) ||
        !checkedMultiply(elementCount, sizeof(std::uint32_t), frameBytes) ||
        sumBytes > std::numeric_limits<std::size_t>::max() - frameBytes) {
        clear();
        result.status = RollingSumStatus::SizeOverflow;
        return result;
    }

    if (budgetBytes < sumBytes + frameBytes) {
        clear();
        result.status = RollingSumStatus::BudgetTooSmall;
        result.requiredBytes = sumBytes + frameBytes;
        return result;
    }

    const std::size_t budgetedFrames = (budgetBytes - sumBytes) / frameBytes;
    const std::size_t effectiveFrames = std::min(requestedFrames, budgetedFrames);
    if (effectiveFrames == 0) {
        clear();
        result.status = RollingSumStatus::BudgetTooSmall;
        result.requiredBytes = sumBytes + frameBytes;
        return result;
    }

    std::size_t retainedBytes = 0;
    if (!checkedMultiply(effectiveFrames, frameBytes, retainedBytes) ||
        retainedBytes > std::numeric_limits<std::size_t>::max() - sumBytes) {
        clear();
        result.status = RollingSumStatus::SizeOverflow;
        return result;
    }
    result.requiredBytes = sumBytes + retainedBytes;

    try {
        if (elementCount_ != elementCount || sum_.size() != elementCount) {
            frames_.clear();
            std::vector<std::uint64_t> replacement(elementCount, 0);
            sum_.swap(replacement);
            elementCount_ = elementCount;
        }

        requestedFrames_ = requestedFrames;
        effectiveFrames_ = effectiveFrames;
        budgetBytes_ = budgetBytes;

        while (frames_.size() > effectiveFrames_) {
            const std::vector<std::uint32_t>& oldest = frames_.front();
            for (std::size_t index = 0; index < elementCount_; ++index) {
                sum_[index] -= oldest[index];
            }
            frames_.pop_front();
        }
    } catch (const std::bad_alloc&) {
        clear();
        result.status = RollingSumStatus::AllocationFailure;
        return result;
    }

    result.status = RollingSumStatus::Ok;
    result.effectiveFrames = effectiveFrames_;
    result.limited = effectiveFrames_ < requestedFrames_;
    return result;
}

template <typename InputType>
RollingSumStatus RollingWindowSum::pushImpl(const InputType* values, std::size_t count)
{
    if (!values || count != elementCount_ || !configured()) {
        return RollingSumStatus::FrameSizeMismatch;
    }

    std::vector<std::uint32_t> next;
    try {
        next.assign(values, values + count);
    } catch (const std::bad_alloc&) {
        return RollingSumStatus::AllocationFailure;
    }

    const bool evict = frames_.size() == effectiveFrames_;
    const std::vector<std::uint32_t>* oldest = evict ? &frames_.front() : nullptr;
    for (std::size_t index = 0; index < elementCount_; ++index) {
        const std::uint64_t base = sum_[index] - (oldest ? (*oldest)[index] : 0U);
        if (std::numeric_limits<std::uint64_t>::max() - base < next[index]) {
            return RollingSumStatus::NumericOverflow;
        }
    }

    // Insert before changing the sum so deque allocation failure leaves state intact.
    try {
        frames_.push_back(std::move(next));
    } catch (const std::bad_alloc&) {
        return RollingSumStatus::AllocationFailure;
    }

    const std::vector<std::uint32_t>& inserted = frames_.back();
    for (std::size_t index = 0; index < elementCount_; ++index) {
        const std::uint64_t base =
            sum_[index] - (evict ? frames_.front()[index] : 0U);
        sum_[index] = base + inserted[index];
    }
    if (evict) {
        frames_.pop_front();
    }
    return RollingSumStatus::Ok;
}

RollingSumStatus RollingWindowSum::push(const std::uint16_t* values, std::size_t count)
{
    return pushImpl(values, count);
}

RollingSumStatus RollingWindowSum::push(const std::uint32_t* values, std::size_t count)
{
    return pushImpl(values, count);
}

void RollingWindowSum::reset()
{
    frames_.clear();
    std::fill(sum_.begin(), sum_.end(), 0);
}

void RollingWindowSum::clear()
{
    frames_.clear();
    sum_.clear();
    elementCount_ = 0;
    requestedFrames_ = 0;
    effectiveFrames_ = 0;
    budgetBytes_ = 0;
}

std::size_t RollingWindowSum::memoryBytes() const
{
    std::size_t total = sum_.capacity() * sizeof(std::uint64_t);
    for (const std::vector<std::uint32_t>& frame : frames_) {
        total += frame.capacity() * sizeof(std::uint32_t);
    }
    return total;
}

}  // namespace ADTimePix3Accumulation
