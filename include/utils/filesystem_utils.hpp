#pragma once

/**
 * @file utils/filesystem_utils.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <filesystem>
#include <optional>
#include <string>

namespace frqs::utils {

class FileSystemUtils {
public:
    // Securely resolve a path relative to a root directory
    // Returns nullopt if the path tries to escape the root (path traversal attack)
    [[nodiscard]] static std::optional<std::filesystem::path> 
    securePath(const std::filesystem::path& root, std::string_view requested_path) ;
    
    // Check if a path is safely contained within root
    [[nodiscard]] static bool isSafePath(
        const std::filesystem::path& root,
        const std::filesystem::path& target
    ) ;
    
    // Read file contents safely
    [[nodiscard]] static std::optional<std::string> 
    readFile(const std::filesystem::path& path, size_t max_size = 10 * 1024 * 1024) ;
    
    // Normalize path (remove .., ., etc.)
    [[nodiscard]] static std::filesystem::path normalizePath(std::string_view path) ;
} ;

} // namespace frqs::utils