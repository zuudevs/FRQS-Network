#pragma once

/**
 * @file utils/config.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <mutex>

namespace frqs::utils {

class Config {
public:
    static Config& instance() {
        static Config cfg ;
        return cfg ;
    }
    
    Config(const Config&) = delete ;
    Config& operator=(const Config&) = delete ;
    Config(Config&&) = delete ;
    Config& operator=(Config&&) = delete ;
    
    // Load configuration from file
    bool load(const std::filesystem::path& config_file) ;
    
    // Get configuration values
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const ;
    [[nodiscard]] std::optional<int> getInt(std::string_view key) const ;
    [[nodiscard]] std::optional<bool> getBool(std::string_view key) const ;
    
    // Convenience getters with defaults
    [[nodiscard]] uint16_t getPort() const { return getInt("PORT").value_or(8080) ; }
    [[nodiscard]] std::string getDocRoot() const { return get("DOC_ROOT").value_or("public") ; }
    [[nodiscard]] std::string getAuthToken() const { return get("AUTH_TOKEN").value_or("") ; }
    [[nodiscard]] int getFpsLimit() const { return getInt("FPS_LIMIT").value_or(15) ; }
    [[nodiscard]] int getScaleFactor() const { return getInt("SCALE_FACTOR").value_or(2) ; }
    [[nodiscard]] std::string getUploadDir() const { return get("UPLOAD_DIR").value_or("uploads") ; }
    [[nodiscard]] size_t getMaxUploadSize() const { 
        return static_cast<size_t>(getInt("MAX_UPLOAD_SIZE").value_or(50 * 1024 * 1024)) ; 
    }
    [[nodiscard]] int getThreadCount() const { 
        return getInt("THREAD_COUNT").value_or(static_cast<int>(std::thread::hardware_concurrency())) ; 
    }
    [[nodiscard]] std::string getMasterServerUrl() const { return get("MASTER_SERVER_URL").value_or("") ; }
    [[nodiscard]] int getHeartbeatInterval() const { return getInt("HEARTBEAT_INTERVAL").value_or(30) ; }
    
    // Set values programmatically
    void set(std::string_view key, std::string_view value) ;

private:
    Config() = default ;
    mutable std::mutex mutex_ ;
    std::unordered_map<std::string, std::string> values_ ;
    
    void parseLine(std::string_view line) ;
    static std::string trim(std::string_view str) ;
} ;

} // namespace frqs::utils