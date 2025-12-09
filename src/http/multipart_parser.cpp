/**
 * @file http/multipart_parser.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "http/multipart_parser.hpp"
#include <algorithm>
#include <cctype>

namespace frqs::http {

bool MultipartParser::parse(std::string_view body, std::string_view boundary) {
    parts_.clear();
    
    if (boundary.empty()) {
        return false;
    }
    
    // Build boundary markers
    std::string boundary_start = "--" + std::string(boundary);
    std::string boundary_end = "--" + std::string(boundary) + "--";
    
    size_t pos = 0;
    
    // Find first boundary
    size_t start = body.find(boundary_start, pos);
    if (start == std::string_view::npos) {
        return false;
    }
    
    start += boundary_start.length();
    
    while (start < body.size()) {
        // Skip CRLF after boundary
        if (start < body.size() - 1 && body[start] == '\r' && body[start + 1] == '\n') {
            start += 2;
        }
        
        // Find next boundary
        size_t next_boundary = body.find(boundary_start, start);
        if (next_boundary == std::string_view::npos) {
            break;
        }
        
        // Extract part (between boundaries)
        std::string_view part_data = body.substr(start, next_boundary - start);
        
        // Find headers/body separator (double CRLF)
        size_t header_end = part_data.find("\r\n\r\n");
        if (header_end == std::string_view::npos) {
            start = next_boundary + boundary_start.length();
            continue;
        }
        
        MultipartPart part;
        
        // Parse headers
        std::string_view headers_section = part_data.substr(0, header_end);
        parsePartHeaders(headers_section, part);
        
        // Extract body (remove trailing CRLF before boundary)
        std::string_view body_section = part_data.substr(header_end + 4);
        while (!body_section.empty() && 
               (body_section.back() == '\r' || body_section.back() == '\n')) {
            body_section.remove_suffix(1);
        }
        
        // Copy binary data
        part.data.assign(body_section.begin(), body_section.end());
        
        parts_.push_back(std::move(part));
        
        // Move to next part
        start = next_boundary + boundary_start.length();
        
        // Check for end boundary
        if (body.substr(next_boundary, boundary_end.length()) == boundary_end) {
            break;
        }
    }
    
    return !parts_.empty();
}

void MultipartParser::parsePartHeaders(std::string_view header_section, MultipartPart& part) {
    size_t pos = 0;
    
    while (pos < header_section.size()) {
        size_t line_end = header_section.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            line_end = header_section.size();
        }
        
        std::string_view line = header_section.substr(pos, line_end - pos);
        
        if (!line.empty()) {
            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string name = trim(line.substr(0, colon));
                std::string value = trim(line.substr(colon + 1));
                
                // Convert header name to lowercase
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                
                part.headers[name] = value;
                
                // Parse Content-Disposition
                if (name == "content-disposition") {
                    parseContentDisposition(value, part);
                }
                // Parse Content-Type
                else if (name == "content-type") {
                    part.content_type = value;
                }
            }
        }
        
        pos = line_end + 2;
    }
}

void MultipartParser::parseContentDisposition(std::string_view value, MultipartPart& part) {
    // Format: form-data; name="field"; filename="file.txt"
    
    size_t pos = 0;
    while (pos < value.size()) {
        // Skip whitespace and semicolons
        while (pos < value.size() && (std::isspace(value[pos]) || value[pos] == ';')) {
            ++pos;
        }
        
        // Find next semicolon or end
        size_t end = value.find(';', pos);
        if (end == std::string_view::npos) {
            end = value.size();
        }
        
        std::string_view param = trim(value.substr(pos, end - pos));
        
        // Parse key=value
        size_t eq = param.find('=');
        if (eq != std::string_view::npos) {
            std::string key = trim(param.substr(0, eq));
            std::string val = trim(param.substr(eq + 1));
            
            // Remove quotes
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            
            if (key == "name") {
                part.name = val;
            } else if (key == "filename") {
                part.filename = val;
            }
        }
        
        pos = end;
    }
}

std::string MultipartParser::trim(std::string_view str) {
    auto start = std::find_if_not(str.begin(), str.end(), 
        [](unsigned char ch) { return std::isspace(ch); });
    
    auto end = std::find_if_not(str.rbegin(), str.rend(), 
        [](unsigned char ch) { return std::isspace(ch); }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

std::optional<MultipartPart> MultipartParser::findPart(std::string_view name) const {
    for (const auto& part : parts_) {
        if (part.name == name) {
            return part;
        }
    }
    return std::nullopt;
}

std::vector<MultipartPart> MultipartParser::getFileParts() const {
    std::vector<MultipartPart> files;
    for (const auto& part : parts_) {
        if (!part.filename.empty()) {
            files.push_back(part);
        }
    }
    return files;
}

} // namespace frqs::http