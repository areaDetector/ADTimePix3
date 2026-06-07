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
#include "serval_http.h"

#include <cstdio>
#include <cstring>
#include <iostream>
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

    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;

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
                            // Wait for Serval to release ports (minimum: 100ms)
                            epicsThreadSleep(0.1);  // 100ms - allows port release
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

    string startMeasurementURL = this->serverURL + std::string("/measurement/start");
    r = ADTimePix3ServalHttp::get(startMeasurementURL);

    if (r.status_code != 200){
        logHttpFailure("acquireStart GET /measurement/start", "GET", startMeasurementURL, (long)r.status_code, r.text);
        setStringParam(ADStatusMessage, "Failed to start acquisition");
        // Ensure any partially started worker thread is stopped
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
        if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(prvImgWorkerThreadId_);
            prvImgWorkerThreadId_ = NULL;
        }
        prvImgDisconnect();
        
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
        
        return asynError;
    }

    this->callbackThreadId = epicsThreadCreateOpt("timePixCallback", timePixCallbackC, this, &opts);
    this->acquiring = true;
    
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
    asynStatus status;

    this->acquiring=false;
    
    // Stop callback thread first
    if(this->callbackThreadId != NULL && this->callbackThreadId != epicsThreadGetIdSelf())
        epicsThreadMustJoin(this->callbackThreadId);

    this->callbackThreadId = NULL;

    // Stop Serval measurement FIRST to tell Serval to stop sending new data
    // This MUST happen before we signal worker threads to exit, otherwise
    // worker threads will close the socket while Serval is still trying to write
    string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
    cpr::Response r = ADTimePix3ServalHttp::get(stopMeasurementURL);

    if (r.status_code != 200){
        logHttpFailure("acquireStop GET /measurement/stop", "GET", stopMeasurementURL, (long)r.status_code, r.text);
        setStringParam(ADStatusMessage, "Failed to stop acquisition");
        setIntegerParam(ADStatus, ADStatusError);
        return asynError;
    }

    // Wait for Serval to process the stop command and stop its TcpSender threads
    // Serval needs time to:
    // 1. Process the stop command
    // 2. Signal its TcpSender threads to stop
    // 3. Allow TcpSender threads to finish sending any buffered data
    // 4. Close Serval's side of the socket gracefully
    // Delay to prevent "Broken pipe" errors (minimum: 300ms for reliable operation)
    epicsThreadSleep(0.3);  // 300ms - allows Serval TcpSender threads to stop cleanly
    
    // NOW signal ALL worker threads to stop (after Serval has stopped sending)
    // Signal all channels at once to ensure clean shutdown of all TcpSender threads
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        // Reset metadata tracking for next acquisition
        prvImgFirstFrameReceived_ = false;
        prvImgAcquisitionRate_ = 0.0;
        prvImgRateSamples_.clear();
        setDoubleParam(ADTimePixPrvImgAcqRate, 0.0);
        epicsMutexUnlock(prvImgMutex_);
    }
    
    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        // Reset metadata tracking for next acquisition
        imgFirstFrameReceived_ = false;
        imgAcquisitionRate_ = 0.0;
        imgRateSamples_.clear();
        setDoubleParam(ADTimePixImgAcqRate, 0.0);
        // Reset accumulation data
        resetImgAccumulation();
        epicsMutexUnlock(imgMutex_);
    }
    
    // Signal PrvHst to stop at the same time as other channels
    if (prvHstMutex_) {
        epicsMutexLock(prvHstMutex_);
        prvHstRunning_ = false;
        // Reset metadata tracking for next acquisition
        prvHstFirstFrameReceived_ = false;
        prvHstAcquisitionRate_ = 0.0;
        prvHstRateSamples_.clear();
        epicsMutexUnlock(prvHstMutex_);
    }
    
    // Join worker threads - they will detect closed connection (bytes_read <= 0) and exit
    // or exit when they see prvImgRunning_/imgRunning_/prvHstRunning_ is false
    if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    
    if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    
    // Signal PrvHst worker thread to stop
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
    // The thread may have already exited when connection closed and cleared its own thread ID
    // So we need to check if thread ID is still valid before trying to join
    epicsMutexLock(prvHstMutex_);
    epicsThreadId prvHstThreadId = prvHstWorkerThreadId_;
    epicsMutexUnlock(prvHstMutex_);
    
    if (prvHstThreadId != NULL && prvHstThreadId != epicsThreadGetIdSelf()) {
        // Give thread a moment to exit gracefully if it's still running
        epicsThreadSleep(0.2);
        
        // Re-check if thread ID is still valid (thread may have cleared it)
        epicsMutexLock(prvHstMutex_);
        if (prvHstWorkerThreadId_ == prvHstThreadId) {
            // Thread ID still valid, try to join
            prvHstWorkerThreadId_ = NULL;  // Clear pointer first
            epicsMutexUnlock(prvHstMutex_);
            epicsThreadMustJoin(prvHstThreadId);
        } else {
            // Thread already cleared its own ID, so it already exited
            epicsMutexUnlock(prvHstMutex_);
            printf("PrvHst worker thread already exited (cleared its own thread ID)\n");
        }
    }
    
    // Explicitly disconnect to ensure clean state
    // Worker threads may have already disconnected when they detected the closed connection
    prvImgDisconnect();
    imgDisconnect();
    prvHstDisconnect();

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
            // Status might be null, so use dump() which handles null safely
            setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
        }
    }
    callParamCallbacks();

    return status;
}
