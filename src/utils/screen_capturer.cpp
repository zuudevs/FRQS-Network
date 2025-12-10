/**
 * @file utils/screen_capturer.cpp
 * @brief Optimized implementation with frame diff and JPEG compression
 * @version 1.1.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 */

#include "utils/screen_capturer.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace frqs::utils {

#ifdef _WIN32

// GDI+ initialization helper
static ULONG_PTR g_gdiplus_token = 0;
static int g_gdiplus_refcount = 0;

bool ScreenCapturer::initGdiPlus() {
    if (g_gdiplus_refcount++ == 0) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        return Gdiplus::GdiplusStartup(&g_gdiplus_token, &gdiplusStartupInput, nullptr) == Gdiplus::Ok;
    }
    return true;
}

void ScreenCapturer::shutdownGdiPlus() {
    if (--g_gdiplus_refcount == 0 && g_gdiplus_token) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }
}

ScreenCapturer::ScreenCapturer() 
    : screen_dc_(nullptr)
    , mem_dc_(nullptr)
    , bitmap_(nullptr)
    , gdiplus_token_(0)
    , screen_width_(0)
    , screen_height_(0) {
    
    std::memset(&bitmap_info_, 0, sizeof(bitmap_info_));
    
    if (!initGdiPlus()) {
        throw std::runtime_error("Failed to initialize GDI+");
    }
    
    if (!initialize()) {
        throw std::runtime_error("Failed to initialize screen capturer");
    }
    
    stats_.start_time = std::chrono::steady_clock::now();
}

