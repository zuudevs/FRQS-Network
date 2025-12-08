#pragma once

/**
 * @file utils/logger.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifdef ERROR
	#undef ERROR
#endif

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <chrono>

namespace frqs::utils {

enum class Level : uint8_t {INFO, WARN, ERROR} ;

[[nodiscard]] inline std::string CreateLog(Level level, std::string_view msg) noexcept {
    std::string_view level_str ;
    switch (level) {
        using enum Level ;
        case INFO:  level_str = "INFO" ; break ;
        case WARN:  level_str = "WARN" ; break ;
        case ERROR: level_str = "ERROR" ; break ;
    }

    auto now = std::chrono::system_clock::now() ;
    auto local_time = std::chrono::zoned_time{std::chrono::current_zone(), now} ;

    return std::format("[{:%F %T}] [{:<5}] {}", local_time, level_str, msg) ;
}

// Helper functions for logging
void logInfo(std::string_view message) ;
void logWarn(std::string_view message) ;
void logError(std::string_view message) ;
void enableFileLogging(const std::string& filename) ;

} // namespace frqs::utils