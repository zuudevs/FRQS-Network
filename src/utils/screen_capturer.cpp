/**
 * @file utils/screen_capturer.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/screen_capturer.hpp"
#include <cstring>

namespace frqs::utils {

#ifdef _WIN32

ScreenCapturer::ScreenCapturer() 
    : screen_dc_(nullptr)
    , mem_dc_(nullptr)
    , bitmap_(nullptr)
    , screen_width_(0)
    , screen_height_(0) {
    
    std::memset(&bitmap_info_, 0, sizeof(bitmap_info_));
    initialize();
}

ScreenCapturer::~ScreenCapturer() {
    cleanup();
}

bool ScreenCapturer::initialize() {
    screen_width_ = GetSystemMetrics(SM_CXSCREEN);
    screen_height_ = GetSystemMetrics(SM_CYSCREEN);
    
    screen_dc_ = GetDC(nullptr);
    if (!screen_dc_) {
        return false;
    }
    
    mem_dc_ = CreateCompatibleDC(screen_dc_);
    if (!mem_dc_) {
        cleanup();
        return false;
    }
    
    return true;
}

void ScreenCapturer::cleanup() {
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (mem_dc_) {
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
    }
    if (screen_dc_) {
        ReleaseDC(nullptr, screen_dc_);
        screen_dc_ = nullptr;
    }
}

std::optional<ScreenFrame> ScreenCapturer::captureFrame(int scale_factor) {
    if (!screen_dc_ || !mem_dc_ || scale_factor < 1) {
        return std::nullopt;
    }
    
    int capture_width = screen_width_ / scale_factor;
    int capture_height = screen_height_ / scale_factor;
    
    // Create bitmap for this capture
    HBITMAP temp_bitmap = CreateCompatibleBitmap(screen_dc_, capture_width, capture_height);
    if (!temp_bitmap) {
        return std::nullopt;
    }
    
    HGDIOBJ old_bitmap = SelectObject(mem_dc_, temp_bitmap);
    
    // Stretch blit for scaling
    if (scale_factor == 1) {
        BitBlt(mem_dc_, 0, 0, capture_width, capture_height, 
               screen_dc_, 0, 0, SRCCOPY);
    } else {
        SetStretchBltMode(mem_dc_, HALFTONE);
        StretchBlt(mem_dc_, 0, 0, capture_width, capture_height,
                   screen_dc_, 0, 0, screen_width_, screen_height_, SRCCOPY);
    }
    
    // Setup bitmap info
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = capture_width;
    bmi.bmiHeader.biHeight = -capture_height;  // Negative for top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;  // 3 bytes per pixel (BGR)
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Calculate data size
    int row_size = ((capture_width * 3 + 3) / 4) * 4;  // Align to 4 bytes
    size_t data_size = static_cast<size_t>(row_size) * capture_height;
    
    ScreenFrame frame;
    frame.width = capture_width;
    frame.height = capture_height;
    frame.bytes_per_pixel = 3;
    frame.data.resize(data_size);
    
    // Get bitmap bits
    int lines = GetDIBits(mem_dc_, temp_bitmap, 0, capture_height, 
                          frame.data.data(), &bmi, DIB_RGB_COLORS);
    
    SelectObject(mem_dc_, old_bitmap);
    DeleteObject(temp_bitmap);
    
    if (lines == 0) {
        return std::nullopt;
    }
    
    return frame;
}

std::vector<uint8_t> ScreenCapturer::frameToBMP(const ScreenFrame& frame) {
    int row_size = ((frame.width * 3 + 3) / 4) * 4;
    size_t pixel_data_size = static_cast<size_t>(row_size) * frame.height;
    size_t file_size = 14 + 40 + pixel_data_size;  // File header + Info header + pixels
    
    std::vector<uint8_t> bmp_data(file_size);
    uint8_t* ptr = bmp_data.data();
    
    // BMP File Header (14 bytes)
    ptr[0] = 'B'; ptr[1] = 'M';
    *reinterpret_cast<uint32_t*>(ptr + 2) = static_cast<uint32_t>(file_size);
    *reinterpret_cast<uint32_t*>(ptr + 6) = 0;  // Reserved
    *reinterpret_cast<uint32_t*>(ptr + 10) = 54;  // Pixel data offset
    
    // BMP Info Header (40 bytes)
    ptr += 14;
    *reinterpret_cast<uint32_t*>(ptr) = 40;  // Header size
    *reinterpret_cast<int32_t*>(ptr + 4) = frame.width;
    *reinterpret_cast<int32_t*>(ptr + 8) = -frame.height;  // Negative for top-down
    *reinterpret_cast<uint16_t*>(ptr + 12) = 1;  // Planes
    *reinterpret_cast<uint16_t*>(ptr + 14) = 24;  // Bits per pixel
    *reinterpret_cast<uint32_t*>(ptr + 16) = 0;  // Compression (none)
    *reinterpret_cast<uint32_t*>(ptr + 20) = static_cast<uint32_t>(pixel_data_size);
    *reinterpret_cast<int32_t*>(ptr + 24) = 2835;  // X pixels per meter
    *reinterpret_cast<int32_t*>(ptr + 28) = 2835;  // Y pixels per meter
    *reinterpret_cast<uint32_t*>(ptr + 32) = 0;  // Colors used
    *reinterpret_cast<uint32_t*>(ptr + 36) = 0;  // Important colors
    
    // Copy pixel data
    std::memcpy(ptr + 40, frame.data.data(), frame.data.size());
    
    return bmp_data;
}

#else
// Stub for non-Windows platforms
ScreenCapturer::ScreenCapturer() : screen_width_(0), screen_height_(0) {}
ScreenCapturer::~ScreenCapturer() {}
bool ScreenCapturer::initialize() { return false; }
void ScreenCapturer::cleanup() {}
std::optional<ScreenFrame> ScreenCapturer::captureFrame(int) { return std::nullopt; }
std::vector<uint8_t> ScreenCapturer::frameToBMP(const ScreenFrame&) { return {}; }
#endif

} // namespace frqs::utils