ScreenCapturer::~ScreenCapturer() {
    cleanup();
    shutdownGdiPlus();
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

std::optional<ScreenFrame> ScreenCapturer::captureFrame(int scale_factor, bool force_capture) {
    if (!screen_dc_ || !mem_dc_ || scale_factor < 1) {
        return std::nullopt;
    }
    
    stats_.frames_captured++;
    
    int capture_width = screen_width_ / scale_factor;
    int capture_height = screen_height_ / scale_factor;
    
    // Create bitmap for this capture
    HBITMAP temp_bitmap = CreateCompatibleBitmap(screen_dc_, capture_width, capture_height);
    if (!temp_bitmap) {
        return std::nullopt;
    }
    
    HGDIOBJ old_bitmap = SelectObject(mem_dc_, temp_bitmap);
    
    // Capture screen
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
    bmi.bmiHeader.biHeight = -capture_height;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Calculate data size
    int row_size = ((capture_width * 3 + 3) / 4) * 4;
    size_t data_size = static_cast<size_t>(row_size) * capture_height;
    
    ScreenFrame frame;
    frame.width = capture_width;
    frame.height = capture_height;
    frame.bytes_per_pixel = 3;
    frame.data.resize(data_size);
    frame.timestamp = std::chrono::steady_clock::now();
    
    // Get bitmap bits
    int lines = GetDIBits(mem_dc_, temp_bitmap, 0, capture_height, 
                          frame.data.data(), &bmi, DIB_RGB_COLORS);
    
    SelectObject(mem_dc_, old_bitmap);
    DeleteObject(temp_bitmap);
    
    if (lines == 0) {
        return std::nullopt;
    }
    
    // ========== FRAME DIFFERENCING ==========
    if (!force_capture && !previous_frame_.empty()) {
        // Check if dimensions changed
        if (previous_width_ == capture_width && previous_height_ == capture_height) {
            float diff = calculateFrameDiff(
                frame.data.data(), 
                previous_frame_.data(), 
                std::min(frame.data.size(), previous_frame_.size())
            );
            
            // Skip frame if change is too small
            if (diff < 0.01f) {  // Less than 1% changed
                stats_.frames_skipped++;
                stats_.total_bytes_saved += data_size;
                return std::nullopt;
            }
        }
    }
    
    // Update previous frame
    previous_frame_ = frame.data;
    previous_width_ = capture_width;
    previous_height_ = capture_height;
    
    stats_.frames_sent++;
    
    return frame;
}

float ScreenCapturer::calculateFrameDiff(
    const uint8_t* current, 
    const uint8_t* previous,
    size_t size
) {
    if (!current || !previous || size == 0) {
        return 1.0f;
    }
    
    size_t changed_pixels = 0;
    size_t total_pixels = size / 3;  // RGB data
    
    // Sample every 3rd pixel for speed (still accurate)
    for (size_t i = 0; i < size; i += 9) {  // Step by 3 pixels (9 bytes)
        int diff_b = std::abs(static_cast<int>(current[i]) - static_cast<int>(previous[i]));
        int diff_g = std::abs(static_cast<int>(current[i+1]) - static_cast<int>(previous[i+1]));
        int diff_r = std::abs(static_cast<int>(current[i+2]) - static_cast<int>(previous[i+2]));
        
        int total_diff = diff_b + diff_g + diff_r;
        
        if (total_diff > motion_threshold_ * 3) {
            changed_pixels++;
        }
    }
    
    return static_cast<float>(changed_pixels) / (total_pixels / 3);
}

bool ScreenCapturer::hasSignificantChanges(float threshold) {
    if (previous_frame_.empty()) {
        return true;
    }
    
    // Capture current frame temporarily
    auto frame = captureFrame(2, true);
    if (!frame) {
        return false;
    }
    
    float diff = calculateFrameDiff(
        frame->data.data(),
        previous_frame_.data(),
        std::min(frame->data.size(), previous_frame_.size())
    );
    
    return diff >= threshold;
}

std::vector<DirtyRegion> ScreenCapturer::getDirtyRegions(int grid_size) {
    std::vector<DirtyRegion> regions;
    
    if (previous_frame_.empty() || previous_width_ == 0 || previous_height_ == 0) {
        // First frame - everything is dirty
        regions.push_back({0, 0, previous_width_, previous_height_, 1.0f});
        return regions;
    }
    
    // Capture current for comparison
    auto frame = captureFrame(2, true);
    if (!frame) {
        return regions;
    }
    
    int blocks_x = frame->width / grid_size;
    int blocks_y = frame->height / grid_size;
    int row_size = ((frame->width * 3 + 3) / 4) * 4;
    
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            int changed = 0;
            int total = grid_size * grid_size;
            
            // Check pixels in this block
            for (int y = 0; y < grid_size && (by * grid_size + y) < frame->height; y++) {
                for (int x = 0; x < grid_size && (bx * grid_size + x) < frame->width; x++) {
                    int px = bx * grid_size + x;
                    int py = by * grid_size + y;
                    size_t idx = py * row_size + px * 3;
                    
                    if (idx + 2 < frame->data.size() && idx + 2 < previous_frame_.size()) {
                        int diff_b = std::abs(frame->data[idx] - previous_frame_[idx]);
                        int diff_g = std::abs(frame->data[idx+1] - previous_frame_[idx+1]);
                        int diff_r = std::abs(frame->data[idx+2] - previous_frame_[idx+2]);
                        
                        if ((diff_b + diff_g + diff_r) > motion_threshold_ * 3) {
                            changed++;
                        }
                    }
                }
            }
            
            float change_pct = static_cast<float>(changed) / total;
            if (change_pct > 0.05f) {  // 5% threshold
                regions.push_back({
                    bx * grid_size,
                    by * grid_size,
                    grid_size,
                    grid_size,
                    change_pct
                });
            }
        }
    }
    
    return regions;
}

