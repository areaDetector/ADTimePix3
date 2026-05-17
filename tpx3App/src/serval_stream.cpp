/*
 * ADTimePix3 - PrvImg / Img TCP jsonimage streaming (Serval)
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "network_client.h"

#include <NDAttribute.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include <epicsTime.h>
#include <epicsThread.h>

#include <json.hpp>

using json = nlohmann::json;

extern const char* driverName;

bool ADTimePix::parseTcpPath(const std::string& filePath, std::string& host, int& port) {
    // Parse tcp://listen@hostname:port or tcp://hostname:port
    // Examples: tcp://listen@localhost:8089, tcp://127.0.0.1:8089
    
    if (filePath.find("tcp://") != 0) {
        return false;
    }
    
    std::string remaining = filePath.substr(6); // Remove "tcp://"
    
    // Handle "listen@hostname:port" format
    size_t at_pos = remaining.find('@');
    if (at_pos != std::string::npos) {
        remaining = remaining.substr(at_pos + 1); // Skip "listen@"
    }
    
    // Parse hostname:port
    size_t colon_pos = remaining.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    host = remaining.substr(0, colon_pos);
    std::string port_str = remaining.substr(colon_pos + 1);
    
    try {
        port = std::stoi(port_str);
        if (port < 1 || port > 65535) {
            ERR_ARGS("Invalid port number: %d (must be 1-65535)", port);
            return false;
        }
    } catch (const std::exception& e) {
        ERR_ARGS("Failed to parse port: %s", port_str.c_str());
        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------------
// TCP Streaming Implementation for PrvImg Channel
//----------------------------------------------------------------------------

void ADTimePix::prvImgWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->prvImgWorkerThread();
}

void ADTimePix::prvImgWorkerThread() {
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!prvImgMutex_) {
        ERR("PrvImg worker thread: Mutex not initialized");
        return;
    }
    
    prvImgLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvImgTotalRead_ = 0;
    
    while (prvImgRunning_) {
        epicsMutexLock(prvImgMutex_);
        bool should_connect = prvImgRunning_ && !prvImgConnected_;
        std::string host = prvImgHost_;
        int port = prvImgPort_;
        epicsMutexUnlock(prvImgMutex_);
        
        if (should_connect && !host.empty() && port > 0) {
            prvImgConnect();
        }
        
        if (!prvImgRunning_) {
            break;
        }
        
        epicsMutexLock(prvImgMutex_);
        bool connected = prvImgConnected_;
        epicsMutexUnlock(prvImgMutex_);
        
        if (connected && prvImgNetworkClient_) {
            try {
                epicsMutexLock(prvImgMutex_);
                ssize_t bytes_read = prvImgNetworkClient_->receive(
                    prvImgLineBuffer_.data() + prvImgTotalRead_,
                    MAX_BUFFER_SIZE - prvImgTotalRead_ - 1
                );
                epicsMutexUnlock(prvImgMutex_);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(prvImgMutex_);
                        prvImgConnected_ = false;
                        prvImgRunning_ = false;
                        epicsMutexUnlock(prvImgMutex_);
                        printf("PrvImg TCP connection closed by peer\n");
            break;
                    } else {
                        epicsMutexLock(prvImgMutex_);
                        if (prvImgConnected_) {
                            prvImgConnected_ = false;
                            prvImgRunning_ = false;
                            LOG_ARGS("PrvImg TCP socket error: %s", strerror(errno));
                        }
                        epicsMutexUnlock(prvImgMutex_);
            break;
                    }
                }
                
                epicsMutexLock(prvImgMutex_);
                prvImgTotalRead_ += bytes_read;
                prvImgLineBuffer_[prvImgTotalRead_] = '\0';
                
                // Look for newline to find complete JSON line
                char* newline_pos = static_cast<char*>(memchr(prvImgLineBuffer_.data(), '\n', prvImgTotalRead_));
                
                if (newline_pos) {
                    // Found a newline - check if there's valid JSON before it
                    char* json_start = nullptr;
                    
                    // Try to find {" pattern (most reliable indicator of JSON)
                    for (char* p = prvImgLineBuffer_.data(); p < newline_pos - 1; ++p) {
                        if (*p == '{' && p[1] == '"') {
                            json_start = p;
            break;
        }
                    }
                    
                    // If we didn't find {", try finding { followed by valid JSON structure
                    if (!json_start) {
                        for (char* p = prvImgLineBuffer_.data(); p < newline_pos - 2; ++p) {
                            if (*p == '{') {
                                bool looks_like_json = false;
                                size_t check_len = std::min(size_t(newline_pos - p - 1), size_t(100));
                                
                                int json_chars = 0;
                                for (size_t i = 1; i < check_len; ++i) {
                                    char c = p[i];
                                    if (c == '"' || c == ':' || c == ',' || c == '}' || c == '[' || c == ']') {
                                        looks_like_json = true;
            break;
                                    }
                                    if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.') {
                                        json_chars++;
                                    } else if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            break;
                                    }
                                }
                                
                                if (looks_like_json || json_chars > 5) {
                                    json_start = p;
            break;
                                }
                            }
                        }
                    }
                    
                    bool valid_json_start = (json_start != nullptr);
                    
                    if (valid_json_start) {
                        // Try to parse the JSON to verify it's valid
                        bool is_valid_json = false;
                        try {
                            std::string json_str(json_start, newline_pos - json_start);
                            json test_json = json::parse(json_str);
                            if (test_json.contains("width") || test_json.contains("frameNumber") ||
                                test_json.contains("height") || test_json.contains("timeAtFrame")) {
                                is_valid_json = true;
                            }
                        } catch (...) {
                            is_valid_json = false;
                        }
                        
                        if (is_valid_json) {
                            *newline_pos = '\0';
                            
                            // Process the JSON line
                            if (!processPrvImgDataLine(json_start, newline_pos, prvImgTotalRead_)) {
                                epicsMutexUnlock(prvImgMutex_);
            break;
        }
                            
                            // Move remaining data to start of buffer
                            size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvImgTotalRead_ = remaining;
                        } else {
                            // Found { but it's not valid JSON - skip this newline
                            size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvImgTotalRead_ = remaining;
                        }
                    } else {
                        // Found newline but no valid JSON - might be binary data
                        size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                        if (remaining > 0) {
                            memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                        }
                        prvImgTotalRead_ = remaining;
                    }
                } else {
                    // No newline found yet - check if buffer is getting too full
                    if (prvImgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                        LOG("PrvImg TCP buffer full without finding newline, resetting");
                        prvImgTotalRead_ = 0;
                    }
                }
                
                if (prvImgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    LOG("PrvImg TCP buffer full, resetting");
                    prvImgTotalRead_ = 0;
                }
                
                epicsMutexUnlock(prvImgMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(prvImgMutex_);
                ERR_ARGS("Error in PrvImg worker thread: %s", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    prvImgDisconnect();
    LOG("PrvImg worker thread exiting");
}

void ADTimePix::imgWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->imgWorkerThread();
}

void ADTimePix::imgWorkerThread() {
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!imgMutex_) {
        ERR("Img worker thread: Mutex not initialized");
        return;
    }
    
    imgLineBuffer_.resize(MAX_BUFFER_SIZE);
    imgTotalRead_ = 0;
    
    while (imgRunning_) {
        epicsMutexLock(imgMutex_);
        bool should_connect = imgRunning_ && !imgConnected_;
        std::string host = imgHost_;
        int port = imgPort_;
        epicsMutexUnlock(imgMutex_);
        
        if (should_connect && !host.empty() && port > 0) {
            imgConnect();
        }
        
        if (!imgRunning_) {
            break;
        }
        
        epicsMutexLock(imgMutex_);
        bool connected = imgConnected_;
        epicsMutexUnlock(imgMutex_);
        
        if (connected && imgNetworkClient_) {
            try {
                epicsMutexLock(imgMutex_);
                ssize_t bytes_read = imgNetworkClient_->receive(
                    imgLineBuffer_.data() + imgTotalRead_,
                    MAX_BUFFER_SIZE - imgTotalRead_ - 1
                );
                epicsMutexUnlock(imgMutex_);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(imgMutex_);
                        imgConnected_ = false;
                        imgRunning_ = false;
                        epicsMutexUnlock(imgMutex_);
                        printf("Img TCP connection closed by peer\n");
                        break;
                    } else {
                        epicsMutexLock(imgMutex_);
                        if (imgConnected_) {
                            imgConnected_ = false;
                            imgRunning_ = false;
                            LOG_ARGS("Img TCP socket error: %s", strerror(errno));
                        }
                        epicsMutexUnlock(imgMutex_);
                        break;
                    }
                }
                
                epicsMutexLock(imgMutex_);
                imgTotalRead_ += bytes_read;
                imgLineBuffer_[imgTotalRead_] = '\0';
                
                // Look for newline to find complete JSON line
                char* newline_pos = static_cast<char*>(memchr(imgLineBuffer_.data(), '\n', imgTotalRead_));
                
                if (newline_pos) {
                    // Found a newline - check if there's valid JSON before it
                    char* json_start = nullptr;
                    
                    // Try to find {" pattern (most reliable indicator of JSON)
                    for (char* p = imgLineBuffer_.data(); p < newline_pos - 1; ++p) {
                        if (*p == '{' && p[1] == '"') {
                            json_start = p;
                            break;
                        }
                    }
                    
                    // If we didn't find {", try finding { followed by valid JSON structure
                    if (!json_start) {
                        for (char* p = imgLineBuffer_.data(); p < newline_pos - 2; ++p) {
                            if (*p == '{') {
                                bool looks_like_json = false;
                                size_t check_len = std::min(size_t(newline_pos - p - 1), size_t(100));
                                
                                int json_chars = 0;
                                for (size_t i = 1; i < check_len; ++i) {
                                    char c = p[i];
                                    if (c == '"' || c == ':' || c == ',' || c == '}' || c == '[' || c == ']') {
                                        looks_like_json = true;
                                        break;
                                    }
                                    if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.') {
                                        json_chars++;
                                    } else if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                                        break;
                                    }
                                }
                                
                                if (looks_like_json || json_chars > 5) {
                                    json_start = p;
                                    break;
                                }
                            }
                        }
                    }
                    
                    bool valid_json_start = (json_start != nullptr);
                    
                    if (valid_json_start) {
                        // Try to parse the JSON to verify it's valid
                        bool is_valid_json = false;
                        try {
                            std::string json_str(json_start, newline_pos - json_start);
                            json test_json = json::parse(json_str);
                            if (test_json.contains("width") || test_json.contains("frameNumber") ||
                                test_json.contains("height") || test_json.contains("timeAtFrame")) {
                                is_valid_json = true;
                            }
                        } catch (...) {
                            is_valid_json = false;
                        }
                        
                        if (is_valid_json) {
                            *newline_pos = '\0';
                            
                            // Process the JSON line
                            if (!processImgDataLine(json_start, newline_pos, imgTotalRead_)) {
                                epicsMutexUnlock(imgMutex_);
                                break;
                            }
                            
                            // Move remaining data to start of buffer
                            size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            imgTotalRead_ = remaining;
                        } else {
                            // Found { but it's not valid JSON - skip this newline
                            size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            imgTotalRead_ = remaining;
                        }
                    } else {
                        // Found newline but no valid JSON - might be binary data
                        size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                        if (remaining > 0) {
                            memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                        }
                        imgTotalRead_ = remaining;
                    }
                } else {
                    // No newline found yet - check if buffer is getting too full
                    if (imgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                        LOG("Img TCP buffer full without finding newline, resetting");
                        imgTotalRead_ = 0;
                    }
                }
                
                if (imgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    LOG("Img TCP buffer full, resetting");
                    imgTotalRead_ = 0;
                }
                
                epicsMutexUnlock(imgMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(imgMutex_);
                ERR_ARGS("Error in Img worker thread: %s", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    imgDisconnect();
    LOG("Img worker thread exiting");
}

bool ADTimePix::processImgDataLine(char* line_buffer, char* newline_pos, size_t total_read) {
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
    
    // Skip non-printable characters until we find '{'
    while (*json_start != '\0' && *json_start != '{' &&
           (*json_start < 32 || *json_start > 126)) {
        json_start++;
    }
    
    if (*json_start == '\0' || *json_start != '{') {
        return true;
    }
    
    json j;
    try {
        j = json::parse(json_start);
    } catch (const json::parse_error& e) {
        if (*json_start == '{') {
            ERR_ARGS("JSON parse error: %s", e.what());
        }
        return true;
    }
    
    try {
        // Extract header information for jsonimage
        int width = j["width"];
        int height = j["height"];
        std::string pixel_format_str = j.value("pixelFormat", "uint16");
        
        // Extract additional frame data
        int frame_number = j.value("frameNumber", 0);
        double time_at_frame = j.value("timeAtFrame", 0.0);
        
        // Determine pixel format
        bool is_uint32 = (pixel_format_str == "uint32" || pixel_format_str == "UINT32");
        NDDataType_t dataType = is_uint32 ? NDUInt32 : NDUInt16;
        
        // Calculate pixel data size
        size_t pixel_count = width * height;
        size_t bytes_per_pixel = is_uint32 ? sizeof(uint32_t) : sizeof(uint16_t);
        size_t binary_needed = pixel_count * bytes_per_pixel;
        
        // Validate dimensions
        if (width <= 0 || height <= 0 || width > 100000 || height > 100000) {
            ERR_ARGS("Invalid image dimensions: width=%d, height=%d", width, height);
            return false;
        }
        
        // Create NDArray - check if pool is available
        if (!this->pNDArrayPool) {
            ERR("NDArray pool is not available");
            return false;
        }
        
        size_t dims[3];
        dims[0] = width;
        dims[1] = height;
        dims[2] = 0;
        
        NDArray *pImage = nullptr;
        // Use pArrays[1] for Img channel to avoid conflict with PrvImg (pArrays[0])
        if (this->pArrays && this->pArrays[1]) {
            pImage = this->pArrays[1];
            pImage->release();
        }
        
        this->pArrays[1] = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        pImage = this->pArrays[1];
        
        if (!pImage || !pImage->pData) {
            ERR("Failed to allocate NDArray or NDArray has no data pointer");
            return false;
        }
        
        // Copy any binary data we already have after the newline
        size_t remaining = total_read - (newline_pos - line_buffer + 1);
        size_t binary_read = 0;
        
        std::vector<char> pixel_buffer(binary_needed);
        
        if (remaining > 0) {
            size_t to_copy = std::min(remaining, binary_needed);
            memcpy(pixel_buffer.data(), newline_pos + 1, to_copy);
            binary_read = to_copy;
        }
        
        // Read any remaining binary data needed
        epicsMutexLock(imgMutex_);
        if (binary_read < binary_needed && imgNetworkClient_ && imgNetworkClient_->is_connected()) {
            if (!imgNetworkClient_->receive_exact(
                pixel_buffer.data() + binary_read,
                binary_needed - binary_read)) {
                epicsMutexUnlock(imgMutex_);
                ERR("Failed to read binary pixel data");
                return false;
            }
        }
        epicsMutexUnlock(imgMutex_);
        
        // Validate pixel buffer size
        if (pixel_buffer.size() < binary_needed) {
            ERR_ARGS("Pixel buffer too small: have %zu, need %zu", pixel_buffer.size(), binary_needed);
            return false;
        }
        
        // Convert network byte order to host byte order and copy to NDArray
        if (!pImage->pData) {
            ERR("NDArray pData is null");
            return false;
        }
        
        if (is_uint32) {
            uint32_t* pixels = reinterpret_cast<uint32_t*>(pixel_buffer.data());
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap32(pixels[i]);
            }
        } else {
            uint16_t* pixels = reinterpret_cast<uint16_t*>(pixel_buffer.data());
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap16(pixels[i]);
            }
        }
        
        // Set image parameters (thread-safe via asynPortDriver)
        setIntegerParam(ADSizeX, width);
        setIntegerParam(NDArraySizeX, width);
        setIntegerParam(ADSizeY, height);
        setIntegerParam(NDArraySizeY, height);
        
        // Set data type
        int dataTypeValue = (int)dataType;
        setIntegerParam(NDDataType, dataTypeValue);
        setIntegerParam(NDColorMode, NDColorModeMono);
        
        NDArrayInfo_t arrayInfo;
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize, (int)arrayInfo.totalBytes);
        
        // Increment array counter (thread-safe)
        int imagesAcquired = 0;
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        imagesAcquired++;
        setIntegerParam(NDArrayCounter, imagesAcquired);
        
        // Set timestamp
        pImage->uniqueId = frame_number;
        epicsTimeStamp timestamp;
        epicsTimeGetCurrent(&timestamp);
        pImage->timeStamp = timestamp.secPastEpoch + timestamp.nsec / 1.e9;
        updateTimeStamp(&pImage->epicsTS);
        
        // Set Img metadata PVs
        setIntegerParam(ADTimePixImgFrameNumber, frame_number);
        setDoubleParam(ADTimePixImgTimeAtFrame, time_at_frame);
        
        // Calculate acquisition rate
        epicsTimeStamp current_time;
        epicsTimeGetCurrent(&current_time);
        double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
        
        if (!imgFirstFrameReceived_) {
            imgPreviousFrameNumber_ = frame_number;
            imgPreviousTimeAtFrame_ = current_time_seconds;
            imgFirstFrameReceived_ = true;
            imgAcquisitionRate_ = 0.0;
        } else {
            int frame_diff = frame_number - imgPreviousFrameNumber_;
            double time_diff_seconds = current_time_seconds - imgPreviousTimeAtFrame_;
            
            if (frame_diff > 1) {
                LOG_ARGS("Img frame loss detected! Expected frame %d, got frame %d (lost %d frames)", 
                         imgPreviousFrameNumber_ + 1, frame_number, frame_diff - 1);
            }
            
            if (frame_diff > 0 && time_diff_seconds > 0.0) {
                double current_rate = frame_diff / time_diff_seconds;
                
                imgRateSamples_.push_back(current_rate);
                if (imgRateSamples_.size() > IMG_MAX_RATE_SAMPLES) {
                    imgRateSamples_.erase(imgRateSamples_.begin());
                }
                
                double sum = 0.0;
                for (size_t i = 0; i < imgRateSamples_.size(); ++i) {
                    sum += imgRateSamples_[i];
                }
                imgAcquisitionRate_ = sum / imgRateSamples_.size();
                
                if (current_time_seconds - imgLastRateUpdateTime_ >= 1.0) {
                    setDoubleParam(ADTimePixImgAcqRate, imgAcquisitionRate_);
                    imgLastRateUpdateTime_ = current_time_seconds;
                }
            }
            
            imgPreviousFrameNumber_ = frame_number;
            imgPreviousTimeAtFrame_ = current_time_seconds;
        }
        
        // Get attributes
        if (pImage->pAttributeList) {
            this->getAttributes(pImage->pAttributeList);
        }
        
        // NEW: Create ImageData from frame for accumulation
        ImageData::PixelFormat imgDataFormat = is_uint32 ? ImageData::PixelFormat::UINT32 : ImageData::PixelFormat::UINT16;
        ImageData frame_image(width, height, imgDataFormat, ImageData::DataType::FRAME_DATA);
        
        // Copy pixel data from NDArray to ImageData
        if (is_uint32) {
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            for (size_t y = 0; y < static_cast<size_t>(height); ++y) {
                for (size_t x = 0; x < static_cast<size_t>(width); ++x) {
                    size_t idx = y * width + x;
                    frame_image.set_pixel_32(x, y, pData[idx]);
                }
            }
        } else {
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            for (size_t y = 0; y < static_cast<size_t>(height); ++y) {
                for (size_t x = 0; x < static_cast<size_t>(width); ++x) {
                    size_t idx = y * width + x;
                    frame_image.set_pixel_16(x, y, pData[idx]);
                }
            }
        }
        
        // NEW: Process frame for accumulation (only if enabled)
        int accumulationEnable = 0;
        getIntegerParam(ADTimePixImgAccumulationEnable, &accumulationEnable);
        if (accumulationEnable) {
            processImgFrame(frame_image);
        }
        
        // Call parameter callbacks to update EPICS PVs (thread-safe)
        callParamCallbacks();
        
        // Trigger NDArray callbacks (thread-safe) - Img channel uses address 1
        int arrayCallbacks = 0;
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if (arrayCallbacks && pImage) {
            doCallbacksGenericPointer(pImage, NDArrayData, 1);
        }
        
        LOG_ARGS("Processed Img frame: width=%d, height=%d, format=%s, frame=%d, counter=%d", 
                 width, height, pixel_format_str.c_str(), frame_number, imagesAcquired);
        
    } catch (const std::exception& e) {
        ERR_ARGS("Error processing Img frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::processImgFrame(const ImageData& frame_data) {
    epicsTimeStamp processing_start_time;
    epicsTimeGetCurrent(&processing_start_time);
    // NDArrays to emit after releasing imgMutex_ (addresses 2 and 3)
    NDArray* pImgSumArray = nullptr;   // running sum (ImgImageData) on addr 2
    NDArray* pImgSumNArray = nullptr;  // sum of N frames (ImgImageSumNFrames) on addr 3
    bool emitImgSumArray = false;
    bool emitImgSumNArray = false;

    epicsMutexLock(imgMutex_);
    
    // Initialize running sum if needed
    if (!imgRunningSum_) {
        imgRunningSum_.reset(new ImageData(
            frame_data.get_width(), 
            frame_data.get_height(),
            frame_data.get_pixel_format(),
            ImageData::DataType::RUNNING_SUM
        ));
    }
    
    // Check for dimension mismatch
    if (imgRunningSum_->get_width() != frame_data.get_width() || 
        imgRunningSum_->get_height() != frame_data.get_height()) {
        WARN_ARGS("Img image size mismatch! Running sum has %zux%zu, frame has %zux%zu. Reinitializing running sum.",
                  imgRunningSum_->get_width(), imgRunningSum_->get_height(),
                  frame_data.get_width(), frame_data.get_height());
        
        imgRunningSum_.reset(new ImageData(
            frame_data.get_width(), 
            frame_data.get_height(),
            frame_data.get_pixel_format(),
            ImageData::DataType::RUNNING_SUM
        ));
        
        // Reinitialize current frame with new dimensions
        imgCurrentFrame_ = frame_data;
        
        imgTotalCounts_ = 0;
        imgAccumulatedFrameCount_ = 0;
        setInteger64Param(ADTimePixImgTotalCounts, 0);
    }
    
    // Add frame to running sum
    try {
        imgRunningSum_->add_image(frame_data);
    } catch (const std::exception& e) {
        ERR_ARGS("Failed to add image to running sum: %s", e.what());
        epicsMutexUnlock(imgMutex_);
        return;
    }
    
    // Store current frame for IMAGE_FRAME PV
    imgCurrentFrame_ = frame_data;
    
    // Calculate total counts for this frame
    size_t pixel_count = frame_data.get_pixel_count();
    uint64_t frame_total = 0;
    
    if (frame_data.get_pixel_format() == ImageData::PixelFormat::UINT16) {
        const uint16_t* frame_pixels = frame_data.get_pixels_16_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            frame_total += frame_pixels[i];
        }
    } else {
        const uint32_t* frame_pixels = frame_data.get_pixels_32_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            frame_total += frame_pixels[i];
        }
    }
    imgTotalCounts_ += frame_total;
    imgAccumulatedFrameCount_++;
    
    // Add to frame buffer (must be done BEFORE checking update condition)
    imgFrameBuffer_.push_back(frame_data);
    while (imgFrameBuffer_.size() > static_cast<size_t>(imgFramesToSum_)) {
        imgFrameBuffer_.pop_front();
    }
    
    // Increment frame counter for sum update interval
    imgFramesSinceLastSumUpdate_++;

    // Snapshot accumulated frame count to use as NDArray uniqueId (32-bit)
    epicsInt32 imgUid = static_cast<epicsInt32>(imgAccumulatedFrameCount_);
    
    // Prepare data for callbacks (while holding mutex)
    // Copy data to buffers that will be used for callbacks
    size_t image_data_size = 0;
    size_t image_frame_size = 0;
    size_t image_sum_size = 0;
    bool has_running_sum = (imgRunningSum_ != nullptr);
    bool has_current_frame = (imgCurrentFrame_.get_pixel_count() > 0);
    bool should_update_sum = (imgFramesSinceLastSumUpdate_ >= imgSumUpdateIntervalFrames_ && !imgFrameBuffer_.empty());
    
    if (has_running_sum) {
        image_data_size = imgRunningSum_->get_pixel_count();
        if (imgArrayData64Buffer_.size() < image_data_size) {
            imgArrayData64Buffer_.resize(image_data_size);
        }
        const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
        for (size_t i = 0; i < image_data_size; ++i) {
            imgArrayData64Buffer_[i] = static_cast<epicsInt64>(pixels[i]);
        }
    }
    
    if (has_current_frame) {
        image_frame_size = imgCurrentFrame_.get_pixel_count();
        if (imgFrameArrayDataBuffer_.size() < image_frame_size) {
            imgFrameArrayDataBuffer_.resize(image_frame_size);
        }
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            const uint16_t* pixels = imgCurrentFrame_.get_pixels_16_ptr();
            for (size_t i = 0; i < image_frame_size; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        } else {
            const uint32_t* pixels = imgCurrentFrame_.get_pixels_32_ptr();
            for (size_t i = 0; i < image_frame_size; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        }
    }
    
    if (should_update_sum) {
        imgFramesSinceLastSumUpdate_ = 0;
        
        // Use dimensions from first frame in buffer (should match current frame)
        size_t sum_pixel_count = imgFrameBuffer_[0].get_pixel_count();
        size_t sum_frame_width = imgFrameBuffer_[0].get_width();
        size_t sum_frame_height = imgFrameBuffer_[0].get_height();
        
        if (imgSumArray64WorkBuffer_.size() < sum_pixel_count) {
            imgSumArray64WorkBuffer_.resize(sum_pixel_count);
            imgSumArray64Buffer_.resize(sum_pixel_count);
        }
        
        // Initialize sum array to zero
        std::memset(imgSumArray64WorkBuffer_.data(), 0, sum_pixel_count * sizeof(uint64_t));
        
        // Sum all frames in buffer
        size_t frames_summed = 0;
        for (const auto& frame : imgFrameBuffer_) {
            if (frame.get_width() == sum_frame_width && 
                frame.get_height() == sum_frame_height) {
                frames_summed++;
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
        image_sum_size = sum_pixel_count;
    }
    
    // Calculate processing time
    epicsTimeStamp processing_end_time;
    epicsTimeGetCurrent(&processing_end_time);
    double processing_time_ms = ((processing_end_time.secPastEpoch - processing_start_time.secPastEpoch) * 1000.0) +
                                 ((processing_end_time.nsec - processing_start_time.nsec) / 1e6);
    
    imgProcessingTimeSamples_.push_back(processing_time_ms);
    if (imgProcessingTimeSamples_.size() > IMG_MAX_PROCESSING_TIME_SAMPLES) {
        imgProcessingTimeSamples_.erase(imgProcessingTimeSamples_.begin());
    }
    
    // Calculate average processing time from samples
    if (imgProcessingTimeSamples_.size() > 0) {
        double sum = 0.0;
        for (size_t i = 0; i < imgProcessingTimeSamples_.size(); ++i) {
            sum += imgProcessingTimeSamples_[i];
        }
        imgProcessingTime_ = sum / imgProcessingTimeSamples_.size();
    } else {
        imgProcessingTime_ = 0.0;
    }
    
    // Update EPICS PV periodically (every 1 second) or when we have enough samples
    double current_time_seconds = processing_end_time.secPastEpoch + processing_end_time.nsec / 1e9;
    if (current_time_seconds - imgLastProcessingTimeUpdate_ >= 1.0 || 
        imgProcessingTimeSamples_.size() >= IMG_MAX_PROCESSING_TIME_SAMPLES) {
        setDoubleParam(ADTimePixImgProcessingTime, imgProcessingTime_);
        callParamCallbacks(ADTimePixImgProcessingTime);
        imgLastProcessingTimeUpdate_ = current_time_seconds;
    }
    
    // Update performance metrics
    updateImgPerformanceMetrics();

    // Prepare NDArray for running sum (addr 2) if requested and pool available
    if (has_running_sum && image_data_size > 0 && pNDArrayPool) {
        size_t width = imgRunningSum_->get_width();
        size_t height = imgRunningSum_->get_height();
        size_t dims[2] = { width, height };
        NDArray* pArray = pNDArrayPool->alloc(2, dims, NDUInt64, 0, NULL);
        if (pArray && pArray->pData) {
            const uint64_t* src = imgRunningSum_->get_pixels_64_ptr();
            epicsUInt64* dst = static_cast<epicsUInt64*>(pArray->pData);
            size_t nPixels = imgRunningSum_->get_pixel_count();
            std::memcpy(dst, src, nPixels * sizeof(epicsUInt64));
            // Metadata
            pArray->uniqueId = imgUid;
            epicsTimeStamp ts;
            epicsTimeGetCurrent(&ts);
            pArray->timeStamp = ts.secPastEpoch + ts.nsec / 1.e9;
            updateTimeStamp(&pArray->epicsTS);
            if (pArray->pAttributeList) {
                getAttributes(pArray->pAttributeList);
            }
            pImgSumArray = pArray;
            emitImgSumArray = true;
        } else if (pArray) {
            pArray->release();
        }
    }

    // Prepare NDArray for sum of N frames (addr 3) if requested and pool available
    if (should_update_sum && image_sum_size > 0 && pNDArrayPool && !imgSumArray64Buffer_.empty()) {
        // Use current frame dimensions for NDArray shape
        size_t width = imgCurrentFrame_.get_width();
        size_t height = imgCurrentFrame_.get_height();
        size_t dims[2] = { width, height };
        size_t nPixels = width * height;
        if (imgSumArray64Buffer_.size() >= nPixels) {
            NDArray* pArrayN = pNDArrayPool->alloc(2, dims, NDUInt64, 0, NULL);
            if (pArrayN && pArrayN->pData) {
                const epicsInt64* src = imgSumArray64Buffer_.data();
                epicsUInt64* dst = static_cast<epicsUInt64*>(pArrayN->pData);
                for (size_t i = 0; i < nPixels; ++i) {
                    dst[i] = static_cast<epicsUInt64>(src[i]);
                }
                pArrayN->uniqueId = imgUid;
                epicsTimeStamp ts;
                epicsTimeGetCurrent(&ts);
                pArrayN->timeStamp = ts.secPastEpoch + ts.nsec / 1.e9;
                updateTimeStamp(&pArrayN->epicsTS);
                if (pArrayN->pAttributeList) {
                    getAttributes(pArrayN->pAttributeList);
                }
                pImgSumNArray = pArrayN;
                emitImgSumNArray = true;
            } else if (pArrayN) {
                pArrayN->release();
            }
        }
    }

    epicsMutexUnlock(imgMutex_);
    
    // Trigger callbacks OUTSIDE mutex to avoid deadlocks (waveform-style arrays)
    if (has_running_sum && image_data_size > 0) {
        doCallbacksInt64Array(imgArrayData64Buffer_.data(), image_data_size, 
                              ADTimePixImgImageData, 0);
    }
    
    if (has_current_frame && image_frame_size > 0) {
        doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), image_frame_size,
                              ADTimePixImgImageFrame, 0);
    }
    
    if (should_update_sum && image_sum_size > 0) {
        doCallbacksInt64Array(imgSumArray64Buffer_.data(), image_sum_size,
                              ADTimePixImgImageSumNFrames, 0);
    }

    // Emit NDArray streams for ImgImageData (addr 2) and ImgImageSumNFrames (addr 3)
    if (emitImgSumArray && pImgSumArray) {
        doCallbacksGenericPointer(pImgSumArray, NDArrayData, 2);
        pImgSumArray->release();
    }
    if (emitImgSumNArray && pImgSumNArray) {
        doCallbacksGenericPointer(pImgSumNArray, NDArrayData, 3);
        pImgSumNArray->release();
    }
}

void ADTimePix::updateImgDisplayData() {
    
    // Update IMAGE_DATA (running sum)
    if (imgRunningSum_) {
        size_t pixel_count = imgRunningSum_->get_pixel_count();
        // Resize buffer if needed
        if (imgArrayData64Buffer_.size() < pixel_count) {
            imgArrayData64Buffer_.resize(pixel_count);
        }
        // Copy running sum to buffer
        const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            imgArrayData64Buffer_[i] = static_cast<epicsInt64>(pixels[i]);
        }
        // Trigger callback - must be done while holding mutex to ensure data consistency
        asynStatus status = doCallbacksInt64Array(imgArrayData64Buffer_.data(), pixel_count, 
                                                   ADTimePixImgImageData, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_DATA: status=%d", status);
        }
    } else {
        // No running sum yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgArrayData64Buffer_.size() < default_pixel_count) {
            imgArrayData64Buffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt64Array(imgArrayData64Buffer_.data(), default_pixel_count,
                              ADTimePixImgImageData, 0);
    }
    
    // Update IMAGE_FRAME (current frame)
    size_t pixel_count = imgCurrentFrame_.get_pixel_count();
    if (pixel_count > 0) {
        if (imgFrameArrayDataBuffer_.size() < pixel_count) {
            imgFrameArrayDataBuffer_.resize(pixel_count);
        }
        // Copy current frame to buffer
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            const uint16_t* pixels = imgCurrentFrame_.get_pixels_16_ptr();
            for (size_t i = 0; i < pixel_count; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        } else {
            const uint32_t* pixels = imgCurrentFrame_.get_pixels_32_ptr();
            for (size_t i = 0; i < pixel_count; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        }
        asynStatus status = doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), pixel_count,
                                                   ADTimePixImgImageFrame, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_FRAME: status=%d", status);
        }
    } else {
        // No current frame yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgFrameArrayDataBuffer_.size() < default_pixel_count) {
            imgFrameArrayDataBuffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), default_pixel_count,
                              ADTimePixImgImageFrame, 0);
    }
    
    // Update IMAGE_SUM_N_FRAMES (sum of last N frames)
    imgFramesSinceLastSumUpdate_++;
    if (imgFramesSinceLastSumUpdate_ >= imgSumUpdateIntervalFrames_ && !imgFrameBuffer_.empty()) {
        imgFramesSinceLastSumUpdate_ = 0;
        
        // Calculate sum of frames in buffer
        size_t pixel_count = imgFrameBuffer_[0].get_pixel_count();
        size_t frame_width = imgFrameBuffer_[0].get_width();
        size_t frame_height = imgFrameBuffer_[0].get_height();
        
        if (imgSumArray64WorkBuffer_.size() < pixel_count) {
            imgSumArray64WorkBuffer_.resize(pixel_count);
            imgSumArray64Buffer_.resize(pixel_count);
        }
        
        std::memset(imgSumArray64WorkBuffer_.data(), 0, pixel_count * sizeof(uint64_t));
        
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
        
        // Convert to epicsInt64
        for (size_t i = 0; i < pixel_count; ++i) {
            imgSumArray64Buffer_[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
        }
        
        // Trigger callback
        asynStatus status = doCallbacksInt64Array(imgSumArray64Buffer_.data(), pixel_count,
                                                   ADTimePixImgImageSumNFrames, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_SUM_N_FRAMES: status=%d", status);
        }
    } else if (imgFrameBuffer_.empty()) {
        // No frames in buffer yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgSumArray64Buffer_.size() < default_pixel_count) {
            imgSumArray64Buffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt64Array(imgSumArray64Buffer_.data(), default_pixel_count,
                              ADTimePixImgImageSumNFrames, 0);
    }
}

void ADTimePix::updateImgPerformanceMetrics() {
    epicsTimeStamp current_time;
    epicsTimeGetCurrent(&current_time);
    double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
    
    // Update total counts
    setInteger64Param(ADTimePixImgTotalCounts, imgTotalCounts_);
    
    // Calculate memory usage periodically (every 5 seconds) or more frequently if buffer is growing
    // Update more frequently if frame buffer is near capacity to catch memory growth
    bool should_update_memory = (current_time_seconds - imgLastMemoryUpdateTime_ >= IMG_MEMORY_UPDATE_INTERVAL_SEC) ||
                                 (imgFrameBuffer_.size() >= static_cast<size_t>(imgFramesToSum_) * 0.9);
    
    if (should_update_memory) {
        imgMemoryUsage_ = calculateImgMemoryUsageMB();
        setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
        callParamCallbacks(ADTimePixImgMemoryUsage);
        imgLastMemoryUpdateTime_ = current_time_seconds;
    }
    
    // Call parameter callbacks for updated values
    callParamCallbacks(ADTimePixImgTotalCounts);
}

double ADTimePix::calculateImgMemoryUsageMB() {
    double total_mb = 0.0;
    
    // Memory for running sum (64-bit pixels)
    if (imgRunningSum_) {
        total_mb += imgRunningSum_->get_pixel_count() * sizeof(uint64_t) / (1024.0 * 1024.0);
    }
    
    // Memory for current frame
    size_t current_frame_pixels = imgCurrentFrame_.get_pixel_count();
    if (current_frame_pixels > 0) {
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            total_mb += current_frame_pixels * sizeof(uint16_t) / (1024.0 * 1024.0);
        } else {
            total_mb += current_frame_pixels * sizeof(uint32_t) / (1024.0 * 1024.0);
        }
    }
    
    // Memory for frame buffer (actual frames stored, up to imgFramesToSum_)
    for (const auto& frame : imgFrameBuffer_) {
        if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            total_mb += frame.get_pixel_count() * sizeof(uint16_t) / (1024.0 * 1024.0);
        } else {
            total_mb += frame.get_pixel_count() * sizeof(uint32_t) / (1024.0 * 1024.0);
        }
    }
    
    // Memory for EPICS array buffers (use maximum potential size based on imgFramesToSum_)
    // Calculate maximum pixels based on current frame dimensions or default
    size_t max_pixels = 0;
    if (imgRunningSum_) {
        max_pixels = imgRunningSum_->get_pixel_count();
    } else if (current_frame_pixels > 0) {
        max_pixels = current_frame_pixels;
    } else {
        max_pixels = 512 * 512; // Default detector size
    }
    
    // EPICS waveform buffers: IMAGE_DATA and IMAGE_SUM_N_FRAMES use 64-bit, IMAGE_FRAME uses 32-bit
    // Account for maximum potential allocation
    total_mb += max_pixels * sizeof(epicsInt64) / (1024.0 * 1024.0); // IMAGE_DATA (64-bit)
    total_mb += max_pixels * sizeof(epicsInt64) / (1024.0 * 1024.0); // IMAGE_SUM_N_FRAMES (64-bit)
    total_mb += max_pixels * sizeof(epicsInt32) / (1024.0 * 1024.0); // IMAGE_FRAME (32-bit)
    
    // Internal work buffers
    total_mb += (imgSumArray64WorkBuffer_.size() * sizeof(uint64_t)) / (1024.0 * 1024.0);
    
    // Add overhead for std::vector and std::deque structures (approximate)
    // Each ImageData object has some overhead, and std::deque has overhead per element
    size_t frame_buffer_overhead = imgFrameBuffer_.size() * 64; // Approximate overhead per frame in deque
    total_mb += frame_buffer_overhead / (1024.0 * 1024.0);
    
    // Add small overhead for other structures
    total_mb += 0.1; // Overhead for rate samples, processing time samples, etc.
    
    return total_mb;
}

void ADTimePix::resetImgAccumulation() {
    imgRunningSum_.reset();
    imgFrameBuffer_.clear();
    imgTotalCounts_ = 0;
    imgAccumulatedFrameCount_ = 0;
    imgFramesSinceLastSumUpdate_ = 0;
    imgProcessingTime_ = 0.0;
    imgProcessingTimeSamples_.clear();
    setInteger64Param(ADTimePixImgTotalCounts, 0);
    setDoubleParam(ADTimePixImgProcessingTime, 0.0);
    // Update memory usage after reset
    imgMemoryUsage_ = calculateImgMemoryUsageMB();
    setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
    // Note: Array callbacks will be triggered when next frame arrives and finds empty buffers
    // For immediate update, we could trigger callbacks with zero arrays here, but it's cleaner
    // to wait for the next frame to populate the arrays
}

void ADTimePix::pushProcessedImgToPlugins() {
    if (!imgMutex_ || !pNDArrayPool) return;
    epicsMutexLock(imgMutex_);
    
    int outputType = 0;  // 0=Sum (NDInt64), 1=Average (NDInt32)
    getIntegerParam(ADTimePixProcessedImgOutputType, &outputType);
    if (outputType != 0 && outputType != 1) outputType = 0;
    
    // Save shared params (used by image channels 0 and 1)
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
    
    if (imgRunningSum_ && arrayCallbacks) {
        size_t width = imgRunningSum_->get_width();
        size_t height = imgRunningSum_->get_height();
        size_t pixel_count = imgRunningSum_->get_pixel_count();
        const uint64_t* sum_ptr = imgRunningSum_->get_pixels_64_ptr();
        
        // Address 2: running sum (ImgImageData)
        size_t dims[3] = { width, height, 0 };
        NDDataType_t dataType = (outputType == 1) ? NDInt32 : NDInt64;
        NDArray* pArr2 = pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        if (pArr2 && pArr2->pData) {
            epicsUInt32 nFrames = (imgAccumulatedFrameCount_ > 0) ? static_cast<epicsUInt32>(imgAccumulatedFrameCount_) : 1;
            if (outputType == 0) {
                epicsInt64* pData = reinterpret_cast<epicsInt64*>(pArr2->pData);
                for (size_t i = 0; i < pixel_count; i++) pData[i] = static_cast<epicsInt64>(sum_ptr[i]);
            } else {
                epicsInt32* pData = reinterpret_cast<epicsInt32*>(pArr2->pData);
                for (size_t i = 0; i < pixel_count; i++)
                    pData[i] = (nFrames > 0) ? static_cast<epicsInt32>(sum_ptr[i] / nFrames) : 0;
            }
            epicsTimeGetCurrent(&pArr2->epicsTS);
            pArr2->timeStamp = pArr2->epicsTS.secPastEpoch + pArr2->epicsTS.nsec / 1.e9;
            if (pArr2->pAttributeList) getAttributes(pArr2->pAttributeList);
            doCallbacksGenericPointer(pArr2, NDArrayData, 2);
            pArr2->release();
        } else if (pArr2) {
            pArr2->release();
        }
        
        // Address 3: sum of last N frames (ImgImageSumNFrames)
        size_t nSumFrames = imgFrameBuffer_.size();
        if (nSumFrames > 0 && imgSumArray64Buffer_.size() >= pixel_count) {
            NDArray* pArr3 = pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
            if (pArr3 && pArr3->pData) {
                const epicsInt64* sumN = imgSumArray64Buffer_.data();
                epicsUInt32 nN = static_cast<epicsUInt32>(nSumFrames);
                if (outputType == 0) {
                    epicsInt64* pData = reinterpret_cast<epicsInt64*>(pArr3->pData);
                    for (size_t i = 0; i < pixel_count; i++) pData[i] = sumN[i];
                } else {
                    epicsInt32* pData = reinterpret_cast<epicsInt32*>(pArr3->pData);
                    for (size_t i = 0; i < pixel_count; i++)
                        pData[i] = (nN > 0) ? static_cast<epicsInt32>(sumN[i] / nN) : 0;
                }
                epicsTimeGetCurrent(&pArr3->epicsTS);
                pArr3->timeStamp = pArr3->epicsTS.secPastEpoch + pArr3->epicsTS.nsec / 1.e9;
                if (pArr3->pAttributeList) getAttributes(pArr3->pAttributeList);
                doCallbacksGenericPointer(pArr3, NDArrayData, 3);
                pArr3->release();
            } else if (pArr3) {
                pArr3->release();
            }
        }
        
        // Restore NDArrayCounter so processed-image push does not affect main counter (like histogram)
        int curCounter = 0;
        getIntegerParam(NDArrayCounter, &curCounter);
        int expectedDelta = (imgFrameBuffer_.size() > 0 && imgSumArray64Buffer_.size() >= pixel_count) ? 2 : 1;
        if (curCounter == savedArrayCounter + expectedDelta) setIntegerParam(NDArrayCounter, savedArrayCounter);
    }
    
    setIntegerParam(ADSizeX, savedSizeX);
    setIntegerParam(ADSizeY, savedSizeY);
    setIntegerParam(NDArraySizeX, savedArraySizeX);
    setIntegerParam(NDArraySizeY, savedArraySizeY);
    setIntegerParam(NDDataType, savedDataType);
    setIntegerParam(NDArraySize, savedArraySize);
    epicsMutexUnlock(imgMutex_);
}

bool ADTimePix::processPrvImgDataLine(char* line_buffer, char* newline_pos, size_t total_read) {
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
    
    // Skip non-printable characters until we find '{'
    while (*json_start != '\0' && *json_start != '{' &&
           (*json_start < 32 || *json_start > 126)) {
        json_start++;
    }
    
    if (*json_start == '\0' || *json_start != '{') {
        return true;
    }
    
    json j;
    try {
        j = json::parse(json_start);
    } catch (const json::parse_error& e) {
        if (*json_start == '{') {
            ERR_ARGS("JSON parse error: %s", e.what());
        }
        return true;
    }
    
    try {
        // Extract header information for jsonimage
        int width = j["width"];
        int height = j["height"];
        std::string pixel_format_str = j.value("pixelFormat", "uint16");
        
        // Extract additional frame data
        int frame_number = j.value("frameNumber", 0);
        double time_at_frame = j.value("timeAtFrame", 0.0);
        
        // Determine pixel format
        bool is_uint32 = (pixel_format_str == "uint32" || pixel_format_str == "UINT32");
        NDDataType_t dataType = is_uint32 ? NDUInt32 : NDUInt16;
        
        // Calculate pixel data size
        size_t pixel_count = width * height;
        size_t bytes_per_pixel = is_uint32 ? sizeof(uint32_t) : sizeof(uint16_t);
        size_t binary_needed = pixel_count * bytes_per_pixel;
        
        // Validate dimensions
        if (width <= 0 || height <= 0 || width > 100000 || height > 100000) {
            ERR_ARGS("Invalid image dimensions: width=%d, height=%d", width, height);
            return false;
        }
        
        // Create NDArray - check if pool is available
        if (!this->pNDArrayPool) {
            ERR("NDArray pool is not available");
            return false;
        }
        
        size_t dims[3];
        dims[0] = width;
        dims[1] = height;
        dims[2] = 0;
        
        NDArray *pImage = nullptr;
        if (this->pArrays && this->pArrays[0]) {
        pImage = this->pArrays[0];  
            pImage->release();
        }
        
        this->pArrays[0] = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        pImage = this->pArrays[0];
        
        if (!pImage || !pImage->pData) {
            ERR("Failed to allocate NDArray or NDArray has no data pointer");
            return false;
        }
        
        // Copy any binary data we already have after the newline
        size_t remaining = total_read - (newline_pos - line_buffer + 1);
        size_t binary_read = 0;
        
        std::vector<char> pixel_buffer(binary_needed);
        
        if (remaining > 0) {
            size_t to_copy = std::min(remaining, binary_needed);
            memcpy(pixel_buffer.data(), newline_pos + 1, to_copy);
            binary_read = to_copy;
        }
        
        // Read any remaining binary data needed
        epicsMutexLock(prvImgMutex_);
        if (binary_read < binary_needed && prvImgNetworkClient_ && prvImgNetworkClient_->is_connected()) {
            if (!prvImgNetworkClient_->receive_exact(
                pixel_buffer.data() + binary_read,
                binary_needed - binary_read)) {
                epicsMutexUnlock(prvImgMutex_);
                ERR("Failed to read binary pixel data");
                return false;
            }
        }
        epicsMutexUnlock(prvImgMutex_);
        
        // Validate pixel buffer size
        if (pixel_buffer.size() < binary_needed) {
            ERR_ARGS("Pixel buffer too small: have %zu, need %zu", pixel_buffer.size(), binary_needed);
            return false;
        }
        
        // Convert network byte order to host byte order and copy to NDArray
        if (!pImage->pData) {
            ERR("NDArray pData is null");
            return false;
        }
        
        if (is_uint32) {
            uint32_t* pixels = reinterpret_cast<uint32_t*>(pixel_buffer.data());
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap32(pixels[i]);
            }
        } else {
            uint16_t* pixels = reinterpret_cast<uint16_t*>(pixel_buffer.data());
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap16(pixels[i]);
            }
        }
        
        // Set image parameters (thread-safe via asynPortDriver)
        setIntegerParam(ADSizeX, width);
        setIntegerParam(NDArraySizeX, width);
        setIntegerParam(ADSizeY, height);
        setIntegerParam(NDArraySizeY, height);
        
        // Set data type
        int dataTypeValue = (int)dataType;
        setIntegerParam(NDDataType, dataTypeValue);
        setIntegerParam(NDColorMode, NDColorModeMono);
        
        NDArrayInfo_t arrayInfo;
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize, (int)arrayInfo.totalBytes);
        
        // Note: PrvImg channel does NOT increment NDArrayCounter to avoid double-counting.
        // Only the Img channel (main acquisition) increments the shared NDArrayCounter.
        // ArrayRate_RBV reflects the rate of the main Img channel, not preview channels.
        
        // Set timestamp
        pImage->uniqueId = frame_number;
        epicsTimeStamp timestamp;
        epicsTimeGetCurrent(&timestamp);
        pImage->timeStamp = timestamp.secPastEpoch + timestamp.nsec / 1.e9;
        updateTimeStamp(&pImage->epicsTS);
        
        // Set PrvImg metadata PVs
        setIntegerParam(ADTimePixPrvImgFrameNumber, frame_number);
        setDoubleParam(ADTimePixPrvImgTimeAtFrame, time_at_frame);
        
        // Calculate acquisition rate
        epicsTimeStamp current_time;
        epicsTimeGetCurrent(&current_time);
        double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
        
        if (!prvImgFirstFrameReceived_) {
            prvImgPreviousFrameNumber_ = frame_number;
            prvImgPreviousTimeAtFrame_ = current_time_seconds;
            prvImgFirstFrameReceived_ = true;
            prvImgAcquisitionRate_ = 0.0;
        } else {
            int frame_diff = frame_number - prvImgPreviousFrameNumber_;
            double time_diff_seconds = current_time_seconds - prvImgPreviousTimeAtFrame_;
            
            if (frame_diff > 1) {
                LOG_ARGS("PrvImg frame loss detected! Expected frame %d, got frame %d (lost %d frames)", 
                         prvImgPreviousFrameNumber_ + 1, frame_number, frame_diff - 1);
            }
            
            if (frame_diff > 0 && time_diff_seconds > 0.0) {
                double current_rate = frame_diff / time_diff_seconds;
                
                prvImgRateSamples_.push_back(current_rate);
                if (prvImgRateSamples_.size() > PRVIMG_MAX_RATE_SAMPLES) {
                    prvImgRateSamples_.erase(prvImgRateSamples_.begin());
                }
                
                double sum = 0.0;
                for (size_t i = 0; i < prvImgRateSamples_.size(); ++i) {
                    sum += prvImgRateSamples_[i];
                }
                prvImgAcquisitionRate_ = sum / prvImgRateSamples_.size();
                
                if (current_time_seconds - prvImgLastRateUpdateTime_ >= 1.0) {
                    setDoubleParam(ADTimePixPrvImgAcqRate, prvImgAcquisitionRate_);
                    prvImgLastRateUpdateTime_ = current_time_seconds;
                }
            }
            
            prvImgPreviousFrameNumber_ = frame_number;
            prvImgPreviousTimeAtFrame_ = current_time_seconds;
        }
        
        // Get attributes
        if (pImage->pAttributeList) {
            this->getAttributes(pImage->pAttributeList);
        }
        
        // Call parameter callbacks to update EPICS PVs (thread-safe)
        callParamCallbacks();
        
        // Trigger NDArray callbacks (thread-safe) - PrvImg channel uses address 0
        int arrayCallbacks = 0;
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if (arrayCallbacks && pImage) {
            doCallbacksGenericPointer(pImage, NDArrayData, 0);
        }
        
        LOG_ARGS("Processed PrvImg frame: width=%d, height=%d, format=%s, frame=%d", 
                 width, height, pixel_format_str.c_str(), frame_number);
        
    } catch (const std::exception& e) {
        ERR_ARGS("Error processing PrvImg frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::imgConnect() {
    
    if (!imgMutex_) {
        ERR("Img TCP: Mutex not initialized");
        return;
    }
    
    epicsMutexLock(imgMutex_);
    std::string host = imgHost_;
    int port = imgPort_;
    epicsMutexUnlock(imgMutex_);
    
    if (host.empty() || port <= 0) {
        ERR("Img TCP: Invalid host or port");
        return;
    }
    
    imgDisconnect(); // Ensure clean state
    
    imgNetworkClient_.reset(new NetworkClient());
    if (!imgNetworkClient_) {
        ERR("Img TCP: Failed to create NetworkClient");
        return;
    }
    
    if (imgNetworkClient_->connect(host, port)) {
        epicsMutexLock(imgMutex_);
        imgConnected_ = true;
        epicsMutexUnlock(imgMutex_);
        LOG_ARGS("Img TCP connected to %s:%d", host.c_str(), port);
    } else {
        ERR_ARGS("Img TCP failed to connect to %s:%d", host.c_str(), port);
        imgNetworkClient_.reset();
    }
}

void ADTimePix::imgDisconnect() {
    epicsMutexLock(imgMutex_);
    imgConnected_ = false;
    epicsMutexUnlock(imgMutex_);
    
    if (imgNetworkClient_) {
        imgNetworkClient_->disconnect();
        imgNetworkClient_.reset();
    }
}

void ADTimePix::prvImgConnect() {
    
    if (!prvImgMutex_) {
        ERR("PrvImg TCP: Mutex not initialized");
        return;
    }
    
    epicsMutexLock(prvImgMutex_);
    std::string host = prvImgHost_;
    int port = prvImgPort_;
    epicsMutexUnlock(prvImgMutex_);
    
    if (host.empty() || port <= 0) {
        ERR("PrvImg TCP: Invalid host or port");
        return;
    }
    
    prvImgDisconnect(); // Ensure clean state
    
    prvImgNetworkClient_.reset(new NetworkClient());
    if (!prvImgNetworkClient_) {
        ERR("PrvImg TCP: Failed to create NetworkClient");
        return;
    }
    
    if (prvImgNetworkClient_->connect(host, port)) {
        epicsMutexLock(prvImgMutex_);
        prvImgConnected_ = true;
        epicsMutexUnlock(prvImgMutex_);
        LOG_ARGS("PrvImg TCP connected to %s:%d", host.c_str(), port);
    } else {
        ERR_ARGS("PrvImg TCP failed to connect to %s:%d", host.c_str(), port);
        prvImgNetworkClient_.reset();
    }
}

void ADTimePix::prvImgDisconnect() {
    
    epicsMutexLock(prvImgMutex_);
    prvImgConnected_ = false;
    epicsMutexUnlock(prvImgMutex_);
    
    if (prvImgNetworkClient_) {
        prvImgNetworkClient_->disconnect();
        prvImgNetworkClient_.reset();
    }
    
    LOG("PrvImg TCP disconnected");
}

asynStatus ADTimePix::readImageFromTCP() {
    
    // Check if we should use TCP streaming
    std::string filePath;
    getStringParam(ADTimePixPrvImgBase, filePath);
    
    if (filePath.find("tcp://") != 0) {
        // Not TCP: TCP streaming is required for preview images
        // GraphicsMagick HTTP method has been removed
        ERR("PrvImg requires TCP streaming (tcp:// format). GraphicsMagick HTTP method no longer supported.");
        return asynError;
    }
    
    // Check format - must be jsonimage (format index 3)
    int format;
    getIntegerParam(ADTimePixPrvImgFormat, &format);
    if (format != 3) {
        ERR_ARGS("PrvImg TCP streaming requires jsonimage format (3), got %d", format);
        return asynError;
    }
    
    // The worker thread is started in acquireStart() when WritePrvImg is enabled
    // This function just checks if TCP streaming should be used
    // The worker thread processes frames and updates pArrays[0] asynchronously
    
    // Wait briefly for a frame to be available (worker thread processes it)
    epicsMutexLock(prvImgMutex_);
    bool connected = prvImgConnected_;
    epicsMutexUnlock(prvImgMutex_);
    
    if (!connected) {
        // Connection not established yet - this is OK, worker thread will connect
        return asynSuccess;
    }
    
    // Frame will be processed by worker thread and pArrays[0] will be updated
    return asynSuccess;
}
