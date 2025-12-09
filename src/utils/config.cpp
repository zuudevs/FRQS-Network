/**
 * @file utils/config.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/config.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace frqs::utils {

bool Config::load(const std::filesystem::path& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(config_file);
    if (!file) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        parseLine(line);
    }
    
    return true;
}

void Config::parseLine(std::string_view line) {
    // Skip comments and empty lines
    line = trim(line);
    if (line.empty() || line.starts_with('#')) {
        return;
    }
    
    // Find equals sign
    auto eq_pos = line.find('=');
    if (eq_pos == std::string_view::npos) {
        return;
    }
    
    auto key = trim(line.substr(0, eq_pos));
    auto value = trim(line.substr(eq_pos + 1));
    
    if (!key.empty()) {
        values_[std::string(key)] = std::string(value);
    }
}

std::string Config::trim(std::string_view str) {
    auto start = std::find_if_not(str.begin(), str.end(), 
        [](unsigned char ch) { return std::isspace(ch); });
    
    auto end = std::find_if_not(str.rbegin(), str.rend(), 
        [](unsigned char ch) { return std::isspace(ch); }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

std::optional<std::string> Config::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = values_.find(std::string(key));
    if (it != values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<int> Config::getInt(std::string_view key) const {
    auto value = get(key);
    if (!value) {
        return std::nullopt;
    }
    
    try {
        return std::stoi(*value);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> Config::getBool(std::string_view key) const {
    auto value = get(key);
    if (!value) {
        return std::nullopt;
    }
    
    std::string lower = *value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return (lower == "true" || lower == "1" || lower == "yes");
}

void Config::set(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[std::string(key)] = std::string(value);
}

} // namespace frqs::utils