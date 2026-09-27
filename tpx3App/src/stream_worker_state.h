/*
 * ADTimePix3 - synchronized TCP stream worker state
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_STREAM_WORKER_STATE_H
#define ADTIMEPIX_STREAM_WORKER_STATE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace ADTimePix3StreamWorker {

class State {
public:
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    bool start();
    void requestStop();
    void fail();
    void markConnected();
    void markDisconnected();

    bool running() const noexcept;
    bool connected() const noexcept;

    bool waitForStopFor(std::chrono::milliseconds timeout);
    bool waitForConnectedFor(std::chrono::milliseconds timeout);

private:
    enum class Phase { Stopped, Running, Connected };
    std::atomic<Phase> phase_{Phase::Stopped};
    std::mutex waitMutex_;
    std::condition_variable stateChanged_;
};

}  // namespace ADTimePix3StreamWorker

#endif
