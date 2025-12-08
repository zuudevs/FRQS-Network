/**
 * @file http/response.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "http/response.hpp"
#include <sstream>

namespace frqs::http {

HTTPResponse& HTTPResponse::setStatus(uint16_t code, std::string_view message) {
    status_code_ = code;
    status_message_ = message.empty() ? 
        std::string(getDefaultStatusMessage(code)) : 
        std::string(message);
    return *this;
}

HTTPResponse& HTTPResponse::setHeader(std::string_view name, std::string_view value) {
    headers_[std::string(name)] = value;
    return *this;
}

HTTPResponse& HTTPResponse::setBody(std::string body) {
    body_ = std::move(body);
    return *this;
}

HTTPResponse& HTTPResponse::setContentType(std::string_view type) {
    return setHeader("Content-Type", type);
}

HTTPResponse& HTTPResponse::ok(std::string body) {
    setStatus(200, "OK");
    if (!body.empty()) {
        setBody(std::move(body));
    }
    return *this;
}

HTTPResponse& HTTPResponse::notFound(std::string body) {
    setStatus(404, "Not Found");
    if (body.empty()) {
        body = "<html><body><h1>404 - Not Found</h1></body></html>";
    }
    setBody(std::move(body));
    setContentType("text/html");
    return *this;
}

HTTPResponse& HTTPResponse::badRequest(std::string body) {
    setStatus(400, "Bad Request");
    if (body.empty()) {
        body = "<html><body><h1>400 - Bad Request</h1></body></html>";
    }
    setBody(std::move(body));
    setContentType("text/html");
    return *this;
}

HTTPResponse& HTTPResponse::internalError(std::string body) {
    setStatus(500, "Internal Server Error");
    if (body.empty()) {
        body = "<html><body><h1>500 - Internal Server Error</h1></body></html>";
    }
    setBody(std::move(body));
    setContentType("text/html");
    return *this;
}

HTTPResponse& HTTPResponse::forbidden(std::string body) {
    setStatus(403, "Forbidden");
    if (body.empty()) {
        body = "<html><body><h1>403 - Forbidden</h1></body></html>";
    }
    setBody(std::move(body));
    setContentType("text/html");
    return *this;
}

std::string HTTPResponse::build() const {
    std::ostringstream oss;
    
    // Status line
    oss << "HTTP/1.1 " << status_code_ << " " << status_message_ << "\r\n";
    
    // Add Content-Length if not already set
    bool has_content_length = headers_.contains("Content-Length");
    if (!has_content_length && !body_.empty()) {
        oss << "Content-Length: " << body_.size() << "\r\n";
    }
    
    // Headers
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    // End of headers
    oss << "\r\n";
    
    // Body
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

std::string_view HTTPResponse::getDefaultStatusMessage(uint16_t code) noexcept {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

} // namespace frqs::http