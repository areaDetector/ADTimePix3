#ifndef IMG_ACCUMULATION_H
#define IMG_ACCUMULATION_H

#include <vector>
#include <deque>
#include <memory>
#include <cstdint>
#include <cstddef>

/**
 * @brief Represents image data with pixel values
 * 
 * Supports both individual frame data (16-bit or 32-bit) and accumulated
 * running sum data (64-bit) for long-term accumulation without overflow.
 * 
 * Adapted from tpx3image IOC for use in ADTimePix3 driver.
 */
class ImageData {
public:
    enum class DataType {
        FRAME_DATA,      // Individual frame data (16-bit or 32-bit)
        RUNNING_SUM      // Accumulated data (64-bit)
    };
    
    enum class PixelFormat {
        UINT16,          // 16-bit unsigned integer pixels
        UINT32           // 32-bit unsigned integer pixels
    };
    
    ImageData(size_t width, size_t height, PixelFormat format = PixelFormat::UINT16, 
              DataType type = DataType::FRAME_DATA);
    ImageData(const ImageData& other);
    ImageData(ImageData&& other) noexcept;
    ImageData& operator=(const ImageData& other);
    ImageData& operator=(ImageData&& other) noexcept;
    ~ImageData() = default;
    
    // Getters
    size_t get_width() const { return width_; }
    size_t get_height() const { return height_; }
    size_t get_pixel_count() const { return width_ * height_; }
    PixelFormat get_pixel_format() const { return pixel_format_; }
    DataType get_data_type() const { return data_type_; }
    
    // Pixel access
    uint16_t get_pixel_16(size_t x, size_t y) const;
    uint32_t get_pixel_32(size_t x, size_t y) const;
    uint64_t get_pixel_64(size_t x, size_t y) const;
    void set_pixel_16(size_t x, size_t y, uint16_t value);
    void set_pixel_32(size_t x, size_t y, uint32_t value);
    void set_pixel_64(size_t x, size_t y, uint64_t value);
    
    // Direct pointer access (performance optimization)
    const uint16_t* get_pixels_16_ptr() const { return pixels_16_.data(); }
    const uint32_t* get_pixels_32_ptr() const { return pixels_32_.data(); }
    const uint64_t* get_pixels_64_ptr() const { return pixels_64_.data(); }
    
    // Add another image to this one (for running sum)
    void add_image(const ImageData& other);
    
private:
    size_t width_;
    size_t height_;
    PixelFormat pixel_format_;
    DataType data_type_;
    std::vector<uint16_t> pixels_16_;
    std::vector<uint32_t> pixels_32_;
    std::vector<uint64_t> pixels_64_;
    
    size_t get_index(size_t x, size_t y) const { return y * width_ + x; }
};

#endif // IMG_ACCUMULATION_H
