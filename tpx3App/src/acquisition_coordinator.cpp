/*
 * ADTimePix3 - transactional acquisition lifecycle coordinator
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "acquisition_coordinator.h"

namespace ADTimePix3Acquisition {

std::uint32_t Coordinator::mask(Resource resource) noexcept
{
    return static_cast<std::uint32_t>(resource);
}

bool Snapshot::has(Resource resource) const noexcept
{
    return (resources & static_cast<std::uint32_t>(resource)) != 0;
}

bool Coordinator::beginStart()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if ((phase_ != Phase::Idle && phase_ != Phase::Fault) || resources_ != 0) {
        return false;
    }
    phase_ = Phase::Starting;
    faultPending_ = false;
    changed_.notify_all();
    return true;
}

bool Coordinator::noteResource(Resource resource)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (phase_ != Phase::Starting) {
        return false;
    }
    resources_ |= mask(resource);
    return true;
}

bool Coordinator::commitStart()
{
    std::lock_guard<std::mutex> guard(mutex_);
    const std::uint32_t required =
        mask(Resource::RemoteMeasurement) | mask(Resource::MonitorThread);
    if (phase_ != Phase::Starting || (resources_ & required) != required) {
        return false;
    }
    phase_ = Phase::Running;
    changed_.notify_all();
    return true;
}

CleanupClaim Coordinator::beginStop(bool fault)
{
    std::lock_guard<std::mutex> guard(mutex_);
    faultPending_ = faultPending_ || fault;

    CleanupClaim claim;
    claim.fault = faultPending_;
    claim.snapshot = {phase_, resources_};

    if (phase_ == Phase::Stopping) {
        return claim;
    }
    if ((phase_ == Phase::Idle || phase_ == Phase::Fault) && resources_ == 0) {
        if (faultPending_) {
            phase_ = Phase::Fault;
            changed_.notify_all();
        }
        return claim;
    }

    phase_ = Phase::Stopping;
    claim.owner = true;
    claim.snapshot.phase = phase_;
    changed_.notify_all();
    return claim;
}

Phase Coordinator::finishStop(bool remoteStopped, bool fault)
{
    std::lock_guard<std::mutex> guard(mutex_);
    resources_ &= mask(Resource::RemoteMeasurement);
    if (remoteStopped) {
        resources_ = 0;
    }
    const bool finalFault = fault || faultPending_ || resources_ != 0;
    phase_ = finalFault ? Phase::Fault : Phase::Idle;
    faultPending_ = false;
    changed_.notify_all();
    return phase_;
}

Snapshot Coordinator::snapshot() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return {phase_, resources_};
}

bool Coordinator::monitoring() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return phase_ == Phase::Starting || phase_ == Phase::Running;
}

bool Coordinator::requestedAcquire() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    return phase_ == Phase::Starting || phase_ == Phase::Running;
}

bool Coordinator::waitForRunningFor(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> guard(mutex_);
    const bool stable = changed_.wait_for(guard, timeout, [this]() {
        return phase_ != Phase::Starting;
    });
    return stable && phase_ == Phase::Running;
}

const char* phaseName(Phase phase) noexcept
{
    switch (phase) {
    case Phase::Idle:
        return "Idle";
    case Phase::Starting:
        return "Starting";
    case Phase::Running:
        return "Running";
    case Phase::Stopping:
        return "Stopping";
    case Phase::Fault:
        return "Fault";
    }
    return "Unknown";
}

const char* connectedStatusMessage(const Snapshot& snapshot) noexcept
{
    switch (snapshot.phase) {
    case Phase::Idle:
        return "OK";
    case Phase::Starting:
        return "Starting acquisition...";
    case Phase::Running:
        return "Acquisition running";
    case Phase::Stopping:
        return "Stopping acquisition...";
    case Phase::Fault:
        return snapshot.resources != 0
                   ? "Acquisition fault; write Acquire=0 to retry cleanup"
                   : "Acquisition fault; retry acquisition";
    }
    return "Unknown acquisition state";
}

}  // namespace ADTimePix3Acquisition
