#pragma once

/**
 * @file http/mime_types.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string_view>
#include <filesystem>

namespace frqs::http {

class MimeTypes {
public:
    [[nodiscard]] static std::string_view fromExtension(std::string_view ext) noexcept;
    [[nodiscard]] static std::string_view fromPath(const std::filesystem::path& path) noexcept;
    [[nodiscard]] static constexpr std::string_view defaultType() noexcept {
        return "application/octet-stream";
    }
};

} // namespace frqs::http