std::vector<uint8_t> ScreenCapturer::frameToJPEG(const ScreenFrame& frame, int quality) {
    std::vector<uint8_t> result;
    
    // Clamp quality
    quality = std::clamp(quality, 1, 100);
    
    // Create GDI+ bitmap from raw data
    Gdiplus::Bitmap bitmap(
        frame.width, 
        frame.height, 
        ((frame.width * 3 + 3) / 4) * 4,  // Row stride
        PixelFormat24bppRGB,
        const_cast<BYTE*>(frame.data.data())
    );
    
    // Get JPEG encoder CLSID
    CLSID jpegClsid;
    {
        UINT num = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size == 0) return result;
        
        std::vector<BYTE> buffer(size);
        Gdiplus::ImageCodecInfo* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
        Gdiplus::GetImageEncoders(num, size, codecs);
        
        bool found = false;
        for (UINT i = 0; i < num; i++) {
            if (wcscmp(codecs[i].MimeType, L"image/jpeg") == 0) {
                jpegClsid = codecs[i].Clsid;
                found = true;
                break;
            }
        }
        if (!found) return result;
    }
    
    // Set JPEG quality
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    ULONG qualityValue = quality;
    encoderParams.Parameter[0].Value = &qualityValue;
    
    // Save to memory stream
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) == S_OK) {
        if (bitmap.Save(stream, &jpegClsid, &encoderParams) == Gdiplus::Ok) {
            STATSTG stats;
            if (stream->Stat(&stats, STATFLAG_NONAME) == S_OK) {
                LARGE_INTEGER zero = {0};
                stream->Seek(zero, STREAM_SEEK_SET, nullptr);
                
                result.resize(stats.cbSize.LowPart);
                ULONG read = 0;
                stream->Read(result.data(), stats.cbSize.LowPart, &read);
            }
        }
        stream->Release();
    }
    
    return result;
}

std::vector<uint8_t> ScreenCapturer::frameToBMP(const ScreenFrame& frame) {
    int row_size = ((frame.width * 3 + 3) / 4) * 4;
    size_t pixel_data_size = static_cast<size_t>(row_size) * frame.height;
    size_t file_size = 14 + 40 + pixel_data_size;
    
    std::vector<uint8_t> bmp_data(file_size);
    uint8_t* ptr = bmp_data.data();
    
    // File Header
    ptr[0] = 'B'; ptr[1] = 'M';
    *reinterpret_cast<uint32_t*>(ptr + 2) = static_cast<uint32_t>(file_size);
    *reinterpret_cast<uint32_t*>(ptr + 6) = 0;
    *reinterpret_cast<uint32_t*>(ptr + 10) = 54;
    
    // Info Header
    ptr += 14;
    *reinterpret_cast<uint32_t*>(ptr) = 40;
    *reinterpret_cast<int32_t*>(ptr + 4) = frame.width;
    *reinterpret_cast<int32_t*>(ptr + 8) = -frame.height;
    *reinterpret_cast<uint16_t*>(ptr + 12) = 1;
    *reinterpret_cast<uint16_t*>(ptr + 14) = 24;
    *reinterpret_cast<uint32_t*>(ptr + 16) = 0;
    *reinterpret_cast<uint32_t*>(ptr + 20) = static_cast<uint32_t>(pixel_data_size);
    *reinterpret_cast<int32_t*>(ptr + 24) = 2835;
    *reinterpret_cast<int32_t*>(ptr + 28) = 2835;
    *reinterpret_cast<uint32_t*>(ptr + 32) = 0;
    *reinterpret_cast<uint32_t*>(ptr + 36) = 0;
    
    std::memcpy(ptr + 40, frame.data.data(), frame.data.size());
    
    return bmp_data;
}

void ScreenCapturer::resetFrameHistory() {
    previous_frame_.clear();
    previous_width_ = 0;
    previous_height_ = 0;
}

#else
// Stub for non-Windows
ScreenCapturer::ScreenCapturer() : screen_width_(0), screen_height_(0) {}
ScreenCapturer::~ScreenCapturer() {}
bool ScreenCapturer::initialize() { return false; }
void ScreenCapturer::cleanup() {}
std::optional<ScreenFrame> ScreenCapturer::captureFrame(int, bool) { return std::nullopt; }
float ScreenCapturer::calculateFrameDiff(const uint8_t*, const uint8_t*, size_t) { return 0.0f; }
bool ScreenCapturer::hasSignificantChanges(float) { return false; }
std::vector<DirtyRegion> ScreenCapturer::getDirtyRegions(int) { return {}; }
std::vector<uint8_t> ScreenCapturer::frameToJPEG(const ScreenFrame&, int) { return {}; }
std::vector<uint8_t> ScreenCapturer::frameToBMP(const ScreenFrame&) { return {}; }
void ScreenCapturer::resetFrameHistory() {}
bool ScreenCapturer::initGdiPlus() { return false; }
void ScreenCapturer::shutdownGdiPlus() {}
#endif

} // namespace frqs::utils