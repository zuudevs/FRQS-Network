/**
 * @file net/http_client.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "net/http_client.hpp"
#include "net/ipv4.hpp"
#include <format>
#include <algorithm>

namespace frqs::net {

std::optional<HttpClient::UrlParts> HttpClient::parseUrl(std::string_view url) {
    // Simple parser for http://host:port/path
    if (!url.starts_with("http://")) {
        return std::nullopt;
    }
    
    url.remove_prefix(7);  // Remove "http://"
    
    UrlParts parts;
    parts.port = 80;  // Default
    
    // Find path separator
    size_t path_pos = url.find('/');
    std::string_view host_port = (path_pos != std::string_view::npos) 
        ? url.substr(0, path_pos) 
        : url;
    
    parts.path = (path_pos != std::string_view::npos) 
        ? std::string(url.substr(path_pos)) 
        : "/";
    
    // Parse host and port
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string_view::npos) {
        parts.host = std::string(host_port.substr(0, colon_pos));
        try {
            parts.port = static_cast<uint16_t>(std::stoi(std::string(host_port.substr(colon_pos + 1))));
        } catch (...) {
            return std::nullopt;
        }
    } else {
        parts.host = std::string(host_port);
    }
    
    return parts;
}

std::optional<HttpResponse> HttpClient::get(std::string_view url, std::string_view auth_token) {
    auto url_parts = parseUrl(url);
    if (!url_parts) {
        return std::nullopt;
    }
    
    return sendRequest("GET", *url_parts, "", "", auth_token);
}

std::optional<HttpResponse> HttpClient::post(
    std::string_view url,
    std::string_view body,
    std::string_view content_type,
    std::string_view auth_token
) {
    auto url_parts = parseUrl(url);
    if (!url_parts) {
        return std::nullopt;
    }
    
    return sendRequest("POST", *url_parts, body, content_type, auth_token);
}

std::optional<HttpResponse> HttpClient::sendRequest(
    std::string_view method,
    const UrlParts& url_parts,
    std::string_view body,
    std::string_view content_type,
    std::string_view auth_token
) {
    try {
        // Resolve host (simplified - assumes numeric IP or localhost)
        IPv4 ip;
        if (url_parts.host == "localhost") {
            ip = IPv4({127, 0, 0, 1});
        } else {
            ip = IPv4(url_parts.host);
        }
        
        SockAddr addr(ip, url_parts.port);
        
        Socket socket;
        socket.connect(addr);
        
        // Build HTTP request
        std::string request = std::format("{} {} HTTP/1.1\r\n", method, url_parts.path);
        request += std::format("Host: {}\r\n", url_parts.host);
        request += "Connection: close\r\n";
        
        if (!auth_token.empty()) {
            request += std::format("Authorization: Bearer {}\r\n", auth_token);
        }
        
        if (!body.empty()) {
            request += std::format("Content-Type: {}\r\n", content_type);
            request += std::format("Content-Length: {}\r\n", body.size());
        }
        
        request += "\r\n";
        
        if (!body.empty()) {
            request += body;
        }
        
        // Send request
        socket.send(request);
        
        // Receive response (simplified - reads until connection closes)
        std::string response_data;
        while (true) {
            auto chunk = socket.receive(4096);
            if (chunk.empty()) {
                break;
            }
            response_data.append(chunk.begin(), chunk.end());
        }
        
        return parseResponse(response_data);
        
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<HttpResponse> HttpClient::parseResponse(std::string_view raw_response) {
    // Find status line
    size_t first_line_end = raw_response.find("\r\n");
    if (first_line_end == std::string_view::npos) {
        return std::nullopt;
    }
    
    std::string_view status_line = raw_response.substr(0, first_line_end);
    
    // Parse status line: HTTP/1.1 200 OK
    size_t first_space = status_line.find(' ');
    if (first_space == std::string_view::npos) {
        return std::nullopt;
    }
    
    size_t second_space = status_line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) {
        return std::nullopt;
    }
    
    HttpResponse response;
    
    try {
        response.status_code = std::stoi(std::string(
            status_line.substr(first_space + 1, second_space - first_space - 1)
        ));
    } catch (...) {
        return std::nullopt;
    }
    
    response.status_message = std::string(status_line.substr(second_space + 1));
    
    // Find headers end
    size_t headers_end = raw_response.find("\r\n\r\n");
    if (headers_end == std::string_view::npos) {
        return std::nullopt;
    }
    
    // Parse headers
    std::string_view headers_section = raw_response.substr(
        first_line_end + 2, 
        headers_end - first_line_end - 2
    );
    
    size_t pos = 0;
    while (pos < headers_section.size()) {
        size_t line_end = headers_section.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            line_end = headers_section.size();
        }
        
        std::string_view line = headers_section.substr(pos, line_end - pos);
        
        size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string name(line.substr(0, colon));
            std::string value(line.substr(colon + 1));
            
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            response.headers[name] = value;
        }
        
        pos = line_end + 2;
    }
    
    // Extract body
    response.body = std::string(raw_response.substr(headers_end + 4));
    
    return response;
}

} // namespace frqs::net