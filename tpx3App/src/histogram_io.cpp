#include "histogram_io.h"
#include "ADTimePix.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <json.hpp>
#include <asynDriver.h>

using json = nlohmann::json;

// Error message formatters (same as ADTimePix.cpp)
#define ERR(msg)                                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: %s\n", driverName, functionName, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__)

// Warning message formatters
#define WARN(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: %s\n", driverName, functionName, msg)

#define WARN_ARGS(fmt, ...)                                                            \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__)

// Log message formatters
#define LOG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: %s\n", driverName, functionName, msg)

#define LOG_ARGS(fmt, ...)                                                                       \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: " fmt "\n", driverName, functionName, \
              __VA_ARGS__)

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
        
        int bin_size = j["binSize"];
                int bin_width = j["binWidth"];
                int bin_offset = j["binOffset"];
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
        
        // Read any remaining binary data needed
                if (binary_read < binary_needed) {
                                    if (!prvHstMutex_) {
                                return false;
            }
            
            epicsMutexLock(prvHstMutex_);
                        if (!prvHstNetworkClient_) {
                                epicsMutexUnlock(prvHstMutex_);
                return false;
            }
            
            if (!prvHstNetworkClient_->is_connected()) {
                                epicsMutexUnlock(prvHstMutex_);
                return false;
            }
            
                        char* dest_ptr = reinterpret_cast<char*>(tof_bin_values.data()) + binary_read;
                        if (!prvHstNetworkClient_->receive_exact(dest_ptr, binary_needed - binary_read)) {
                                epicsMutexUnlock(prvHstMutex_);
                fprintf(stderr, "ERROR | ADTimePix::%s: Failed to read binary histogram data\n", functionName);
                return false;
            }
            
                        epicsMutexUnlock(prvHstMutex_);
                    } else {
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
               frame_number, bin_size);
                processPrvHstFrame(frame_histogram);
        
            } catch (const std::exception& e) {
        ERR_ARGS("Error processing PrvHst frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::processPrvHstFrame(const HistogramData& frame_data) {
    const char* functionName = "processPrvHstFrame";
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
               frame_data.get_bin_size());
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
    
           running_sum_bin_size, frame_bin_size);
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
        prvHstFrameBuffer_.clear();  // Clear frame buffer on bin size change
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
    
        // Add to frame buffer (circular buffer for sum of N frames)
        prvHstFrameBuffer_.push_back(frame_data);
    
        // Remove old frames if buffer exceeds frames_to_sum_
           prvHstFrameBuffer_.size(), prvHstFramesToSum_);
        while (prvHstFrameBuffer_.size() > static_cast<size_t>(prvHstFramesToSum_)) {
                prvHstFrameBuffer_.pop_front();
    }
    
        // Update sum of last N frames if needed
        prvHstFramesSinceLastSumUpdate_++;
    bool should_update_sum = (prvHstFramesSinceLastSumUpdate_ >= prvHstSumUpdateIntervalFrames_);
    
           should_update_sum, prvHstFramesSinceLastSumUpdate_, prvHstSumUpdateIntervalFrames_);
        if (should_update_sum && !prvHstFrameBuffer_.empty()) {
                prvHstFramesSinceLastSumUpdate_ = 0;
        
        // Calculate sum of frames in buffer
        size_t frame_bin_size = prvHstFrameBuffer_[0].get_bin_size();
        
        if (prvHstSumArray64WorkBuffer_.size() < frame_bin_size) {
            prvHstSumArray64WorkBuffer_.resize(frame_bin_size);
            prvHstSumArray64Buffer_.resize(frame_bin_size);
        }
        
        std::memset(prvHstSumArray64WorkBuffer_.data(), 0, frame_bin_size * sizeof(uint64_t));
        
        for (const auto& frame : prvHstFrameBuffer_) {
            if (frame.get_bin_size() == frame_bin_size) {
                for (size_t i = 0; i < frame_bin_size; ++i) {
                    prvHstSumArray64WorkBuffer_[i] += frame.get_bin_value_32(i);
                }
            }
        }
        
        // Convert to epicsInt64
        for (size_t i = 0; i < frame_bin_size; ++i) {
            prvHstSumArray64Buffer_[i] = static_cast<epicsInt64>(prvHstSumArray64WorkBuffer_[i]);
        }
        
        // Update EPICS PV via callback for sum of N frames
        doCallbacksInt64Array(prvHstSumArray64Buffer_.data(), frame_bin_size, ADTimePixPrvHstHistogramSumNFrames, 0);
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
        
               bin_size, prvHstFrameBinOffset_, prvHstFrameBinWidth_);
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
               ADTimePixPrvHstHistogramTimeMs, bin_size, prvHstTimeMsBuffer_.data());
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
                   prvHstTimeMsBuffer_.size(), 
                   bin_size > 0 ? prvHstTimeMsBuffer_[0] : 0.0,
                   bin_size > 0 ? prvHstTimeMsBuffer_[bin_size-1] : 0.0);
                        // Try calling with error checking
            try {
                asynStatus status = doCallbacksFloat64Array(prvHstTimeMsBuffer_.data(), bin_size, ADTimePixPrvHstHistogramTimeMs, 0);
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
            } else {
                       frame_bin_size, bin_size);
                            }
        } else {
                    }
    } else {
        // Re-lock mutex if we didn't enter the prvHstRunningSum_ block
                epicsMutexLock(prvHstMutex_);
    }
    
    // Create NDArray for histogram (1D array)
    if (prvHstRunningSum_ && this->pNDArrayPool) {
        size_t bin_size = prvHstRunningSum_->get_bin_size();
        size_t dims[3];
        dims[0] = bin_size;
        dims[1] = 0;
        dims[2] = 0;
        
        // Allocate new NDArray for histogram (1D array)
        NDArray *pHistArray = this->pNDArrayPool->alloc(1, dims, NDInt64, 0, NULL);
        
        if (pHistArray && pHistArray->pData) {
            // Copy histogram data to NDArray
            epicsInt64* pData = reinterpret_cast<epicsInt64*>(pHistArray->pData);
            for (size_t i = 0; i < bin_size; ++i) {
                pData[i] = static_cast<epicsInt64>(prvHstRunningSum_->get_bin_value_64(i));
            }
            
            // Set image parameters
            setIntegerParam(ADSizeX, static_cast<int>(bin_size));
            setIntegerParam(NDArraySizeX, static_cast<int>(bin_size));
            setIntegerParam(ADSizeY, 1);
            setIntegerParam(NDArraySizeY, 1);
            setIntegerParam(NDDataType, NDInt64);
            setIntegerParam(NDColorMode, NDColorModeMono);
            
            NDArrayInfo_t arrayInfo;
            pHistArray->getInfo(&arrayInfo);
            setIntegerParam(NDArraySize, static_cast<int>(arrayInfo.totalBytes));
            
            // Set timestamp
            epicsTimeStamp timestamp;
            epicsTimeGetCurrent(&timestamp);
            pHistArray->timeStamp = timestamp.secPastEpoch + timestamp.nsec / 1.e9;
            updateTimeStamp(&pHistArray->epicsTS);
            
            // Get attributes
            if (pHistArray->pAttributeList) {
                this->getAttributes(pHistArray->pAttributeList);
            }
            
            // Trigger NDArray callbacks with address 5 for PrvHst
            int arrayCallbacks = 0;
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
            if (arrayCallbacks && pHistArray) {
                doCallbacksGenericPointer(pHistArray, NDArrayData, 5);
            }
            
            // Release the array (callbacks will increment reference count if needed)
            pHistArray->release();
        }
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
        // Calculate memory usage (similar to standalone histogram IOC)
        double total_memory_mb = 0.0;
        if (prvHstRunningSum_) {
            size_t bin_size = prvHstRunningSum_->get_bin_size();
            total_memory_mb += (bin_size * sizeof(uint64_t) + (bin_size + 1) * sizeof(double)) / (1024.0 * 1024.0);
        }
        total_memory_mb += prvHstTimeMsBuffer_.size() * sizeof(epicsFloat64) / (1024.0 * 1024.0);
        for (const auto& frame : prvHstFrameBuffer_) {
            size_t bin_size = frame.get_bin_size();
            total_memory_mb += (bin_size * sizeof(uint32_t) + (bin_size + 1) * sizeof(double)) / (1024.0 * 1024.0);
        }
        total_memory_mb += (prvHstRateSamples_.size() + prvHstProcessingTimeSamples_.size()) * sizeof(double) / (1024.0 * 1024.0);
        total_memory_mb += prvHstLineBuffer_.size() * sizeof(char) / (1024.0 * 1024.0);
        total_memory_mb += 0.1;  // Estimated overhead
        
        prvHstMemoryUsage_ = total_memory_mb;
        setDoubleParam(ADTimePixPrvHstMemoryUsage, prvHstMemoryUsage_);
        callParamCallbacks(ADTimePixPrvHstMemoryUsage);
        prvHstLastMemoryUpdateTime_ = current_time_seconds;
    }
    
    // Update frames to sum and sum update interval PVs
    setIntegerParam(ADTimePixPrvHstFramesToSum, prvHstFramesToSum_);
    setIntegerParam(ADTimePixPrvHstSumUpdateInterval, prvHstSumUpdateIntervalFrames_);
    
    epicsMutexUnlock(prvHstMutex_);
    
    // Call parameter callbacks to update EPICS PVs
    callParamCallbacks();
}

