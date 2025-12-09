#pragma once

/**
 * @file http/multipart_parser.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace frqs::http {

struct MultipartPart {
    std::unordered_map<std::string, std::string> headers;
    std::string name;
    std::string filename;
    std::string content_type;
    std::vector<uint8_t> data;
};

class MultipartParser {
public:
    MultipartParser() = default;
    
    // Parse multipart/form-data body
    [[nodiscard]] bool parse(std::string_view body, std::string_view boundary);
    
    // Get parsed parts
    [[nodiscard]] const std::vector<MultipartPart>& getParts() const noexcept { 
        return parts_; 
    }
    
    // Find part by field name
    [[nodiscard]] std::optional<MultipartPart> findPart(std::string_view name) const;
    
    // Get all file parts
    [[nodiscard]] std::vector<MultipartPart> getFileParts() const;

private:
    std::vector<MultipartPart> parts_;
    
    void parsePartHeaders(std::string_view header_section, MultipartPart& part);
    void parseContentDisposition(std::string_view value, MultipartPart& part);
    static std::string trim(std::string_view str);
};

} // namespace frqs::http