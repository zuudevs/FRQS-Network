#pragma once

/**
 * @file utils/screen_capturer.hpp
 * @brief Optimized screen capturer with frame differencing and JPEG compression
 * @version 1.1.0
 * @date 2025-12-09
 * 
 * CHANGELOG v1.1.0:
 * - Added frame differencing (only send changed frames)
 * - JPEG compression support (10-20x smaller than BMP)
 * - Motion detection with configurable threshold
 * - Region-based updates (chunked streaming)
 * - Adaptive quality based on bandwidth
 * - Performance metrics tracking
 * 
 * @copyright Copyright (c) 2025
 */

#ifdef _WIN32
    #include <windows.h>
    #include <gdiplus.h>
    #pragma comment(lib, "gdiplus.lib")
#endif

#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include <chrono>

namespace frqs::utils {

/**
 * @brief Frame data structure
 */
struct ScreenFrame {
    int width;
    int height;
    int bytes_per_pixel;
    std::vector<uint8_t> data;  // Raw bitmap data (BGR format)
    
    // Metadata
    size_t compressed_size = 0;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Region of interest for partial updates
 */
struct DirtyRegion {
    int x, y;
    int width, height;
    float change_percentage;
};

/**
 * @brief Streaming statistics
 */
struct StreamStats {
    uint64_t frames_captured = 0;
    uint64_t frames_sent = 0;
    uint64_t frames_skipped = 0;
    uint64_t total_bytes_sent = 0;
    uint64_t total_bytes_saved = 0;
    double average_fps = 0.0;
    double compression_ratio = 0.0;
    std::chrono::steady_clock::time_point start_time;
};

/**
 * @brief Optimized screen capturer with frame diff and compression
 * 
 * Features:
 * - Frame differencing: Only sends frames when changes detected
 * - JPEG compression: 10-20x smaller than BMP
 * - Motion detection: Configurable sensitivity
 * - Region-based updates: Send only changed areas
 * - Adaptive quality: Balance speed vs quality
 */
class ScreenCapturer {
public:
    /**
     * @brief Quality presets for compression
     */
    enum class Quality {
        LOW = 50,      // Fastest, smallest size (~5KB typical)
        MEDIUM = 70,   // Balanced (~15KB typical)
        HIGH = 85,     // Best quality (~30KB typical)
        ULTRA = 95     // Near-lossless (~60KB typical)
    };
    
    ScreenCapturer();
    ~ScreenCapturer();
    
    ScreenCapturer(const ScreenCapturer&) = delete;
    ScreenCapturer& operator=(const ScreenCapturer&) = delete;
    
    /**
     * @brief Capture frame with automatic diff detection
     * @param scale_factor Downscaling factor (1=full, 2=half, 4=quarter)
     * @param force_capture Force capture even if no changes detected
     * @return Frame data if changes detected or forced, nullopt otherwise
     */
    [[nodiscard]] std::optional<ScreenFrame> captureFrame(
        int scale_factor = 2,
        bool force_capture = false
    );
    
    /**
     * @brief Check if frame has significant changes from previous
     * @param threshold Change percentage threshold (0.0 - 1.0)
     * @return true if changes exceed threshold
     */
    [[nodiscard]] bool hasSignificantChanges(float threshold = 0.01f);
    
    /**
     * @brief Get regions that changed since last frame
     * @param grid_size Size of detection grid (e.g., 8x8 blocks)
     * @return Vector of dirty regions
     */
    [[nodiscard]] std::vector<DirtyRegion> getDirtyRegions(int grid_size = 64);
    
    /**
     * @brief Convert frame to JPEG with quality control
     * @param frame Raw frame data
     * @param quality JPEG quality (1-100)
     * @return Compressed JPEG data
     */
    [[nodiscard]] static std::vector<uint8_t> frameToJPEG(
        const ScreenFrame& frame, 
        int quality = 75
    );
    
    /**
     * @brief Convert frame to BMP (legacy support)
     */
    [[nodiscard]] static std::vector<uint8_t> frameToBMP(const ScreenFrame& frame);
    
    /**
     * @brief Set motion detection sensitivity
     * @param threshold Pixel difference threshold (0-255)
     */
    void setMotionThreshold(uint8_t threshold) { motion_threshold_ = threshold; }
    
    /**
     * @brief Set JPEG quality preset
     */
    void setQuality(Quality quality) { jpeg_quality_ = static_cast<int>(quality); }
    
    /**
     * @brief Reset frame history (force full update on next capture)
     */
    void resetFrameHistory();
    
    /**
     * @brief Get streaming statistics
     */
    [[nodiscard]] StreamStats getStats() const { return stats_; }
    
    /**
     * @brief Get screen dimensions
     */
    [[nodiscard]] int getScreenWidth() const noexcept { return screen_width_; }
    [[nodiscard]] int getScreenHeight() const noexcept { return screen_height_; }

private:
#ifdef _WIN32
    HDC screen_dc_;
    HDC mem_dc_;
    HBITMAP bitmap_;
    BITMAPINFO bitmap_info_;
    ULONG_PTR gdiplus_token_;
#endif
    
    int screen_width_;
    int screen_height_;
    
    // Frame differencing
    std::vector<uint8_t> previous_frame_;
    int previous_width_ = 0;
    int previous_height_ = 0;
    
    // Configuration
    uint8_t motion_threshold_ = 10;  // Pixel diff threshold
    int jpeg_quality_ = 75;           // JPEG quality (1-100)
    
    // Statistics
    mutable StreamStats stats_;
    
    void cleanup();
    bool initialize();
    
    /**
     * @brief Calculate difference between two frames
     * @return Percentage of pixels that changed (0.0 - 1.0)
     */
    float calculateFrameDiff(
        const uint8_t* current, 
        const uint8_t* previous,
        size_t size
    );
    
    /**
     * @brief Initialize GDI+ for JPEG encoding
     */
    static bool initGdiPlus();
    static void shutdownGdiPlus();
};

} // namespace frqs::utils
