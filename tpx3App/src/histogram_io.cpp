/*
 * ADTimePix3 - PrvHst / ToF histogram TCP streaming and accumulation
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "histogram_io.h"
#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "serval_stream_framing.h"
#include <NDAttribute.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

// Driver name constant (extern from ADTimePix.cpp)
extern const char* driverName;

// HistogramData class implementation
HistogramData::HistogramData(size_t bin_size, DataType type)
    : bin_size_(bin_size), data_type_(type) {
    
    bin_edges_.resize(bin_size + 1);
    
    if (type == DataType::FRAME_DATA) {
        bin_values_32_.resize(bin_size, 0);
    } else {
        bin_values_64_.resize(bin_size, 0);
    }
}

// Copy constructor
HistogramData::HistogramData(const HistogramData& other)
    : bin_size_(other.bin_size_), data_type_(other.data_type_) {
    
    bin_edges_ = other.bin_edges_;
    
    if (data_type_ == DataType::FRAME_DATA) {
        bin_values_32_ = other.bin_values_32_;
    } else {
        bin_values_64_ = other.bin_values_64_;
    }
}

// Move constructor
HistogramData::HistogramData(HistogramData&& other) noexcept
    : bin_size_(other.bin_size_), data_type_(other.data_type_) {
    
    bin_edges_ = std::move(other.bin_edges_);
    bin_values_32_ = std::move(other.bin_values_32_);
    bin_values_64_ = std::move(other.bin_values_64_);
    
    other.bin_size_ = 0;
    other.data_type_ = DataType::FRAME_DATA;
}

// Assignment operators
HistogramData& HistogramData::operator=(const HistogramData& other) {
    if (this != &other) {
        bin_size_ = other.bin_size_;
        data_type_ = other.data_type_;
        bin_edges_ = other.bin_edges_;
        
        if (data_type_ == DataType::FRAME_DATA) {
            bin_values_32_ = other.bin_values_32_;
        } else {
            bin_values_64_ = other.bin_values_64_;
        }
    }
    return *this;
}

HistogramData& HistogramData::operator=(HistogramData&& other) noexcept {
    if (this != &other) {
        bin_size_ = other.bin_size_;
        data_type_ = other.data_type_;
        bin_edges_ = std::move(other.bin_edges_);
        bin_values_32_ = std::move(other.bin_values_32_);
        bin_values_64_ = std::move(other.bin_values_64_);
        
        other.bin_size_ = 0;
        other.data_type_ = DataType::FRAME_DATA;
    }
    return *this;
}

// Access bin values based on type
uint32_t HistogramData::get_bin_value_32(size_t index) const {
    if (data_type_ != DataType::FRAME_DATA || index >= bin_values_32_.size()) {
        throw std::out_of_range("Invalid index or data type for 32-bit access");
    }
    return bin_values_32_[index];
}

uint64_t HistogramData::get_bin_value_64(size_t index) const {
    if (data_type_ != DataType::RUNNING_SUM || index >= bin_values_64_.size()) {
        throw std::out_of_range("Invalid index or data type for 64-bit access");
    }
    return bin_values_64_[index];
}

// Setters
void HistogramData::set_bin_edge(size_t index, double value) {
    if (index >= bin_edges_.size()) {
        throw std::out_of_range("Bin edge index out of range");
    }
    bin_edges_[index] = value;
}

void HistogramData::set_bin_value_32(size_t index, uint32_t value) {
    if (data_type_ != DataType::FRAME_DATA || index >= bin_values_32_.size()) {
        throw std::out_of_range("Invalid index or data type for 32-bit access");
    }
    bin_values_32_[index] = value;
}

void HistogramData::set_bin_value_64(size_t index, uint64_t value) {
    if (data_type_ != DataType::RUNNING_SUM || index >= bin_values_64_.size()) {
        throw std::out_of_range("Invalid index or data type for 64-bit access");
    }
    bin_values_64_[index] = value;
}

// Calculate bin edges from parameters
void HistogramData::calculate_bin_edges(int bin_width, int bin_offset) {
    for (size_t i = 0; i < bin_edges_.size(); ++i) {
        bin_edges_[i] = (bin_offset + (i * bin_width)) * TPX3_TDC_CLOCK_PERIOD_SEC;
    }
}

// Add another histogram to this one (for running sum)
void HistogramData::add_histogram(const HistogramData& other) {
    if (other.data_type_ != DataType::FRAME_DATA || data_type_ != DataType::RUNNING_SUM) {
        throw std::invalid_argument("Can only add frame data to running sum");
    }
    
    if (other.bin_size_ != bin_size_) {
        throw std::invalid_argument("Bin sizes must match for addition");
    }

    for (size_t i = 0; i < bin_size_; ++i) {
        uint64_t new_value = bin_values_64_[i] + other.bin_values_32_[i];
        if (new_value < bin_values_64_[i]) {
            // Overflow detected - cap at maximum value
            bin_values_64_[i] = UINT64_MAX;
        } else {
            bin_values_64_[i] = new_value;
        }
    }
}

// PrvHst TCP streaming methods implementation

bool ADTimePix::processPrvHstDataLine(char* line_buffer, char* newline_pos, size_t total_read) {
    static const char* functionName = "processPrvHstDataLine";
    
    // Early return if accumulation is disabled - don't process data at all
    int accumulationEnable = 0;
    getIntegerParam(ADTimePixPrvHstAccumulationEnable, &accumulationEnable);
    if (!accumulationEnable) {
        return true;  // Skip processing when accumulation is disabled
    }
    
    // Validate pointers
    if (!line_buffer || !newline_pos || newline_pos <= line_buffer) {
        fprintf(stderr, "ERROR | ADTimePix::%s: Invalid pointers\n", functionName);
        return false;
    }
    
    // Calculate safe length (up to newline, not using strlen which requires null terminator)
    size_t line_length = newline_pos - line_buffer;
    if (line_length == 0) {
        return true;  // Empty line
    }
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
    char* line_end = newline_pos;  // Don't go past newline
    while (json_start < line_end && *json_start != '{' &&
           (*json_start < 32 || *json_start > 126)) {
        json_start++;
    }
    
    if (json_start >= line_end || *json_start != '{') {
        return true;  // No valid JSON found
    }
    
    // Calculate JSON length (from json_start to newline)
    size_t json_length = newline_pos - json_start;
    
        json j;
    try {
        // Parse only up to newline, not using strlen
        std::string json_str(json_start, json_length);
        j = json::parse(json_str);
                // Validate JSON object is not null
        if (j.is_null() || j.is_discarded()) {
                        return true;
        }
        
            } catch (const json::parse_error& e) {
        fprintf(stderr, "ERROR | ADTimePix::%s: JSON parse error in PrvHst: %s\n", functionName, e.what());
        return true;  // Continue processing
    }
    
    try {
                // Extract header information for jsonhisto
        if (!j.contains("binSize")) {
                        return true;
        }
        
        // binWidth/binOffset may be null in malformed or edge-case frames — avoid .get<int>() abort
        auto histInt = [](const json& o, const char* key, int def) -> int {
            if (!o.contains(key) || o[key].is_null()) return def;
            const json& v = o[key];
            if (v.is_number_integer()) return v.get<int>();
            if (v.is_number()) return static_cast<int>(v.get<double>());
            return def;
        };
        int bin_size = histInt(j, "binSize", 0);
        int bin_width = histInt(j, "binWidth", 0);
        int bin_offset = histInt(j, "binOffset", 0);
                // Extract additional frame data
                int frame_number = j.value("frameNumber", 0);
                        double time_at_frame = j.value("timeAtFrame", 0.0);
                // Store frame metadata (will be used in processPrvHstFrame)
                if (!prvHstMutex_) {
                        return false;
        }
        
        epicsMutexLock(prvHstMutex_);
                        prvHstTimeAtFrame_ = time_at_frame;
                prvHstFrameBinSize_ = bin_size;
                prvHstFrameBinWidth_ = bin_width;
                prvHstFrameBinOffset_ = bin_offset;
                epicsMutexUnlock(prvHstMutex_);
                // Validate bin size
                if (bin_size <= 0 || bin_size > 1000000) {
                        fprintf(stderr, "ERROR | ADTimePix::%s: Invalid bin size: %d\n", functionName, bin_size);
            return false;
        }
        
                // Calculate acquisition rate (similar to PrvImg/Img)
                if (!prvHstMutex_) {
                        return false;
        }
        
        epicsMutexLock(prvHstMutex_);
                        epicsTimeStamp current_time;
        epicsTimeGetCurrent(&current_time);
        double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
        
                if (!prvHstFirstFrameReceived_) {
                        prvHstPreviousFrameNumber_ = frame_number;
                        prvHstPreviousTimeAtFrame_ = current_time_seconds;
                                    prvHstFirstFrameReceived_ = true;
                                    prvHstAcquisitionRate_ = 0.0;
                    } else {
            int frame_diff = frame_number - prvHstPreviousFrameNumber_;
            double time_diff_seconds = current_time_seconds - prvHstPreviousTimeAtFrame_;
            
            if (frame_diff > 0 && time_diff_seconds > 0.0) {
                                double current_rate = frame_diff / time_diff_seconds;
                
                                prvHstRateSamples_.push_back(current_rate);
                
                                constexpr size_t PRVHST_MAX_RATE_SAMPLES = 100;
                if (prvHstRateSamples_.size() > PRVHST_MAX_RATE_SAMPLES) {
                    prvHstRateSamples_.erase(prvHstRateSamples_.begin());
                }
                
                double sum = 0.0;
                for (size_t i = 0; i < prvHstRateSamples_.size(); ++i) {
                    sum += prvHstRateSamples_[i];
                }
                prvHstAcquisitionRate_ = sum / prvHstRateSamples_.size();
                
                if (current_time_seconds - prvHstLastRateUpdateTime_ >= 1.0) {
                    // Update rate PV if it exists (may need to add this parameter)
                    prvHstLastRateUpdateTime_ = current_time_seconds;
                }
            }
            
            prvHstPreviousFrameNumber_ = frame_number;
            prvHstPreviousTimeAtFrame_ = current_time_seconds;
        }
        
                epicsMutexUnlock(prvHstMutex_);
        
                // Create frame histogram
                HistogramData frame_histogram(bin_size, HistogramData::DataType::FRAME_DATA);
        
                // Calculate bin edges
                frame_histogram.calculate_bin_edges(bin_width, bin_offset);
        
                // Read binary data
                std::vector<uint32_t> tof_bin_values(bin_size);
        
                size_t binary_needed = bin_size * sizeof(uint32_t);
                // Copy any binary data we already have after the newline
        size_t remaining = total_read - (newline_pos - line_buffer + 1);
                size_t binary_read = 0;
        
        if (remaining > 0) {
            size_t to_copy = std::min(remaining, binary_needed);
                        memcpy(tof_bin_values.data(), newline_pos + 1, to_copy);
            binary_read = to_copy;
            
                    }
        
        if (binary_read != binary_needed) {
            fprintf(stderr,
                    "ERROR | ADTimePix::%s: Incomplete framed histogram payload: have %zu, need %zu\n",
                    functionName, binary_read, binary_needed);
            return false;
        }
        
        // Convert network byte order to host byte order
                for (int i = 0; i < bin_size; ++i) {
            if (i == 0) {
                            }
            
            tof_bin_values[i] = __builtin_bswap32(tof_bin_values[i]);
            
            if (i == 0) {
                            }
            
            if (i < 5 || i == bin_size - 1) {
                            }
            
            frame_histogram.set_bin_value_32(i, tof_bin_values[i]);
            
            if (i < 5 || i == bin_size - 1) {
                            }
        }
        
        // Process frame
        processPrvHstFrame(frame_histogram);
        
            } catch (const std::exception& e) {
        ERR_ARGS("Error processing PrvHst frame: %s", e.what());
        return false;
    }
    
    return true;
}

bool ADTimePix::configurePrvHstRollingWindow(size_t binCount)
{
    const size_t budgetBytes =
        static_cast<size_t>(prvHstRetentionLimitMB_) * 1024U * 1024U;
    const ADTimePix3Accumulation::RollingSumConfiguration result =
        prvHstWindowSum_.configure(binCount,
                                   static_cast<size_t>(prvHstFramesToSum_),
                                   budgetBytes);

    char statusMessage[40];
    if (result.status != ADTimePix3Accumulation::RollingSumStatus::Ok) {
        std::snprintf(statusMessage, sizeof(statusMessage),
                      "Unavailable: %s",
                      ADTimePix3Accumulation::rollingSumStatusName(result.status));
        setIntegerParam(ADTimePixPrvHstEffectiveFrames, 0);
        setIntegerParam(ADTimePixPrvHstFramesSummed, 0);
        setStringParam(ADTimePixPrvHstRetentionStatus, statusMessage);
        WARN_ARGS("PrvHst rolling window unavailable: %s; geometry=%zu, budget=%d MiB",
                  statusMessage, binCount, prvHstRetentionLimitMB_);
        return false;
    }

    setIntegerParam(ADTimePixPrvHstEffectiveFrames,
                    static_cast<int>(result.effectiveFrames));
    setIntegerParam(ADTimePixPrvHstFramesSummed,
                    static_cast<int>(prvHstWindowSum_.frameCount()));
    if (result.limited) {
        std::snprintf(statusMessage, sizeof(statusMessage),
                      "Capped: %zu/%zu frames, %d MiB",
                      result.effectiveFrames, result.requestedFrames,
                      prvHstRetentionLimitMB_);
        WARN_ARGS("PrvHst rolling window %s", statusMessage);
    } else {
        std::snprintf(statusMessage, sizeof(statusMessage),
                      "OK: %zu frames, %d MiB",
                      result.effectiveFrames, prvHstRetentionLimitMB_);
    }
    setStringParam(ADTimePixPrvHstRetentionStatus, statusMessage);
    return true;
}

double ADTimePix::calculatePrvHstMemoryUsageMB()
{
    double totalMemoryMB = 0.0;
    if (prvHstRunningSum_) {
        const size_t binCount = prvHstRunningSum_->get_bin_size();
        totalMemoryMB +=
            (binCount * sizeof(uint64_t) +
             (binCount + 1U) * sizeof(double)) /
            (1024.0 * 1024.0);
    }
    totalMemoryMB +=
        prvHstTimeMsBuffer_.size() * sizeof(epicsFloat64) /
        (1024.0 * 1024.0);
    totalMemoryMB += prvHstWindowSum_.memoryBytes() / (1024.0 * 1024.0);
    totalMemoryMB +=
        (prvHstRateSamples_.size() + prvHstProcessingTimeSamples_.size()) *
        sizeof(double) / (1024.0 * 1024.0);
    totalMemoryMB +=
        prvHstLineBuffer_.size() * sizeof(char) / (1024.0 * 1024.0);
    totalMemoryMB += 0.1;  // Estimated container and allocator overhead.
    return totalMemoryMB;
}

void ADTimePix::processPrvHstFrame(const HistogramData& frame_data) {
    const char* functionName = "processPrvHstFrame";
    bool rollingWindowParamsChanged = false;
    // NOTE: Avoid asynPrint macros here while debugging a segfault in the worker thread.
    // Use printf/fprintf so we don't depend on pasynUserSelf being valid in this thread.
    
        // Check if accumulation is enabled
        int accumulationEnable = 0;
    getIntegerParam(ADTimePixPrvHstAccumulationEnable, &accumulationEnable);
    
        if (!accumulationEnable) {
        // Accumulation disabled - don't process frames
                return;
    }
    
            epicsTimeStamp processing_start_time;
    epicsTimeGetCurrent(&processing_start_time);
    
            if (!prvHstMutex_) {
                return;
    }
    
    epicsMutexLock(prvHstMutex_);
    
        // Initialize running sum if needed
        if (!prvHstRunningSum_) {
            prvHstRunningSum_.reset(new HistogramData(
            frame_data.get_bin_size(), 
            HistogramData::DataType::RUNNING_SUM
        ));
        
                // Copy bin edges
                for (size_t i = 0; i < frame_data.get_bin_edges().size(); ++i) {
            prvHstRunningSum_->set_bin_edge(i, frame_data.get_bin_edges()[i]);
        }
        
            } else {
            }
    
    // Check if bin sizes match
        if (!prvHstRunningSum_) {
                epicsMutexUnlock(prvHstMutex_);
        return;
    }
    
        size_t running_sum_bin_size = prvHstRunningSum_->get_bin_size();
        size_t frame_bin_size = frame_data.get_bin_size();
        
        if (running_sum_bin_size != frame_bin_size) {
                WARN_ARGS("PrvHst bin size mismatch! Running sum has %zu bins, frame has %zu bins. Reinitializing running sum.",
                  prvHstRunningSum_->get_bin_size(), frame_data.get_bin_size());
        
        prvHstRunningSum_.reset(new HistogramData(
            frame_data.get_bin_size(), 
            HistogramData::DataType::RUNNING_SUM
        ));
        
        // Copy bin edges from frame data
        for (size_t i = 0; i < frame_data.get_bin_edges().size(); ++i) {
            prvHstRunningSum_->set_bin_edge(i, frame_data.get_bin_edges()[i]);
        }
        
        // Reset frame count and total counts since we're starting with new bin configuration
        prvHstFrameCount_ = 0;
        prvHstTotalCounts_ = 0;
        prvHstWindowSum_.clear();  // Clear rolling window on bin size change
        prvHstFramesSinceLastSumUpdate_ = 0;  // Reset sum update counter
        
        // Update parameters
        setIntegerParam(ADTimePixPrvHstFrameCount, static_cast<epicsInt32>(prvHstFrameCount_));
        setInteger64Param(ADTimePixPrvHstTotalCounts, static_cast<epicsInt64>(prvHstTotalCounts_));
        callParamCallbacks(ADTimePixPrvHstFrameCount);
        callParamCallbacks(ADTimePixPrvHstTotalCounts);
    }
    
    // Add frame data to running sum
        if (!prvHstRunningSum_) {
                epicsMutexUnlock(prvHstMutex_);
        return;
    }
    
    try {
                prvHstRunningSum_->add_histogram(frame_data);
        
            } catch (const std::exception& e) {
        fprintf(stderr, "ERROR | ADTimePix::%s: Failed to add histogram to running sum: %s\n", functionName, e.what());
        epicsMutexUnlock(prvHstMutex_);
        return;
    }
    
        prvHstFrameCount_++;
    
        // Store current frame
        if (!prvHstCurrentFrame_) {
                prvHstCurrentFrame_.reset(new HistogramData(frame_data.get_bin_size(), HistogramData::DataType::FRAME_DATA));
            } else {
            }
    
        *prvHstCurrentFrame_ = frame_data;
    
        // Calculate total counts for this frame
        size_t bin_size = frame_data.get_bin_size();
        uint64_t frame_total = 0;
        for (size_t i = 0; i < bin_size; ++i) {
        frame_total += frame_data.get_bin_value_32(i);
    }
    
        prvHstTotalCounts_ += frame_total;
    
        // Update frame metadata PVs
            setDoubleParam(ADTimePixPrvHstTimeAtFrame, prvHstTimeAtFrame_);
    
        setIntegerParam(ADTimePixPrvHstFrameBinSize, prvHstFrameBinSize_);
    
        setIntegerParam(ADTimePixPrvHstFrameBinWidth, prvHstFrameBinWidth_);
    
        setIntegerParam(ADTimePixPrvHstFrameBinOffset, prvHstFrameBinOffset_);
    
    // Update frame count and total counts PVs
            setIntegerParam(ADTimePixPrvHstFrameCount, static_cast<epicsInt32>(prvHstFrameCount_));
    
        setInteger64Param(ADTimePixPrvHstTotalCounts, static_cast<epicsInt64>(prvHstTotalCounts_));
    
    // Update acquisition rate PV
        setDoubleParam(ADTimePixPrvHstAcqRate, prvHstAcquisitionRate_);
    
        // Update the bounded rolling sum in one bin pass.
        const size_t windowBudgetBytes =
            static_cast<size_t>(prvHstRetentionLimitMB_) * 1024U * 1024U;
        bool windowPushOk = true;
        if (prvHstWindowSum_.elementCount() != bin_size ||
            prvHstWindowSum_.requestedFrames() != static_cast<size_t>(prvHstFramesToSum_) ||
            prvHstWindowSum_.budgetBytes() != windowBudgetBytes) {
            rollingWindowParamsChanged = true;
            windowPushOk = configurePrvHstRollingWindow(bin_size);
        }
        if (windowPushOk) {
            const ADTimePix3Accumulation::RollingSumStatus windowStatus =
                prvHstWindowSum_.push(frame_data.get_bins_32_ptr(), bin_size);
            if (windowStatus != ADTimePix3Accumulation::RollingSumStatus::Ok) {
                char statusMessage[160];
                std::snprintf(statusMessage, sizeof(statusMessage),
                              "Update failed: %s",
                              ADTimePix3Accumulation::rollingSumStatusName(windowStatus));
                setStringParam(ADTimePixPrvHstRetentionStatus, statusMessage);
                rollingWindowParamsChanged = true;
                ERR_ARGS("PrvHst rolling window update failed: %s", statusMessage);
                windowPushOk = false;
            }
        }

        int previousFramesSummed = 0;
        getIntegerParam(ADTimePixPrvHstFramesSummed, &previousFramesSummed);
        const int framesSummed =
            static_cast<int>(prvHstWindowSum_.frameCount());
        setIntegerParam(ADTimePixPrvHstFramesSummed, framesSummed);
        if (framesSummed != previousFramesSummed) {
            rollingWindowParamsChanged = true;
        }

        // Publish the sum-of-N product at the configured interval.
        prvHstFramesSinceLastSumUpdate_++;
        bool should_update_sum =
            windowPushOk &&
            prvHstFramesSinceLastSumUpdate_ >= prvHstSumUpdateIntervalFrames_;
        bool prvHstSumNUpdatedThisFrame = false;

        if (should_update_sum && prvHstWindowSum_.frameCount() > 0) {
            prvHstFramesSinceLastSumUpdate_ = 0;
            const std::vector<uint64_t>& rollingSum = prvHstWindowSum_.sum();
            prvHstSumArray64Buffer_.resize(rollingSum.size());
            for (size_t index = 0; index < rollingSum.size(); ++index) {
                prvHstSumArray64Buffer_[index] =
                    static_cast<epicsInt64>(rollingSum[index]);
            }
            doCallbacksInt64Array(prvHstSumArray64Buffer_.data(),
                                  prvHstSumArray64Buffer_.size(),
                                  ADTimePixPrvHstHistogramSumNFrames, 0);
            prvHstSumNUpdatedThisFrame = true;
        }

    
    // Update histogram data PVs via callbacks
    if (prvHstRunningSum_) {
        size_t bin_size = prvHstRunningSum_->get_bin_size();
        
        // Resize buffers if needed
        if (prvHstArrayData32Buffer_.size() < bin_size) {
            prvHstArrayData32Buffer_.resize(bin_size);
        }
        if (prvHstTimeMsBuffer_.size() < bin_size + 1) {
            prvHstTimeMsBuffer_.resize(bin_size + 1);
        }
        
        // Create time axis from frame parameters (convert seconds to milliseconds)
        // For plotting, we use bin centers: bin_offset + i * bin_width (convert to milliseconds)
        // This matches the standalone histogram IOC calculation
        if (prvHstTimeMsBuffer_.size() < bin_size) {
            prvHstTimeMsBuffer_.resize(bin_size);
        }
        
        // Calculate bin centers using frame parameters (same as standalone histogram IOC)
        for (size_t i = 0; i < bin_size; ++i) {
            // Time value for bin i: bin_offset + i * bin_width (convert to milliseconds)
            prvHstTimeMsBuffer_[i] = (prvHstFrameBinOffset_ + i * prvHstFrameBinWidth_) * TPX3_TDC_CLOCK_PERIOD_SEC * 1e3;
        }
        
        // Copy running sum to buffer (convert 64-bit to 32-bit with overflow protection for display)
        // For accumulated data, use 64-bit array
        std::vector<epicsInt64> prvHstData64Buffer(bin_size);
        for (size_t i = 0; i < bin_size; ++i) {
            uint64_t val64 = prvHstRunningSum_->get_bin_value_64(i);
            prvHstData64Buffer[i] = static_cast<epicsInt64>(val64);
            // Also store in 32-bit buffer for frame display
            prvHstArrayData32Buffer_[i] = (val64 > UINT32_MAX) ? UINT32_MAX : static_cast<epicsInt32>(val64);
        }
        
        // Unlock mutex before callbacks to avoid deadlocks (similar to processImgFrame)
        epicsMutexUnlock(prvHstMutex_);
        
        // Update time axis waveform (bin_size elements for bin centers)
        // Callbacks must be done OUTSIDE mutex to avoid deadlocks
        // Validate parameter index and buffer
        if (ADTimePixPrvHstHistogramTimeMs < 0) {
            fprintf(stderr, "ERROR: ADTimePixPrvHstHistogramTimeMs parameter index is invalid: %d\n", 
                    ADTimePixPrvHstHistogramTimeMs);
            fflush(stderr);
        } else if (!prvHstTimeMsBuffer_.data() || prvHstTimeMsBuffer_.size() < bin_size) {
            fprintf(stderr, "ERROR: prvHstTimeMsBuffer_ is invalid: ptr=%p, size=%zu, needed=%zu\n",
                    prvHstTimeMsBuffer_.data(), prvHstTimeMsBuffer_.size(), bin_size);
            fflush(stderr);
        } else {
            // Try calling with error checking
            try {
                doCallbacksFloat64Array(prvHstTimeMsBuffer_.data(), bin_size, ADTimePixPrvHstHistogramTimeMs, 0);
            } catch (...) {
                fprintf(stderr, "ERROR: Exception caught in doCallbacksFloat64Array\n");
                fflush(stderr);
            }
        }
        
        // Update accumulated histogram data (64-bit)
        doCallbacksInt64Array(prvHstData64Buffer.data(), bin_size, ADTimePixPrvHstHistogramData, 0);
        
        // Update current frame histogram data (32-bit)
        // Need to re-lock mutex to safely access prvHstCurrentFrame_
        epicsMutexLock(prvHstMutex_);
        
        if (prvHstCurrentFrame_) {
            size_t frame_bin_size = prvHstCurrentFrame_->get_bin_size();
            if (frame_bin_size == bin_size) {
                // Copy frame data to local buffer before unlocking
                std::vector<epicsInt32> frameBuffer(bin_size);
                for (size_t i = 0; i < bin_size; ++i) {
                    frameBuffer[i] = static_cast<epicsInt32>(prvHstCurrentFrame_->get_bin_value_32(i));
                }
                
                epicsMutexUnlock(prvHstMutex_);
                
                doCallbacksInt32Array(frameBuffer.data(), bin_size, ADTimePixPrvHstHistogramFrame, 0);
                
                // Re-lock mutex for remaining operations
                epicsMutexLock(prvHstMutex_);
            }
        }
    } else {
        // Re-lock mutex if we didn't enter the prvHstRunningSum_ block
        epicsMutexLock(prvHstMutex_);
    }
    
    // NDArrays for PrvHst file plugins: addr 4=sum-of-N, 5=running sum, 6=current frame, 7=ToF bin centers (ms)
    if (prvHstRunningSum_ && this->pNDArrayPool) {
        size_t bin_size = prvHstRunningSum_->get_bin_size();
        size_t dims[3];
        dims[0] = bin_size;
        dims[1] = 0;
        dims[2] = 0;

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

        epicsTimeStamp timestamp;
        epicsTimeGetCurrent(&timestamp);
        const double tsFloat = timestamp.secPastEpoch + timestamp.nsec / 1.e9;

        int nCb = 0;
        auto doOneHistNdArray = [&](NDArray* pArr, int addr) {
            if (!pArr) return;
            if (pArr->pData && arrayCallbacks) {
                pArr->timeStamp = tsFloat;
                updateTimeStamp(&pArr->epicsTS);
                doCallbacksGenericPointer(pArr, NDArrayData, addr);
                nCb++;
            }
            pArr->release();
        };

        auto addLinearTimeAttrs = [&](NDArray* pArr) {
            if (!pArr || !pArr->pAttributeList || bin_size == 0 || prvHstTimeMsBuffer_.size() < bin_size) return;
            double t0ms = prvHstTimeMsBuffer_[0];
            double tStepMs = (bin_size > 1) ? (prvHstTimeMsBuffer_[1] - prvHstTimeMsBuffer_[0]) : 0.0;
            epicsInt32 nBinsA = static_cast<epicsInt32>(bin_size);
            pArr->pAttributeList->add("PrvHstTimeBin0Ms", "First bin center (ms)", NDAttrFloat64, &t0ms);
            pArr->pAttributeList->add("PrvHstTimeBinStepMs", "Bin center spacing (ms)", NDAttrFloat64, &tStepMs);
            pArr->pAttributeList->add("PrvHstNumBins", "Number of bins", NDAttrInt32, &nBinsA);
        };

        if (bin_size > 0) {
            NDArray* pHistArray = this->pNDArrayPool->alloc(1, dims, NDInt64, 0, NULL);
            if (pHistArray && pHistArray->pData) {
                epicsInt64* pData = reinterpret_cast<epicsInt64*>(pHistArray->pData);
                for (size_t i = 0; i < bin_size; ++i) {
                    pData[i] = static_cast<epicsInt64>(prvHstRunningSum_->get_bin_value_64(i));
                }
                if (pHistArray->pAttributeList) {
                    this->getAttributes(pHistArray->pAttributeList);
                    addLinearTimeAttrs(pHistArray);
                }
                doOneHistNdArray(pHistArray, 5);
            } else if (pHistArray) {
                pHistArray->release();
            }

            NDArray* pTime = this->pNDArrayPool->alloc(1, dims, NDFloat64, 0, NULL);
            if (pTime && pTime->pData && prvHstTimeMsBuffer_.size() >= bin_size) {
                epicsFloat64* pT = reinterpret_cast<epicsFloat64*>(pTime->pData);
                for (size_t i = 0; i < bin_size; ++i) pT[i] = prvHstTimeMsBuffer_[i];
                if (pTime->pAttributeList) this->getAttributes(pTime->pAttributeList);
                doOneHistNdArray(pTime, 7);
            } else if (pTime) {
                pTime->release();
            }

            if (prvHstCurrentFrame_ && prvHstCurrentFrame_->get_bin_size() == bin_size) {
                NDArray* pFr = this->pNDArrayPool->alloc(1, dims, NDInt32, 0, NULL);
                if (pFr && pFr->pData) {
                    epicsInt32* pD = reinterpret_cast<epicsInt32*>(pFr->pData);
                    for (size_t i = 0; i < bin_size; ++i) {
                        pD[i] = static_cast<epicsInt32>(prvHstCurrentFrame_->get_bin_value_32(i));
                    }
                    if (pFr->pAttributeList) this->getAttributes(pFr->pAttributeList);
                    doOneHistNdArray(pFr, 6);
                } else if (pFr) {
                    pFr->release();
                }
            }

            if (prvHstSumNUpdatedThisFrame && prvHstSumArray64Buffer_.size() >= bin_size) {
                NDArray* pSumN = this->pNDArrayPool->alloc(1, dims, NDInt64, 0, NULL);
                if (pSumN && pSumN->pData) {
                    epicsInt64* pD = reinterpret_cast<epicsInt64*>(pSumN->pData);
                    for (size_t i = 0; i < bin_size; ++i) pD[i] = prvHstSumArray64Buffer_[i];
                    if (pSumN->pAttributeList) {
                        this->getAttributes(pSumN->pAttributeList);
                        addLinearTimeAttrs(pSumN);
                    }
                    doOneHistNdArray(pSumN, 4);
                } else if (pSumN) {
                    pSumN->release();
                }
            }
        }

        int currentArrayCounter = 0;
        getIntegerParam(NDArrayCounter, &currentArrayCounter);
        if (nCb > 0 && currentArrayCounter == savedArrayCounter + nCb) {
            setIntegerParam(NDArrayCounter, savedArrayCounter);
        }

        setIntegerParam(ADSizeX, savedSizeX);
        setIntegerParam(ADSizeY, savedSizeY);
        setIntegerParam(NDArraySizeX, savedArraySizeX);
        setIntegerParam(NDArraySizeY, savedArraySizeY);
        setIntegerParam(NDDataType, savedDataType);
        setIntegerParam(NDArraySize, savedArraySize);
    }
    
    // Calculate processing time for this frame
    epicsTimeStamp processing_end_time;
    epicsTimeGetCurrent(&processing_end_time);
    double processing_time_ms = ((processing_end_time.secPastEpoch - processing_start_time.secPastEpoch) * 1000.0) +
                               ((processing_end_time.nsec - processing_start_time.nsec) / 1e6);
    
    // Add to processing time samples for averaging
    prvHstProcessingTimeSamples_.push_back(processing_time_ms);
    if (prvHstProcessingTimeSamples_.size() > PRVHST_MAX_PROCESSING_TIME_SAMPLES) {
        prvHstProcessingTimeSamples_.erase(prvHstProcessingTimeSamples_.begin());
    }
    
    // Calculate average processing time
    double sum = 0.0;
    for (size_t i = 0; i < prvHstProcessingTimeSamples_.size(); ++i) {
        sum += prvHstProcessingTimeSamples_[i];
    }
    prvHstProcessingTime_ = sum / prvHstProcessingTimeSamples_.size();
    
    // Update processing time PV once per second
    double current_time_seconds = processing_end_time.secPastEpoch + processing_end_time.nsec / 1e9;
    if (current_time_seconds - prvHstLastProcessingTimeUpdate_ >= 1.0) {
        setDoubleParam(ADTimePixPrvHstProcessingTime, prvHstProcessingTime_);
        callParamCallbacks(ADTimePixPrvHstProcessingTime);
        prvHstLastProcessingTimeUpdate_ = current_time_seconds;
    }
    
    // Update memory usage every 5 seconds
    if (current_time_seconds - prvHstLastMemoryUpdateTime_ >= PRVHST_MEMORY_UPDATE_INTERVAL_SEC) {
        prvHstMemoryUsage_ = calculatePrvHstMemoryUsageMB();
        setDoubleParam(ADTimePixPrvHstMemoryUsage, prvHstMemoryUsage_);
        callParamCallbacks(ADTimePixPrvHstMemoryUsage);
        prvHstLastMemoryUpdateTime_ = current_time_seconds;
    }
    
    // Update frames to sum and sum update interval PVs
    setIntegerParam(ADTimePixPrvHstFramesToSum, prvHstFramesToSum_);
    setIntegerParam(ADTimePixPrvHstSumUpdateInterval, prvHstSumUpdateIntervalFrames_);
    
    epicsMutexUnlock(prvHstMutex_);
    
    // Call parameter callbacks only for histogram-specific parameters to avoid triggering
    // unnecessary updates to shared parameters (ADSizeX, ADSizeY, etc.) that could cause
    // PVA plugin flickering. Individual parameter callbacks are already called above for
    // processing time and memory usage when they update.
    callParamCallbacks(ADTimePixPrvHstFramesToSum);
    callParamCallbacks(ADTimePixPrvHstSumUpdateInterval);
    if (rollingWindowParamsChanged) {
        // Publish the new effective window/status on the driver address.
        callParamCallbacks(0);
    }
}

void ADTimePix::prvHstConnect() {
    const char* functionName = "prvHstConnect";
    if (!prvHstMutex_) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP: Mutex not initialized\n", functionName);
        prvHstWorkerState_.fail();
        return;
    }

    epicsMutexLock(prvHstMutex_);
    const std::string host = prvHstHost_;
    const int port = prvHstPort_;
    epicsMutexUnlock(prvHstMutex_);

    if (!prvHstWorkerState_.running()) {
        return;
    }
    if (host.empty() || port <= 0) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP: Invalid host or port\n",
                functionName);
        return;
    }

    prvHstDisconnect();
    const std::shared_ptr<NetworkClient> client = std::make_shared<NetworkClient>();
    std::atomic_store(&prvHstNetworkClient_, client);
    if (!prvHstWorkerState_.running()) {
        prvHstDisconnect();
        return;
    }
    if (client->connect(host, port)) {
        prvHstWorkerState_.markConnected();
        if (prvHstWorkerState_.connected()) {
            printf("PrvHst TCP connected to %s:%d\n", host.c_str(), port);
            return;
        }
    }

    if (prvHstWorkerState_.running()) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP failed to connect to %s:%d\n",
                functionName, host.c_str(), port);
    }
    prvHstDisconnect();
}

void ADTimePix::prvHstDisconnect() {
    prvHstWorkerState_.markDisconnected();
    std::shared_ptr<NetworkClient> client =
        std::atomic_exchange(&prvHstNetworkClient_, std::shared_ptr<NetworkClient>());
    if (client) {
        client->disconnect();
        printf("PrvHst TCP disconnected\n");
    }
}

void ADTimePix::prvHstWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->prvHstWorkerThread();
}

void ADTimePix::prvHstWorkerThread() {
    const char* functionName = "prvHstWorkerThread";
    constexpr auto reconnectDelay = std::chrono::milliseconds(1000);
    constexpr auto accumulationDelay = std::chrono::milliseconds(100);

    if (!prvHstMutex_) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst worker thread: Mutex not initialized\n", functionName);
        prvHstWorkerState_.fail();
        return;
    }

    prvHstLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvHstTotalRead_ = 0;
    constexpr std::size_t maxHistogramBins = 1000000;
    constexpr std::size_t maxHistogramPayload = maxHistogramBins * sizeof(uint32_t);
    ADTimePix3Stream::FrameBuffer frameBuffer(
        MAX_BUFFER_SIZE, maxHistogramPayload, MAX_BUFFER_SIZE);

    const auto resolvePayloadSize =
        [](const std::string& header, std::size_t& payloadBytes,
           std::string& error) {
            const json parsed = json::parse(header, nullptr, false);
            if (!parsed.is_object() || !parsed.contains("binSize") ||
                !parsed["binSize"].is_number_integer()) {
                error = "malformed jsonhisto header or invalid binSize";
                return false;
            }

            const long long binSize = parsed["binSize"].get<long long>();
            if (binSize <= 0 ||
                static_cast<unsigned long long>(binSize) > maxHistogramBins) {
                error = "binSize is outside the supported range";
                return false;
            }
            payloadBytes = static_cast<std::size_t>(binSize) * sizeof(uint32_t);
            return true;
        };

    bool frameError = false;
    while (prvHstWorkerState_.running() && !frameError) {
        epicsMutexLock(prvHstMutex_);
        const std::string host = prvHstHost_;
        const int port = prvHstPort_;
        epicsMutexUnlock(prvHstMutex_);

        if (!prvHstWorkerState_.connected() && !host.empty() && port > 0) {
            printf("PrvHst worker thread: Attempting to connect to %s:%d\n",
                   host.c_str(), port);
            prvHstConnect();
        }
        if (!prvHstWorkerState_.running()) {
            break;
        }
        if (!prvHstWorkerState_.connected()) {
            prvHstWorkerState_.waitForStopFor(reconnectDelay);
            continue;
        }

        int accumulationEnable = 0;
        getIntegerParam(ADTimePixPrvHstAccumulationEnable, &accumulationEnable);
        if (!accumulationEnable) {
            prvHstWorkerState_.waitForStopFor(accumulationDelay);
            continue;
        }

        const std::shared_ptr<NetworkClient> client =
            std::atomic_load(&prvHstNetworkClient_);
        if (!client) {
            prvHstWorkerState_.waitForStopFor(reconnectDelay);
            continue;
        }

        try {
            const ssize_t bytesRead = client->receive(
                prvHstLineBuffer_.data(), prvHstLineBuffer_.size());
            const int receiveError = (bytesRead < 0) ? errno : 0;

            if (bytesRead <= 0) {
                if (bytesRead < 0 && NetworkClient::isReceiveTimeout(receiveError) &&
                    prvHstWorkerState_.running()) {
                    continue;
                }
                if (bytesRead == 0) {
                    const ADTimePix3Stream::EndOfStreamStatus endStatus =
                        frameBuffer.endOfStreamStatus();
                    if (endStatus != ADTimePix3Stream::EndOfStreamStatus::Clean &&
                        prvHstWorkerState_.running()) {
                        fprintf(stderr,
                                "ERROR | ADTimePix::%s: PrvHst TCP stream ended with %s\n",
                                functionName,
                                ADTimePix3Stream::endOfStreamStatusMessage(endStatus));
                    }
                    printf("PrvHst TCP connection closed by peer\n");
                } else if (prvHstWorkerState_.running()) {
                    fprintf(stderr, "PrvHst TCP socket error: %s\n",
                            strerror(receiveError));
                }
                prvHstWorkerState_.fail();
                break;
            }

            const ADTimePix3Stream::FrameResult appendResult =
                frameBuffer.append(prvHstLineBuffer_.data(),
                                   static_cast<std::size_t>(bytesRead));
            if (appendResult == ADTimePix3Stream::FrameResult::BufferLimitExceeded) {
                fprintf(stderr,
                        "ERROR | ADTimePix::%s: PrvHst rejected TCP stream: %s\n",
                        functionName,
                        ADTimePix3Stream::frameResultMessage(appendResult));
                frameError = true;
                break;
            }
            prvHstTotalRead_ = frameBuffer.bufferedBytes();

            while (prvHstWorkerState_.running()) {
                ADTimePix3Stream::FramedMessage frame;
                std::string framingError;
                const ADTimePix3Stream::FrameResult frameResult =
                    frameBuffer.next(resolvePayloadSize, frame, framingError);
                if (frameResult == ADTimePix3Stream::FrameResult::NeedMoreData) {
                    break;
                }
                if (frameResult != ADTimePix3Stream::FrameResult::FrameReady) {
                    fprintf(stderr,
                            "ERROR | ADTimePix::%s: PrvHst rejected TCP stream: %s%s%s\n",
                            functionName,
                            ADTimePix3Stream::frameResultMessage(frameResult),
                            framingError.empty() ? "" : ": ",
                            framingError.c_str());
                    frameError = true;
                    break;
                }

                std::vector<char> completeFrame;
                completeFrame.reserve(frame.header.size() + 1 + frame.payload.size());
                completeFrame.insert(completeFrame.end(),
                                     frame.header.begin(), frame.header.end());
                completeFrame.push_back(0);
                completeFrame.insert(completeFrame.end(),
                                     frame.payload.begin(), frame.payload.end());
                char* separator = completeFrame.data() + frame.header.size();
                if (!processPrvHstDataLine(completeFrame.data(), separator,
                                           completeFrame.size())) {
                    frameError = true;
                    break;
                }
                prvHstTotalRead_ = frameBuffer.bufferedBytes();
            }
        } catch (const std::exception& exception) {
            fprintf(stderr, "ERROR | ADTimePix::%s: %s\n",
                    functionName, exception.what());
            frameError = true;
        } catch (...) {
            fprintf(stderr, "ERROR | ADTimePix::%s: unknown exception\n", functionName);
            frameError = true;
        }
    }

    if (frameError) {
        prvHstWorkerState_.fail();
    }
    prvHstTotalRead_ = 0;
    prvHstDisconnect();
    printf("PrvHst worker thread exiting\n");
}
