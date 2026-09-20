/*
 * ADTimePix3 - one-shot action parameter semantics
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ONE_SHOT_ACTION_H
#define ONE_SHOT_ACTION_H

namespace ADTimePix3Action {

struct OneShotDecision {
    bool execute;
    int storedValue;
};

constexpr OneShotDecision oneShotDecision(int requestedValue)
{
    return {requestedValue == 1, 0};
}

}  // namespace ADTimePix3Action

#endif
