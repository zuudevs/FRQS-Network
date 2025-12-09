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
#include <iostream>

namespace frqs::utils {

bool Config::load(const std::filesystem::path& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "\n🔍 Attempting to load config from: " << config_file << std::endl;
    std::cout << "   Absolute path: " << std::filesystem::absolute(config_file) << std::endl;
    std::cout << "   File exists: " << (std::filesystem::exists(config_file) ? "YES" : "NO") << std::endl;
    
    std::ifstream file(config_file);
    if (!file) {
        std::cerr << "❌ FAILED to open config file!" << std::endl;
        return false;
    }
    
    std::cout << "✅ Config file opened successfully\n" << std::endl;
    
    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        
        // FIXED: Trim in-place before displaying
        std::string trimmed = trim(line);
        
        if (!trimmed.empty() && !trimmed.starts_with('#')) {
            std::cout << "  Line " << line_num << ": [" << trimmed << "]" << std::endl;
        }
        
        // FIXED: Parse the trimmed string
        parseLine(trimmed);
    }
    
    // Print loaded values
    std::cout << "\n📋 Loaded configuration values:" << std::endl;
    std::cout << "   ┌─────────────────────────────────────┐" << std::endl;
    for (const auto& [key, value] : values_) {
        std::cout << "   │ " << key;
        // Pad to 20 chars
        for (size_t i = key.length(); i < 20; ++i) std::cout << " ";
        std::cout << "= " << value << std::endl;
    }
    std::cout << "   └─────────────────────────────────────┘\n" << std::endl;
    
    return true;
}

void Config::parseLine(std::string_view line) {
    // FIXED: Don't re-trim, line is already trimmed
    // Skip comments and empty lines
    if (line.empty() || line.starts_with('#')) {
        return;
    }
    
    // Find equals sign
    auto eq_pos = line.find('=');
    if (eq_pos == std::string_view::npos) {
        return;
    }
    
    // FIXED: Create std::string for key and value to avoid dangling
    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));
    
    if (!key.empty()) {
        values_[key] = value;
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