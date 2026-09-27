/*
 * ADTimePix3 - Measurement acquire start/stop and status callback
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "network_client.h"
#include "serval_http.h"
#include "serval_measurement.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

#include <epicsThread.h>
#include <epicsTime.h>

#include <json.hpp>

using json = nlohmann::json;
using std::string;

extern const char* driverName;

static void timePixCallbackC(void* pPvt) {
    static_cast<ADTimePix*>(pPvt)->timePixCallback();
}

// -----------------------------------------------------------------------
// Acquisition Functions
// -----------------------------------------------------------------------

void ADTimePix::publishMeasurementSnapshot(
    const ADTimePix3ServalMeasurement::StatusSnapshot& snapshot) {
    if (snapshot.hasPixelEventRate)
        setIntegerParam(ADTimePixPelRate, snapshot.pixelEventRate);
    if (snapshot.hasTdc1EventRate)
        setIntegerParam(ADTimePixTdc1Rate, snapshot.tdc1EventRate);
    if (snapshot.hasTdc2EventRate)
        setIntegerParam(ADTimePixTdc2Rate, snapshot.tdc2EventRate);
    if (snapshot.hasStartDateTime)
        setInteger64Param(ADTimePixStartTime, snapshot.startDateTime);
    if (snapshot.hasElapsedTime)
        setDoubleParam(ADTimePixElapsedTime, snapshot.elapsedTime);
    if (snapshot.hasTimeLeft)
        setDoubleParam(ADTimePixTimeLeft, snapshot.timeLeft);
    if (snapshot.hasFrameCount)
        setIntegerParam(ADTimePixFrameCount, snapshot.frameCount);
    if (snapshot.hasDroppedFrames)
        setIntegerParam(ADTimePixDroppedFrames, snapshot.droppedFrames);
    if (snapshot.hasStatus)
        updateMeasurementStatusFromJson(snapshot.status);
}

namespace {

int pipelineStateIndex(const std::string& status) {
    if (status == "DA_STARTING") return 0;
    if (status == "DA_RECORDING") return 1;
    if (status == "DA_STOPPING") return 2;
    if (status == "DA_IDLE" || status == "DA_STOPPED") return 3;
    return -1;
}

const json* measurementStatusJson(const json& meas) {
    if (meas.contains("Info") && meas["Info"].is_object() && meas["Info"].contains("Status")) {
        return &meas["Info"]["Status"];
    }
    if (meas.contains("Status")) {
        return &meas["Status"];
    }
    return nullptr;
}

}  // namespace

void ADTimePix::updateMeasurementStatusFromJson(const json& statusVal) {
    if (statusVal.is_null()) {
        setStringParam(ADTimePixStatus, "");
        setIntegerParam(ADTimePixPipelineState, -1);
        return;
    }
    if (statusVal.is_string()) {
        const std::string status = statusVal.get<std::string>();
        setStringParam(ADTimePixStatus, status.c_str());
        setIntegerParam(ADTimePixPipelineState, pipelineStateIndex(status));
        return;
    }
    setStringParam(ADTimePixStatus, statusVal.dump().c_str());
    setIntegerParam(ADTimePixPipelineState, -1);
}

void ADTimePix::updateMeasurementFromDashboard(const json& dashboard_j) {
    if (!dashboard_j.is_object() || !dashboard_j.contains("Measurement") ||
        dashboard_j["Measurement"].is_null()) {
        // Dashboard clears Measurement when idle; do not overwrite last status (e.g. DA_IDLE after stop).
        return;
    }
    const json& meas = dashboard_j["Measurement"];
    if (const json* status = measurementStatusJson(meas)) {
        updateMeasurementStatusFromJson(*status);
    }
    // No Status in dashboard snapshot; keep last PipelineState (e.g. DA_IDLE after stop).
}


namespace {

std::string makeListenTcpPath(const std::string& host, int port) {
    return "tcp://listen@" + host + ":" + std::to_string(port);
}

int nextFreeTcpPort(const std::string& host, int startPort, const std::set<int>& reserved) {
    for (int port = startPort; port <= 65535; ++port) {
        if (reserved.count(port) != 0) {
            continue;
        }
        if (!NetworkClient::isTcpPortInUse(host, port)) {
            return port;
        }
    }
    return -1;
}

}  // namespace

bool ADTimePix::startStreamWorker(
    ADTimePix3StreamWorker::State& workerState,
    epicsThreadId& threadId,
    const char* threadName,
    EPICSTHREADFUNC entryPoint,
    const epicsThreadOpts& options)
{
    if (!streamLifecycleMutex_) {
        ERR("Stream lifecycle mutex is not initialized");
        return false;
    }

    epicsMutexLock(streamLifecycleMutex_);
    bool started = false;
    if (threadId == nullptr && workerState.start()) {
        threadId = epicsThreadCreateOpt(threadName, entryPoint, this, &options);
        started = (threadId != nullptr);
        if (!started) {
            workerState.fail();
        }
    }
    epicsMutexUnlock(streamLifecycleMutex_);
    return started;
}

void ADTimePix::stopAndJoinStreamWorkers()
{
    if (!streamLifecycleMutex_) {
        return;
    }

    epicsMutexLock(streamLifecycleMutex_);
    prvImgWorkerState_.requestStop();
    prvImg1WorkerState_.requestStop();
    imgWorkerState_.requestStop();
    prvHstWorkerState_.requestStop();

    const std::shared_ptr<NetworkClient> clients[] = {
        std::atomic_load(&prvImgNetworkClient_),
        std::atomic_load(&prvImg1NetworkClient_),
        std::atomic_load(&imgNetworkClient_),
        std::atomic_load(&prvHstNetworkClient_)};
    for (const std::shared_ptr<NetworkClient>& client : clients) {
        if (client) {
            client->interrupt();
        }
    }

    epicsThreadId* handles[] = {
        &prvImgWorkerThreadId_, &prvImg1WorkerThreadId_,
        &imgWorkerThreadId_, &prvHstWorkerThreadId_};
    const epicsThreadId self = epicsThreadGetIdSelf();
    for (epicsThreadId* handle : handles) {
        if (*handle != nullptr && *handle != self) {
            const epicsThreadId joinable = *handle;
            *handle = nullptr;
            epicsThreadMustJoin(joinable);
        }
    }

    prvImgDisconnect();
    prvImg1Disconnect();
    imgDisconnect();
    prvHstDisconnect();
    epicsMutexUnlock(streamLifecycleMutex_);
}

void ADTimePix::syncTcpStreamEndpoints() {
    int writePrvImg = 0;
    getIntegerParam(ADTimePixWritePrvImg, &writePrvImg);
    if (writePrvImg != 0) {
        (void)checkPrvImgPath();
    }

    int writePrvImg1 = 0;
    getIntegerParam(ADTimePixWritePrvImg1, &writePrvImg1);
    if (writePrvImg1 != 0) {
        (void)checkPrvImg1Path();
    }

    int writeImg = 0;
    getIntegerParam(ADTimePixWriteImg, &writeImg);
    if (writeImg != 0) {
        std::string imgPath;
        getStringParam(ADTimePixImgBase, imgPath);
        if (imgPath.find("tcp://") == 0) {
            (void)checkImgPath();
        }
    }
}

/**
 * Serval keeps preview TCP listeners bound after measurement/stop. Reassign only when
 * the configured port is already listening locally (stale Serval TcpSender).
 */
