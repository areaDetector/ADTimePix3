/*
 * ADTimePix3 deterministic test support
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_TEST_BOUNDED_WAIT_H
#define ADTIMEPIX_TEST_BOUNDED_WAIT_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace adtimepix_test {

template <typename Predicate>
bool boundedWait(std::condition_variable& condition,
                 std::unique_lock<std::mutex>& lock,
                 std::chrono::milliseconds timeout,
                 Predicate predicate)
{
    return condition.wait_for(lock, timeout, predicate);
}

}  // namespace adtimepix_test

#endif
