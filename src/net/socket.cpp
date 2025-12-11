/**
 * @file net/socket.cpp
 * @brief Fixed version with SO_REUSEADDR and better error handling
 * @version 1.0.1
 */

#include "net/socket.hpp"
#include <stdexcept>
#include <string>

#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <errno.h>
    #include <netinet/in.h>
    #include <cstring>
#endif

namespace frqs::net {

NetworkInit::NetworkInit() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif
}

NetworkInit::~NetworkInit() {
#ifdef _WIN32
    ::WSACleanup();
#endif
}

Socket::Socket() {
    handle_ = ::socket(AF_INET, SOCK_STREAM, 0);
    
    if (invalid()) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("Failed to create socket. Error: " + std::to_string(err));
#else
        throw std::runtime_error("Failed to create socket");
#endif
    }
    
    // ✅ CRITICAL FIX: Enable SO_REUSEADDR to prevent "Address already in use"
    int opt = 1;
    if (::setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, 
                     reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        ::closesocket(handle_);
        throw std::runtime_error("Failed to set SO_REUSEADDR. Error: " + std::to_string(err));
#else
        ::close(handle_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
#endif
    }
    
#ifdef _WIN32
    // Windows-specific: Disable exclusive address use
    BOOL bOptVal = FALSE;
    if (::setsockopt(handle_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 
                     reinterpret_cast<const char*>(&bOptVal), sizeof(bOptVal)) < 0) {
        // Non-critical, just log
    }
#endif
}

Socket::Socket(native_handle_t h) : handle_(h) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : handle_(std::exchange(other.handle_, invalid_handle)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, invalid_handle);
    }
    return *this;
}

void Socket::bind(const SockAddr& addr) {
    auto native_addr = addr.native();
    
    if (::bind(handle_, reinterpret_cast<const sockaddr*>(&native_addr), 
               sizeof(native_addr)) != 0) {
        
        // ✅ IMPROVED ERROR MESSAGE with detailed information
        std::string error_msg = "Bind failed on " + addr.toString();
        
#ifdef _WIN32
        int error_code = WSAGetLastError();
        
        switch(error_code) {
            case WSAEADDRINUSE:
                error_msg += " - Port already in use. ";
                error_msg += "Another process is using this port. ";
                error_msg += "Run: netstat -ano | findstr :" + std::to_string(addr.getPort());
                break;
            case WSAEACCES:
                error_msg += " - Permission denied. ";
                error_msg += "Try running as Administrator or use port > 1024";
                break;
            case WSAEADDRNOTAVAIL:
                error_msg += " - Address not available. ";
                error_msg += "The specified address is not valid for this machine";
                break;
            case WSAEINVAL:
                error_msg += " - Invalid argument. ";
                error_msg += "Socket may already be bound";
                break;
            default:
                error_msg += " - WSA Error Code: " + std::to_string(error_code);
        }
#else
        int error_code = errno;
        
        switch(error_code) {
            case EADDRINUSE:
                error_msg += " - Port already in use. ";
                error_msg += "Another process is using this port. ";
                error_msg += "Run: sudo lsof -i :" + std::to_string(addr.getPort());
                break;
            case EACCES:
                error_msg += " - Permission denied. ";
                error_msg += "Try running with sudo or use port > 1024";
                break;
            case EADDRNOTAVAIL:
                error_msg += " - Address not available. ";
                error_msg += "The specified address is not valid for this machine";
                break;
            case EINVAL:
                error_msg += " - Invalid argument. ";
                error_msg += "Socket may already be bound";
                break;
            default:
                error_msg += " - " + std::string(strerror(error_code));
        }
#endif
        
        throw std::runtime_error(error_msg);
    }
}

void Socket::listen(int backlog) {
    if (::listen(handle_, backlog) != 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("Listen failed. Error: " + std::to_string(err));
#else
        throw std::runtime_error("Listen failed: " + std::string(strerror(errno)));
#endif
    }
}

void Socket::connect(const SockAddr& addr) {
    auto native_addr = addr.native();
    if (::connect(handle_, reinterpret_cast<const sockaddr*>(&native_addr), 
                  sizeof(native_addr)) != 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("Connect to " + addr.toString() + 
                               " failed. Error: " + std::to_string(err));
#else
        throw std::runtime_error("Connect to " + addr.toString() + 
                               " failed: " + std::string(strerror(errno)));
#endif
    }
}

Socket Socket::accept(SockAddr* out_client_addr) {
    SockAddr::native_t client_native{};
    socklen_t len = sizeof(client_native);
    
    native_handle_t client_fd = ::accept(
        handle_, 
        reinterpret_cast<sockaddr*>(&client_native), 
        &len
    );
    
    if (client_fd == invalid_handle) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEINTR) { // Ignore interrupt
            throw std::runtime_error("Accept failed. Error: " + std::to_string(err));
        }
#else
        if (errno != EINTR) { // Ignore interrupt
            throw std::runtime_error("Accept failed: " + std::string(strerror(errno)));
        }
#endif
        throw std::runtime_error("Accept failed");
    }
    
    if (out_client_addr) {
        *out_client_addr = SockAddr(client_native);
    }
    
    return Socket(client_fd);
}

size_t Socket::send(const void* data, size_t size) {
    auto sent = ::send(handle_, static_cast<const char*>(data), 
                       static_cast<int>(size), 0);
    if (sent < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("Send failed. Error: " + std::to_string(err));
#else
        throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
#endif
    }
    return static_cast<size_t>(sent);
}

size_t Socket::send(std::string_view data) {
    return send(data.data(), data.size());
}

size_t Socket::receive(void* buffer, size_t size) {
    auto received = ::recv(handle_, static_cast<char*>(buffer), 
                          static_cast<int>(size), 0);
    if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        throw std::runtime_error("Receive failed. Error: " + std::to_string(err));
#else
        throw std::runtime_error("Receive failed: " + std::string(strerror(errno)));
#endif
    }
    return static_cast<size_t>(received);
}

std::vector<char> Socket::receive(size_t max_size) {
    std::vector<char> buffer(max_size);
    size_t bytes = receive(buffer.data(), max_size);
    buffer.resize(bytes);
    return buffer;
}

void Socket::close() {
    if (handle_ != invalid_handle) {
#ifdef _WIN32
        ::closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = invalid_handle;
    }
}

void Socket::shutdown(int how) {
    if (handle_ != invalid_handle) {
        ::shutdown(handle_, how);
    }
}

} // namespace frqs::net