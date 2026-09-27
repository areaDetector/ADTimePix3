/*
 * ADTimePix3 - numeric range and conversion contract
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "numeric_range.h"

#include <cstring>
#include <limits>

namespace ADTimePix3Numeric {

std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right,
                            bool& saturated)
{
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (right > maximum - left) {
        saturated = true;
        return maximum;
    }
    saturated = false;
    return left + right;
}

std::int64_t clampToInt64(std::uint64_t value, bool& clamped)
{
    const std::uint64_t maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    clamped = value > maximum;
    return clamped ? std::numeric_limits<std::int64_t>::max()
                   : static_cast<std::int64_t>(value);
}

std::uint32_t clampAverageToUInt32(std::uint64_t sum, std::uint64_t divisor,
                                   bool& clamped)
{
    if (divisor == 0) {
        clamped = false;
        return 0;
    }
    const std::uint64_t average = sum / divisor;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
    clamped = average > maximum;
    return clamped ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(average);
}

std::int64_t preserveUInt64Bits(std::uint64_t value)
{
    static_assert(sizeof(std::int64_t) == sizeof(std::uint64_t),
                  "64-bit callback types must have equal width");
    std::int64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::int32_t preserveUInt32Bits(std::uint32_t value)
{
    static_assert(sizeof(std::int32_t) == sizeof(std::uint32_t),
                  "32-bit callback types must have equal width");
    std::int32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

RangeState promoteRangeState(RangeState current, RangeState reported)
{
    if (reported == RangeState::Ok) return RangeState::Ok;
    return static_cast<int>(reported) > static_cast<int>(current)
        ? reported : current;
}

const char* rangeStateName(RangeState state)
{
    switch (state) {
    case RangeState::Ok:
        return "OK";
    case RangeState::OutputClamped:
        return "Published output clamped";
    case RangeState::AccumulatorSaturated:
        return "Accumulator saturated";
    }
    return "Unknown range state";
}

}  // namespace ADTimePix3Numeric
