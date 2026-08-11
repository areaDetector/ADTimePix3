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

void ADTimePix::updateTdcRatesFromMeasurementInfo(const json& info) {
    if (!info.is_object()) return;
    if (info.contains("Tdc1EventRate") && info["Tdc1EventRate"].is_number()) {
        setIntegerParam(ADTimePixTdc1Rate, info["Tdc1EventRate"].get<int>());
    }
    if (info.contains("Tdc2EventRate") && info["Tdc2EventRate"].is_number()) {
        setIntegerParam(ADTimePixTdc2Rate, info["Tdc2EventRate"].get<int>());
    }
    // Serval 3.0 / 3.1: single TdcEventRate (when split fields are absent)
    if (info.contains("TdcEventRate") && info["TdcEventRate"].is_number() && !info.contains("Tdc1EventRate")) {
        setIntegerParam(ADTimePixTdc1Rate, info["TdcEventRate"].get<int>());
    }
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
    asynStatus status = asynSuccess;

    // Ensure any existing PrvImg TCP connection is disconnected before starting new measurement
    // This prevents port conflicts
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
    }
    if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    prvImgDisconnect();

    if (prvImg1Mutex_) {
        epicsMutexLock(prvImg1Mutex_);
        prvImg1Running_ = false;
        epicsMutexUnlock(prvImg1Mutex_);
    }
    if (prvImg1WorkerThreadId_ != NULL && prvImg1WorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImg1WorkerThreadId_);
        prvImg1WorkerThreadId_ = NULL;
    }
    prvImg1Disconnect();
    
    // Ensure any existing Img TCP connection is disconnected before starting new measurement
    // This prevents port conflicts
    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        epicsMutexUnlock(imgMutex_);
    }
    if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    imgDisconnect();

    setIntegerParam(ADStatus, ADStatusAcquire);
    setStringParam(ADStatusMessage, "Starting acquisition...");

    int triggerMode = 0;
    getIntegerParam(ADTriggerMode, &triggerMode);
    if (mpx3BothCountersTriggerConflict(triggerMode)) {
        ERR_ARGS("%s", kMpx3BothCountersTriggerMsg);
        setStringParam(ADStatusMessage, kMpx3BothCountersTriggerMsg);
        setIntegerParam(ADStatus, ADStatusError);
        callParamCallbacks();
        return asynError;
    }

    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;

    // Stop any prior measurement so Serval can re-bind preview TCP ports.
    // Serval may leave listeners open after stop; ensurePreviewTcpPortsFree() handles that.
    {
        string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
        cpr::Response stop_r = ADTimePix3ServalHttp::get(stopMeasurementURL);
        if (stop_r.status_code == 200) {
            epicsThreadSleep(0.2);
        } else if (stop_r.status_code != 404) {
            logHttpWarning("acquireStart stop prior measurement", "GET", stopMeasurementURL,
                           (long)stop_r.status_code, stop_r.text);
        }
    }

    // Check if measurement is already running and stop it first to free ports
    string measurementURL = this->serverURL + std::string("/measurement");
    cpr::Response r = ADTimePix3ServalHttp::get(measurementURL);
    
    if (r.status_code == 200 && !r.text.empty()) {
        try {
            json measurement_j;
            try {
                measurement_j = json::parse(r.text.c_str());
            } catch (const json::parse_error& e) {
                WARN_ARGS("Failed to parse measurement JSON: %s, continuing anyway", e.what());
                // Continue without checking status
                measurement_j = json::object();
            }
            // Safely check if Info and Status exist and are not null
            if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
                if (measurement_j["Info"].contains("Status") && measurement_j["Info"]["Status"].is_string()) {
                    std::string status = measurement_j["Info"]["Status"].get<std::string>();
                    if (status != "DA_IDLE" && status != "DA_STOPPED") {
                        LOG_ARGS("Measurement is running (status: %s), stopping it first", status.c_str());
                        string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
                        cpr::Response stop_r = ADTimePix3ServalHttp::get(stopMeasurementURL);
                        if (stop_r.status_code == 200) {
                            epicsThreadSleep(0.2);
                        } else {
                            logHttpWarning("acquireStart stop prior measurement", "GET", stopMeasurementURL,
                                           (long)stop_r.status_code, stop_r.text);
                        }
                    }
                } else {
                    // Status field is missing or null, assume measurement is not running
                    LOG("Measurement status field is missing or null, assuming not running");
                }
            } else {
                // Info field is missing or not an object
                LOG("Measurement Info field is missing or invalid, assuming not running");
            }
        } catch (const json::parse_error& e) {
            WARN_ARGS("Failed to parse measurement JSON: %s, continuing anyway", e.what());
        } catch (const json::type_error& e) {
            WARN_ARGS("JSON type error when checking measurement status: %s, continuing anyway", e.what());
        } catch (const std::exception& e) {
            WARN_ARGS("Failed to check measurement status: %s, continuing anyway", e.what());
        }
    }

    status = ensurePreviewTcpPortsFree();
    if (status != asynSuccess) {
        setStringParam(ADStatusMessage, "Failed to reassign occupied preview TCP ports");
        setIntegerParam(ADStatus, ADStatusIdle);
        return asynError;
    }

    string startMeasurementURL = this->serverURL + std::string("/measurement/start");
    r = ADTimePix3ServalHttp::get(startMeasurementURL);

    if (r.status_code != 200 && r.text.find("Address already in use") != std::string::npos) {
        WARN("measurement/start failed with port conflict; rotating preview ports and retrying once");
        status = ensurePreviewTcpPortsFree(true);
        if (status == asynSuccess) {
            epicsThreadSleep(0.3);
            r = ADTimePix3ServalHttp::get(startMeasurementURL);
        }
    }

    if (r.status_code != 200){
        logHttpFailure("acquireStart GET /measurement/start", "GET", startMeasurementURL, (long)r.status_code, r.text);
        if (r.text.find("Address already in use") != std::string::npos) {
            setStringParam(ADStatusMessage,
                           "Preview TCP port in use (Serval did not release a prior listener). "
                           "Restart Serval or change preview ports, then WriteData=1.");
        } else {
            setStringParam(ADStatusMessage, "Failed to start acquisition");
        }
        // Ensure any partially started worker thread is stopped
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
        if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(prvImgWorkerThreadId_);
            prvImgWorkerThreadId_ = NULL;
        }
        prvImgDisconnect();

        if (prvImg1Mutex_) {
            epicsMutexLock(prvImg1Mutex_);
            prvImg1Running_ = false;
            epicsMutexUnlock(prvImg1Mutex_);
        }
        if (prvImg1WorkerThreadId_ != NULL && prvImg1WorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(prvImg1WorkerThreadId_);
            prvImg1WorkerThreadId_ = NULL;
        }
        prvImg1Disconnect();
        
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        epicsMutexUnlock(imgMutex_);
        if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(imgWorkerThreadId_);
            imgWorkerThreadId_ = NULL;
        }
        imgDisconnect();
        
        // Also stop PrvHst if it was started
        if (prvHstMutex_) {
            epicsMutexLock(prvHstMutex_);
            prvHstRunning_ = false;
            epicsMutexUnlock(prvHstMutex_);
        }
    // Signal PrvHst worker thread to stop FIRST (before trying to join)
    if (prvHstMutex_) {
        epicsMutexLock(prvHstMutex_);
        prvHstRunning_ = false;
        // Reset metadata tracking for next acquisition (but keep accumulated data)
        prvHstFirstFrameReceived_ = false;
        prvHstAcquisitionRate_ = 0.0;
        prvHstRateSamples_.clear();
        // Don't reset counters here - let user control via reset PV
        // Only reset rate tracking
        setDoubleParam(ADTimePixPrvHstAcqRate, 0.0);
        epicsMutexUnlock(prvHstMutex_);
    }
    
    // Wait for PrvHst worker thread to exit
    // The thread may have already exited when connection closed, so we need to handle that
    if (prvHstWorkerThreadId_ != NULL && prvHstWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadId threadId = prvHstWorkerThreadId_;
        prvHstWorkerThreadId_ = NULL;  // Clear pointer first to avoid double-join attempts
        
        // Give thread a moment to exit gracefully if it's still running
        epicsThreadSleep(0.2);
        
        // Try to join - if thread already exited, epicsThreadMustJoin will fail
        // We need to check if thread is still valid before joining
        if (threadId != NULL) {
            // epicsThreadMustJoin will handle the case where thread already exited
            // but it will call cantProceed if thread is not joinable
            // So we need to check if we can proceed first
            try {
                epicsThreadMustJoin(threadId);
            } catch (...) {
                // Thread already exited or not joinable - this is OK
                printf("PrvHst worker thread already exited or not joinable\n");
            }
        }
    }
        prvHstDisconnect();
        
        setIntegerParam(ADStatus, ADStatusIdle);
        return asynError;
    }

    this->callbackThreadId = epicsThreadCreateOpt("timePixCallback", timePixCallbackC, this, &opts);
    this->acquiring = true;

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
    
    // Start PrvImg TCP streaming worker thread if WritePrvImg is enabled and path is TCP
    // Wait a bit for Serval to bind to the port before trying to connect
    int writePrvImg;
    getIntegerParam(ADTimePixWritePrvImg, &writePrvImg);
    if (writePrvImg != 0) {
        std::string prvImgPath;
        getStringParam(ADTimePixPrvImgBase, prvImgPath);
        if (prvImgPath.find("tcp://") == 0) {
            // Give Serval time to bind to the TCP port (minimum: 200ms)
            epicsThreadSleep(0.2);  // 200ms - allows Serval to bind TCP port and start server
            
            epicsMutexLock(prvImgMutex_);
            if (!prvImgRunning_ && !prvImgWorkerThreadId_) {
                prvImgRunning_ = true;
                prvImgWorkerThreadId_ = epicsThreadCreateOpt("prvImgWorker", prvImgWorkerThreadC, this, &opts);
                if (!prvImgWorkerThreadId_) {
                    ERR("Failed to create PrvImg worker thread");
                    prvImgRunning_ = false;
                } else {
                    LOG("Started PrvImg TCP worker thread in acquireStart");
                }
            }
            epicsMutexUnlock(prvImgMutex_);
        }
    }

    // Start PrvImg1 TCP worker when WritePrvImg1 is enabled (integrated preview on 8089)
    int writePrvImg1;
    getIntegerParam(ADTimePixWritePrvImg1, &writePrvImg1);
    if (writePrvImg1 != 0) {
        std::string prvImg1Path;
        getStringParam(ADTimePixPrvImg1Base, prvImg1Path);
        if (prvImg1Path.find("tcp://") == 0) {
            epicsThreadSleep(0.2);

            epicsMutexLock(prvImg1Mutex_);
            if (!prvImg1Running_ && !prvImg1WorkerThreadId_) {
                prvImg1Running_ = true;
                prvImg1WorkerThreadId_ = epicsThreadCreateOpt("prvImg1Worker", prvImg1WorkerThreadC, this, &opts);
                if (!prvImg1WorkerThreadId_) {
                    ERR("Failed to create PrvImg1 worker thread");
                    prvImg1Running_ = false;
                } else {
                    LOG("Started PrvImg1 TCP worker thread in acquireStart");
                }
            }
            epicsMutexUnlock(prvImg1Mutex_);
        }
    }
    
    // Start Img TCP streaming worker thread if WriteImg is enabled, path is TCP, and accumulation is enabled
    // If accumulation is disabled, don't connect to TCP port so other clients can connect
    int writeImg;
    getIntegerParam(ADTimePixWriteImg, &writeImg);
    if (writeImg != 0) {
        std::string imgPath;
        getStringParam(ADTimePixImgBase, imgPath);
        if (imgPath.find("tcp://") == 0) {
            int accumulationEnable;
            getIntegerParam(ADTimePixImgAccumulationEnable, &accumulationEnable);
            if (accumulationEnable) {
                // Give Serval time to bind to the TCP port (minimum: 200ms)
                epicsThreadSleep(0.2);  // 200ms - allows Serval to bind TCP port and start server
                
                epicsMutexLock(imgMutex_);
                if (!imgRunning_ && !imgWorkerThreadId_) {
                    imgRunning_ = true;
                    imgWorkerThreadId_ = epicsThreadCreateOpt("imgWorker", imgWorkerThreadC, this, &opts);
                    if (!imgWorkerThreadId_) {
                        ERR("Failed to create Img worker thread");
                        imgRunning_ = false;
                    } else {
                        LOG("Started Img TCP worker thread in acquireStart");
                    }
                }
                epicsMutexUnlock(imgMutex_);
            } else {
                LOG("ImgAccumulationEnable is disabled - not connecting to TCP port (other clients can connect)");
            }
        }
    }
    
    // Start PrvHst TCP streaming if enabled, path is TCP, format is jsonhisto, and accumulation is enabled
    // If accumulation is disabled, don't connect to TCP port so other clients can connect
    // Skip PrvHst setup if mutex is not initialized (defensive check to prevent segfault)
    if (!prvHstMutex_) {
        // Mutex not initialized, skip PrvHst setup silently
        return status;
    }
    
    // Check if parameter indices are valid before using them (defensive check)
    if (ADTimePixWritePrvHst < 0) {
        // Parameter not initialized, skip PrvHst setup
        return status;
    }
    
    try {
        int writePrvHst = 0;
        asynStatus paramStatus = getIntegerParam(ADTimePixWritePrvHst, &writePrvHst);
        if (paramStatus != asynSuccess) {
            // Parameter might not exist or be accessible, skip PrvHst setup
            return status;
        }
        if (writePrvHst == 0) {
            // PrvHst not enabled: skip all PrvHst setup and messaging
        } else {
        // Use printf for initial logging to avoid potential issues with LOG_ARGS
        printf("PrvHst: Checking if TCP streaming should start - WritePrvHst=%d\n", writePrvHst);
        {
            if (ADTimePixPrvHstBase < 0 || ADTimePixPrvHstFormat < 0 || ADTimePixPrvHstAccumulationEnable < 0) {
                ERR("PrvHst parameters not initialized");
                return status;
            }
            
            std::string prvHstPath;
            paramStatus = getStringParam(ADTimePixPrvHstBase, prvHstPath);
            if (paramStatus != asynSuccess) {
                printf("PrvHst: Failed to get PrvHstBase parameter\n");
                return status;
            }
            printf("PrvHst: Path=%s\n", prvHstPath.c_str());
            
            if (prvHstPath.find("tcp://") == 0) {
                if (ADTimePixPrvHstFormat < 0) {
                    printf("PrvHst: PrvHstFormat parameter not initialized\n");
                    return status;
                }
                int format = 0;
                paramStatus = getIntegerParam(ADTimePixPrvHstFormat, &format);
                if (paramStatus != asynSuccess) {
                    printf("PrvHst: Failed to get PrvHstFormat parameter\n");
                    return status;
                }
                printf("PrvHst: Format=%d (4=jsonhisto)\n", format);
                if (format == 4) {  // jsonhisto format
                    if (ADTimePixPrvHstAccumulationEnable < 0) {
                        printf("PrvHst: PrvHstAccumulationEnable parameter not initialized\n");
                        return status;
                    }
                    int accumulationEnable = 0;
                    paramStatus = getIntegerParam(ADTimePixPrvHstAccumulationEnable, &accumulationEnable);
                    if (paramStatus != asynSuccess) {
                        printf("PrvHst: Failed to get PrvHstAccumulationEnable parameter\n");
                        return status;
                    }
                    printf("PrvHst: AccumulationEnable=%d\n", accumulationEnable);
                    if (accumulationEnable) {
                        // Parse TCP path
                        std::string host;
                        int port;
                        printf("PrvHst: Parsing TCP path: %s\n", prvHstPath.c_str());
                        if (parseTcpPath(prvHstPath, host, port)) {
                            printf("PrvHst: Parsed TCP path - host=%s, port=%d\n", host.c_str(), port);
                            if (!prvHstMutex_) {
                                printf("PrvHst: Mutex became null before lock\n");
                                return status;
                            }
                            epicsMutexLock(prvHstMutex_);
                            prvHstHost_ = host;
                            prvHstPort_ = port;
                            prvHstFormat_ = format;
                            epicsMutexUnlock(prvHstMutex_);
                            
                            // Give Serval time to bind to the TCP port
                            printf("PrvHst: Waiting 200ms for Serval to bind TCP port...\n");
                            epicsThreadSleep(0.2);  // 200ms
                            
                            epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
                            opts.priority = epicsThreadPriorityMedium;
                            opts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
                            opts.joinable = 1;  // Required: acquireStop uses epicsThreadMustJoin
                            
                            if (!prvHstMutex_) {
                                printf("PrvHst: Mutex became null before second lock\n");
                                return status;
                            }
                            epicsMutexLock(prvHstMutex_);
                            if (!prvHstRunning_ && !prvHstWorkerThreadId_) {
                                prvHstRunning_ = true;
                                prvHstWorkerThreadId_ = epicsThreadCreateOpt("prvHstWorker", prvHstWorkerThreadC, this, &opts);
                                if (!prvHstWorkerThreadId_) {
                                    printf("PrvHst: Failed to create worker thread\n");
                                    prvHstRunning_ = false;
                                } else {
                                    printf("PrvHst: Started TCP worker thread (host=%s, port=%d)\n", host.c_str(), port);
                                }
                            } else {
                                printf("PrvHst: Worker thread already running or exists (running=%d, threadId=%p)\n", 
                                       prvHstRunning_, prvHstWorkerThreadId_);
                            }
                            epicsMutexUnlock(prvHstMutex_);
                        } else {
                            printf("PrvHst: Failed to parse TCP path: %s\n", prvHstPath.c_str());
                        }
                    } else {
                        printf("PrvHst: AccumulationEnable is disabled - not connecting to TCP port\n");
                    }
                } else {
                    printf("PrvHst: Format is not jsonhisto (4), got %d - TCP streaming not started\n", format);
                }
            } else {
                printf("PrvHst: Path is not TCP (doesn't start with tcp://): %s\n", prvHstPath.c_str());
            }
        }
        }
    } catch (const std::exception& e) {
        printf("PrvHst: Exception in TCP streaming setup: %s\n", e.what());
    } catch (...) {
        printf("PrvHst: Unknown exception in TCP streaming setup\n");
    }
    
    // Update status message on successful start
    if (status == asynSuccess) {
        setStringParam(ADStatusMessage, "Acquisition running");
        callParamCallbacks();
    }
    
    return status;
}