asynStatus ADTimePix::ensurePreviewTcpPortsFree(bool forceRotate) {
    struct PreviewTcpChannel {
        int writeParam;
        int baseParam;
    };

    const PreviewTcpChannel channels[] = {
        {ADTimePixWritePrvImg, ADTimePixPrvImgBase},
        {ADTimePixWritePrvImg1, ADTimePixPrvImg1Base},
        {ADTimePixWriteImg, ADTimePixImgBase},
        {ADTimePixWritePrvHst, ADTimePixPrvHstBase},
    };

    std::set<int> reservedPorts;
    bool changed = false;

    for (const PreviewTcpChannel& channel : channels) {
        int writeChannel = 0;
        getIntegerParam(channel.writeParam, &writeChannel);
        if (writeChannel == 0) {
            continue;
        }

        std::string path;
        getStringParam(channel.baseParam, path);
        if (path.find("tcp://") != 0) {
            continue;
        }

        std::string host;
        int port = 0;
        if (!parseTcpPath(path, host, port)) {
            continue;
        }

        reservedPorts.insert(port);

        const bool portBusy = NetworkClient::isTcpPortInUse(host, port);
        if (!forceRotate && !portBusy) {
            continue;
        }

        const int searchFrom = forceRotate ? (port + 1) : (portBusy ? port + 1 : port);
        const int candidate = nextFreeTcpPort(host, searchFrom, reservedPorts);
        if (candidate < 0) {
            ERR_ARGS("No free TCP port found for preview channel (starting at %d)", searchFrom);
            setStringParam(ADStatusMessage,
                             "Preview TCP ports in use; restart Serval or choose different preview ports");
            return asynError;
        }

        reservedPorts.insert(candidate);
        if (candidate == port) {
            continue;
        }

        const std::string newPath = makeListenTcpPath(host, candidate);
        setStringParam(channel.baseParam, newPath);
        LOG_ARGS("Preview TCP port %d -> %d (%s)", port, candidate,
                 forceRotate ? "forced rotate" : "port in use");
        changed = true;
    }

    syncTcpStreamEndpoints();

    if (!changed) {
        return asynSuccess;
    }

    callParamCallbacks();
    return fileWriter();
}


