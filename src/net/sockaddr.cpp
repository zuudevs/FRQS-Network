/**
 * @file net/sockaddr.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "net/sockaddr.hpp"

namespace frqs::net {

auto SockAddr::invert(std::unsigned_integral auto value) const noexcept -> decltype(value) {
    return IS_LITTLE_ENDIAN ? std::byteswap(value) : value ;
}

void SockAddr::append_uint16(std::string& dst, uint16_t port) const noexcept {
    if (port == 0) {
        dst.push_back('0') ;
        return ;
    }

    char buffer[5] ;
    int count = 0 ;

    uint16_t temp = port ;
    while (temp > 0) {
        buffer[count++] = (temp % 10) + '0' ;
        temp /= 10 ;
    }

    for (int i = count - 1 ; i >= 0 ; --i)
        dst.push_back(buffer[i]) ;
}

SockAddr::SockAddr(const IPv4& address, uint16_t port) noexcept
    : address_(address), port_(invert(port)) {}

SockAddr::SockAddr(const native_t& native) noexcept
    : address_(invert(native.sin_addr.s_addr))
    , port_(native.sin_port) {}

uint16_t SockAddr::getPort() const noexcept {
    return invert(port_) ;
}

void SockAddr::setPort(uint16_t port) noexcept {
    port_ = invert(port) ;
}

std::string SockAddr::toString() const noexcept {
    std::string r ;
    r.reserve(21) ;
    r += address_.toString() ;
    r += ':' ;
    append_uint16(r, getPort()) ;
    return r ;
}

SockAddr::native_t SockAddr::native() const noexcept {
    native_t r{} ;
    r.sin_family = AF_INET ;
    r.sin_port = port_ ;
    r.sin_addr.s_addr = address_.Nbo_toUint32() ;
    return r ;
}

} // namespace frqs::net