void ADTimePix::timePixCallback(){


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
    // Pre-size response buffer so repeated large JSON bodies do not reallocate every poll.
    cpr::ReserveSize reserveSize = cpr::ReserveSize{1024 * 1024 * 4};
    session.SetOption(reserveSize);
    //session.SetReserveSize(reserveSize);
    cpr::Authentication authentication = cpr::Authentication("user", "pass", cpr::AuthMode::BASIC);
    session.SetOption(authentication);
    cpr::Parameters parameters = cpr::Parameters{{"anon", "true"}, {"key", "value"}};
    session.SetOption(parameters);
    cpr::Response r = session.Get();

    json measurement_j = json::object();
    if (r.status_code != 200) {
        logHttpFailure("timePixCallback GET /measurement", "GET", measurement, (long)r.status_code, r.text);
        setStringParam(ADStatusMessage, "Measurement HTTP error; acquisition stopped");
        setIntegerParam(ADStatus, ADStatusIdle);
        this->acquiring = false;
        callParamCallbacks();
        return;
    }
    try {
        measurement_j = json::parse(r.text.c_str());
    } catch (const std::exception& e) {
        ERR_ARGS("timePixCallback: measurement JSON parse failed: %s", e.what());
        setStringParam(ADStatusMessage, "Invalid measurement JSON; acquisition stopped");
        setIntegerParam(ADStatus, ADStatusIdle);
        this->acquiring = false;
        callParamCallbacks();
        return;
    }

    // Safely extract measurement info with null checks
    if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
        if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
            setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
        }

        updateTdcRatesFromMeasurementInfo(measurement_j["Info"]);

        if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
            setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
        }
        if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
            setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
        }
        if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
            setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
        }
        if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
            setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
        }
        if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
            setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
        }
        if (measurement_j["Info"].contains("Status")) {
            // Status might be null, so use dump() which handles null safely
            setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
        }
    }   
    callParamCallbacks();

    while(this->acquiring){

        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &imageCounter);
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        epicsTimeGetCurrent(&startTime);

        // Wait for new frame
        while(frameCounter == new_frame_num){
            r = session.Get();  // same Session as initial GET; see setup above

            if (r.status_code != 200) {
                logHttpWarning("timePixCallback poll GET /measurement", "GET", measurement, (long)r.status_code,
                               r.text);
                break;
            }
            try {
                measurement_j = json::parse(r.text.c_str());
            } catch (const std::exception& e) {
                ERR_ARGS("timePixCallback: poll JSON parse failed: %s", e.what());
                this->acquiring = false;
                break;
            }

            // Safely extract measurement info with null checks
            if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
                if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
                    setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
                }

                updateTdcRatesFromMeasurementInfo(measurement_j["Info"]);

                if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
                    setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
                }
                if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
                    setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
                }
                if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
                    setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
                }
                if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
                    setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
            new_frame_num = measurement_j["Info"]["FrameCount"].get<int>();
                }
                if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
                    setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
                }
                if (measurement_j["Info"].contains("Status")) {
                    // Status might be null, so use dump() which handles null safely
                    setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
                    // Check if status is "DA_IDLE" (only if it's a string)
                    if (measurement_j["Info"]["Status"].is_string() && 
                        measurement_j["Info"]["Status"].get<std::string>() == "DA_IDLE") {
                isIdle = true;
                    }
                }
            }
            callParamCallbacks();
            
            if (isIdle || this->acquiring == false) {
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

            if(this->acquiring){
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

        if (isIdle)  acquireStop();

    }
}



