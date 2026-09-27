/*
 * ADTimePix3 - numeric range and conversion contract
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX3_NUMERIC_RANGE_H
#define ADTIMEPIX3_NUMERIC_RANGE_H

#include <cstdint>

namespace ADTimePix3Numeric {

enum class RangeState {
    Ok = 0,
    OutputClamped = 1,
    AccumulatorSaturated = 2
};

std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right,
                            bool& saturated);
std::int64_t clampToInt64(std::uint64_t value, bool& clamped);
std::uint32_t clampAverageToUInt32(std::uint64_t sum, std::uint64_t divisor,
                                   bool& clamped);
std::int64_t preserveUInt64Bits(std::uint64_t value);
std::int32_t preserveUInt32Bits(std::uint32_t value);
RangeState promoteRangeState(RangeState current, RangeState reported);
const char* rangeStateName(RangeState state);

}  // namespace ADTimePix3Numeric

#endif  // ADTIMEPIX3_NUMERIC_RANGE_H
