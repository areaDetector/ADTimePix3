#include "histogram_io.h"
#include "ADTimePix.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
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
    
    // Skip empty lines
    if (strlen(line_buffer) == 0) {
        return true;
    }
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
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
        ERR_ARGS("JSON parse error in PrvHst: %s", e.what());
        return true;  // Continue processing
    }
    
    try {
        // Extract header information for jsonhisto
        int bin_size = j["binSize"];
        int bin_width = j["binWidth"];
        int bin_offset = j["binOffset"];
        
        // Extract additional frame data
        int frame_number = j.value("frameNumber", 0);
        
        // Validate bin size
        if (bin_size <= 0 || bin_size > 1000000) {
            ERR_ARGS("Invalid bin size: %d", bin_size);
            return false;
        }
        
        // Calculate acquisition rate (similar to PrvImg/Img)
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
        epicsMutexLock(prvHstMutex_);
        if (binary_read < binary_needed && prvHstNetworkClient_ && prvHstNetworkClient_->is_connected()) {
            if (!prvHstNetworkClient_->receive_exact(
                reinterpret_cast<char*>(tof_bin_values.data()) + binary_read,
                binary_needed - binary_read)) {
                epicsMutexUnlock(prvHstMutex_);
                ERR("Failed to read binary histogram data");
                return false;
            }
        }
        epicsMutexUnlock(prvHstMutex_);
        
        // Convert network byte order to host byte order
        for (int i = 0; i < bin_size; ++i) {
            tof_bin_values[i] = __builtin_bswap32(tof_bin_values[i]);
            frame_histogram.set_bin_value_32(i, tof_bin_values[i]);
        }
        
        // Process frame
        processPrvHstFrame(frame_histogram);
        
    } catch (const std::exception& e) {
        ERR_ARGS("Error processing PrvHst frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::processPrvHstFrame(const HistogramData& frame_data) {
    const char* functionName = "processPrvHstFrame";
    const char* driverName = "ADTimePix";
    epicsTimeStamp processing_start_time;
    epicsTimeGetCurrent(&processing_start_time);
    
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
    }
    
    // Check if bin sizes match
    if (prvHstRunningSum_->get_bin_size() != frame_data.get_bin_size()) {
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
    }
    
    // Add frame data to running sum
    try {
        prvHstRunningSum_->add_histogram(frame_data);
    } catch (const std::exception& e) {
        ERR_ARGS("Failed to add histogram to running sum: %s", e.what());
        epicsMutexUnlock(prvHstMutex_);
        return;
    }
    
    // Store current frame
    if (!prvHstCurrentFrame_) {
        prvHstCurrentFrame_.reset(new HistogramData(frame_data.get_bin_size(), HistogramData::DataType::FRAME_DATA));
    }
    *prvHstCurrentFrame_ = frame_data;
    
    // Calculate total counts for this frame
    size_t bin_size = frame_data.get_bin_size();
    uint64_t frame_total = 0;
    for (size_t i = 0; i < bin_size; ++i) {
        frame_total += frame_data.get_bin_value_32(i);
    }
    prvHstTotalCounts_ += frame_total;
    
    // Add to frame buffer (circular buffer for sum of N frames)
    prvHstFrameBuffer_.push_back(frame_data);
    
    // Remove old frames if buffer exceeds frames_to_sum_
    while (prvHstFrameBuffer_.size() > static_cast<size_t>(prvHstFramesToSum_)) {
        prvHstFrameBuffer_.pop_front();
    }
    
    // Update sum of last N frames if needed
    prvHstFramesSinceLastSumUpdate_++;
    bool should_update_sum = (prvHstFramesSinceLastSumUpdate_ >= prvHstSumUpdateIntervalFrames_);
    
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
        
        // Create time axis from bin edges (convert seconds to milliseconds)
        // For plotting, we use bin centers (not edges), so bin_size elements
        const auto& bin_edges = prvHstRunningSum_->get_bin_edges();
        if (prvHstTimeMsBuffer_.size() < bin_size) {
            prvHstTimeMsBuffer_.resize(bin_size);
        }
        // Calculate bin centers: average of left and right edges
        for (size_t i = 0; i < bin_size; ++i) {
            double bin_center = (bin_edges[i] + bin_edges[i + 1]) / 2.0;
            prvHstTimeMsBuffer_[i] = bin_center * 1000.0;  // Convert to milliseconds
        }
        
        // Update time axis waveform (bin_size elements for bin centers)
        doCallbacksFloat64Array(prvHstTimeMsBuffer_.data(), bin_size, ADTimePixPrvHstHistogramTimeMs, 0);
        
        // Copy running sum to buffer (convert 64-bit to 32-bit with overflow protection for display)
        // For accumulated data, use 64-bit array
        std::vector<epicsInt64> prvHstData64Buffer(bin_size);
        for (size_t i = 0; i < bin_size; ++i) {
            uint64_t val64 = prvHstRunningSum_->get_bin_value_64(i);
            prvHstData64Buffer[i] = static_cast<epicsInt64>(val64);
            // Also store in 32-bit buffer for frame display
            prvHstArrayData32Buffer_[i] = (val64 > UINT32_MAX) ? UINT32_MAX : static_cast<epicsInt32>(val64);
        }
        
        // Update accumulated histogram data (64-bit)
        doCallbacksInt64Array(prvHstData64Buffer.data(), bin_size, ADTimePixPrvHstHistogramData, 0);
        
        // Update current frame histogram data (32-bit)
        if (prvHstCurrentFrame_) {
            size_t frame_bin_size = prvHstCurrentFrame_->get_bin_size();
            if (frame_bin_size == bin_size) {
                std::vector<epicsInt32> frameBuffer(bin_size);
                for (size_t i = 0; i < bin_size; ++i) {
                    frameBuffer[i] = static_cast<epicsInt32>(prvHstCurrentFrame_->get_bin_value_32(i));
                }
                doCallbacksInt32Array(frameBuffer.data(), bin_size, ADTimePixPrvHstHistogramFrame, 0);
            }
        }
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
    
    epicsMutexUnlock(prvHstMutex_);
    
    // Call parameter callbacks to update EPICS PVs
    callParamCallbacks();
}