/*
 * Function that is used to initialize and connect to the device.
 * 
 * NOTE: Again, it is possible that for your camera, a different connection type is used (such as a product ID [int])
 * Make sure you use the same connection type as passed in the ADTimePixConfig function and in the constructor.
 * 
 * Acquire Start command. if this command was successful, image acquisition started.
 * 
 * @return: status  -> error if no device, camera values not set, or execute command fails. Otherwise, success
 */
asynStatus ADTimePix::acquireStart(){
    using ADTimePix3Acquisition::Phase;
    using ADTimePix3Acquisition::Resource;

    asynStatus status = asynSuccess;
    if (!acquisitionCoordinator_.beginStart()) {
        const auto state = acquisitionCoordinator_.snapshot();
        const std::string message =
            std::string("Acquisition start rejected while state is ") +
            ADTimePix3Acquisition::phaseName(state.phase);
        publishAcquisitionPhase(state.phase, message);
        return asynError;
    }
    publishAcquisitionPhase(Phase::Starting, "Starting acquisition...");

    const auto failStart = [this](const std::string& message) {
        (void)stopAcquisition(true, message);
        return asynError;
    };

    // Start from one known stream-worker generation. This requests stop,
    // interrupts any blocking receive, joins every old worker, and only then
    // releases its socket ownership.
    stopAndJoinStreamWorkers();

    int triggerMode = 0;
    getIntegerParam(ADTriggerMode, &triggerMode);
    if (mpx3BothCountersTriggerConflict(triggerMode)) {
        ERR_ARGS("%s", kMpx3BothCountersTriggerMsg);
        return failStart(kMpx3BothCountersTriggerMsg);
    }

    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;

    // Stop any prior measurement so Serval can re-bind preview TCP ports.
    // Serval may leave listeners open after stop; ensurePreviewTcpPortsFree() handles that.
    {
        string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
        cpr::Response stop_r = ADTimePix3ServalHttp::get(stopMeasurementURL);
        if (stop_r.status_code != 200 && stop_r.status_code != 404) {
            logHttpWarning("acquireStart stop prior measurement", "GET", stopMeasurementURL,
                           (long)stop_r.status_code, stop_r.text);
        }
    }

    // Check if measurement is already running and stop it first to free ports
    string measurementURL = this->serverURL + std::string("/measurement");
    cpr::Response r = ADTimePix3ServalHttp::get(measurementURL);
    
    if (r.status_code == 200 && !r.text.empty()) {
        ADTimePix3ServalMeasurement::StatusSnapshot snapshot;
        const auto responseError =
            ADTimePix3ServalMeasurement::parseStatusResponse(r.text, snapshot);
        if (responseError != ADTimePix3ServalMeasurement::StatusResponseError::None) {
            WARN_ARGS("Failed to validate measurement status: %s, continuing after prior stop",
                      ADTimePix3ServalMeasurement::statusResponseErrorMessage(responseError));
        } else if (snapshot.hasStatus && snapshot.status != "DA_IDLE" &&
                   snapshot.status != "DA_STOPPED") {
            LOG_ARGS("Measurement is running (status: %s), stopping it first",
                     snapshot.status.c_str());
            string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
            cpr::Response stop_r = ADTimePix3ServalHttp::get(stopMeasurementURL);
            if (stop_r.status_code != 200) {
                logHttpWarning("acquireStart stop prior measurement", "GET", stopMeasurementURL,
                               (long)stop_r.status_code, stop_r.text);
            }
        }
    }

    status = ensurePreviewTcpPortsFree();
    if (status != asynSuccess) {
        return failStart("Failed to reassign occupied preview TCP ports");
    }

    string startMeasurementURL = this->serverURL + std::string("/measurement/start");
    if (!acquisitionCoordinator_.noteResource(Resource::RemoteMeasurement)) {
        return failStart("Acquisition start was cancelled");
    }
    r = ADTimePix3ServalHttp::get(startMeasurementURL);

    if (r.status_code != 200 && r.text.find("Address already in use") != std::string::npos) {
        WARN("measurement/start failed with port conflict; rotating preview ports and retrying once");
        status = ensurePreviewTcpPortsFree(true);
        if (status == asynSuccess) {
            r = ADTimePix3ServalHttp::get(startMeasurementURL);
        }
    }

    if (r.status_code != 200){
        logHttpFailure("acquireStart GET /measurement/start", "GET", startMeasurementURL,
                       (long)r.status_code, r.text);
        const std::string failureMessage =
            r.text.find("Address already in use") != std::string::npos
                ? "Preview TCP port in use (Serval did not release a prior listener)"
                : "Failed to start acquisition";
        return failStart(failureMessage);
    }

    {
        int logHeaders = 0;
        getIntegerParam(ADTimePixPrvImgLogHeaders, &logHeaders);
        prvImgJsonHeadersRemaining_ = (logHeaders > 0) ? logHeaders : 0;
        prvImgFirstFrameReceived_ = false;
        prvImgT1ReadyForDiff_ = false;
        prvImgT0OrphanForDiff_ = false;
        prvImgLastSeenFrameForPair_ = -1;
        prvImgLastDiffT0Frame_ = -1;
        prvImg1JsonHeadersRemaining_ = (logHeaders > 0) ? logHeaders : 0;
        prvImg1FirstFrameReceived_ = false;
        prvImg1T1ReadyForDiff_ = false;
        prvImg1T0OrphanForDiff_ = false;
        prvImg1LastSeenFrameForPair_ = -1;
        prvImg1LastDiffT0Frame_ = -1;
        releasePreviewBandArrays();
    }

    // Path PV may have changed (port rotation or Phoebus WriteData); refresh before connect.
    syncTcpStreamEndpoints();
    
    // Every enabled TCP receiver is required: startup commits only after each
    // worker has connected within its bounded readiness deadline.
    int writePrvImg = 0;
    getIntegerParam(ADTimePixWritePrvImg, &writePrvImg);
    if (writePrvImg != 0) {
        std::string path;
        getStringParam(ADTimePixPrvImgBase, path);
        if (path.find("tcp://") == 0 &&
            !startRequiredStreamWorker(
                prvImgWorkerState_, prvImgWorkerThreadId_, "prvImgWorker",
                prvImgWorkerThreadC, opts, Resource::PreviewWorker)) {
            return failStart("Preview stream did not become ready");
        }
    }

    int writePrvImg1 = 0;
    getIntegerParam(ADTimePixWritePrvImg1, &writePrvImg1);
    if (writePrvImg1 != 0) {
        std::string path;
        getStringParam(ADTimePixPrvImg1Base, path);
        if (path.find("tcp://") == 0 &&
            !startRequiredStreamWorker(
                prvImg1WorkerState_, prvImg1WorkerThreadId_, "prvImg1Worker",
                prvImg1WorkerThreadC, opts,
                Resource::IntegratedPreviewWorker)) {
            return failStart("Integrated preview stream did not become ready");
        }
    }

    int writeImg = 0;
    getIntegerParam(ADTimePixWriteImg, &writeImg);
    if (writeImg != 0) {
        std::string path;
        int accumulationEnable = 0;
        getStringParam(ADTimePixImgBase, path);
        getIntegerParam(ADTimePixImgAccumulationEnable, &accumulationEnable);
        if (path.find("tcp://") == 0 && accumulationEnable != 0 &&
            !startRequiredStreamWorker(
                imgWorkerState_, imgWorkerThreadId_, "imgWorker",
                imgWorkerThreadC, opts, Resource::ImageWorker)) {
            return failStart("Image stream did not become ready");
        }
    }

    int writePrvHst = 0;
    if (!prvHstMutex_ || ADTimePixWritePrvHst < 0 ||
        getIntegerParam(ADTimePixWritePrvHst, &writePrvHst) != asynSuccess) {
        return failStart("Histogram stream parameters are unavailable");
    }
    if (writePrvHst != 0) {
        std::string path;
        int format = 0;
        int accumulationEnable = 0;
        if (getStringParam(ADTimePixPrvHstBase, path) != asynSuccess ||
            getIntegerParam(ADTimePixPrvHstFormat, &format) != asynSuccess ||
            getIntegerParam(ADTimePixPrvHstAccumulationEnable,
                            &accumulationEnable) != asynSuccess) {
            return failStart("Histogram stream configuration is unavailable");
        }
        if (path.find("tcp://") == 0 && format == 4 &&
            accumulationEnable != 0) {
            std::string host;
            int port = 0;
            if (!parseTcpPath(path, host, port)) {
                return failStart("Histogram TCP path is invalid");
            }
            epicsMutexLock(prvHstMutex_);
            prvHstHost_ = host;
            prvHstPort_ = port;
            prvHstFormat_ = format;
            epicsMutexUnlock(prvHstMutex_);

            epicsThreadOpts histogramOpts = EPICS_THREAD_OPTS_INIT;
            histogramOpts.priority = epicsThreadPriorityMedium;
            histogramOpts.stackSize =
                epicsThreadGetStackSize(epicsThreadStackMedium);
            histogramOpts.joinable = 1;
            if (!startRequiredStreamWorker(
                    prvHstWorkerState_, prvHstWorkerThreadId_,
                    "prvHstWorker", prvHstWorkerThreadC, histogramOpts,
                    Resource::HistogramWorker)) {
                return failStart("Histogram stream did not become ready");
            }
        }
    }

    callbackThreadId =
        epicsThreadCreateOpt("timePixCallback", timePixCallbackC, this, &opts);
    if (callbackThreadId == nullptr) {
        return failStart("Failed to create measurement monitor thread");
    }
    if (!acquisitionCoordinator_.noteResource(Resource::MonitorThread) ||
        !acquisitionCoordinator_.commitStart()) {
        return failStart("Acquisition start was cancelled before commit");
    }

    // Update status message on successful start
    if (status == asynSuccess) {
        publishAcquisitionPhase(Phase::Running, "Acquisition running");
    }
    
    return status;
}


