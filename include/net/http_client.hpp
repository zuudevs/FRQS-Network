#pragma once

/**
 * @file net/http_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "socket.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <unordered_map>

namespace frqs::net {

struct HttpResponse {
    int status_code;
    std::string status_message;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class HttpClient {
public:
    HttpClient() = default;
    
    // Simple GET request
    [[nodiscard]] std::optional<HttpResponse> get(
        std::string_view url,
        std::string_view auth_token = ""
    );
    
    // Simple POST request
    [[nodiscard]] std::optional<HttpResponse> post(
        std::string_view url,
        std::string_view body,
        std::string_view content_type = "application/json",
        std::string_view auth_token = ""
    );
    
    // Set timeout (milliseconds)
    void setTimeout(int timeout_ms) { timeout_ms_ = timeout_ms; }

private:
    int timeout_ms_ = 5000;
    
    struct UrlParts {
        std::string host;
        uint16_t port;
        std::string path;
    };
    
    [[nodiscard]] std::optional<UrlParts> parseUrl(std::string_view url);
    [[nodiscard]] std::optional<HttpResponse> sendRequest(
        std::string_view method,
        const UrlParts& url_parts,
        std::string_view body,
        std::string_view content_type,
        std::string_view auth_token
    );
    [[nodiscard]] std::optional<HttpResponse> parseResponse(std::string_view raw_response);
};

} // namespace frqs::net