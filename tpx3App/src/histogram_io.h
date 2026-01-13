#ifndef HISTOGRAM_IO_H
#define HISTOGRAM_IO_H

#include <vector>
#include <deque>
#include <memory>
#include <cstdint>
#include <cstddef>

// TDC clock period constant (from histogram IOC)
constexpr double TPX3_TDC_CLOCK_PERIOD_SEC = (1.5625 / 6.0) * 1e-9;

/**
 * @brief Represents histogram data with bin edges and values
 * 
 * Supports both individual frame data (32-bit) and accumulated
 * running sum data (64-bit) for long-term accumulation without overflow.
 * 
 * Adapted from histogram IOC for use in ADTimePix3 driver.
 */
class HistogramData {
public:
    enum class DataType {
        FRAME_DATA,      // Individual frame data (32-bit)
        RUNNING_SUM      // Accumulated data (64-bit)
    };

    HistogramData(size_t bin_size, DataType type = DataType::FRAME_DATA);
    HistogramData(const HistogramData& other);
    HistogramData(HistogramData&& other) noexcept;
    HistogramData& operator=(const HistogramData& other);
    HistogramData& operator=(HistogramData&& other) noexcept;
    ~HistogramData() = default;

    // Getters
    size_t get_bin_size() const { return bin_size_; }
    DataType get_data_type() const { return data_type_; }
    const std::vector<double>& get_bin_edges() const { return bin_edges_; }
    
    // Access bin values based on type
    uint32_t get_bin_value_32(size_t index) const;
    uint64_t get_bin_value_64(size_t index) const;

    // Setters
    void set_bin_edge(size_t index, double value);
    void set_bin_value_32(size_t index, uint32_t value);
    void set_bin_value_64(size_t index, uint64_t value);

    // Calculate bin edges from parameters
    void calculate_bin_edges(int bin_width, int bin_offset);

    // Add another histogram to this one (for running sum)
    void add_histogram(const HistogramData& other);

private:
    size_t bin_size_;
    DataType data_type_;
    std::vector<double> bin_edges_;
    std::vector<uint32_t> bin_values_32_;
    std::vector<uint64_t> bin_values_64_;
};

#endif // HISTOGRAM_IO_H
