/*
 * ADTimePix3 - EPICS areaDetector driver for TimePix3 (Serval)
 *
 * Author: Kazimierz Gofron (BNL, 2022; ORNL, 2022-present)
 * Created: June 2022 (Brookhaven National Laboratory)
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

// Standard includes
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <sys/stat.h>

// EPICS includes
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <iocsh.h>
#include <epicsExport.h>


// Area Detector include
#include "ADTimePix.h"
#include "ADTimePixLog.h"

#define delim "/"

using namespace std;
using json = nlohmann::json;
// Add any additional namespaces here

const char* driverName = "ADTimePix";

// Add any driver constants here


// -----------------------------------------------------------------------
// ADTimePix Utility Functions (Reporting/Logging/ExternalC)
// -----------------------------------------------------------------------


/*
 * External configuration function for ADTimePix.
 * Envokes the constructor to create a new ADTimePix object
 * This is the function that initializes the driver, and is called in the IOC startup script
 * 
 * NOTE: When implementing a new driver with ADDriverTemplate, your camera may use a different connection method than
 * a const char* serverURL. Just edit the param to fit your device, and make sure to make the same edit to the constructor below
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADTimePixConfig(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize){
#ifdef ASYN_DESTRUCTIBLE
    new ADTimePix(portName, serverURL, maxBuffers, maxMemory, priority, stackSize, ASYN_DESTRUCTIBLE);
#else
    new ADTimePix(portName, serverURL, maxBuffers, maxMemory, priority, stackSize, 0);
#endif
    return(asynSuccess);
}

/** Optional config with asyn flags (e.g. ASYN_DESTRUCTIBLE for asyn R4-45+). Use when building with ASYN_DESTRUCTIBLE defined. */
extern "C" int ADTimePixConfigWithFlags(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize, int asynFlags){
    new ADTimePix(portName, serverURL, maxBuffers, maxMemory, priority, stackSize, asynFlags);
    return(asynSuccess);
}

/*
 * Called when IOC exits if the driver was not created with ASYN_DESTRUCTIBLE
 * (e.g. older asyn). Deletes the driver so the destructor runs and threads
 * and resources are cleaned up. When ASYN_DESTRUCTIBLE is used, asyn performs
 * teardown and this callback is not registered.
 */
static void exitCallbackC(void* pPvt){
    ADTimePix* pTimePix = (ADTimePix*) pPvt;
    delete pTimePix;
}

/** Checks whether the directory specified exists.
  *
  * This is a convenience function that determines the directory specified exists.
  * It adds a trailing '/' or '\' character to the path if one is not present.
  * It returns true if the directory exists and false if it does not
  */
bool ADTimePix::checkPath(std::string &filePath)
{
    char lastChar;
    struct stat buff;
    int istat;
    size_t len;
    int isDir=0;
    bool pathExists=false;

    len = filePath.size();
    if (len == 0) return false;
    /* If the path contains a trailing '/' or '\' remove it, because Windows won't find
     * the directory if it has that trailing character */
    lastChar = filePath[len-1];

    if (lastChar == '/') {
        filePath.resize(len-1);
    }
    istat = stat(filePath.c_str(), &buff);
    if (!istat) isDir = (S_IFDIR & buff.st_mode);
    if (!istat && isDir) {
        pathExists = true;
    }
    /* Add a terminator even if it did not have one originally */
    filePath.append(delim);
    return pathExists;
}


/** Checks whether the directory specified BPC/DACS/Raw parameter exists.
  *
  * This is a convenience function that determines the directory specified BPC/DACS/Raw Path parameter exists.
  * It sets the value of xxxPathExists to 0 (does not exist) or 1 (exists).
  * It also adds a trailing '/' character to the path if one is not present.
  * Returns an error status if the directory does not exist.
  */
/* checkBPCPath() implemented in mask_io.cpp with other BPC/mask path logic */

asynStatus ADTimePix::checkDACSPath()
{
    asynStatus status;
    std::string filePath;
    int pathExists;

    getStringParam(ADTimePixDACSFilePath, filePath);
    if (filePath.size() == 0) return asynSuccess;
    pathExists = checkPath(filePath);
    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixDACSFilePath, filePath);
    setIntegerParam(ADTimePixDACSFilePathExists, pathExists);
    return status;
}

/**
 * Unified path checking function for all channel types
 * Replaces individual checkRawPath(), checkRaw1Path(), checkImgPath(), etc.
 */
asynStatus ADTimePix::checkChannelPath(int baseParam, int streamParam, int filePathExistsParam, 
                                      const std::string& channelName, const std::string& errorMessage) {
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(baseParam, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        // Check for file:/ protocol with strict single slash enforcement
        if (filePath.compare(0, 5, "file:") == 0) {
            // Check that the character at position 5 is exactly one '/'
            // The format should be 'file:/path' where the '/' is at position 5
            // Additional '/' characters in the path are valid
            if (filePath.length() > 5 && filePath[5] == '/' && (filePath.length() == 6 || filePath[6] != '/')) {
                // Valid format: 'file:/path' - single '/' at position 5, not followed by another '/'
                if (streamParam != -1) {  // Only set stream parameter if it exists
                    setIntegerParam(streamParam, 0);
                }
                fileOrStream = filePath.substr(5);  // Remove "file:" prefix, keep the "/"
                pathExists = checkPath(fileOrStream);
            } else {
                printf("Invalid file path format: '%s'. Expected format: 'file:/path'.\n", filePath.c_str());
                printf("Debug: length=%zu, char[5]='%c', char[6]='%c'\n", 
                       filePath.length(), 
                       filePath.length() > 5 ? filePath[5] : '?',
                       filePath.length() > 6 ? filePath[6] : '?');
                if (streamParam != -1) {
                    setIntegerParam(streamParam, 3);
                }
                pathExists = 0;
            }
        }
        // Check for http:// protocol using find() instead of substr()
        else if (filePath.find("http://") == 0) {       // streaming, http://localhost:8081
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 1);
            }
            fileOrStream = filePath.substr(7);  // Remove "http://" prefix
            pathExists = 1;
        }   
        // Check for tcp:// protocol using find() instead of substr()
        else if (filePath.find("tcp://") == 0) {       // streaming, tcp://localhost:8085
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 2);
            }
            fileOrStream = filePath.substr(6);  // Remove "tcp://" prefix
            pathExists = 1;
        }
        else {
            printf("%s\n", errorMessage.c_str());
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 3);
            }
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(baseParam, filePath);
    setIntegerParam(filePathExistsParam, pathExists);
    return status;
}

asynStatus ADTimePix::checkRawPath()
{
    return checkChannelPath(ADTimePixRawBase, ADTimePixRawStream, ADTimePixRawFilePathExists,
                          "Raw", "Raw file path must be file:/path_to_raw_folder, http://localhost:8081, or tcp://localhost:8085");
}

