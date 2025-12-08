#pragma once

/**
 * @file net/socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "sockaddr.hpp"
#include <utility>
#include <vector>
#include <string_view>
#include <cstddef>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace frqs::net {

class Socket {
public:
#ifdef _WIN32
    using native_handle_t = SOCKET ;
    static constexpr native_handle_t invalid_handle = INVALID_SOCKET ;
#else
    using native_handle_t = int32_t ;
    static constexpr native_handle_t invalid_handle = -1 ;
#endif

    Socket() ;
    ~Socket() ;

    Socket(const Socket&) = delete ;
    Socket& operator=(const Socket&) = delete ;

    Socket(Socket&& other) noexcept ;
    Socket& operator=(Socket&& other) noexcept ;

    void bind(const SockAddr& addr) ;
    void listen(int backlog = SOMAXCONN) ;
    void connect(const SockAddr& addr) ;
    
    [[nodiscard]] Socket accept(SockAddr* out_client_addr = nullptr) ;
    
    size_t send(const void* data, size_t size) ;
    size_t send(std::string_view data) ;
    
    size_t receive(void* buffer, size_t size) ;
    [[nodiscard]] std::vector<char> receive(size_t max_size = 4096) ;
    
    void close() ;
    void shutdown(int how = 2) ;
    
    [[nodiscard]] bool invalid() const noexcept { return handle_ == invalid_handle ; }
    [[nodiscard]] native_handle_t native_handle() const noexcept { return handle_ ; }

private:
    explicit Socket(native_handle_t h) ;
    native_handle_t handle_ = invalid_handle ;
} ;

// Network initialization (Windows WSA)
class NetworkInit {
public:
    NetworkInit() ;
    ~NetworkInit() ;
    
    NetworkInit(const NetworkInit&) = delete ;
    NetworkInit& operator=(const NetworkInit&) = delete ;
    NetworkInit(NetworkInit&&) = delete ;
    NetworkInit& operator=(NetworkInit&&) = delete ;
} ;

// Global network initializer
inline NetworkInit g_network_init ;

} // namespace frqs::net