#pragma once

/**
 * @file http/request.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "method.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

namespace frqs::http {

class HTTPRequest {
public:
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024 ; // 1MB limit
    
    HTTPRequest() = default ;
    
    // Parse raw HTTP request (Zero-Copy where possible)
    [[nodiscard]] bool parse(std::string_view raw_data) noexcept ;
    
    // Getters
    [[nodiscard]] Method getMethod() const noexcept { return method_ ; }
    [[nodiscard]] std::string_view getPath() const noexcept { return path_ ; }
    [[nodiscard]] std::string_view getQueryString() const noexcept { return query_string_ ; }
    [[nodiscard]] std::string_view getVersion() const noexcept { return version_ ; }
    
    [[nodiscard]] std::optional<std::string_view> getHeader(std::string_view name) const noexcept ;
    [[nodiscard]] std::optional<std::string_view> getQueryParam(std::string_view name) const noexcept ;
    
    [[nodiscard]] const auto& getHeaders() const noexcept { return headers_ ; }
    [[nodiscard]] const auto& getQueryParams() const noexcept { return query_params_ ; }
    
    [[nodiscard]] std::string_view getBody() const noexcept { return body_ ; }
    
    // Validation
    [[nodiscard]] bool isValid() const noexcept { return is_valid_ ; }
    [[nodiscard]] std::string_view getError() const noexcept { return error_message_ ; }

private:
    // Backing storage for the raw request (owns the data)
    std::string raw_request_ ;
    
    // Zero-copy views into raw_request_
    Method method_ = Method::UNKNOWN ;
    std::string_view path_ ;
    std::string_view query_string_ ;
    std::string_view version_ ;
    std::string_view body_ ;
    
    // Headers and query params (keys/values are views into raw_request_)
    std::unordered_map<std::string_view, std::string_view> headers_ ;
    std::unordered_map<std::string_view, std::string_view> query_params_ ;
    
    bool is_valid_ = false ;
    std::string_view error_message_ ;
    
    // Helper parsing functions
    bool parseRequestLine(std::string_view line) noexcept ;
    void parseHeaders(std::string_view header_section) noexcept ;
    void parseQueryString() noexcept ;
    
    // Case-insensitive comparison for header names
    struct CaseInsensitiveHash {
        size_t operator()(std::string_view sv) const noexcept ;
    } ;
    
    struct CaseInsensitiveEqual {
        bool operator()(std::string_view a, std::string_view b) const noexcept ;
    } ;
} ;

} // namespace frqs::http