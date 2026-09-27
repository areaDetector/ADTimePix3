/*
 * ADTimePix3 - synchronized TCP stream worker state
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "stream_worker_state.h"

namespace ADTimePix3StreamWorker {

bool State::start()
{
    bool started = false;
    {
        std::lock_guard<std::mutex> guard(waitMutex_);
        Phase expected = Phase::Stopped;
        started = phase_.compare_exchange_strong(
            expected, Phase::Running,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }
    if (started) {
        stateChanged_.notify_all();
    }
    return started;
}

void State::requestStop()
{
    {
        std::lock_guard<std::mutex> guard(waitMutex_);
        phase_.store(Phase::Stopped, std::memory_order_release);
    }
    stateChanged_.notify_all();
}

void State::fail()
{
    requestStop();
}

void State::markConnected()
{
    {
        std::lock_guard<std::mutex> guard(waitMutex_);
        Phase expected = Phase::Running;
        (void)phase_.compare_exchange_strong(
            expected, Phase::Connected,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }
    stateChanged_.notify_all();
}

void State::markDisconnected()
{
    {
        std::lock_guard<std::mutex> guard(waitMutex_);
        Phase expected = Phase::Connected;
        (void)phase_.compare_exchange_strong(
            expected, Phase::Running,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }
    stateChanged_.notify_all();
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
    return stateChanged_.wait_for(guard, timeout, [this]() { return !running(); });
}

bool State::waitForConnectedFor(std::chrono::milliseconds timeout)
{
    if (connected()) {
        return true;
    }
    std::unique_lock<std::mutex> guard(waitMutex_);
    (void)stateChanged_.wait_for(
        guard, timeout, [this]() { return connected() || !running(); });
    return connected();
}

}  // namespace ADTimePix3StreamWorker
