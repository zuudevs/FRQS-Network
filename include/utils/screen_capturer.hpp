#pragma once

/**
 * @file utils/screen_capturer.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifdef _WIN32
	#include <windows.h>
#endif

#include <vector>
#include <cstdint>
#include <optional>
#include <memory>

namespace frqs::utils {

struct ScreenFrame {
    int width;
    int height;
    int bytes_per_pixel;
    std::vector<uint8_t> data;  // Raw bitmap data (BGR format)
};

class ScreenCapturer {
public:
    ScreenCapturer();
    ~ScreenCapturer();
    
    ScreenCapturer(const ScreenCapturer&) = delete;
    ScreenCapturer& operator=(const ScreenCapturer&) = delete;
    
    // Capture full screen at specified scale
    // scale_factor: 1 = full res, 2 = half res, 4 = quarter res
    [[nodiscard]] std::optional<ScreenFrame> captureFrame(int scale_factor = 1);
    
    // Get screen dimensions
    [[nodiscard]] int getScreenWidth() const noexcept { return screen_width_; }
    [[nodiscard]] int getScreenHeight() const noexcept { return screen_height_; }
    
    // Convert frame to BMP format (with headers)
    [[nodiscard]] static std::vector<uint8_t> frameToBMP(const ScreenFrame& frame);

private:
#ifdef _WIN32
    HDC screen_dc_;
    HDC mem_dc_;
    HBITMAP bitmap_;
    BITMAPINFO bitmap_info_;
#endif
    
    int screen_width_;
    int screen_height_;
    
    void cleanup();
    bool initialize();
};

} // namespace frqs::utils