/*
 * ADTimePix3 - transactional acquisition lifecycle coordinator
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_ACQUISITION_COORDINATOR_H
#define ADTIMEPIX_ACQUISITION_COORDINATOR_H

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace ADTimePix3Acquisition {

enum class Phase {
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

enum class Resource : std::uint32_t {
    RemoteMeasurement = 1u << 0,
    MonitorThread = 1u << 1,
    PreviewWorker = 1u << 2,
    IntegratedPreviewWorker = 1u << 3,
    ImageWorker = 1u << 4,
    HistogramWorker = 1u << 5
};

struct Snapshot {
    Phase phase = Phase::Idle;
    std::uint32_t resources = 0;

    bool has(Resource resource) const noexcept;
};

struct CleanupClaim {
    bool owner = false;
    bool fault = false;
    Snapshot snapshot;
};

/**
 * Owns the legal acquisition transitions and the resources acquired by a
 * start attempt. The caller that receives an owning CleanupClaim is the only
 * caller permitted to tear resources down; concurrent stop/fault requests are
 * therefore idempotent.
 */
class Coordinator {
public:
    Coordinator() = default;
    Coordinator(const Coordinator&) = delete;
    Coordinator& operator=(const Coordinator&) = delete;

    bool beginStart();
    bool noteResource(Resource resource);
    bool commitStart();

    CleanupClaim beginStop(bool fault);
    Phase finishStop(bool remoteStopped, bool fault);

    Snapshot snapshot() const;
    bool monitoring() const;
    bool requestedAcquire() const;

    bool waitForRunningFor(std::chrono::milliseconds timeout);

private:
    static std::uint32_t mask(Resource resource) noexcept;

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    Phase phase_ = Phase::Idle;
    std::uint32_t resources_ = 0;
    bool faultPending_ = false;
};

const char* phaseName(Phase phase) noexcept;
const char* connectedStatusMessage(const Snapshot& snapshot) noexcept;

}  // namespace ADTimePix3Acquisition

#endif
