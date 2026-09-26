/*
 * ADTimePix3 - synchronized TCP stream worker state
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "stream_worker_state.h"

namespace ADTimePix3StreamWorker {

bool State::start() noexcept
{
    Phase expected = Phase::Stopped;
    return phase_.compare_exchange_strong(
        expected, Phase::Running,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void State::requestStop() noexcept
{
    phase_.store(Phase::Stopped, std::memory_order_release);
    stopRequested_.notify_all();
}

void State::fail() noexcept
{
    requestStop();
}

void State::markConnected() noexcept
{
    Phase expected = Phase::Running;
    (void)phase_.compare_exchange_strong(
        expected, Phase::Connected,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

void State::markDisconnected() noexcept
{
    Phase expected = Phase::Connected;
    (void)phase_.compare_exchange_strong(
        expected, Phase::Running,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool State::running() const noexcept
{
    return phase_.load(std::memory_order_acquire) != Phase::Stopped;
}

bool State::connected() const noexcept
{
    return phase_.load(std::memory_order_acquire) == Phase::Connected;
}

bool State::waitForStopFor(std::chrono::milliseconds timeout)
{
    if (!running()) {
        return true;
    }
    std::unique_lock<std::mutex> guard(waitMutex_);
    return stopRequested_.wait_for(guard, timeout, [this]() { return !running(); });
}

}  // namespace ADTimePix3StreamWorker