void ADTimePix::prvHstConnect() {
    const char* functionName = "prvHstConnect";
    // NOTE: Avoid asynPrint macros here while debugging a segfault in the worker thread.
    // Use printf/fprintf so we don't depend on pasynUserSelf being valid in this thread.
    
    if (!prvHstMutex_) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP: Mutex not initialized\n", functionName);
        return;
    }
    
    epicsMutexLock(prvHstMutex_);
    std::string host = prvHstHost_;
    int port = prvHstPort_;
    epicsMutexUnlock(prvHstMutex_);
    
    if (host.empty() || port <= 0) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP: Invalid host or port\n", functionName);
        return;
    }
    
    prvHstDisconnect(); // Ensure clean state
    
    prvHstNetworkClient_.reset(new NetworkClient());
    if (!prvHstNetworkClient_) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP: Failed to create NetworkClient\n", functionName);
        return;
    }
    
    if (prvHstNetworkClient_->connect(host, port)) {
        epicsMutexLock(prvHstMutex_);
        prvHstConnected_ = true;
        epicsMutexUnlock(prvHstMutex_);
        printf("PrvHst TCP connected to %s:%d\n", host.c_str(), port);
    } else {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst TCP failed to connect to %s:%d\n", functionName, host.c_str(), port);
        prvHstNetworkClient_.reset();
    }
}

