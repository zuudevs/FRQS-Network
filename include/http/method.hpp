#pragma once

/**
 * @file http/method.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <cstdint>
#include <string_view>

namespace frqs::http {

enum class Method : uint8_t {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    UNKNOWN
} ;

[[nodiscard]] constexpr Method parseMethod(std::string_view str) noexcept {
    if (str == "GET") return Method::GET ;
    if (str == "POST") return Method::POST ;
    if (str == "PUT") return Method::PUT ;
    if (str == "DELETE") return Method::DELETE ;
    if (str == "HEAD") return Method::HEAD ;
    if (str == "OPTIONS") return Method::OPTIONS ;
    if (str == "PATCH") return Method::PATCH ;
    return Method::UNKNOWN ;
}

[[nodiscard]] constexpr std::string_view methodToString(Method method) noexcept {
    switch (method) {
        case Method::GET: return "GET" ;
        case Method::POST: return "POST" ;
        case Method::PUT: return "PUT" ;
        case Method::DELETE: return "DELETE" ;
        case Method::HEAD: return "HEAD" ;
        case Method::OPTIONS: return "OPTIONS" ;
        case Method::PATCH: return "PATCH" ;
        default: return "UNKNOWN" ;
    }
}

} // namespace frqs::http