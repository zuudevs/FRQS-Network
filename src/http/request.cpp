/**
 * @file http/request.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "http/request.hpp"
#include <algorithm>
#include <cctype>

namespace frqs::http {

bool HTTPRequest::parse(std::string_view raw_data) noexcept {
    // Security: Check size limit
    if (raw_data.size() > MAX_REQUEST_SIZE) {
        error_message_ = "Request too large" ;
        return false ;
    }
    
    // Store the raw request (this is the ONLY allocation)
    raw_request_ = raw_data ;
    
    // Now work with views into raw_request_
    std::string_view view = raw_request_ ;
    
    // Find the end of headers (double CRLF)
    auto header_end = view.find("\r\n\r\n") ;
    if (header_end == std::string_view::npos) {
        error_message_ = "Malformed request: no header terminator" ;
        return false ;
    }
    
    std::string_view header_section = view.substr(0, header_end) ;
    body_ = view.substr(header_end + 4) ; // +4 to skip \r\n\r\n
    
    // Parse request line (first line)
    auto first_line_end = header_section.find("\r\n") ;
    if (first_line_end == std::string_view::npos) {
        error_message_ = "Malformed request line" ;
        return false ;
    }
    
    std::string_view request_line = header_section.substr(0, first_line_end) ;
    if (!parseRequestLine(request_line)) {
        return false ;
    }
    
    // Parse remaining headers
    std::string_view remaining_headers = header_section.substr(first_line_end + 2) ;
    parseHeaders(remaining_headers) ;
    
    // Parse query string if present
    parseQueryString() ;
    
    is_valid_ = true ;
    return true ;
}

bool HTTPRequest::parseRequestLine(std::string_view line) noexcept {
    // Format: METHOD /path?query HTTP/1.1
    
    // Extract method
    auto method_end = line.find(' ') ;
    if (method_end == std::string_view::npos) {
        error_message_ = "Invalid request line: no method" ;
        return false ;
    }
    
    std::string_view method_str = line.substr(0, method_end) ;
    method_ = parseMethod(method_str) ;
    
    if (method_ == Method::UNKNOWN) {
        error_message_ = "Unsupported HTTP method" ;
        return false ;
    }
    
    // Extract URI
    line = line.substr(method_end + 1) ;
    auto uri_end = line.find(' ') ;
    if (uri_end == std::string_view::npos) {
        error_message_ = "Invalid request line: no URI" ;
        return false ;
    }
    
    std::string_view full_uri = line.substr(0, uri_end) ;
    
    // Split path and query string
    auto query_start = full_uri.find('?') ;
    if (query_start != std::string_view::npos) {
        path_ = full_uri.substr(0, query_start) ;
        query_string_ = full_uri.substr(query_start + 1) ;
    } else {
        path_ = full_uri ;
        query_string_ = "" ;
    }
    
    // Extract version
    version_ = line.substr(uri_end + 1) ;
    
    // Validate version
    if (version_ != "HTTP/1.1" && version_ != "HTTP/1.0") {
        error_message_ = "Unsupported HTTP version" ;
        return false ;
    }
    
    return true ;
}

void HTTPRequest::parseHeaders(std::string_view header_section) noexcept {
    size_t pos = 0 ;
    
    while (pos < header_section.size()) {
        auto line_end = header_section.find("\r\n", pos) ;
        if (line_end == std::string_view::npos) {
            line_end = header_section.size() ;
        }
        
        std::string_view line = header_section.substr(pos, line_end - pos) ;
        
        if (!line.empty()) {
            auto colon_pos = line.find(':') ;
            if (colon_pos != std::string_view::npos) {
                std::string_view name = line.substr(0, colon_pos) ;
                std::string_view value = line.substr(colon_pos + 1) ;
                
                // Trim leading/trailing whitespace from value
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                    value.remove_prefix(1) ;
                }
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                    value.remove_suffix(1) ;
                }
                
                headers_[name] = value ;
            }
        }
        
        pos = line_end + 2 ; // Skip \r\n
    }
}

void HTTPRequest::parseQueryString() noexcept {
    if (query_string_.empty()) {
        return ;
    }
    
    size_t pos = 0 ;
    while (pos < query_string_.size()) {
        auto amp_pos = query_string_.find('&', pos) ;
        if (amp_pos == std::string_view::npos) {
            amp_pos = query_string_.size() ;
        }
        
        std::string_view param = query_string_.substr(pos, amp_pos - pos) ;
        auto eq_pos = param.find('=') ;
        
        if (eq_pos != std::string_view::npos) {
            std::string_view key = param.substr(0, eq_pos) ;
            std::string_view value = param.substr(eq_pos + 1) ;
            query_params_[key] = value ;
        } else {
            query_params_[param] = "" ;
        }
        
        pos = amp_pos + 1 ;
    }
}

std::optional<std::string_view> HTTPRequest::getHeader(std::string_view name) const noexcept {
    auto it = std::find_if(headers_.begin(), headers_.end(),
        [name](const auto& pair) {
            return std::equal(pair.first.begin(), pair.first.end(),
                             name.begin(), name.end(),
                             [](char a, char b) {
                                 return std::tolower(static_cast<unsigned char>(a)) == 
                                        std::tolower(static_cast<unsigned char>(b)) ;
                             }) ;
        }) ;
    
    if (it != headers_.end()) {
        return it->second ;
    }
    return std::nullopt ;
}

std::optional<std::string_view> HTTPRequest::getQueryParam(std::string_view name) const noexcept {
    auto it = query_params_.find(name) ;
    if (it != query_params_.end()) {
        return it->second ;
    }
    return std::nullopt ;
}

size_t HTTPRequest::CaseInsensitiveHash::operator()(std::string_view sv) const noexcept {
    size_t hash = 0 ;
    for (char c : sv) {
        hash = hash * 31 + std::tolower(static_cast<unsigned char>(c)) ;
    }
    return hash ;
}

bool HTTPRequest::CaseInsensitiveEqual::operator()(std::string_view a, std::string_view b) const noexcept {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char ca, char cb) {
            return std::tolower(static_cast<unsigned char>(ca)) == 
                   std::tolower(static_cast<unsigned char>(cb)) ;
        }) ;
}

} // namespace frqs::http