void ADTimePix::timePixCallback(){

    if (!acquisitionCoordinator_.waitForRunningFor(
            std::chrono::milliseconds(5000))) {
        return;
    }


    int numImages;
    int imageCounter;
    int imagesAcquired;
    int mode;
    int frameCounter = 0;
    int new_frame_num = 0;
    bool isIdle = false;
    int writeChannel;

    // NDArray* pImage; // Not used with TCP streaming - worker thread handles image processing
    int arrayCallbacks;
    epicsTimeStamp startTime, endTime;
//    double elapsedTime;

    getIntegerParam(ADImageMode, &mode);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

    string measurement = this->serverURL + std::string("/measurement");   
    cpr::Url url = cpr::Url{measurement};
    // One Session for the whole callback: the inner "wait for new frame" loop issues many GETs
    // to the same /measurement URL. Reusing session.Get() avoids per-request cpr::Get(...) churn
    // (fresh CurlHolder each time) and reduces TCP connection/socket churn under tight polling.
    cpr::Session session;
    session.SetOption(url);
    session.SetOption(cpr::Timeout{ADTimePix3ServalHttp::kDefaultTimeoutMs});
    // Pre-size response buffer so repeated large JSON bodies do not reallocate every poll.
    cpr::ReserveSize reserveSize = cpr::ReserveSize{1024 * 1024 * 4};
    session.SetOption(reserveSize);
    //session.SetReserveSize(reserveSize);
    cpr::Authentication authentication = cpr::Authentication("user", "pass", cpr::AuthMode::BASIC);
    session.SetOption(authentication);
    cpr::Parameters parameters = cpr::Parameters{{"anon", "true"}, {"key", "value"}};
    session.SetOption(parameters);
    cpr::Response r = session.Get();

    if (r.status_code != 200) {
        logHttpFailure("timePixCallback GET /measurement", "GET", measurement, (long)r.status_code, r.text);
        (void)stopAcquisition(
            true, "Measurement HTTP error; acquisition rolled back");
        return;
    }
    ADTimePix3ServalMeasurement::StatusSnapshot measurementSnapshot;
    ADTimePix3ServalMeasurement::StatusResponseError statusError =
        ADTimePix3ServalMeasurement::parseStatusResponse(r.text, measurementSnapshot);
    if (statusError != ADTimePix3ServalMeasurement::StatusResponseError::None) {
        ERR_ARGS("timePixCallback: invalid measurement response: %s",
                 ADTimePix3ServalMeasurement::statusResponseErrorMessage(statusError));
        (void)stopAcquisition(
            true, "Invalid measurement response; acquisition rolled back");
        return;
    }

    publishMeasurementSnapshot(measurementSnapshot);
    if (measurementSnapshot.hasFrameCount) new_frame_num = measurementSnapshot.frameCount;
    callParamCallbacks();

    while(acquisitionCoordinator_.monitoring()){

        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &imageCounter);
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        epicsTimeGetCurrent(&startTime);

        // Wait for new frame
        while(frameCounter == new_frame_num){
            r = session.Get();  // same Session as initial GET; see setup above

            if (r.status_code != 200) {
                logHttpWarning("timePixCallback poll GET /measurement", "GET",
                               measurement, (long)r.status_code, r.text);
                (void)stopAcquisition(
                    true, "Measurement poll HTTP error; acquisition rolled back");
                return;
            }
            statusError = ADTimePix3ServalMeasurement::parseStatusResponse(
                r.text, measurementSnapshot);
            if (statusError != ADTimePix3ServalMeasurement::StatusResponseError::None) {
                ERR_ARGS("timePixCallback: invalid poll response: %s",
                         ADTimePix3ServalMeasurement::statusResponseErrorMessage(statusError));
                (void)stopAcquisition(
                    true, "Invalid measurement JSON; acquisition rolled back");
                return;
            }

            publishMeasurementSnapshot(measurementSnapshot);
            if (measurementSnapshot.hasFrameCount)
                new_frame_num = measurementSnapshot.frameCount;
            isIdle = measurementSnapshot.hasStatus &&
                     (measurementSnapshot.status == "DA_IDLE" ||
                      measurementSnapshot.status == "DA_STOPPED");
            callParamCallbacks();
            
            if (isIdle || !acquisitionCoordinator_.monitoring()) {
                break;
            }

            epicsTimeGetCurrent(&endTime);
            // elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);     // 0.0006->0.97 s
            // elapsedTime = r.elapsed;                                      // 0.00035 s
            // printf("Elapsed Time = %f\n", elapsedTime);

            epicsThreadSleep(0.01);
        //    epicsThreadSleep(0);
        }
        frameCounter = new_frame_num;

        getIntegerParam(ADTimePixWritePrvImg, &writeChannel);
        if (writeChannel != 0) {
            // Preview, ImageChannels[0]

            if(acquisitionCoordinator_.monitoring()){
                // Check if we're using TCP streaming
                std::string prvImgPath;
                getStringParam(ADTimePixPrvImgBase, prvImgPath);
                bool usingTcp = (prvImgPath.find("tcp://") == 0);
                
                if (usingTcp) {
                    // For TCP streaming, the worker thread handles everything
                    // Just ensure it's running - no need to process image here
                    readImageFromTCP(); // Worker thread handles image processing
                    // Worker thread will update pArrays[0] and trigger callbacks asynchronously
                    // We just update counters based on frame count from measurement endpoint
                    setIntegerParam(ADNumImagesCounter, frameCounter);
                callParamCallbacks();
                } else {
                    // Non-TCP path: TCP streaming is required for preview images
                    // GraphicsMagick HTTP method has been removed - use TCP streaming instead
                    WARN("PrvImg requires TCP streaming (tcp:// format). GraphicsMagick HTTP method no longer supported.");
                    // Worker thread handles TCP streaming, so just update counters
                     setIntegerParam(ADNumImagesCounter, frameCounter);
                     callParamCallbacks();
                }
            }
        }

        if (isIdle) {
            (void)acquireStop();
            return;
        }

    }
}



/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADTimePix::acquireStop(){
    return stopAcquisition(false, std::string());
}
