#pragma once

#include "net/socket.hpp"
#include "net/sockaddr.hpp"

#ifdef DELETE
	#undef DELETE
#endif

#include "http/request.hpp"
#include "http/response.hpp"
#include "utils/thread_pool.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <atomic>

namespace frqs::core {

class Server {
public:
    using RequestHandler = std::function<http::HTTPResponse(const http::HTTPRequest&)> ;
    
    explicit Server(
        uint16_t port = 8080,
        size_t thread_count = std::thread::hardware_concurrency()
    ) ;
    
    ~Server() ;
    
    // Delete copy and move
    Server(const Server&) = delete ;
    Server& operator=(const Server&) = delete ;
    Server(Server&&) = delete ;
    Server& operator=(Server&&) = delete ;
    
    // Configuration
    void setDocumentRoot(const std::filesystem::path& root) ;
    void setDefaultFile(std::string filename) ;
    void setRequestHandler(RequestHandler handler) ;
    
    // Server control
    void start() ;
    void stop() ;
    
    [[nodiscard]] bool isRunning() const noexcept { return running_ ; }
    [[nodiscard]] uint16_t getPort() const noexcept { return port_ ; }

private:
    uint16_t port_ ;
    std::filesystem::path document_root_ ;
    std::string default_file_ = "index.html" ;
    
    std::unique_ptr<net::Socket> server_socket_ ;
    std::unique_ptr<utils::ThreadPool> thread_pool_ ;
    
    std::atomic<bool> running_{false} ;
    RequestHandler custom_handler_ ;
    
    // Internal handlers
    void acceptLoop() ;
    void handleClient(net::Socket client, net::SockAddr client_addr) ;
    
    http::HTTPResponse handleRequest(const http::HTTPRequest& request) ;
    http::HTTPResponse serveStaticFile(const http::HTTPRequest& request) ;
} ;

} // namespace frqs::core