/**
 * @file utils/filesystem_utils.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/filesystem_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace frqs::utils {

std::optional<std::filesystem::path> FileSystemUtils::securePath(
    const std::filesystem::path& root,
    std::string_view requested_path
) {
    try {
        // Normalize the root path
        auto canonical_root = std::filesystem::canonical(root) ;
        
        // Normalize and clean the requested path
        auto normalized = normalizePath(requested_path) ;
        
        // Construct the full path
        auto full_path = canonical_root / normalized ;
        
        // If the file exists, canonicalize it
        if (std::filesystem::exists(full_path)) {
            full_path = std::filesystem::canonical(full_path) ;
        } else {
            // If it doesn't exist, we need to check the parent
            auto parent = full_path.parent_path() ;
            if (std::filesystem::exists(parent)) {
                parent = std::filesystem::canonical(parent) ;
                full_path = parent / full_path.filename() ;
            }
        }
        
        // Security check: ensure the resolved path is within root
        if (!isSafePath(canonical_root, full_path)) {
            return std::nullopt ;
        }
        
        return full_path ;
        
    } catch (const std::filesystem::filesystem_error&) {
        return std::nullopt ;
    }
}

bool FileSystemUtils::isSafePath(
    const std::filesystem::path& root,
    const std::filesystem::path& target
) {
    try {
        // Convert both to absolute paths for comparison
        auto abs_root = std::filesystem::absolute(root) ;
        auto abs_target = std::filesystem::absolute(target) ;
        
        // Check if target starts with root
        auto root_str = abs_root.string() ;
        auto target_str = abs_target.string() ;
        
        // Ensure root ends with separator for proper prefix matching
        if (!root_str.empty() && root_str.back() != std::filesystem::path::preferred_separator) {
            root_str += std::filesystem::path::preferred_separator ;
        }
        
        // Check if target starts with root
        return target_str.starts_with(root_str) || abs_target == abs_root ;
        
    } catch (const std::filesystem::filesystem_error&) {
        return false ;
    }
}

std::optional<std::string> FileSystemUtils::readFile(
    const std::filesystem::path& path,
    size_t max_size
) {
    try {
        // Check file size first
        auto file_size = std::filesystem::file_size(path) ;
        if (file_size > max_size) {
            return std::nullopt ;
        }
        
        std::ifstream file(path, std::ios::binary) ;
        if (!file) {
            return std::nullopt ;
        }
        
        std::ostringstream oss ;
        oss << file.rdbuf() ;
        return oss.str() ;
        
    } catch (const std::filesystem::filesystem_error&) {
        return std::nullopt ;
    } catch (const std::exception&) {
        return std::nullopt ;
    }
}

std::filesystem::path FileSystemUtils::normalizePath(std::string_view path) {
    std::filesystem::path result ;
    
    // Remove leading slashes
    while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
        path.remove_prefix(1) ;
    }
    
    // Split by path separators and process
    size_t start = 0 ;
    for (size_t i = 0 ; i <= path.size() ; ++i) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            if (i > start) {
                std::string_view component = path.substr(start, i - start) ;
                
                // Skip "." components
                if (component == ".") {
                    // Do nothing
                }
                // Handle ".." by going up if possible
                else if (component == "..") {
                    if (result.has_relative_path()) {
                        result = result.parent_path() ;
                    }
                    // If we can't go up, ignore it (security)
                }
                // Normal component
                else {
                    result /= component ;
                }
            }
            start = i + 1 ;
        }
    }
    
    return result ;
}

} // namespace frqs::utils