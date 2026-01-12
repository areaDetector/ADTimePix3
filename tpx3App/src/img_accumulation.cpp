#include "img_accumulation.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>

// ImageData class implementation
ImageData::ImageData(size_t width, size_t height, PixelFormat format, DataType type)
    : width_(width), height_(height), pixel_format_(format), data_type_(type) {
    
    size_t pixel_count = width * height;
    
    if (type == DataType::FRAME_DATA) {
        if (format == PixelFormat::UINT16) {
            pixels_16_.resize(pixel_count, 0);
        } else {
            pixels_32_.resize(pixel_count, 0);
        }
    } else {
        pixels_64_.resize(pixel_count, 0);
    }
}

// Copy constructor
ImageData::ImageData(const ImageData& other)
    : width_(other.width_), height_(other.height_), 
      pixel_format_(other.pixel_format_), data_type_(other.data_type_) {
    
    pixels_16_ = other.pixels_16_;
    pixels_32_ = other.pixels_32_;
    pixels_64_ = other.pixels_64_;
}

// Move constructor
ImageData::ImageData(ImageData&& other) noexcept
    : width_(other.width_), height_(other.height_),
      pixel_format_(other.pixel_format_), data_type_(other.data_type_) {
    
    pixels_16_ = std::move(other.pixels_16_);
    pixels_32_ = std::move(other.pixels_32_);
    pixels_64_ = std::move(other.pixels_64_);
    
    other.width_ = 0;
    other.height_ = 0;
    other.pixel_format_ = PixelFormat::UINT16;
    other.data_type_ = DataType::FRAME_DATA;
}

// Assignment operators
ImageData& ImageData::operator=(const ImageData& other) {
    if (this != &other) {
        width_ = other.width_;
        height_ = other.height_;
        pixel_format_ = other.pixel_format_;
        data_type_ = other.data_type_;
        pixels_16_ = other.pixels_16_;
        pixels_32_ = other.pixels_32_;
        pixels_64_ = other.pixels_64_;
    }
    return *this;
}

ImageData& ImageData::operator=(ImageData&& other) noexcept {
    if (this != &other) {
        width_ = other.width_;
        height_ = other.height_;
        pixel_format_ = other.pixel_format_;
        data_type_ = other.data_type_;
        pixels_16_ = std::move(other.pixels_16_);
        pixels_32_ = std::move(other.pixels_32_);
        pixels_64_ = std::move(other.pixels_64_);
        
        other.width_ = 0;
        other.height_ = 0;
        other.pixel_format_ = PixelFormat::UINT16;
        other.data_type_ = DataType::FRAME_DATA;
    }
    return *this;
}

// Access pixel values
uint16_t ImageData::get_pixel_16(size_t x, size_t y) const {
    if (data_type_ != DataType::FRAME_DATA || pixel_format_ != PixelFormat::UINT16 || 
        x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 16-bit access");
    }
    return pixels_16_[get_index(x, y)];
}

uint32_t ImageData::get_pixel_32(size_t x, size_t y) const {
    if (data_type_ != DataType::FRAME_DATA || pixel_format_ != PixelFormat::UINT32 || 
        x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 32-bit access");
    }
    return pixels_32_[get_index(x, y)];
}

uint64_t ImageData::get_pixel_64(size_t x, size_t y) const {
    if (data_type_ != DataType::RUNNING_SUM || x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 64-bit access");
    }
    return pixels_64_[get_index(x, y)];
}

// Setters
void ImageData::set_pixel_16(size_t x, size_t y, uint16_t value) {
    if (data_type_ != DataType::FRAME_DATA || pixel_format_ != PixelFormat::UINT16 || 
        x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 16-bit access");
    }
    pixels_16_[get_index(x, y)] = value;
}

void ImageData::set_pixel_32(size_t x, size_t y, uint32_t value) {
    if (data_type_ != DataType::FRAME_DATA || pixel_format_ != PixelFormat::UINT32 || 
        x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 32-bit access");
    }
    pixels_32_[get_index(x, y)] = value;
}

void ImageData::set_pixel_64(size_t x, size_t y, uint64_t value) {
    if (data_type_ != DataType::RUNNING_SUM || x >= width_ || y >= height_) {
        throw std::out_of_range("Invalid coordinates or data type for 64-bit access");
    }
    pixels_64_[get_index(x, y)] = value;
}

// Add another image to this one (for running sum)
void ImageData::add_image(const ImageData& other) {
    if (other.data_type_ != DataType::FRAME_DATA || data_type_ != DataType::RUNNING_SUM) {
        throw std::invalid_argument("Can only add frame data to running sum");
    }
    
    if (other.width_ != width_ || other.height_ != height_) {
        throw std::invalid_argument("Image dimensions must match for addition");
    }

    size_t pixel_count = width_ * height_;
    for (size_t i = 0; i < pixel_count; ++i) {
        uint64_t other_value = 0;
        if (other.pixel_format_ == PixelFormat::UINT16) {
            other_value = other.pixels_16_[i];
        } else {
            other_value = other.pixels_32_[i];
        }
        
        uint64_t new_value = pixels_64_[i] + other_value;
        if (new_value < pixels_64_[i]) {
            // Overflow detected - cap at maximum value
            pixels_64_[i] = UINT64_MAX;
        } else {
            pixels_64_[i] = new_value;
        }
    }
}