/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADTimePix::acquireStop(){
    asynStatus status = asynSuccess;

    this->acquiring=false;
    
    // Stop callback thread first
    if(this->callbackThreadId != NULL && this->callbackThreadId != epicsThreadGetIdSelf())
        epicsThreadMustJoin(this->callbackThreadId);

    this->callbackThreadId = NULL;

    // Stop TCP worker threads and disconnect before telling Serval to stop.
    // Serval TcpSender threads block on full preview buffers when no client reads
    // (e.g. PrvImg1); closing the IOC side first avoids prolonged waitForClose hangs.
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
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
        prvImg1Running_ = false;
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
        imgRunning_ = false;
        imgFirstFrameReceived_ = false;
        imgAcquisitionRate_ = 0.0;
        imgRateSamples_.clear();
        setDoubleParam(ADTimePixImgAcqRate, 0.0);
        resetImgAccumulation();
        epicsMutexUnlock(imgMutex_);
    }

    if (prvHstMutex_) {
        epicsMutexLock(prvHstMutex_);
        prvHstRunning_ = false;
        prvHstFirstFrameReceived_ = false;
        prvHstAcquisitionRate_ = 0.0;
        prvHstRateSamples_.clear();
        setDoubleParam(ADTimePixPrvHstAcqRate, 0.0);
        epicsMutexUnlock(prvHstMutex_);
    }

    if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }

    if (prvImg1WorkerThreadId_ != NULL && prvImg1WorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImg1WorkerThreadId_);
        prvImg1WorkerThreadId_ = NULL;
    }

    if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }

    if (prvHstWorkerThreadId_ != NULL && prvHstWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadId prvHstThreadId = prvHstWorkerThreadId_;
        prvHstWorkerThreadId_ = NULL;
        epicsThreadSleep(0.1);
        epicsThreadMustJoin(prvHstThreadId);
    }

    prvImgDisconnect();
    prvImg1Disconnect();
    imgDisconnect();
    prvHstDisconnect();

    string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
    cpr::Response r = ADTimePix3ServalHttp::get(stopMeasurementURL);

    if (r.status_code != 200){
        logHttpFailure("acquireStop GET /measurement/stop", "GET", stopMeasurementURL, (long)r.status_code, r.text);
        setStringParam(ADStatusMessage, "Failed to stop acquisition");
        setIntegerParam(ADStatus, ADStatusError);
        return asynError;
    }

    // Allow Serval TcpSender threads to finish closing (may take >300ms when buffers were full)
    epicsThreadSleep(0.5);

    setIntegerParam(ADStatus, ADStatusIdle);
    setStringParam(ADStatusMessage, "Acquisition stopped");
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    FLOW("Stopping Image Acquisition");

    // Update end measurement values
    string measurementURL = this->serverURL + std::string("/measurement");
    r = ADTimePix3ServalHttp::get(measurementURL);

    if (r.status_code != 200){
        logHttpFailure("acquireStop GET /measurement (post-stop)", "GET", measurementURL, (long)r.status_code,
                       r.text);
        return asynError;
    }

    json measurement_j = json::parse(r.text.c_str());

    // Safely extract measurement info with null checks
    if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
        if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
            setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
        }

        updateTdcRatesFromMeasurementInfo(measurement_j["Info"]);

        if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
            setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
        }
        if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
            setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
        }
        if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
            setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
        }
        if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
            setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
        }
        if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
            setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
        }
        if (measurement_j["Info"].contains("Status")) {
            setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
        }
    }
    callParamCallbacks();

    return status;
}
