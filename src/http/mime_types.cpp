/**
 * @file http/mime_types.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "http/mime_types.hpp"
#include <unordered_map>

namespace frqs::http {

std::string_view MimeTypes::fromExtension(std::string_view ext) noexcept {
    static const std::unordered_map<std::string_view, std::string_view> types = {
        // Text
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".txt", "text/plain"},
        {".csv", "text/csv"},
        
        // Images
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".webp", "image/webp"},
        
        // Fonts
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        
        // Archives
        {".zip", "application/zip"},
        {".tar", "application/x-tar"},
        {".gz", "application/gzip"},
        
        // Documents
        {".pdf", "application/pdf"},
        
        // Video
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        
        // Audio
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".ogg", "audio/ogg"}
    };
    
    auto it = types.find(ext);
    return (it != types.end()) ? it->second : defaultType();
}

std::string_view MimeTypes::fromPath(const std::filesystem::path& path) noexcept {
    if (!path.has_extension()) {
        return defaultType();
    }
    return fromExtension(path.extension().string());
}

} // namespace frqs::http