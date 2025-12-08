/**
 * @file utils/logger.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/logger.hpp"
#include <iostream>
#include <fstream>
#include <mutex>

namespace frqs::utils {

class Logger {
public:
    static Logger& instance() {
        static Logger logger ;
        return logger ;
    }
    
    void log(Level level, std::string_view message) {
        std::lock_guard<std::mutex> lock(mutex_) ;
        
        auto log_entry = CreateLog(level, message) ;
        
        // Write to console
        if (level == Level::ERROR) {
            std::cerr << log_entry << '\n' ;
        } else {
            std::cout << log_entry << '\n' ;
        }
        
        // Write to file if enabled
        if (log_file_.is_open()) {
            log_file_ << log_entry << '\n' ;
            log_file_.flush() ;
        }
    }
    
    void enableFileLogging(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_) ;
        if (log_file_.is_open()) {
            log_file_.close() ;
        }
        log_file_.open(filename, std::ios::app) ;
    }
    
private:
    Logger() = default ;
    std::mutex mutex_ ;
    std::ofstream log_file_ ;
} ;

void logInfo(std::string_view message) {
    Logger::instance().log(Level::INFO, message) ;
}

void logWarn(std::string_view message) {
    Logger::instance().log(Level::WARN, message) ;
}

void logError(std::string_view message) {
    Logger::instance().log(Level::ERROR, message) ;
}

void enableFileLogging(const std::string& filename) {
    Logger::instance().enableFileLogging(filename) ;
}

} // namespace frqs::utils