/* Serval 3.3.0 allows writing raw .tpx3 file, and stream data. This is the 2nd Base channel
* to either write .tpx3 file, or stream to socket.
* Operator decides which Raw or Raw1 is used for streaming, and which for .tpx3 file.
*/
asynStatus ADTimePix::checkRaw1Path()
{
    return checkChannelPath(ADTimePixRaw1Base, ADTimePixRaw1Stream, ADTimePixRaw1FilePathExists,
                          "Raw1", "Raw1 file path must be file:/path_to_raw_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkImgPath()
{
    asynStatus status = checkChannelPath(ADTimePixImgBase, -1, ADTimePixImgFilePathExists,
                          "Img", "Img file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://listen@hostname:port");
    
    // If TCP path, parse host and port
    if (status == asynSuccess) {
        std::string filePath;
        getStringParam(ADTimePixImgBase, filePath);
        if (filePath.find("tcp://") == 0) {
            std::string host;
            int port;
            if (parseTcpPath(filePath, host, port)) {
                epicsMutexLock(imgMutex_);
                imgHost_ = host;
                imgPort_ = port;
                getIntegerParam(ADTimePixImgFormat, &imgFormat_);
                epicsMutexUnlock(imgMutex_);
                LOG_ARGS("Parsed Img TCP path: host=%s, port=%d", host.c_str(), port);
            } else {
                ERR_ARGS("Failed to parse Img TCP path: %s", filePath.c_str());
                setIntegerParam(ADTimePixImgFilePathExists, 0);
                return asynError;
            }
        }
    }
    
    return status;
}

asynStatus ADTimePix::checkImg1Path()
{
    return checkChannelPath(ADTimePixImg1Base, -1, ADTimePixImg1FilePathExists,
                          "Img1", "Img1 file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkPrvImgPath()
{
    asynStatus status = checkChannelPath(ADTimePixPrvImgBase, -1, ADTimePixPrvImgFilePathExists,
                          "PrvImg", "PrvImg file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://listen@hostname:port");
    
    // If TCP path, parse host and port
    if (status == asynSuccess) {
        std::string filePath;
        getStringParam(ADTimePixPrvImgBase, filePath);
        if (filePath.find("tcp://") == 0) {
            std::string host;
            int port;
            if (parseTcpPath(filePath, host, port)) {
                epicsMutexLock(prvImgMutex_);
                prvImgHost_ = host;
                prvImgPort_ = port;
                getIntegerParam(ADTimePixPrvImgFormat, &prvImgFormat_);
                epicsMutexUnlock(prvImgMutex_);
                LOG_ARGS("Parsed PrvImg TCP path: host=%s, port=%d", host.c_str(), port);
            } else {
                ERR_ARGS("Failed to parse PrvImg TCP path: %s", filePath.c_str());
                setIntegerParam(ADTimePixPrvImgFilePathExists, 0);
                return asynError;
            }
        }
    }
    
    return status;
}


asynStatus ADTimePix::checkPrvImg1Path()
{
    return checkChannelPath(ADTimePixPrvImg1Base, -1, ADTimePixPrvImg1FilePathExists,
                          "PrvImg1", "PrvImg1 file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkPrvHstPath() // file:/, http://, tcp:// format
{
    return checkChannelPath(ADTimePixPrvHstBase, ADTimePixPrvHstStream, ADTimePixPrvHstFilePathExists,
                          "PrvHst", "Prv Histogram path must be file:/path_to_folder, http://localhost:8081, or tcp://localhost:8085");
}



// ADD ANY OTHER SETTER CAMERA FUNCTIONS HERE, ADD CALL THEM IN WRITE INT32/FLOAT64


//-------------------------------------------------------------------------
// ADDriver function overwrites
//-------------------------------------------------------------------------

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including BPC, and Chips/DACS.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus ADTimePix::writeOctet(asynUser *pasynUser, const char *value,
                                    size_t nChars, size_t *nActual)
{
    int addr;
    int function;
    const char *paramName;
    asynStatus status = asynSuccess;

    status = parseAsynUser(pasynUser, &function, &addr, &paramName);
    if (status != asynSuccess) return status;

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(addr, function, (char *)value);

    if (function == ADTimePixBPCFilePath)  {
        status = this->checkBPCPath();        
    } else if (function == ADTimePixDACSFilePath) {
        status = this->checkDACSPath();
    } else if (function == ADTimePixRawBase) {
        status = this->checkRawPath();
    } else if (function == ADTimePixRaw1Base) {
        status = this->checkRaw1Path();
    } else if (function == ADTimePixImgBase) {
        status = this->checkImgPath();      
    } else if (function == ADTimePixImg1Base) {
        status = this->checkImg1Path();      
    } else if (function == ADTimePixPrvImgBase) {
        status = this->checkPrvImgPath();
    } else if (function == ADTimePixPrvImg1Base) {
        status = this->checkPrvImg1Path();    
    } else if (function == ADTimePixPrvHstBase) {
        status = this->checkPrvHstPath();
    } else if (function == ADTimePixTofTdcReference) {
        status = this->sendMeasurementConfig();
    }
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr, addr);

    if (status)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s:%s: status=%d, function=%d, paramName=%s, value=%s",
                  driverName, ADTPX3_FUNC, status, function, paramName, value);
    else
        LOG_ARGS("function=%d, paramName=%s, value=%s", function, paramName, value);
    *nActual = nChars;
    return status;
}

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADTimePix::writeInt32(asynUser* pasynUser, epicsInt32 value){
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    int addr = 0;
    this->getAddress(pasynUser, &addr);

    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(addr, function, value);
    // start/stop acquisition
    if(function == ADAcquire){
        int currentADStatus;
        getIntegerParam(ADStatus, &currentADStatus);
        printf("ACQUIRE CHANGE: ADAcquire=%d (was %d), current ADStatus=%d\n", value, acquiring, currentADStatus);
        if(value && !acquiring){
            FLOW("Entering acquire start\n");
            status = acquireStart();  // Start acquisition
            if(status < 0) {
                return asynError;
            }
            // After acquireStart, ADStatus should be ADStatusAcquire (1)
            getIntegerParam(ADStatus, &currentADStatus);
            printf("After acquireStart: ADStatus=%d\n", currentADStatus);
        }
        if(!value && acquiring){
            FLOW("Entering acquire stop");
            acquireStop();  // Stop acquisition
            // After acquireStop, ADStatus should be ADStatusIdle (0)
            getIntegerParam(ADStatus, &currentADStatus);
            printf("After acquireStop: ADStatus=%d\n", currentADStatus);
        }
    }

    else if(function == ADImageMode){
        if(acquiring == 1) acquireStop();
        if(value == 0) {
            setIntegerParam(ADNumImages,1);  // Set number of images to 1 for single mode
            status = initAcquisition();
        }
    }

    else if(function == ADTimePixVthresholdFine) {
        status = writeDac(addr, "Vthreshold_fine", value);
    }

    else if(function == ADTimePixVthresholdCoarse) {
        status = writeDac(addr, "Vthreshold_coarse", value);
    }

    else if (function == ADTimePixWriteRaw || function == ADTimePixWriteRaw1 || function == ADTimePixWriteImg \
        || function == ADTimePixWriteImg1 || function == ADTimePixWritePrvImg || function == ADTimePixWritePrvImg1 || function == ADTimePixWritePrvHst) {
       status = getServer();    // Read configured channels from Serval
    }

    else if(function == ADTimePixHealth) { 
        // status = getHealth();
        status = getDashboard();
        status = getDetector();
        (void)getMeasurementConfig();   // Refresh Measurement.Config (Stem, TimeOfFlight) if supported
    //    status = getServer();
    }
    else if(function == ADTimePixRefreshConnection) {
        status = checkConnection();
        setIntegerParam(ADTimePixRefreshConnection, 0);  // Reset so PV can be triggered again
    }
    else if(function == ADTimePixRefreshPixelConfig) {
        if (addr == 0 && value == 1) {
            status = refreshPixelConfigFromServal();
            setIntegerParam(0, ADTimePixRefreshPixelConfig, 0);
        }
    }
    else if(function == ADTimePixApplyConfig) {
        status = fileWriter();
        if (status == asynSuccess) status = getServer();
        setIntegerParam(ADTimePixApplyConfig, 0);  // Reset so PV can be triggered again
    }
    else if(function == ADTimePixWriteProcessedImg) {
        if (value == 1) {
            pushProcessedImgToPlugins();
            setIntegerParam(ADTimePixWriteProcessedImg, 0);
            callParamCallbacks(ADTimePixWriteProcessedImg);
        }
    }
    else if(function == ADTimePixProcessedImgOutputType) {
        int outputType = value;
        if (outputType != 0 && outputType != 1) outputType = 0;
        setIntegerParam(ADTimePixProcessedImgOutputType, outputType);
    }
    else if(function == ADTimePixWriteProcessedHst) {
        if (value == 1) {
            pushProcessedHstToPlugins();
            setIntegerParam(ADTimePixWriteProcessedHst, 0);
            callParamCallbacks(ADTimePixWriteProcessedHst);
        }
    }
    else if(function == ADTimePixProcessedHstOutputType) {
        int outputType = value;
        if (outputType != 0 && outputType != 1) outputType = 0;
        setIntegerParam(ADTimePixProcessedHstOutputType, outputType);
    }
    else if(function == ADTimePixStemScanWidth || function == ADTimePixStemScanHeight
            || function == ADTimePixStemRadiusOuter || function == ADTimePixStemRadiusInner) {
        status = sendMeasurementConfig();
    }
    else if(function == ADTimePixWriteBPCFile) { 
        status = uploadBPC();
    }

    else if(function == ADTimePixWriteDACSFile) { 
        status = uploadDACS();
    }

    else if(function == ADTimePixWriteData) { 
        status = fileWriter();
        status = getServer();    // Read configured channels from Serval
    }

    else if(function == ADTimePixDetectorOrientation) {
        status = rotateLayout();
    }

    else if(function == ADTimePixBiasVolt || function == ADTimePixBiasEnable || function == ADTimePixTriggerIn || function == ADTimePixTriggerOut || function == ADTimePixLogLevel \
                || function == ADTimePixExternalReferenceClock || function == ADTimePixChainMode) {  // set and enable bias, log level
        status = initAcquisition();
    }    

    else if(function == ADNumImages || function == ADTriggerMode) { 
        if(function == ADNumImages) {
            int imageMode;
            getIntegerParam(ADImageMode,&imageMode);
            if (imageMode == 0 && value != 1) {
                /* In single mode (ADImageMode=0) NumImages must effectively be 1.
                 * Instead of returning an error (which causes WRITE INVALID on the record),
                 * clamp the driver parameter back to 1 and accept the write. */
                setIntegerParam(ADNumImages, 1);
                LOG_ARGS("Ignoring NumImages=%d in single mode, forcing NumImages=1", value);
            }
        }
        if (status == asynSuccess) status = initAcquisition();
    }

    else if(function == ADTimePixImgImageDataReset) {
        // Reset accumulated image data when value is set to 1
        if (value == 1) {
            epicsMutexLock(imgMutex_);
            resetImgAccumulation();
            epicsMutexUnlock(imgMutex_);
            // Reset the PV back to 0 (one-shot action)
            setIntegerParam(ADTimePixImgImageDataReset, 0);
            callParamCallbacks(ADTimePixImgImageDataReset);
        }
    }

    else if(function == ADTimePixPrvHstDataReset) {
        // Reset accumulated histogram data when value is set to 1
        if (value == 1) {
            if (prvHstMutex_) {
                epicsMutexLock(prvHstMutex_);
                resetPrvHstAccumulation();
                epicsMutexUnlock(prvHstMutex_);
            }
            // Reset the PV back to 0 (one-shot action)
            setIntegerParam(ADTimePixPrvHstDataReset, 0);
            callParamCallbacks(ADTimePixPrvHstDataReset);
        }
    }

    else if(function == ADTimePixImgFramesToSum) {
        epicsMutexLock(imgMutex_);
        imgFramesToSum_ = value;
        if (imgFramesToSum_ < 1) imgFramesToSum_ = 1;
        if (imgFramesToSum_ > 100000) imgFramesToSum_ = 100000;
        setIntegerParam(ADTimePixImgFramesToSum, imgFramesToSum_);
        
        // Trim frame buffer if new limit is smaller
        while (imgFrameBuffer_.size() > static_cast<size_t>(imgFramesToSum_)) {
            imgFrameBuffer_.pop_front();
        }
        
        // Prepare to recalculate sum of N frames immediately if buffer has frames
        size_t sum_pixel_count = 0;
        bool should_recalc_sum = false;
        if (!imgFrameBuffer_.empty()) {
            sum_pixel_count = imgFrameBuffer_[0].get_pixel_count();
            size_t frame_width = imgFrameBuffer_[0].get_width();
            size_t frame_height = imgFrameBuffer_[0].get_height();
            
            if (imgSumArray64WorkBuffer_.size() < sum_pixel_count) {
                imgSumArray64WorkBuffer_.resize(sum_pixel_count);
                imgSumArray64Buffer_.resize(sum_pixel_count);
            }
            
            std::memset(imgSumArray64WorkBuffer_.data(), 0, sum_pixel_count * sizeof(uint64_t));
            
            for (const auto& frame : imgFrameBuffer_) {
                if (frame.get_width() == frame_width && 
                    frame.get_height() == frame_height) {
                    if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                        const uint16_t* pixels = frame.get_pixels_16_ptr();
                        for (size_t i = 0; i < sum_pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    } else {
                        const uint32_t* pixels = frame.get_pixels_32_ptr();
                        for (size_t i = 0; i < sum_pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    }
                }
            }
            
            // Convert to epicsInt64
            for (size_t i = 0; i < sum_pixel_count; ++i) {
                imgSumArray64Buffer_[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
            }
            
            // Reset update counter to trigger immediate update on next frame
            imgFramesSinceLastSumUpdate_ = imgSumUpdateIntervalFrames_;
            should_recalc_sum = true;
        }
        
        // Recalculate memory usage immediately since buffer size may have changed
        imgMemoryUsage_ = calculateImgMemoryUsageMB();
        setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
        epicsMutexUnlock(imgMutex_);
        
        // Trigger callbacks outside mutex
        callParamCallbacks(ADTimePixImgFramesToSum);
        callParamCallbacks(ADTimePixImgMemoryUsage);
        
        // Trigger sum callback if we recalculated it (outside mutex)
        if (should_recalc_sum && sum_pixel_count > 0) {
            // Copy buffer data while holding mutex
            std::vector<epicsInt64> temp_buffer(sum_pixel_count);
            epicsMutexLock(imgMutex_);
            for (size_t i = 0; i < sum_pixel_count; ++i) {
                temp_buffer[i] = imgSumArray64Buffer_[i];
            }
            epicsMutexUnlock(imgMutex_);
            
            // Trigger callback outside mutex
            doCallbacksInt64Array(temp_buffer.data(), sum_pixel_count,
                                  ADTimePixImgImageSumNFrames, 0);
        }
    }
    
    else if(function == ADTimePixImgSumUpdateIntervalFrames) {
        epicsMutexLock(imgMutex_);
        imgSumUpdateIntervalFrames_ = value;
        if (imgSumUpdateIntervalFrames_ < 1) imgSumUpdateIntervalFrames_ = 1;
        if (imgSumUpdateIntervalFrames_ > 10000) imgSumUpdateIntervalFrames_ = 10000;
        setIntegerParam(ADTimePixImgSumUpdateIntervalFrames, imgSumUpdateIntervalFrames_);
        epicsMutexUnlock(imgMutex_);
        callParamCallbacks(ADTimePixImgSumUpdateIntervalFrames);
    }

    else if(function == ADTimePixPrvHstFramesToSum) {
        epicsMutexLock(prvHstMutex_);
        prvHstFramesToSum_ = value;
        if (prvHstFramesToSum_ < 1) prvHstFramesToSum_ = 1;
        if (prvHstFramesToSum_ > 100000) prvHstFramesToSum_ = 100000;
        setIntegerParam(ADTimePixPrvHstFramesToSum, prvHstFramesToSum_);
        
        // Trim frame buffer if new limit is smaller
        while (prvHstFrameBuffer_.size() > static_cast<size_t>(prvHstFramesToSum_)) {
            prvHstFrameBuffer_.pop_front();
        }
        epicsMutexUnlock(prvHstMutex_);
        callParamCallbacks(ADTimePixPrvHstFramesToSum);
    }

    else if(function == ADTimePixPrvHstSumUpdateInterval) {
        epicsMutexLock(prvHstMutex_);
        prvHstSumUpdateIntervalFrames_ = value;
        if (prvHstSumUpdateIntervalFrames_ < 1) prvHstSumUpdateIntervalFrames_ = 1;
        if (prvHstSumUpdateIntervalFrames_ > 10000) prvHstSumUpdateIntervalFrames_ = 10000;
        setIntegerParam(ADTimePixPrvHstSumUpdateInterval, prvHstSumUpdateIntervalFrames_);
        prvHstFramesSinceLastSumUpdate_ = 0;  // Reset counter on change
        epicsMutexUnlock(prvHstMutex_);
        callParamCallbacks(ADTimePixPrvHstSumUpdateInterval);
    }

    else{
        if (function < ADTIMEPIX_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }

    /* Do callbacks so higher layers see any changes */
	callParamCallbacks(addr);

    // Log status updates
    if(status){
        ERR_ARGS("ERROR status=%d function=%d, value=%d", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%d", function, value);
    return asynSuccess;
}


asynStatus ADTimePix::readInt64Array(asynUser *pasynUser, epicsInt64 *value, size_t nElements, size_t *nIn) {
    int function = pasynUser->reason;
    
    if (function == ADTimePixImgImageData) {
        epicsMutexLock(imgMutex_);
        if (imgRunningSum_) {
            size_t pixel_count = imgRunningSum_->get_pixel_count();
            size_t elements_to_copy = std::min(nElements, pixel_count);
            const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt64>(pixels[i]);
            }
            // Zero out remaining elements
            for (size_t i = elements_to_copy; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        } else {
            // No running sum yet, return zeros
            for (size_t i = 0; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        }
        epicsMutexUnlock(imgMutex_);
        return asynSuccess;
    } else if (function == ADTimePixImgImageSumNFrames) {
        epicsMutexLock(imgMutex_);
        if (!imgFrameBuffer_.empty()) {
            size_t pixel_count = imgFrameBuffer_[0].get_pixel_count();
            size_t elements_to_copy = std::min(nElements, pixel_count);
            
            // Calculate sum of frames in buffer
            if (imgSumArray64WorkBuffer_.size() < pixel_count) {
                imgSumArray64WorkBuffer_.resize(pixel_count);
            }
            std::memset(imgSumArray64WorkBuffer_.data(), 0, pixel_count * sizeof(uint64_t));
            
            size_t frame_width = imgFrameBuffer_[0].get_width();
            size_t frame_height = imgFrameBuffer_[0].get_height();
            
            for (const auto& frame : imgFrameBuffer_) {
                if (frame.get_width() == frame_width && 
                    frame.get_height() == frame_height) {
                    if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                        const uint16_t* pixels = frame.get_pixels_16_ptr();
                        for (size_t i = 0; i < pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    } else {
                        const uint32_t* pixels = frame.get_pixels_32_ptr();
                        for (size_t i = 0; i < pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    }
                }
            }
            
            // Copy to output buffer
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
            }
            // Zero out remaining elements
            for (size_t i = elements_to_copy; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        } else {
            // No frames in buffer, return zeros
            for (size_t i = 0; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        }
        epicsMutexUnlock(imgMutex_);
        return asynSuccess;
    }
    
    return asynPortDriver::readInt64Array(pasynUser, value, nElements, nIn);
}

// Note: readFloat64Array not implemented - using doCallbacksFloat64Array() to push data directly (like histogram IOC)

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 * This is the same functionality as writeInt32, but for processing doubles.
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADTimePix::writeFloat64(asynUser* pasynUser, epicsFloat64 value){
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if(function == ADAcquireTime || function == ADAcquirePeriod || function == ADTimePixTriggerDelay || function == ADTimePixGlobalTimestampInterval){
        if(acquiring) acquireStop();
        status = initAcquisition();
    }
    else if(function == ADTimePixStemDwellTime || function == ADTimePixTofMin || function == ADTimePixTofMax) {
        status = sendMeasurementConfig();
    }
    else{
        if(function < ADTIMEPIX_FIRST_PARAM){
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        ERR_ARGS("ERROR status = %d, function =%d, value = %f", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%f", function, value);
    return asynSuccess;
}


/*
 * Function used for reporting ADTimePix device and library information to an external
 * log file. The function first prints all ADTimePix specific information to the file,
 * then continues on to the base ADDriver 'report' function
 * 
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return: void
 */
void ADTimePix::report(FILE* fp, int details){
    int height;
    int width;
    ERR("reporting to external log file");
    if(details > 0){
        fprintf(fp, " -------------------------------------------------------------------\n");
        fprintf(fp, " Connected Device Information\n");
        // GET CAMERA INFORMATION HERE AND PRINT IT TO fp
        getIntegerParam(ADSizeX, &width);
        getIntegerParam(ADSizeY, &height);
        fprintf(fp, " Image Width           ->      %d\n", width);
        fprintf(fp, " Image Height          ->      %d\n", height);
        fprintf(fp, " -------------------------------------------------------------------\n");
        fprintf(fp, "\n");
        
        ADDriver::report(fp, details);
    }
}


// readImage() method removed - GraphicsMagick HTTP method no longer supported
// Use TCP streaming (tcp:// format) for preview images instead
// GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch


void ADTimePix::pushProcessedHstToPlugins() {
    if (!prvHstMutex_ || !pNDArrayPool) return;
    epicsMutexLock(prvHstMutex_);

    int outputType = 0;
    getIntegerParam(ADTimePixProcessedHstOutputType, &outputType);
    if (outputType != 0 && outputType != 1) outputType = 0;

    int savedSizeX = 0, savedSizeY = 0, savedArraySizeX = 0, savedArraySizeY = 0;
    int savedDataType = 0, savedArraySize = 0;
    getIntegerParam(ADSizeX, &savedSizeX);
    getIntegerParam(ADSizeY, &savedSizeY);
    getIntegerParam(NDArraySizeX, &savedArraySizeX);
    getIntegerParam(NDArraySizeY, &savedArraySizeY);
    getIntegerParam(NDDataType, &savedDataType);
    getIntegerParam(NDArraySize, &savedArraySize);

    int savedArrayCounter = 0;
    getIntegerParam(NDArrayCounter, &savedArrayCounter);

    int arrayCallbacks = 0;
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

    int nCb = 0;
    epicsTimeStamp ts;
    epicsTimeGetCurrent(&ts);
    const double tsFloat = ts.secPastEpoch + ts.nsec / 1.e9;

    auto doHistCallback = [&](NDArray* pArr, int addr) {
        if (!pArr) return;
        if (pArr->pData) {
            pArr->timeStamp = tsFloat;
            updateTimeStamp(&pArr->epicsTS);
            if (arrayCallbacks) {
                doCallbacksGenericPointer(pArr, NDArrayData, addr);
                nCb++;
            }
        }
        pArr->release();
    };

    if (prvHstRunningSum_ && arrayCallbacks) {
        const size_t bin_size = prvHstRunningSum_->get_bin_size();
        if (bin_size > 0 && prvHstTimeMsBuffer_.size() >= bin_size) {
            for (size_t i = 0; i < bin_size; ++i) {
                prvHstTimeMsBuffer_[i] =
                    (prvHstFrameBinOffset_ + static_cast<int>(i) * prvHstFrameBinWidth_) * TPX3_TDC_CLOCK_PERIOD_SEC * 1e3;
            }
        }

        size_t dims[3] = { bin_size, 0, 0 };
        const epicsUInt64 nFrames = (prvHstFrameCount_ > 0) ? prvHstFrameCount_ : 1ULL;
        const epicsUInt32 nBuf = static_cast<epicsUInt32>(prvHstFrameBuffer_.size());
        const epicsUInt32 nForSumN = (nBuf > 0) ? nBuf : 1U;

        if (bin_size > 0) {
            NDDataType_t dtype5 = (outputType == 1) ? NDInt32 : NDInt64;
            NDArray* p5 = pNDArrayPool->alloc(1, dims, dtype5, 0, NULL);
            if (p5 && p5->pData) {
                if (outputType == 0) {
                    epicsInt64* pD = reinterpret_cast<epicsInt64*>(p5->pData);
                    for (size_t i = 0; i < bin_size; ++i)
                        pD[i] = static_cast<epicsInt64>(prvHstRunningSum_->get_bin_value_64(i));
                } else {
                    epicsInt32* pD = reinterpret_cast<epicsInt32*>(p5->pData);
                    for (size_t i = 0; i < bin_size; ++i) {
                        uint64_t v = prvHstRunningSum_->get_bin_value_64(i);
                        pD[i] = static_cast<epicsInt32>(v / nFrames);
                    }
                }
                if (p5->pAttributeList) {
                    getAttributes(p5->pAttributeList);
                    double t0ms = prvHstTimeMsBuffer_[0];
                    double tStepMs = (bin_size > 1) ? (prvHstTimeMsBuffer_[1] - prvHstTimeMsBuffer_[0]) : 0.0;
                    epicsInt32 nBinsA = static_cast<epicsInt32>(bin_size);
                    p5->pAttributeList->add("PrvHstTimeBin0Ms", "First bin center (ms)", NDAttrFloat64, &t0ms);
                    p5->pAttributeList->add("PrvHstTimeBinStepMs", "Bin center spacing (ms)", NDAttrFloat64, &tStepMs);
                    p5->pAttributeList->add("PrvHstNumBins", "Number of bins", NDAttrInt32, &nBinsA);
                }
                doHistCallback(p5, 5);
            } else if (p5) {
                p5->release();
            }

            NDArray* p7 = pNDArrayPool->alloc(1, dims, NDFloat64, 0, NULL);
            if (p7 && p7->pData) {
                epicsFloat64* pT = reinterpret_cast<epicsFloat64*>(p7->pData);
                for (size_t i = 0; i < bin_size; ++i) pT[i] = prvHstTimeMsBuffer_[i];
                if (p7->pAttributeList) getAttributes(p7->pAttributeList);
                doHistCallback(p7, 7);
            } else if (p7) {
                p7->release();
            }

            if (prvHstCurrentFrame_ && prvHstCurrentFrame_->get_bin_size() == bin_size) {
                NDArray* p6 = pNDArrayPool->alloc(1, dims, NDInt32, 0, NULL);
                if (p6 && p6->pData) {
                    epicsInt32* pD = reinterpret_cast<epicsInt32*>(p6->pData);
                    for (size_t i = 0; i < bin_size; ++i)
                        pD[i] = static_cast<epicsInt32>(prvHstCurrentFrame_->get_bin_value_32(i));
                    if (p6->pAttributeList) getAttributes(p6->pAttributeList);
                    doHistCallback(p6, 6);
                } else if (p6) {
                    p6->release();
                }
            }

            if (!prvHstFrameBuffer_.empty() && prvHstSumArray64Buffer_.size() >= bin_size) {
                NDDataType_t dtype4 = (outputType == 1) ? NDInt32 : NDInt64;
                NDArray* p4 = pNDArrayPool->alloc(1, dims, dtype4, 0, NULL);
                if (p4 && p4->pData) {
                    const epicsInt64* sumN = prvHstSumArray64Buffer_.data();
                    if (outputType == 0) {
                        epicsInt64* pD = reinterpret_cast<epicsInt64*>(p4->pData);
                        for (size_t i = 0; i < bin_size; ++i) pD[i] = sumN[i];
                    } else {
                        epicsInt32* pD = reinterpret_cast<epicsInt32*>(p4->pData);
                        for (size_t i = 0; i < bin_size; ++i)
                            pD[i] = static_cast<epicsInt32>(sumN[i] / nForSumN);
                    }
                    if (p4->pAttributeList) {
                        getAttributes(p4->pAttributeList);
                        double t0ms = prvHstTimeMsBuffer_[0];
                        double tStepMs = (bin_size > 1) ? (prvHstTimeMsBuffer_[1] - prvHstTimeMsBuffer_[0]) : 0.0;
                        epicsInt32 nBinsA = static_cast<epicsInt32>(bin_size);
                        p4->pAttributeList->add("PrvHstTimeBin0Ms", "First bin center (ms)", NDAttrFloat64, &t0ms);
                        p4->pAttributeList->add("PrvHstTimeBinStepMs", "Bin center spacing (ms)", NDAttrFloat64, &tStepMs);
                        p4->pAttributeList->add("PrvHstNumBins", "Number of bins", NDAttrInt32, &nBinsA);
                    }
                    doHistCallback(p4, 4);
                } else if (p4) {
                    p4->release();
                }
            }
        }
    }

    int curCounter = 0;
    getIntegerParam(NDArrayCounter, &curCounter);
    if (nCb > 0 && curCounter == savedArrayCounter + nCb)
        setIntegerParam(NDArrayCounter, savedArrayCounter);

    setIntegerParam(ADSizeX, savedSizeX);
    setIntegerParam(ADSizeY, savedSizeY);
    setIntegerParam(NDArraySizeX, savedArraySizeX);
    setIntegerParam(NDArraySizeY, savedArraySizeY);
    setIntegerParam(NDDataType, savedDataType);
    setIntegerParam(NDArraySize, savedArraySize);
    epicsMutexUnlock(prvHstMutex_);
}

void ADTimePix::resetPrvHstAccumulation() {
    // Reset accumulated histogram data
    prvHstRunningSum_.reset();
    prvHstFrameBuffer_.clear();
    prvHstFrameCount_ = 0;
    prvHstTotalCounts_ = 0;
    prvHstFramesSinceLastSumUpdate_ = 0;
    prvHstProcessingTime_ = 0.0;
    prvHstProcessingTimeSamples_.clear();
    
    // Clear buffers
    prvHstSumArray64Buffer_.clear();
    prvHstSumArray64WorkBuffer_.clear();
    prvHstArrayData32Buffer_.clear();
    prvHstTimeMsBuffer_.clear();
    
    // Update PVs
    setIntegerParam(ADTimePixPrvHstFrameCount, 0);
    setInteger64Param(ADTimePixPrvHstTotalCounts, 0);
    setDoubleParam(ADTimePixPrvHstProcessingTime, 0.0);
    
    // Calculate memory usage after reset (similar to histogram_io.cpp)
    double total_memory_mb = 0.0;
    // Running sum is reset, so no memory for it
    // Frame buffer is cleared, so no memory for frames
    // Only buffers remain
    total_memory_mb += prvHstTimeMsBuffer_.size() * sizeof(epicsFloat64) / (1024.0 * 1024.0);
    total_memory_mb += (prvHstRateSamples_.size() + prvHstProcessingTimeSamples_.size()) * sizeof(double) / (1024.0 * 1024.0);
    total_memory_mb += prvHstLineBuffer_.size() * sizeof(char) / (1024.0 * 1024.0);
    total_memory_mb += 0.1;  // Estimated overhead
    prvHstMemoryUsage_ = total_memory_mb;
    setDoubleParam(ADTimePixPrvHstMemoryUsage, prvHstMemoryUsage_);
    
    // Trigger callbacks with zero arrays to clear waveform PVs immediately
    // This ensures the display shows empty arrays right away
    if (ADTimePixPrvHstHistogramData >= 0) {
        // Send zero array for accumulated data (use last known bin size or default)
        size_t zero_bin_size = 16000; // Default bin size
        if (prvHstFrameBinSize_ > 0) {
            zero_bin_size = static_cast<size_t>(prvHstFrameBinSize_);
        }
        std::vector<epicsInt64> zero_data(zero_bin_size, 0);
        doCallbacksInt64Array(zero_data.data(), zero_bin_size, ADTimePixPrvHstHistogramData, 0);
    }
    
    if (ADTimePixPrvHstHistogramSumNFrames >= 0) {
        // Send zero array for sum of N frames
        size_t zero_bin_size = 16000; // Default bin size
        if (prvHstFrameBinSize_ > 0) {
            zero_bin_size = static_cast<size_t>(prvHstFrameBinSize_);
        }
        std::vector<epicsInt64> zero_data(zero_bin_size, 0);
        doCallbacksInt64Array(zero_data.data(), zero_bin_size, ADTimePixPrvHstHistogramSumNFrames, 0);
    }
    
    // Trigger callbacks for counter PVs
    callParamCallbacks(ADTimePixPrvHstFrameCount);
    callParamCallbacks(ADTimePixPrvHstTotalCounts);
    callParamCallbacks(ADTimePixPrvHstProcessingTime);
    callParamCallbacks(ADTimePixPrvHstMemoryUsage);
}



//----------------------------------------------------------------------------
// ADTimePix Constructor/Destructor
//----------------------------------------------------------------------------

/* maxAddr=8: eight asyn addr lists, indices 0..7 — PrvImg=0, Img=1, Img sum=2, Img sumN=3,
 * PrvHst sumN=4, PrvHst running sum=5, PrvHst frame=6, PrvHst ToF bin centers (ms)=7 */
ADTimePix::ADTimePix(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize, int asynFlags)
    : ADDriver(portName, 8, (int)NUM_TIMEPIX_PARAMS, maxBuffers, maxMemory,
        asynInt32Mask | asynInt64Mask | asynOctetMask | asynFloat64Mask | asynEnumMask | asynInt32ArrayMask | asynInt64ArrayMask | asynFloat64ArrayMask | asynDrvUserMask,
        asynInt32Mask | asynInt64Mask | asynOctetMask | asynFloat64Mask | asynEnumMask | asynInt32ArrayMask | asynInt64ArrayMask | asynFloat64ArrayMask | asynDrvUserMask,
        ASYN_MULTIDEVICE | ASYN_CANBLOCK | asynFlags,
        1,
        priority,
        stackSize),
      pixelConfigDiffMutex_(NULL),
      asynFlags_(asynFlags),
      imgCurrentFrame_(512, 512, ImageData::PixelFormat::UINT16, ImageData::DataType::FRAME_DATA)
{

    mDetOrientationMap["UP"] =      0;
    mDetOrientationMap["RIGHT"] =   1;
    mDetOrientationMap["DOWN"] =    2;
    mDetOrientationMap["LEFT"] =    3;
    mDetOrientationMap["UP_MIRRORED"] =     4;
    mDetOrientationMap["RIGHT_MIRRORED"] =  5;
    mDetOrientationMap["DOWN_MIRRORED"] =   6;
    mDetOrientationMap["LEFT_MIRRORED"] =   7;

    // GraphicsMagick initialization removed - TCP streaming is used instead
    // GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch

    this->serverURL = string(serverURL);

    // Call createParam here
  
    // FW timestamp, Detector Type
    createParam(ADTimePixFWTimeStampString,     asynParamOctet,&ADTimePixFWTimeStamp);
    createParam(ADTimePixDetTypeString,         asynParamOctet,&ADTimePixDetType);
    //sets URI http code PV
    createParam(ADTimePixHttpCodeString,        asynParamInt32, &ADTimePixHttpCode);

    // API serval version
    createParam(ADTimePixServerNameString,      asynParamOctet, &ADTimePixServerName);
    createParam(ADTimePixDetConnectedString,    asynParamInt32, &ADTimePixDetConnected);
    createParam(ADTimePixServalConnectedString, asynParamInt32, &ADTimePixServalConnected);

    // Dashboard
    createParam(ADTimePixFreeSpaceString,       asynParamInt64,   &ADTimePixFreeSpace);
    createParam(ADTimePixWriteSpeedString,      asynParamFloat64, &ADTimePixWriteSpeed);
    createParam(ADTimePixLowerLimitString,      asynParamInt64,   &ADTimePixLowerLimit); 
    createParam(ADTimePixLLimReachedString,     asynParamInt32,   &ADTimePixLLimReached);

    // Detector Health (detector/health, detector)
    createParam(ADTimePixLocalTempString,       asynParamFloat64, &ADTimePixLocalTemp);
    createParam(ADTimePixFPGATempString,        asynParamFloat64, &ADTimePixFPGATemp);
    createParam(ADTimePixFan1SpeedString,       asynParamFloat64, &ADTimePixFan1Speed);
    createParam(ADTimePixFan2SpeedString,       asynParamFloat64, &ADTimePixFan2Speed);
    createParam(ADTimePixBiasVoltageString,     asynParamFloat64, &ADTimePixBiasVoltage);
    createParam(ADTimePixHumidityString,        asynParamInt32, &ADTimePixHumidity);
    createParam(ADTimePixChipTemperatureString, asynParamOctet, &ADTimePixChipTemperature);
    createParam(ADTimePixVDDString,             asynParamOctet, &ADTimePixVDD);
    createParam(ADTimePixAVDDString,            asynParamOctet, &ADTimePixAVDD);
    createParam(ADTimePixHealthString,          asynParamInt32, &ADTimePixHealth);

        // Detector Info (detector)
    createParam(ADTimePixIfaceNameString,       asynParamOctet, &ADTimePixIfaceName);
    createParam(ADTimePixSW_versionString,      asynParamOctet, &ADTimePixSW_version);
    createParam(ADTimePixFW_versionString,      asynParamOctet, &ADTimePixFW_version);
    createParam(ADTimePixPixCountString,        asynParamInt32, &ADTimePixPixCount);
    createParam(ADTimePixRowLenString,          asynParamInt32, &ADTimePixRowLen);
    createParam(ADTimePixNumberOfChipsString,   asynParamInt32, &ADTimePixNumberOfChips);
    createParam(ADTimePixNumberOfRowsString,    asynParamInt32, &ADTimePixNumberOfRows);
    createParam(ADTimePixMpxTypeString,         asynParamInt32, &ADTimePixMpxType);

    createParam(ADTimePixBoardsIDString,        asynParamOctet, &ADTimePixBoardsID);
    createParam(ADTimePixBoardsIPString,        asynParamOctet, &ADTimePixBoardsIP);
    createParam(ADTimePixBoardsCh1String,       asynParamOctet, &ADTimePixBoardsCh1);
    createParam(ADTimePixBoardsCh2String,       asynParamOctet, &ADTimePixBoardsCh2);
    createParam(ADTimePixBoardsCh3String,       asynParamOctet, &ADTimePixBoardsCh3);
    createParam(ADTimePixBoardsCh4String,       asynParamOctet, &ADTimePixBoardsCh4);
    createParam(ADTimePixBoards2IDString,         asynParamOctet, &ADTimePixBoards2ID);
    createParam(ADTimePixBoards2IPString,         asynParamOctet, &ADTimePixBoards2IP);
    createParam(ADTimePixBoardsCh5String,       asynParamOctet, &ADTimePixBoardsCh5);
    createParam(ADTimePixBoardsCh6String,       asynParamOctet, &ADTimePixBoardsCh6);
    createParam(ADTimePixBoardsCh7String,       asynParamOctet, &ADTimePixBoardsCh7);
    createParam(ADTimePixBoardsCh8String,       asynParamOctet, &ADTimePixBoardsCh8);

    createParam(ADTimePixSuppAcqModesString,    asynParamInt32,     &ADTimePixSuppAcqModes);
    createParam(ADTimePixClockReadoutString,    asynParamFloat64,   &ADTimePixClockReadout);
    createParam(ADTimePixMaxPulseCountString,   asynParamInt32,     &ADTimePixMaxPulseCount);
    createParam(ADTimePixMaxPulseHeightString,  asynParamFloat64,   &ADTimePixMaxPulseHeight);
    createParam(ADTimePixMaxPulsePeriodString,  asynParamFloat64,   &ADTimePixMaxPulsePeriod);
    createParam(ADTimePixTimerMaxValString,     asynParamFloat64,   &ADTimePixTimerMaxVal);
    createParam(ADTimePixTimerMinValString,     asynParamFloat64,   &ADTimePixTimerMinVal);
    createParam(ADTimePixTimerStepString,       asynParamFloat64,   &ADTimePixTimerStep);
    createParam(ADTimePixClockTimepixString,    asynParamFloat64,   &ADTimePixClockTimepix);

            // Detector Config
    createParam(ADTimePixFan1PWMString,                     asynParamInt32,     &ADTimePixFan1PWM);
    createParam(ADTimePixFan2PWMString,                     asynParamInt32,     &ADTimePixFan2PWM);      
    createParam(ADTimePixBiasVoltString,                    asynParamInt32,     &ADTimePixBiasVolt);
    createParam(ADTimePixBiasEnableString,                  asynParamInt32,     &ADTimePixBiasEnable);
    createParam(ADTimePixChainModeString,                   asynParamInt32,     &ADTimePixChainMode);
    createParam(ADTimePixTriggerInString,                   asynParamInt32,     &ADTimePixTriggerIn);
    createParam(ADTimePixTriggerOutString,                  asynParamInt32,     &ADTimePixTriggerOut);
    createParam(ADTimePixPolarityString,                    asynParamInt32,     &ADTimePixPolarity);
    createParam(ADTimePixTriggerModeString,                 asynParamOctet,     &ADTimePixTriggerMode);
    createParam(ADTimePixExposureTimeString,                asynParamFloat64,   &ADTimePixExposureTime);  
    createParam(ADTimePixTriggerPeriodString,               asynParamFloat64,   &ADTimePixTriggerPeriod);  
    createParam(ADTimePixnTriggersString,                   asynParamInt32,     &ADTimePixnTriggers);
    createParam(ADTimePixDetectorOrientationString,         asynParamInt32,     &ADTimePixDetectorOrientation);
    createParam(ADTimePixPeriphClk80String,                 asynParamInt32,     &ADTimePixPeriphClk80);
    createParam(ADTimePixTriggerDelayString,                asynParamFloat64,   &ADTimePixTriggerDelay);  
    createParam(ADTimePixTdcString,                         asynParamOctet,     &ADTimePixTdc);
    createParam(ADTimePixTdc0String,                        asynParamInt32,     &ADTimePixTdc0);
    createParam(ADTimePixTdc1String,                        asynParamInt32,     &ADTimePixTdc1);
    createParam(ADTimePixGlobalTimestampIntervalString,     asynParamFloat64,   &ADTimePixGlobalTimestampInterval);             
    createParam(ADTimePixExternalReferenceClockString,      asynParamInt32,     &ADTimePixExternalReferenceClock);          
    createParam(ADTimePixLogLevelString,                    asynParamInt32,     &ADTimePixLogLevel);

    // Detector Chips
    createParam(ADTimePixCP_PLLString,             asynParamInt32, &ADTimePixCP_PLL);
    createParam(ADTimePixDiscS1OFFString,          asynParamInt32, &ADTimePixDiscS1OFF);
    createParam(ADTimePixDiscS1ONString,           asynParamInt32, &ADTimePixDiscS1ON);
    createParam(ADTimePixDiscS2OFFString,          asynParamInt32, &ADTimePixDiscS2OFF);
    createParam(ADTimePixDiscS2ONString,           asynParamInt32, &ADTimePixDiscS2ON);
    createParam(ADTimePixIkrumString,              asynParamInt32, &ADTimePixIkrum);
    createParam(ADTimePixPixelDACString,           asynParamInt32, &ADTimePixPixelDAC);
    createParam(ADTimePixPreampOFFString,          asynParamInt32, &ADTimePixPreampOFF);
    createParam(ADTimePixPreampONString,           asynParamInt32, &ADTimePixPreampON);
    createParam(ADTimePixTPbufferInString,         asynParamInt32, &ADTimePixTPbufferIn);
    createParam(ADTimePixTPbufferOutString,        asynParamInt32, &ADTimePixTPbufferOut);
    createParam(ADTimePixPLL_VcntrlString,         asynParamInt32, &ADTimePixPLL_Vcntrl);
    createParam(ADTimePixVPreampNCASString,        asynParamInt32, &ADTimePixVPreampNCAS);
    createParam(ADTimePixVTPcoarseString,          asynParamInt32, &ADTimePixVTPcoarse);
    createParam(ADTimePixVTPfineString,            asynParamInt32, &ADTimePixVTPfine);
    createParam(ADTimePixVfbkString,               asynParamInt32, &ADTimePixVfbk);
    createParam(ADTimePixVthresholdCoarseString,   asynParamInt32, &ADTimePixVthresholdCoarse);
    createParam(ADTimePixVthresholdFineString,     asynParamInt32, &ADTimePixVthresholdFine);
    createParam(ADTimePixAdjustString,             asynParamInt32, &ADTimePixAdjust);
             
    // Detector Chip Layout
    createParam(ADTimePixLayoutString,              asynParamOctet, &ADTimePixLayout);
    // Detector Chip Temperature, VDD, AVDD
    createParam(ADTimePixChipNTemperatureString,    asynParamInt32, &ADTimePixChipNTemperature);
    createParam(ADTimePixChipN_VDDString,           asynParamFloat64, &ADTimePixChipN_VDD);
    createParam(ADTimePixChipN_AVDDString,          asynParamFloat64, &ADTimePixChipN_AVDD);

    // Files BPC, Chip/DACS
    createParam(ADTimePixBPCFilePathString,                asynParamOctet,  &ADTimePixBPCFilePath);            
    createParam(ADTimePixBPCFilePathExistsString,          asynParamInt32,  &ADTimePixBPCFilePathExists);          
    createParam(ADTimePixBPCFileNameString,                asynParamOctet,  &ADTimePixBPCFileName);            
    createParam(ADTimePixDACSFilePathString,               asynParamOctet,  &ADTimePixDACSFilePath);           
    createParam(ADTimePixDACSFilePathExistsString,         asynParamInt32,  &ADTimePixDACSFilePathExists);             
    createParam(ADTimePixDACSFileNameString,               asynParamOctet,  &ADTimePixDACSFileName); 
    createParam(ADTimePixWriteMsgString,                   asynParamOctet,  &ADTimePixWriteMsg); 
    createParam(ADTimePixWriteBPCFileString,               asynParamInt32,  &ADTimePixWriteBPCFile);     
    createParam(ADTimePixWriteDACSFileString,              asynParamInt32,  &ADTimePixWriteDACSFile); 

    // Server, File Writer channels
    createParam(ADTimePixWriteDataString,                  asynParamInt32,  &ADTimePixWriteData);
    createParam(ADTimePixWriteRawString,                   asynParamInt32,  &ADTimePixWriteRaw);
    createParam(ADTimePixWriteRaw1String,                  asynParamInt32,  &ADTimePixWriteRaw1);   // Serval 3.3.0
    createParam(ADTimePixWriteImgString,                   asynParamInt32,  &ADTimePixWriteImg);
    createParam(ADTimePixWriteImg1String,                  asynParamInt32,  &ADTimePixWriteImg1);
    createParam(ADTimePixWritePrvImgString,                asynParamInt32,  &ADTimePixWritePrvImg);
    createParam(ADTimePixWritePrvImg1String,               asynParamInt32,  &ADTimePixWritePrvImg1);
    createParam(ADTimePixWritePrvHstString,                asynParamInt32,  &ADTimePixWritePrvHst);
    // Server, Read back channels from Serval
    createParam(ADTimePixWriteRawReadString,                   asynParamInt32,  &ADTimePixWriteRawRead);
    createParam(ADTimePixWriteRaw1ReadString,                  asynParamInt32,  &ADTimePixWriteRaw1Read);   // Serval 3.3.0
    createParam(ADTimePixWriteImgReadString,                   asynParamInt32,  &ADTimePixWriteImgRead);
    createParam(ADTimePixWriteImg1ReadString,                  asynParamInt32,  &ADTimePixWriteImg1Read);
    createParam(ADTimePixWritePrvImgReadString,                asynParamInt32,  &ADTimePixWritePrvImgRead);
    createParam(ADTimePixWritePrvImg1ReadString,               asynParamInt32,  &ADTimePixWritePrvImg1Read);
    createParam(ADTimePixWritePrvHstReadString,                asynParamInt32,  &ADTimePixWritePrvHstRead);

    // Server, Raw
    createParam(ADTimePixRawBaseString,                    asynParamOctet,  &ADTimePixRawBase);               
    createParam(ADTimePixRawFilePatString,                 asynParamOctet,  &ADTimePixRawFilePat);             
    createParam(ADTimePixRawSplitStrategyString,           asynParamInt32,  &ADTimePixRawSplitStrategy);         
    createParam(ADTimePixRawQueueSizeString,               asynParamInt32,  &ADTimePixRawQueueSize);
    createParam(ADTimePixRawFilePathExistsString,          asynParamInt32,  &ADTimePixRawFilePathExists); 
    // Server, Raw; Serval 3.3.0 allows writing raw file and stream.
    createParam(ADTimePixRaw1BaseString,                   asynParamOctet,  &ADTimePixRaw1Base);
    createParam(ADTimePixRaw1FilePatString,                asynParamOctet,  &ADTimePixRaw1FilePat);
    createParam(ADTimePixRaw1SplitStrategyString,          asynParamInt32,  &ADTimePixRaw1SplitStrategy);
    createParam(ADTimePixRaw1QueueSizeString,              asynParamInt32,  &ADTimePixRaw1QueueSize);
    createParam(ADTimePixRaw1FilePathExistsString,         asynParamInt32,  &ADTimePixRaw1FilePathExists);
    // Server, Image
    createParam(ADTimePixImgBaseString,                   asynParamOctet,    &ADTimePixImgBase);              
    createParam(ADTimePixImgFilePatString,                asynParamOctet,    &ADTimePixImgFilePat);            
    createParam(ADTimePixImgFormatString,                 asynParamInt32,    &ADTimePixImgFormat);            
    createParam(ADTimePixImgModeString,                   asynParamInt32,    &ADTimePixImgMode);              
    createParam(ADTimePixImgThsString,                    asynParamOctet,    &ADTimePixImgThs);             
    createParam(ADTimePixImgIntSizeString,                asynParamInt32,    &ADTimePixImgIntSize); 
    createParam(ADTimePixImgIntModeString,                asynParamInt32,    &ADTimePixImgIntMode);            
    createParam(ADTimePixImgStpOnDskLimString,            asynParamInt32,    &ADTimePixImgStpOnDskLim);        
    createParam(ADTimePixImgQueueSizeString,              asynParamInt32,    &ADTimePixImgQueueSize);         
    createParam(ADTimePixImgFilePathExistsString,         asynParamInt32,    &ADTimePixImgFilePathExists);    
    // Server, Image, ImageChannels[1]
    createParam(ADTimePixImg1BaseString,                  asynParamOctet,    &ADTimePixImg1Base);              
    createParam(ADTimePixImg1FilePatString,               asynParamOctet,    &ADTimePixImg1FilePat);            
    createParam(ADTimePixImg1FormatString,                asynParamInt32,    &ADTimePixImg1Format);            
    createParam(ADTimePixImg1ModeString,                  asynParamInt32,    &ADTimePixImg1Mode);              
    createParam(ADTimePixImg1ThsString,                   asynParamOctet,    &ADTimePixImg1Ths);             
    createParam(ADTimePixImg1IntSizeString,               asynParamInt32,    &ADTimePixImg1IntSize); 
    createParam(ADTimePixImg1IntModeString,               asynParamInt32,    &ADTimePixImg1IntMode);            
    createParam(ADTimePixImg1StpOnDskLimString,           asynParamInt32,    &ADTimePixImg1StpOnDskLim);        
    createParam(ADTimePixImg1QueueSizeString,             asynParamInt32,    &ADTimePixImg1QueueSize);         
    createParam(ADTimePixImg1FilePathExistsString,        asynParamInt32,    &ADTimePixImg1FilePathExists);    
    // Server, Preview   
    createParam(ADTimePixPrvPeriodString,                  asynParamFloat64,  &ADTimePixPrvPeriod);        
    createParam(ADTimePixPrvSamplingModeString,            asynParamInt32,    &ADTimePixPrvSamplingMode);  
    // Server, Preview, ImageChannels[0]  
    createParam(ADTimePixPrvImgBaseString,                   asynParamOctet, &ADTimePixPrvImgBase);            
    createParam(ADTimePixPrvImgFilePatString,                asynParamOctet, &ADTimePixPrvImgFilePat);         
    createParam(ADTimePixPrvImgFormatString,                 asynParamInt32, &ADTimePixPrvImgFormat);          
    createParam(ADTimePixPrvImgModeString,                   asynParamInt32, &ADTimePixPrvImgMode);            
    createParam(ADTimePixPrvImgThsString,                    asynParamOctet, &ADTimePixPrvImgThs);            
    createParam(ADTimePixPrvImgIntSizeString,                asynParamInt32, &ADTimePixPrvImgIntSize);
    createParam(ADTimePixPrvImgIntModeString,                asynParamInt32, &ADTimePixPrvImgIntMode);        
    createParam(ADTimePixPrvImgStpOnDskLimString,            asynParamInt32, &ADTimePixPrvImgStpOnDskLim);    
    createParam(ADTimePixPrvImgQueueSizeString,              asynParamInt32, &ADTimePixPrvImgQueueSize);
    createParam(ADTimePixPrvImgFilePathExistsString,         asynParamInt32, &ADTimePixPrvImgFilePathExists);          
    // PrvImg TCP streaming metadata
    createParam(ADTimePixPrvImgFrameNumberString,            asynParamInt32, &ADTimePixPrvImgFrameNumber);
    createParam(ADTimePixPrvImgTimeAtFrameString,            asynParamFloat64, &ADTimePixPrvImgTimeAtFrame);
    createParam(ADTimePixPrvImgAcqRateString,                asynParamFloat64, &ADTimePixPrvImgAcqRate);
    // Img TCP streaming metadata
    createParam(ADTimePixImgFrameNumberString,               asynParamInt32, &ADTimePixImgFrameNumber);
    createParam(ADTimePixImgTimeAtFrameString,               asynParamFloat64, &ADTimePixImgTimeAtFrame);
    createParam(ADTimePixImgAcqRateString,                   asynParamFloat64, &ADTimePixImgAcqRate);
    // Img channel accumulation and display data
    createParam(ADTimePixImgImageDataString,                 asynParamInt64Array, &ADTimePixImgImageData);
    createParam(ADTimePixImgImageFrameString,                asynParamInt32Array, &ADTimePixImgImageFrame);
    createParam(ADTimePixImgImageSumNFramesString,           asynParamInt64Array, &ADTimePixImgImageSumNFrames);
    createParam(ADTimePixImgAccumulationEnableString,        asynParamInt32, &ADTimePixImgAccumulationEnable);
    createParam(ADTimePixImgImageDataResetString,            asynParamInt32, &ADTimePixImgImageDataReset);
    createParam(ADTimePixImgFramesToSumString,               asynParamInt32, &ADTimePixImgFramesToSum);
    createParam(ADTimePixImgSumUpdateIntervalString,         asynParamInt32, &ADTimePixImgSumUpdateIntervalFrames);
    createParam(ADTimePixImgTotalCountsString,               asynParamInt64, &ADTimePixImgTotalCounts);
    createParam(ADTimePixImgProcessingTimeString,            asynParamFloat64, &ADTimePixImgProcessingTime);
    createParam(ADTimePixImgMemoryUsageString,               asynParamFloat64, &ADTimePixImgMemoryUsage);
    // Server, Preview, ImageChannels[1]   
    createParam(ADTimePixPrvImg1BaseString,                asynParamOctet, &ADTimePixPrvImg1Base);
    createParam(ADTimePixPrvImg1FilePatString,             asynParamOctet, &ADTimePixPrvImg1FilePat);             
    createParam(ADTimePixPrvImg1FormatString,              asynParamInt32, &ADTimePixPrvImg1Format);        
    createParam(ADTimePixPrvImg1ModeString,                asynParamInt32, &ADTimePixPrvImg1Mode);          
    createParam(ADTimePixPrvImg1ThsString,                 asynParamOctet, &ADTimePixPrvImg1Ths);         
    createParam(ADTimePixPrvImg1IntSizeString,             asynParamInt32, &ADTimePixPrvImg1IntSize);    
    createParam(ADTimePixPrvImg1IntModeString,             asynParamInt32, &ADTimePixPrvImg1IntMode);
    createParam(ADTimePixPrvImg1StpOnDskLimString,         asynParamInt32, &ADTimePixPrvImg1StpOnDskLim); 
    createParam(ADTimePixPrvImg1QueueSizeString,           asynParamInt32, &ADTimePixPrvImg1QueueSize);   
    createParam(ADTimePixPrvImg1FilePathExistsString,      asynParamInt32, &ADTimePixPrvImg1FilePathExists); 
    // Server, Preview, HistogramChannels[0]  
    createParam(ADTimePixPrvHstBaseString,                   asynParamOctet,    &ADTimePixPrvHstBase);
    createParam(ADTimePixPrvHstFilePatString,                asynParamOctet,    &ADTimePixPrvHstFilePat);
    createParam(ADTimePixPrvHstFormatString,                 asynParamInt32,    &ADTimePixPrvHstFormat);
    createParam(ADTimePixPrvHstModeString,                   asynParamInt32,    &ADTimePixPrvHstMode);
    createParam(ADTimePixPrvHstThsString,                    asynParamOctet,    &ADTimePixPrvHstThs);
    createParam(ADTimePixPrvHstIntSizeString,                asynParamInt32,    &ADTimePixPrvHstIntSize);
    createParam(ADTimePixPrvHstIntModeString,                asynParamInt32,    &ADTimePixPrvHstIntMode);
    createParam(ADTimePixPrvHstStpOnDskLimString,            asynParamInt32,    &ADTimePixPrvHstStpOnDskLim);
    createParam(ADTimePixPrvHstQueueSizeString,              asynParamInt32,    &ADTimePixPrvHstQueueSize);
    createParam(ADTimePixPrvHstNumBinsString,                asynParamInt32,    &ADTimePixPrvHstNumBins);
    createParam(ADTimePixPrvHstBinWidthString,               asynParamFloat64,  &ADTimePixPrvHstBinWidth);
    createParam(ADTimePixPrvHstOffsetString,                 asynParamFloat64,  &ADTimePixPrvHstOffset);
    createParam(ADTimePixPrvHstFilePathExistsString,         asynParamInt32,    &ADTimePixPrvHstFilePathExists);
    // PrvHst histogram data arrays
    createParam(ADTimePixPrvHstHistogramDataString,          asynParamInt64Array, &ADTimePixPrvHstHistogramData);
    createParam(ADTimePixPrvHstHistogramFrameString,         asynParamInt32Array, &ADTimePixPrvHstHistogramFrame);
    createParam(ADTimePixPrvHstHistogramSumNFramesString,     asynParamInt64Array, &ADTimePixPrvHstHistogramSumNFrames);
    createParam(ADTimePixPrvHstHistogramTimeMsString,        asynParamFloat64Array, &ADTimePixPrvHstHistogramTimeMs);
    createParam(ADTimePixPrvHstAccumulationEnableString,     asynParamInt32, &ADTimePixPrvHstAccumulationEnable);
    // PrvHst metadata from jsonhisto
    createParam(ADTimePixPrvHstTimeAtFrameString,            asynParamFloat64, &ADTimePixPrvHstTimeAtFrame);
    createParam(ADTimePixPrvHstFrameBinSizeString,          asynParamInt32, &ADTimePixPrvHstFrameBinSize);
    createParam(ADTimePixPrvHstFrameBinWidthString,          asynParamInt32, &ADTimePixPrvHstFrameBinWidth);
    createParam(ADTimePixPrvHstFrameBinOffsetString,          asynParamInt32, &ADTimePixPrvHstFrameBinOffset);
    // PrvHst accumulation statistics
    createParam(ADTimePixPrvHstFrameCountString,              asynParamInt32, &ADTimePixPrvHstFrameCount);
    createParam(ADTimePixPrvHstTotalCountsString,             asynParamInt64, &ADTimePixPrvHstTotalCounts);
    createParam(ADTimePixPrvHstAcqRateString,                asynParamFloat64, &ADTimePixPrvHstAcqRate);
    createParam(ADTimePixPrvHstProcessingTimeString,          asynParamFloat64, &ADTimePixPrvHstProcessingTime);
    createParam(ADTimePixPrvHstMemoryUsageString,            asynParamFloat64, &ADTimePixPrvHstMemoryUsage);
    createParam(ADTimePixPrvHstFramesToSumString,            asynParamInt32, &ADTimePixPrvHstFramesToSum);
    createParam(ADTimePixPrvHstSumUpdateIntervalString,      asynParamInt32, &ADTimePixPrvHstSumUpdateInterval);
    createParam(ADTimePixPrvHstDataResetString,               asynParamInt32, &ADTimePixPrvHstDataReset);

    // Measurement
    createParam(ADTimePixPelRateString,                     asynParamInt32,     &ADTimePixPelRate);      
    createParam(ADTimePixTdc1RateString,                    asynParamInt32,     &ADTimePixTdc1Rate);
    createParam(ADTimePixTdc2RateString,                    asynParamInt32,     &ADTimePixTdc2Rate);      
    createParam(ADTimePixStartTimeString,                   asynParamInt64,     &ADTimePixStartTime);    
    createParam(ADTimePixElapsedTimeString,                 asynParamFloat64,   &ADTimePixElapsedTime);  
    createParam(ADTimePixTimeLeftString,                    asynParamFloat64,   &ADTimePixTimeLeft);     
    createParam(ADTimePixFrameCountString,                  asynParamInt32,     &ADTimePixFrameCount);   
    createParam(ADTimePixDroppedFramesString,               asynParamInt32,     &ADTimePixDroppedFrames);
    createParam(ADTimePixStatusString,                      asynParamOctet,     &ADTimePixStatus);
    // Measurement.Config (Stem, TimeOfFlight)
    createParam(ADTimePixStemScanWidthString,              asynParamInt32,     &ADTimePixStemScanWidth);
    createParam(ADTimePixStemScanHeightString,             asynParamInt32,     &ADTimePixStemScanHeight);
    createParam(ADTimePixStemDwellTimeString,              asynParamFloat64,   &ADTimePixStemDwellTime);
    createParam(ADTimePixStemRadiusOuterString,            asynParamInt32,     &ADTimePixStemRadiusOuter);
    createParam(ADTimePixStemRadiusInnerString,            asynParamInt32,     &ADTimePixStemRadiusInner);
    createParam(ADTimePixTofTdcReferenceString,            asynParamOctet,     &ADTimePixTofTdcReference);
    createParam(ADTimePixTofMinString,                     asynParamFloat64,   &ADTimePixTofMin);
    createParam(ADTimePixTofMaxString,                     asynParamFloat64,   &ADTimePixTofMax);
    
    // BPC Mask
    createParam(ADTimePixBPCString,                  asynParamInt32, &ADTimePixBPC);
    createParam(ADTimePixBPCnString,                 asynParamInt32, &ADTimePixBPCn);
    createParam(ADTimePixBPCmaskedString,            asynParamInt32, &ADTimePixBPCmasked);
    createParam(ADTimePixMaskBPCString,              asynParamInt32, &ADTimePixMaskBPC);
    createParam(ADTimePixMaskOnOffPelString,         asynParamInt32, &ADTimePixMaskOnOffPel);
    createParam(ADTimePixMaskResetString,            asynParamInt32, &ADTimePixMaskReset);
    createParam(ADTimePixMaskMinXString,             asynParamInt32, &ADTimePixMaskMinX);
    createParam(ADTimePixMaskSizeXString,            asynParamInt32, &ADTimePixMaskSizeX);
    createParam(ADTimePixMaskMinYString,             asynParamInt32, &ADTimePixMaskMinY);
    createParam(ADTimePixMaskSizeYString,            asynParamInt32, &ADTimePixMaskSizeY);
    createParam(ADTimePixMaskRadiusString,           asynParamInt32, &ADTimePixMaskRadius);
    createParam(ADTimePixMaskRectangleString,        asynParamInt32, &ADTimePixMaskRectangle);
    createParam(ADTimePixMaskCircleString,           asynParamInt32, &ADTimePixMaskCircle);
    createParam(ADTimePixMaskFileNameString,         asynParamOctet, &ADTimePixMaskFileName);
    createParam(ADTimePixMaskPelString,              asynParamInt32, &ADTimePixMaskPel);
    createParam(ADTimePixMaskWriteString,            asynParamInt32, &ADTimePixMaskWrite);

    // Controls
    createParam(ADTimePixRawStreamString,       asynParamInt32,     &ADTimePixRawStream);
    createParam(ADTimePixRaw1StreamString,      asynParamInt32,     &ADTimePixRaw1Stream);
    createParam(ADTimePixPrvHstStreamString,    asynParamInt32,     &ADTimePixPrvHstStream);
    createParam(ADTimePixRefreshConnectionString, asynParamInt32, &ADTimePixRefreshConnection);
    createParam(ADTimePixApplyConfigString, asynParamInt32, &ADTimePixApplyConfig);
    createParam(ADTimePixWriteProcessedImgString, asynParamInt32, &ADTimePixWriteProcessedImg);
    createParam(ADTimePixProcessedImgOutputTypeString, asynParamInt32, &ADTimePixProcessedImgOutputType);
    createParam(ADTimePixWriteProcessedHstString, asynParamInt32, &ADTimePixWriteProcessedHst);
    createParam(ADTimePixProcessedHstOutputTypeString, asynParamInt32, &ADTimePixProcessedHstOutputType);
    createParam(ADTimePixRefreshPixelConfigString, asynParamInt32, &ADTimePixRefreshPixelConfig);
    createParam(ADTimePixPixelConfigLenString, asynParamInt32, &ADTimePixPixelConfigLen);
    createParam(ADTimePixPixelConfigMatchBPCString, asynParamInt32, &ADTimePixPixelConfigMatchBPC);
    createParam(ADTimePixPixelConfigMismatchBytesString, asynParamInt64, &ADTimePixPixelConfigMismatchBytes);
    createParam(ADTimePixPixelConfigStatusString, asynParamOctet, &ADTimePixPixelConfigStatus);
    createParam(ADTimePixPixelConfigDiffString, asynParamInt32Array, &ADTimePixPixelConfigDiff);
    createParam(ADTimePixMaskedPelsJsonPathString, asynParamOctet, &ADTimePixMaskedPelsJsonPath);
    createParam(ADTimePixMaskedPelsCountString, asynParamInt32, &ADTimePixMaskedPelsCount);
    createParam(ADTimePixMaskedPelsExportStatusString, asynParamOctet, &ADTimePixMaskedPelsExportStatus);

    //sets driver version
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADTIMEPIX_VERSION, ADTIMEPIX_REVISION, ADTIMEPIX_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);
    setStringParam(ADTimePixServerName, serverURL);

    // Initialize TCP streaming for PrvImg channel
    prvImgNetworkClient_.reset();
    prvImgHost_ = "";
    prvImgPort_ = 0;
    prvImgConnected_ = false;
    prvImgRunning_ = false;
    prvImgWorkerThreadId_ = nullptr;
    prvImgMutex_ = epicsMutexMustCreate();
    if (!prvImgMutex_) {
        ERR("Failed to create PrvImg mutex");
    }
    prvImgLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvImgTotalRead_ = 0;
    prvImgFormat_ = 0;
    
    // Initialize PrvImg metadata tracking
    prvImgPreviousFrameNumber_ = 0;
    prvImgPreviousTimeAtFrame_ = 0.0;
    prvImgAcquisitionRate_ = 0.0;
    prvImgLastRateUpdateTime_ = 0.0;
    prvImgFirstFrameReceived_ = false;
    
    // Initialize TCP streaming for Img channel
    imgNetworkClient_.reset();
    imgHost_ = "";
    imgPort_ = 0;
    imgConnected_ = false;
    imgRunning_ = false;
    imgWorkerThreadId_ = nullptr;
    imgMutex_ = epicsMutexMustCreate();
    
    // Initialize PrvHst TCP streaming
    prvHstMutex_ = epicsMutexMustCreate();
    if (!prvHstMutex_) {
        ERR("Failed to create PrvHst mutex");
    }
    pixelConfigDiffMutex_ = epicsMutexMustCreate();
    if (!pixelConfigDiffMutex_) {
        ERR("Failed to create PixelConfig diff mutex");
    }
    pixelConfigDiff_.assign(262144, 0);
    prvHstNetworkClient_.reset();
    prvHstHost_ = "";
    prvHstPort_ = 0;
    prvHstConnected_ = false;
    prvHstRunning_ = false;
    prvHstWorkerThreadId_ = NULL;
    prvHstLineBuffer_.clear();
    prvHstTotalRead_ = 0;
    prvHstFormat_ = 0;
    
    // Initialize PrvHst metadata tracking
    prvHstPreviousFrameNumber_ = 0;
    prvHstPreviousTimeAtFrame_ = 0.0;
    prvHstAcquisitionRate_ = 0.0;
    prvHstRateSamples_.clear();
    prvHstLastRateUpdateTime_ = 0.0;
    prvHstFirstFrameReceived_ = false;
    
    // Initialize PrvHst histogram data
    prvHstRunningSum_.reset();
    prvHstFrameBuffer_.clear();
    // prvHstCurrentFrame_ will be initialized when first frame is received
    prvHstFramesToSum_ = 10;  // Default: sum last 10 frames
    prvHstSumUpdateIntervalFrames_ = 1;  // Default: update every frame
    prvHstFramesSinceLastSumUpdate_ = 0;
    prvHstTotalCounts_ = 0;
    prvHstFrameCount_ = 0;  // Initialize frame count
    
    // Initialize PrvHst frame data from JSON
    prvHstTimeAtFrame_ = 0.0;
    prvHstFrameBinSize_ = 0;
    prvHstFrameBinWidth_ = 0;
    prvHstFrameBinOffset_ = 0;
    
    // Initialize PrvHst performance tracking
    prvHstProcessingTimeSamples_.clear();
    prvHstLastProcessingTimeUpdate_ = 0.0;
    prvHstProcessingTime_ = 0.0;
    prvHstLastMemoryUpdateTime_ = 0.0;
    prvHstMemoryUsage_ = 0.0;
    
    // Initialize PrvHst buffers
    prvHstArrayData32Buffer_.clear();
    prvHstSumArray64Buffer_.clear();
    prvHstSumArray64WorkBuffer_.clear();
    prvHstTimeMsBuffer_.clear();
    if (!imgMutex_) {
        ERR("Failed to create Img mutex");
    }
    imgLineBuffer_.resize(MAX_BUFFER_SIZE);
    imgTotalRead_ = 0;
    imgFormat_ = 0;
    
    // Initialize Img metadata tracking
    imgPreviousFrameNumber_ = 0;
    imgPreviousTimeAtFrame_ = 0.0;
    imgAcquisitionRate_ = 0.0;
    imgLastRateUpdateTime_ = 0.0;
    imgFirstFrameReceived_ = false;
    
    // Initialize Img channel accumulation and frame buffer
    imgRunningSum_.reset();
    imgFrameBuffer_.clear();
    // imgCurrentFrame_ is initialized in constructor initialization list
    imgFramesToSum_ = 10;
    imgSumUpdateIntervalFrames_ = 1;
    imgFramesSinceLastSumUpdate_ = 0;
    
    // Initialize Img channel performance tracking
    imgProcessingTimeSamples_.clear();
    imgLastProcessingTimeUpdate_ = 0.0;
    imgLastMemoryUpdateTime_ = 0.0;
    imgProcessingTime_ = 0.0;
    imgMemoryUsage_ = 0.0;
    imgTotalCounts_ = 0;
    imgAccumulatedFrameCount_ = 0;
    
    // Set initial parameter values
    setIntegerParam(ADTimePixImgAccumulationEnable, 1);  // Default: enabled
    setIntegerParam(ADTimePixPrvHstAccumulationEnable, 1);  // Default: enabled
    setIntegerParam(ADTimePixImgFramesToSum, imgFramesToSum_);
    setIntegerParam(ADTimePixImgSumUpdateIntervalFrames, imgSumUpdateIntervalFrames_);
    setInteger64Param(ADTimePixImgTotalCounts, 0);
    setDoubleParam(ADTimePixImgProcessingTime, 0.0);
    // Calculate initial memory usage (will be 0.0 initially since buffers are empty)
    imgMemoryUsage_ = calculateImgMemoryUsageMB();
    setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
    setIntegerParam(ADTimePixWriteProcessedImg, 0);
    setIntegerParam(ADTimePixProcessedImgOutputType, 0);  // 0=Sum (NDInt64), 1=Average (NDInt32)
    setIntegerParam(ADTimePixWriteProcessedHst, 0);
    setIntegerParam(ADTimePixProcessedHstOutputType, 0);
    // Initialize NumImages to 0 (unlimited) for continuous mode
    // This prevents INVALID status from very large default values
    setIntegerParam(ADNumImages, 0);

//    callParamCallbacks();   // Apply to EPICS, at end of file

    if(strlen(serverURL) <= 0){
        ERR("Connection failed, abort");
    }
// asynSuccess = 0, so use !0 for true/connected    
    else{
        asynStatus connected = initialServerCheckConnection();
        if(connected == asynSuccess) {
            FLOW("Acquiring device information");
        //    getDashboard(serverURL); 
            printf("Dashboard done HERE!\n\n");
        //    getServer();
            printf("Server done HERE!\n\n");
        //    initCamera(); /* Used for testing and emulator, replaced with loadFile for BPC and Chip/DACS */
            printf("initCamera done HERE!\n\n");
        }
    }

    // Connection poll (CONNECT/DISCONNECT): lightweight periodic check, optional config refresh on reconnect
    connectionPollThreadId_ = NULL;
    connectionPollEvent_ = epicsEventMustCreate(epicsEventEmpty);
    connectionPollPeriodSec_ = 5.0;
    connectionPollEnable_ = 1;
    getIntegerParam(ADTimePixServalConnected, &lastServalConnected_);
    getIntegerParam(ADTimePixDetConnected, &lastDetConnected_);
    /* Skip first poll checkConnection: initialServerCheckConnection already GET /dashboard when URL set. */
    connectionPollSkipOnce_ = (strlen(serverURL) > 0) ? 1 : 0;
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.priority = epicsThreadPriorityLow;
    opts.stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    opts.joinable = 1;  // Required so destructor can epicsThreadMustJoin
    connectionPollThreadId_ = epicsThreadCreateOpt("connectionPoll", connectionPollThreadC, this, &opts);
    if (!connectionPollThreadId_) {
        ERR("Failed to create connection poll thread");
    }

    // When ASYN_DESTRUCTIBLE (asyn R4-45+, ADCore destructible support): asyn performs teardown; do not register epicsAtExit.
    // Otherwise (older asyn or driver created without the flag): register exit callback so the driver is deleted and destructor runs.
#ifdef ASYN_DESTRUCTIBLE
    if ((asynFlags_ & ASYN_DESTRUCTIBLE) == 0)
#endif
    {
        epicsAtExit(exitCallbackC, this);
    }
}


void ADTimePix::shutdownPortDriver() {
    FLOW("ADTimePix shutdownPortDriver");

    // Stop callback thread first so it cannot touch driver state during teardown
    this->acquiring = false;
    if (this->callbackThreadId != NULL && this->callbackThreadId != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(this->callbackThreadId);
        this->callbackThreadId = NULL;
    }

    // Stop connection poll thread
    connectionPollEnable_ = 0;
    if (connectionPollEvent_) {
        epicsEventSignal(connectionPollEvent_);
    }
    if (connectionPollThreadId_ != NULL && connectionPollThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(connectionPollThreadId_);
        connectionPollThreadId_ = NULL;
    }

    // Stop PrvImg TCP streaming
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
    }
    if (prvImgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    prvImgDisconnect();

    // Stop Img TCP streaming
    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        epicsMutexUnlock(imgMutex_);
    }
    if (imgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    imgDisconnect();

    // Stop PrvHst TCP streaming
    if (prvHstMutex_) {
        epicsMutexLock(prvHstMutex_);
        prvHstRunning_ = false;
        epicsMutexUnlock(prvHstMutex_);
    }
    if (prvHstWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(prvHstWorkerThreadId_);
        prvHstWorkerThreadId_ = NULL;
    }
    prvHstDisconnect();

#ifdef ASYN_DESTRUCTIBLE
    asynPortDriver::shutdownPortDriver();
#endif
}

ADTimePix::~ADTimePix(){
    FLOW("ADTimePix driver exiting");

    // Stop callback thread first so it cannot touch driver state during teardown (idempotent if shutdownPortDriver already ran)
    this->acquiring = false;
    if (this->callbackThreadId != NULL && this->callbackThreadId != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(this->callbackThreadId);
        this->callbackThreadId = NULL;
    }

    // Stop connection poll thread so it cannot run during teardown (avoids SIGSEGV)
    connectionPollEnable_ = 0;
    if (connectionPollEvent_) {
        epicsEventSignal(connectionPollEvent_);
    }
    if (connectionPollThreadId_ != NULL && connectionPollThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(connectionPollThreadId_);
        connectionPollThreadId_ = NULL;
    }
    if (connectionPollEvent_) {
        epicsEventDestroy(connectionPollEvent_);
        connectionPollEvent_ = NULL;
    }

    // Stop PrvImg TCP streaming
    epicsMutexLock(prvImgMutex_);
    prvImgRunning_ = false;
    epicsMutexUnlock(prvImgMutex_);
    
    if (prvImgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    prvImgDisconnect();
    
    if (prvImgMutex_) {
        epicsMutexDestroy(prvImgMutex_);
        prvImgMutex_ = NULL;
    }
    
    // Stop Img TCP streaming
    epicsMutexLock(imgMutex_);
    imgRunning_ = false;
    epicsMutexUnlock(imgMutex_);
    
    if (imgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    imgDisconnect();
    
    if (imgMutex_) {
        epicsMutexDestroy(imgMutex_);
        imgMutex_ = NULL;
    }
    
    // Stop PrvHst TCP streaming
    epicsMutexLock(prvHstMutex_);
    prvHstRunning_ = false;
    epicsMutexUnlock(prvHstMutex_);
    
    if (prvHstWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(prvHstWorkerThreadId_);
        prvHstWorkerThreadId_ = NULL;
    }
    prvHstDisconnect();
    
    if (prvHstMutex_) {
        epicsMutexDestroy(prvHstMutex_);
        prvHstMutex_ = NULL;
    }

    if (pixelConfigDiffMutex_) {
        epicsMutexDestroy(pixelConfigDiffMutex_);
        pixelConfigDiffMutex_ = NULL;
    }

    // Do not call disconnect(this->pasynUserSelf) here. It can trigger asyn disconnect
    // handling (e.g. callbacks) that may touch driver state or param lists after we have
    // already torn down our resources, leading to SIGSEGV on IOC exit. The port is torn
    // down by the base destructors (~asynNDArrayDriver, ~asynPortDriver).
}


//-------------------------------------------------------------
// ADTimePix ioc shell registration
//-------------------------------------------------------------

/* ADTimePixConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg ADTimePixConfigArg0 = { "Port name",        iocshArgString };
static const iocshArg ADTimePixConfigArg1 = { "Server URL",       iocshArgString };
static const iocshArg ADTimePixConfigArg2 = { "maxBuffers",       iocshArgInt };
static const iocshArg ADTimePixConfigArg3 = { "maxMemory",        iocshArgInt };
static const iocshArg ADTimePixConfigArg4 = { "priority",         iocshArgInt };
static const iocshArg ADTimePixConfigArg5 = { "stackSize",        iocshArgInt };
static const iocshArg ADTimePixConfigArg6 = { "asynFlags",        iocshArgInt };


/* Array of config args */
static const iocshArg * const ADTimePixConfigArgs[] =
        { &ADTimePixConfigArg0, &ADTimePixConfigArg1, &ADTimePixConfigArg2,
        &ADTimePixConfigArg3, &ADTimePixConfigArg4, &ADTimePixConfigArg5 };

static const iocshArg * const ADTimePixConfigWithFlagsArgs[] =
        { &ADTimePixConfigArg0, &ADTimePixConfigArg1, &ADTimePixConfigArg2,
        &ADTimePixConfigArg3, &ADTimePixConfigArg4, &ADTimePixConfigArg5, &ADTimePixConfigArg6 };


/* what function to call at config */
static void configADTimePixCallFunc(const iocshArgBuf *args){
    ADTimePixConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}

static void configADTimePixWithFlagsCallFunc(const iocshArgBuf *args){
    ADTimePixConfigWithFlags(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, args[5].ival, args[6].ival);
}


/* information about the configuration function */
static const iocshFuncDef configADTimePix = { "ADTimePixConfig", 6, ADTimePixConfigArgs };
static const iocshFuncDef configADTimePixWithFlags = { "ADTimePixConfigWithFlags", 7, ADTimePixConfigWithFlagsArgs };


/* IOC register function */
static void ADTimePixRegister(void) {
    iocshRegister(&configADTimePix, configADTimePixCallFunc);
    iocshRegister(&configADTimePixWithFlags, configADTimePixWithFlagsCallFunc);
}


/* external function for IOC register */
extern "C" {
    epicsExportRegistrar(ADTimePixRegister);
}