void ADTimePix::prvHstDisconnect() {
    // NOTE: Avoid asynPrint macros here while debugging a segfault in the worker thread.
    // Use printf/fprintf so we don't depend on pasynUserSelf being valid in this thread.
    
    epicsMutexLock(prvHstMutex_);
    prvHstConnected_ = false;
    epicsMutexUnlock(prvHstMutex_);
    
    if (prvHstNetworkClient_) {
        prvHstNetworkClient_->disconnect();
        prvHstNetworkClient_.reset();
    }
    
    printf("PrvHst TCP disconnected\n");
}

void ADTimePix::prvHstWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->prvHstWorkerThread();
}

void ADTimePix::prvHstWorkerThread() {
    const char* functionName = "prvHstWorkerThread";
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!prvHstMutex_) {
        fprintf(stderr, "ERROR | ADTimePix::%s: PrvHst worker thread: Mutex not initialized\n", functionName);
        return;
    }
    
    prvHstLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvHstTotalRead_ = 0;
    
    while (prvHstRunning_) {
        epicsMutexLock(prvHstMutex_);
        bool should_connect = prvHstRunning_ && !prvHstConnected_;
        std::string host = prvHstHost_;
        int port = prvHstPort_;
        epicsMutexUnlock(prvHstMutex_);
        
        if (should_connect && !host.empty() && port > 0) {
            printf("PrvHst worker thread: Attempting to connect to %s:%d\n", host.c_str(), port);
            prvHstConnect();
            epicsMutexLock(prvHstMutex_);
            bool connected_after = prvHstConnected_;
            epicsMutexUnlock(prvHstMutex_);
            if (!connected_after) {
                printf("PrvHst worker thread: Connection failed, will retry in %.1f seconds\n", RECONNECT_DELAY_SEC);
                epicsThreadSleep(RECONNECT_DELAY_SEC);
                continue;
            } else {
                printf("PrvHst worker thread: Connection successful, starting data reception\n");
            }
        } else if (should_connect) {
            if (host.empty() || port <= 0) {
                printf("PrvHst worker thread: Invalid host/port (host='%s', port=%d), waiting...\n", host.c_str(), port);
                epicsThreadSleep(RECONNECT_DELAY_SEC);
                continue;
            }
        }
        
        if (!prvHstRunning_) {
            break;
        }
        
        epicsMutexLock(prvHstMutex_);
        bool connected = prvHstConnected_;
        bool has_client = (prvHstNetworkClient_ != nullptr);
        epicsMutexUnlock(prvHstMutex_);
        
        if (connected && has_client) {
            try {
                // Defensive checks before accessing network client
                if (!prvHstMutex_) {
                                        break;
                }
                
                epicsMutexLock(prvHstMutex_);
                
                // Re-check inside mutex to avoid race condition
                if (!prvHstNetworkClient_) {
                                        epicsMutexUnlock(prvHstMutex_);
                    break;
                }
                
                if (prvHstTotalRead_ >= MAX_BUFFER_SIZE) {
                    prvHstTotalRead_ = 0;
                }
                
                if (prvHstLineBuffer_.size() < MAX_BUFFER_SIZE) {
                    prvHstLineBuffer_.resize(MAX_BUFFER_SIZE);
                }
                
                size_t available_space = MAX_BUFFER_SIZE - prvHstTotalRead_ - 1;
                if (available_space == 0 || available_space > MAX_BUFFER_SIZE) {
                    epicsMutexUnlock(prvHstMutex_);
                    break;
                }
                
                // Store pointer locally to avoid issues if it's reset during receive
                NetworkClient* client = prvHstNetworkClient_.get();
                if (!client) {
                                        epicsMutexUnlock(prvHstMutex_);
                    break;
                }
                
                // Get buffer pointer and validate
                char* buffer_ptr = prvHstLineBuffer_.data() + prvHstTotalRead_;
                if (!buffer_ptr) {
                                        epicsMutexUnlock(prvHstMutex_);
                    break;
                }
                
                                ssize_t bytes_read = client->receive(
                    buffer_ptr,
                    available_space
                );
                
                                epicsMutexUnlock(prvHstMutex_);
                
                                if (bytes_read > 0) {
                    static int log_counter = 0;
                    if (++log_counter % 100 == 0) {  // Log every 100 reads to avoid spam
                    }
                }
                
                                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(prvHstMutex_);
                        prvHstConnected_ = false;
                        prvHstRunning_ = false;
                        epicsMutexUnlock(prvHstMutex_);
                        printf("PrvHst TCP connection closed by peer\n");
                        break;
                    } else {
                        epicsMutexLock(prvHstMutex_);
                        if (prvHstConnected_) {
                            prvHstConnected_ = false;
                            prvHstRunning_ = false;
                            printf("PrvHst TCP socket error: %s\n", strerror(errno));
                        }
                        epicsMutexUnlock(prvHstMutex_);
                        break;
                    }
                }
                
                                epicsMutexLock(prvHstMutex_);
                
                                prvHstTotalRead_ += bytes_read;
                
                                // Ensure we don't write out of bounds
                if (prvHstTotalRead_ >= MAX_BUFFER_SIZE) {
                    fprintf(stderr, "ERROR | ADTimePix::prvHstWorkerThread: Buffer overflow! prvHstTotalRead_=%zu, MAX_BUFFER_SIZE=%zu\n", 
                            prvHstTotalRead_, MAX_BUFFER_SIZE);
                    prvHstTotalRead_ = MAX_BUFFER_SIZE - 1;
                }
                
                                prvHstLineBuffer_[prvHstTotalRead_] = '\0';
                
                                // Look for newline to find complete JSON line
                                char* buffer_data = prvHstLineBuffer_.data();
                if (!buffer_data) {
                                        epicsMutexUnlock(prvHstMutex_);
                    break;
                }
                
                                char* newline_pos = static_cast<char*>(memchr(buffer_data, '\n', prvHstTotalRead_));
                
                                if (newline_pos) {
                                        // Found a newline - check if there's valid JSON before it
                    char* json_start = nullptr;
                    
                    // Try to find {" pattern (most reliable indicator of JSON)
                    for (char* p = prvHstLineBuffer_.data(); p < newline_pos - 1; ++p) {
                        if (*p == '{' && p[1] == '"') {
                            json_start = p;
                            break;
                        }
                    }
                    
                    // If we didn't find {", try finding { followed by valid JSON structure
                    if (!json_start) {
                        for (char* p = prvHstLineBuffer_.data(); p < newline_pos - 2; ++p) {
                            if (*p == '{') {
                                bool looks_like_json = false;
                                size_t check_len = std::min(size_t(newline_pos - p - 1), size_t(100));
                                
                                for (size_t i = 1; i < check_len; ++i) {
                                    char c = p[i];
                                    if (c == '"' || c == ':' || c == ',' || c == '}' || c == '[' || c == ']') {
                                        looks_like_json = true;
                                        break;
                                    }
                                    if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                                        break;
                                    }
                                }
                                
                                if (looks_like_json) {
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
                            if (test_json.contains("binSize") || test_json.contains("binWidth") ||
                                test_json.contains("timeAtFrame")) {
                                is_valid_json = true;
                            }
                        } catch (...) {
                            is_valid_json = false;
                        }
                        
                        if (is_valid_json) {
                            *newline_pos = '\0';
                            
                            // Process the JSON line
                                                        if (!processPrvHstDataLine(json_start, newline_pos, prvHstTotalRead_)) {
                                                                epicsMutexUnlock(prvHstMutex_);
                                break;
                            }
                            
                                                        // Move remaining data to start of buffer
                            size_t remaining = prvHstTotalRead_ - (newline_pos - prvHstLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvHstLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvHstTotalRead_ = remaining;
                        } else {
                            // Found { but it's not valid JSON - skip this newline
                            size_t remaining = prvHstTotalRead_ - (newline_pos - prvHstLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvHstLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvHstTotalRead_ = remaining;
                        }
                    } else {
                        // Found newline but no valid JSON - might be binary data
                        size_t remaining = prvHstTotalRead_ - (newline_pos - prvHstLineBuffer_.data() + 1);
                        if (remaining > 0) {
                            memmove(prvHstLineBuffer_.data(), newline_pos + 1, remaining);
                        }
                        prvHstTotalRead_ = remaining;
                    }
                } else {
                    // No newline found yet - check if buffer is getting too full
                    if (prvHstTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                        printf("PrvHst TCP buffer full without finding newline, resetting\n");
                        prvHstTotalRead_ = 0;
                    }
                }
                
                if (prvHstTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    printf("PrvHst TCP buffer full, resetting\n");
                    prvHstTotalRead_ = 0;
                }
                
                epicsMutexUnlock(prvHstMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(prvHstMutex_);
                fprintf(stderr, "ERROR | ADTimePix::prvHstWorkerThread: Error in worker thread: %s\n", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    prvHstDisconnect();
    // Clear thread ID before exiting to allow acquireStop to detect thread already exited
    epicsMutexLock(prvHstMutex_);
    prvHstWorkerThreadId_ = NULL;
    epicsMutexUnlock(prvHstMutex_);
    
    printf("PrvHst worker thread exiting\n");
}