void ADTimePix::prvHstConnect() {
    const char* functionName = "prvHstConnect";
    const char* driverName = "ADTimePix";
    
    if (!prvHstMutex_) {
        ERR("PrvHst TCP: Mutex not initialized");
        return;
    }
    
    epicsMutexLock(prvHstMutex_);
    std::string host = prvHstHost_;
    int port = prvHstPort_;
    epicsMutexUnlock(prvHstMutex_);
    
    if (host.empty() || port <= 0) {
        ERR("PrvHst TCP: Invalid host or port");
        return;
    }
    
    prvHstDisconnect(); // Ensure clean state
    
    prvHstNetworkClient_.reset(new NetworkClient());
    if (!prvHstNetworkClient_) {
        ERR("PrvHst TCP: Failed to create NetworkClient");
        return;
    }
    
    if (prvHstNetworkClient_->connect(host, port)) {
        epicsMutexLock(prvHstMutex_);
        prvHstConnected_ = true;
        epicsMutexUnlock(prvHstMutex_);
        LOG_ARGS("PrvHst TCP connected to %s:%d", host.c_str(), port);
    } else {
        ERR_ARGS("PrvHst TCP failed to connect to %s:%d", host.c_str(), port);
        prvHstNetworkClient_.reset();
    }
}

void ADTimePix::prvHstDisconnect() {
    const char* functionName = "prvHstDisconnect";
    const char* driverName = "ADTimePix";
    
    epicsMutexLock(prvHstMutex_);
    prvHstConnected_ = false;
    epicsMutexUnlock(prvHstMutex_);
    
    if (prvHstNetworkClient_) {
        prvHstNetworkClient_->disconnect();
        prvHstNetworkClient_.reset();
    }
    
    LOG("PrvHst TCP disconnected");
}

void ADTimePix::prvHstWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->prvHstWorkerThread();
}

void ADTimePix::prvHstWorkerThread() {
    const char* functionName = "prvHstWorkerThread";
    const char* driverName = "ADTimePix";
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!prvHstMutex_) {
        ERR("PrvHst worker thread: Mutex not initialized");
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
            prvHstConnect();
        }
        
        if (!prvHstRunning_) {
            break;
        }
        
        epicsMutexLock(prvHstMutex_);
        bool connected = prvHstConnected_;
        epicsMutexUnlock(prvHstMutex_);
        
        if (connected && prvHstNetworkClient_) {
            try {
                epicsMutexLock(prvHstMutex_);
                ssize_t bytes_read = prvHstNetworkClient_->receive(
                    prvHstLineBuffer_.data() + prvHstTotalRead_,
                    MAX_BUFFER_SIZE - prvHstTotalRead_ - 1
                );
                epicsMutexUnlock(prvHstMutex_);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(prvHstMutex_);
                        prvHstConnected_ = false;
                        prvHstRunning_ = false;
                        epicsMutexUnlock(prvHstMutex_);
                        LOG("PrvHst TCP connection closed by peer");
                        break;
                    } else {
                        epicsMutexLock(prvHstMutex_);
                        if (prvHstConnected_) {
                            prvHstConnected_ = false;
                            prvHstRunning_ = false;
                            LOG_ARGS("PrvHst TCP socket error: %s", strerror(errno));
                        }
                        epicsMutexUnlock(prvHstMutex_);
                        break;
                    }
                }
                
                epicsMutexLock(prvHstMutex_);
                prvHstTotalRead_ += bytes_read;
                prvHstLineBuffer_[prvHstTotalRead_] = '\0';
                
                // Look for newline to find complete JSON line
                char* newline_pos = static_cast<char*>(memchr(prvHstLineBuffer_.data(), '\n', prvHstTotalRead_));
                
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
                        LOG("PrvHst TCP buffer full without finding newline, resetting");
                        prvHstTotalRead_ = 0;
                    }
                }
                
                if (prvHstTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    LOG("PrvHst TCP buffer full, resetting");
                    prvHstTotalRead_ = 0;
                }
                
                epicsMutexUnlock(prvHstMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(prvHstMutex_);
                ERR_ARGS("Error in PrvHst worker thread: %s", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    prvHstDisconnect();
    LOG("PrvHst worker thread exiting");
}
