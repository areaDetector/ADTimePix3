/*
 * ADTimePix3 - transactional acquisition cleanup and state publication
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "serval_http.h"
#include "serval_measurement.h"

#include <chrono>
#include <string>

using std::string;

extern const char* driverName;

namespace {

constexpr std::chrono::milliseconds kStreamReadyTimeout(5000);
constexpr std::chrono::milliseconds kStreamCloseTimeout(2000);

bool isRemoteIdle(
    const ADTimePix3ServalMeasurement::StatusSnapshot& snapshot)
{
    return !snapshot.hasStatus || snapshot.status == "DA_IDLE" ||
           snapshot.status == "DA_STOPPED";
}

}  // namespace

void ADTimePix::publishAcquisitionPhase(
    ADTimePix3Acquisition::Phase phase, const std::string& message)
{
    using ADTimePix3Acquisition::Phase;

    switch (phase) {
    case Phase::Starting:
    case Phase::Running:
        setIntegerParam(ADAcquire, 1);
        setIntegerParam(ADStatus, ADStatusAcquire);
        break;
    case Phase::Stopping:
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(ADStatus, ADStatusAcquire);
        break;
    case Phase::Fault:
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(ADStatus, ADStatusError);
        break;
    case Phase::Idle:
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(ADStatus, ADStatusIdle);
        break;
    }

    if (!message.empty()) {
        setStringParam(ADStatusMessage, message.c_str());
    }
    callParamCallbacks();
}

bool ADTimePix::startRequiredStreamWorker(
    ADTimePix3StreamWorker::State& workerState,
    epicsThreadId& threadId,
    const char* threadName,
    EPICSTHREADFUNC entryPoint,
    const epicsThreadOpts& options,
    ADTimePix3Acquisition::Resource resource)
{
    if (!startStreamWorker(
            workerState, threadId, threadName, entryPoint, options)) {
        return false;
    }
    if (!acquisitionCoordinator_.noteResource(resource)) {
        return false;
    }
    return workerState.waitForConnectedFor(kStreamReadyTimeout);
}

void ADTimePix::waitForStreamWorkersToStop(
    std::chrono::milliseconds timeout)
{
    ADTimePix3StreamWorker::State* states[] = {
        &prvImgWorkerState_,
        &prvImg1WorkerState_,
        &imgWorkerState_,
        &prvHstWorkerState_};

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (ADTimePix3StreamWorker::State* state : states) {
        if (!state->running()) {
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now);
        (void)state->waitForStopFor(remaining);
    }
}

void ADTimePix::resetAcquisitionRuntimeState()
{
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgFirstFrameReceived_ = false;
        prvImgT1ReadyForDiff_ = false;
        prvImgT0OrphanForDiff_ = false;
        prvImgLastSeenFrameForPair_ = -1;
        prvImgLastDiffT0Frame_ = -1;
        prvImgAcquisitionRate_ = 0.0;
        prvImgRateSamples_.clear();
        setDoubleParam(ADTimePixPrvImgAcqRate, 0.0);
        epicsMutexUnlock(prvImgMutex_);
    }

    if (prvImg1Mutex_) {
        epicsMutexLock(prvImg1Mutex_);
        prvImg1FirstFrameReceived_ = false;
        prvImg1T1ReadyForDiff_ = false;
        prvImg1T0OrphanForDiff_ = false;
        prvImg1LastSeenFrameForPair_ = -1;
        prvImg1LastDiffT0Frame_ = -1;
        prvImg1AcquisitionRate_ = 0.0;
        prvImg1RateSamples_.clear();
        epicsMutexUnlock(prvImg1Mutex_);
    }

    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgFirstFrameReceived_ = false;
        imgAcquisitionRate_ = 0.0;
        imgRateSamples_.clear();
        setDoubleParam(ADTimePixImgAcqRate, 0.0);
        resetImgAccumulation();
        epicsMutexUnlock(imgMutex_);
    }

    if (prvHstMutex_) {
        epicsMutexLock(prvHstMutex_);
        prvHstFirstFrameReceived_ = false;
        prvHstAcquisitionRate_ = 0.0;
        prvHstRateSamples_.clear();
        setDoubleParam(ADTimePixPrvHstAcqRate, 0.0);
        epicsMutexUnlock(prvHstMutex_);
    }
}

asynStatus ADTimePix::stopAcquisition(
    bool fault, const std::string& reason)
{
    using ADTimePix3Acquisition::CleanupClaim;
    using ADTimePix3Acquisition::Phase;
    using ADTimePix3Acquisition::Resource;

    const CleanupClaim claim = acquisitionCoordinator_.beginStop(fault);
    if (!claim.owner) {
        const ADTimePix3Acquisition::Snapshot state =
            acquisitionCoordinator_.snapshot();
        if (state.phase == Phase::Idle) {
            publishAcquisitionPhase(Phase::Idle, "Acquisition stopped");
            return asynSuccess;
        }
        if (state.phase == Phase::Stopping) {
            return asynSuccess;
        }
        if (state.phase == Phase::Fault) {
            if (!reason.empty()) {
                publishAcquisitionPhase(Phase::Fault, reason);
            }
            return asynError;
        }
        return asynError;
    }

    publishAcquisitionPhase(
        Phase::Stopping,
        fault ? "Rolling back acquisition..." : "Stopping acquisition...");

    const epicsThreadId self = epicsThreadGetIdSelf();
    if (callbackThreadId != nullptr && callbackThreadId != self) {
        const epicsThreadId monitor = callbackThreadId;
        callbackThreadId = nullptr;
        epicsThreadMustJoin(monitor);
    } else if (callbackThreadId == self) {
        callbackThreadId = nullptr;
    }

    const bool hadRemote = claim.snapshot.has(Resource::RemoteMeasurement);
    bool stopRequestSucceeded = !hadRemote;
    cpr::Response stopResponse;
    if (hadRemote) {
        const string stopUrl = serverURL + "/measurement/stop";
        stopResponse = ADTimePix3ServalHttp::get(stopUrl);
        stopRequestSucceeded = (stopResponse.status_code == 200);
        if (!stopRequestSucceeded) {
            logHttpFailure(
                "transactional stop GET /measurement/stop", "GET", stopUrl,
                static_cast<long>(stopResponse.status_code), stopResponse.text);
        } else {
            waitForStreamWorkersToStop(kStreamCloseTimeout);
        }
    }

    stopAndJoinStreamWorkers();
    resetAcquisitionRuntimeState();

    bool remoteStopped = !hadRemote;
    bool measurementValid = false;
    ADTimePix3ServalMeasurement::StatusSnapshot measurementSnapshot;
    if (hadRemote) {
        const string measurementUrl = serverURL + "/measurement";
        const cpr::Response measurementResponse =
            ADTimePix3ServalHttp::get(measurementUrl);
        if (measurementResponse.status_code == 200) {
            const auto parseError =
                ADTimePix3ServalMeasurement::parseStatusResponse(
                    measurementResponse.text, measurementSnapshot);
            measurementValid =
                parseError ==
                ADTimePix3ServalMeasurement::StatusResponseError::None;
            if (measurementValid) {
                remoteStopped = isRemoteIdle(measurementSnapshot);
                publishMeasurementSnapshot(measurementSnapshot);
            } else {
                ERR_ARGS(
                    "transactional stop: invalid measurement response: %s",
                    ADTimePix3ServalMeasurement::statusResponseErrorMessage(
                        parseError));
            }
        } else {
            logHttpFailure(
                "transactional stop GET /measurement", "GET", measurementUrl,
                static_cast<long>(measurementResponse.status_code),
                measurementResponse.text);
        }
    }

    const Phase finalPhase =
        acquisitionCoordinator_.finishStop(remoteStopped, fault);
    if (finalPhase == Phase::Fault) {
        string message = reason.empty() ? "Acquisition cleanup failed" : reason;
        if (hadRemote && !remoteStopped) {
            message += "; Serval measurement state not confirmed stopped";
        }
        publishAcquisitionPhase(Phase::Fault, message);
        return asynError;
    }

    if (!measurementValid) {
        updateMeasurementStatusFromJson("DA_IDLE");
    }
    publishAcquisitionPhase(
        Phase::Idle,
        stopRequestSucceeded ? "Acquisition stopped"
                             : "Acquisition stopped (remote state reconciled)");
    FLOW("Stopping Image Acquisition");
    return asynSuccess;
}
