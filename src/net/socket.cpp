/**
 * @file net/socket.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "net/socket.hpp"
#include <stdexcept>

#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <errno.h>
    #include <netinet/in.h>
#endif

namespace frqs::net {

NetworkInit::NetworkInit() {
#ifdef _WIN32
    WSADATA wsaData ;
    int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData) ;
    if (result != 0) {
        throw std::runtime_error("Failed to initialize Winsock") ;
    }
#endif
}

NetworkInit::~NetworkInit() {
#ifdef _WIN32
    ::WSACleanup() ;
#endif
}

Socket::Socket() {
    handle_ = ::socket(AF_INET, SOCK_STREAM, 0) ;
    
    if (invalid()) {
        throw std::runtime_error("Failed to create socket") ;
    }
}

Socket::Socket(native_handle_t h) : handle_(h) {}

Socket::~Socket() {
    close() ;
}

Socket::Socket(Socket&& other) noexcept
    : handle_(std::exchange(other.handle_, invalid_handle)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close() ;
        handle_ = std::exchange(other.handle_, invalid_handle) ;
    }
    return *this ;
}

void Socket::bind(const SockAddr& addr) {
    auto native_addr = addr.native() ;
    if (::bind(handle_, reinterpret_cast<const sockaddr*>(&native_addr), 
               sizeof(native_addr)) != 0) {
        throw std::runtime_error("Bind failed") ;
    }
}

void Socket::listen(int backlog) {
    if (::listen(handle_, backlog) != 0) {
        throw std::runtime_error("Listen failed") ;
    }
}

void Socket::connect(const SockAddr& addr) {
    auto native_addr = addr.native() ;
    if (::connect(handle_, reinterpret_cast<const sockaddr*>(&native_addr), 
                  sizeof(native_addr)) != 0) {
        throw std::runtime_error("Connect failed") ;
    }
}

Socket Socket::accept(SockAddr* out_client_addr) {
    SockAddr::native_t client_native{} ;
    socklen_t len = sizeof(client_native) ;
    
    native_handle_t client_fd = ::accept(
        handle_, 
        reinterpret_cast<sockaddr*>(&client_native), 
        &len
    ) ;
    
    if (client_fd == invalid_handle) {
        throw std::runtime_error("Accept failed") ;
    }
    
    if (out_client_addr) {
        *out_client_addr = SockAddr(client_native) ;
    }
    
    return Socket(client_fd) ;
}

size_t Socket::send(const void* data, size_t size) {
    auto sent = ::send(handle_, static_cast<const char*>(data), 
                       static_cast<int>(size), 0) ;
    if (sent < 0) {
        throw std::runtime_error("Send failed") ;
    }
    return static_cast<size_t>(sent) ;
}

size_t Socket::send(std::string_view data) {
    return send(data.data(), data.size()) ;
}

size_t Socket::receive(void* buffer, size_t size) {
    auto received = ::recv(handle_, static_cast<char*>(buffer), 
                          static_cast<int>(size), 0) ;
    if (received < 0) {
        throw std::runtime_error("Receive failed") ;
    }
    return static_cast<size_t>(received) ;
}

std::vector<char> Socket::receive(size_t max_size) {
    std::vector<char> buffer(max_size) ;
    size_t bytes = receive(buffer.data(), max_size) ;
    buffer.resize(bytes) ;
    return buffer ;
}

void Socket::close() {
    if (handle_ != invalid_handle) {
#ifdef _WIN32
        ::closesocket(handle_) ;
#else
        ::close(handle_) ;
#endif
        handle_ = invalid_handle ;
    }
}

void Socket::shutdown(int how) {
    if (handle_ != invalid_handle) {
        ::shutdown(handle_, how) ;
    }
}

} // namespace frqs::net