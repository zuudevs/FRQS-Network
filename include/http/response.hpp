#pragma once

/**
 * @file http/response.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace frqs::http {

class HTTPResponse {
public:
    HTTPResponse() = default ;
    
    // Fluent API for building responses
    HTTPResponse& setStatus(uint16_t code, std::string_view message = "") ;
    HTTPResponse& setHeader(std::string_view name, std::string_view value) ;
    HTTPResponse& setBody(std::string body) ;
    HTTPResponse& setContentType(std::string_view type) ;
    
    // Common status codes
    HTTPResponse& ok(std::string body = "") ;
    HTTPResponse& notFound(std::string body = "") ;
    HTTPResponse& badRequest(std::string body = "") ;
    HTTPResponse& internalError(std::string body = "") ;
    HTTPResponse& forbidden(std::string body = "") ;
    
    // Build the complete HTTP response
    [[nodiscard]] std::string build() const ;
    
    // Direct access
    [[nodiscard]] uint16_t getStatus() const noexcept { return status_code_ ; }
    [[nodiscard]] const std::string& getBody() const noexcept { return body_ ; }

private:
    uint16_t status_code_ = 200 ;
    std::string status_message_ = "OK" ;
    std::string body_ ;
    std::unordered_map<std::string, std::string> headers_ ;
    
    [[nodiscard]] static std::string_view getDefaultStatusMessage(uint16_t code) noexcept ;
} ;

} // namespace frqs::http