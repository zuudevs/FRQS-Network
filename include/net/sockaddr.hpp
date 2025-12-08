#pragma once

/**
 * @file net/sockaddr.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "ipv4.hpp"

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

namespace frqs::net {

class SockAddr {
public:
    using native_t = sockaddr_in ;

    SockAddr() noexcept = default ;
    SockAddr(const SockAddr&) noexcept = default ;
    SockAddr(SockAddr&&) noexcept = default ;
    SockAddr& operator=(const SockAddr&) noexcept = default ;
    SockAddr& operator=(SockAddr&&) noexcept = default ;
    std::strong_ordering operator<=>(const SockAddr&) const noexcept = default ;
    ~SockAddr() noexcept = default ;

    SockAddr(const IPv4& address, uint16_t port) noexcept ;
    SockAddr(const native_t& native) noexcept ;

    const IPv4& getAddress() const noexcept { return address_ ; }
    IPv4& getAddress() noexcept { return address_ ; }
    uint16_t getPort() const noexcept ;
    void setPort(uint16_t port) noexcept ;

    std::string toString() const noexcept ;
    native_t native() const noexcept ;

private:
    IPv4 address_{} ;
    uint16_t port_{} ;  // Stored in network byte order

    auto invert(std::unsigned_integral auto value) const noexcept -> decltype(value) ;
    void append_uint16(std::string& dst, uint16_t port) const noexcept ;
} ;

} // namespace frqs::net