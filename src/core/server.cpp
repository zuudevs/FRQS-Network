#include "core/server.hpp"
#include "http/mime_types.hpp"

#ifdef ERROR
	#undef ERROR
#endif

#include "utils/logger.hpp"
#include "utils/filesystem_utils.hpp"
#include <format>
#include <thread>

namespace frqs::core {

Server::Server(uint16_t port, size_t thread_count)
    : port_(port)
    , document_root_(std::filesystem::current_path() / "public")
    , thread_pool_(std::make_unique<utils::ThreadPool>(thread_count))
{
    utils::logInfo(std::format("Server initialized on port {} with {} threads", 
                                port_, thread_count));
}

Server::~Server() {
    stop();
}

void Server::setDocumentRoot(const std::filesystem::path& root) {
    document_root_ = root;
    utils::logInfo(std::format("Document root set to: {}", root.string()));
}

void Server::setDefaultFile(std::string filename) {
    default_file_ = std::move(filename);
}

void Server::setRequestHandler(RequestHandler handler) {
    custom_handler_ = std::move(handler);
}

void Server::start() {
    if (running_) {
        utils::logWarn("Server is already running");
        return;
    }
    
    try {
        server_socket_ = std::make_unique<net::Socket>();
        
        net::SockAddr bind_addr(net::IPv4(0u), port_);
        server_socket_->bind(bind_addr);
        server_socket_->listen();
        
        running_ = true;
        
        utils::logInfo(std::format("Server listening on {}", bind_addr.toString()));
        utils::logInfo(std::format("Document root: {}", document_root_.string()));
        
        acceptLoop();
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Server error: {}", e.what()));
        running_ = false;
        throw;
    }
}

void Server::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_socket_) {
        server_socket_->close();
    }
    
    utils::logInfo("Server stopped");
}

void Server::acceptLoop() {
    while (running_) {
        try {
            net::SockAddr client_addr;
            net::Socket client = server_socket_->accept(&client_addr);
            
            utils::logInfo(std::format("Connection from {}", client_addr.toString()));
            
            // Dispatch to thread pool
            thread_pool_->submit([this, client = std::move(client), client_addr]() mutable {
                handleClient(std::move(client), client_addr);
            });
            
        } catch (const std::exception& e) {
            if (running_) {
                utils::logError(std::format("Accept error: {}", e.what()));
            }
        }
    }
}

void Server::handleClient(net::Socket client, net::SockAddr client_addr) {
    try {
        // Read request (with timeout consideration in production)
        auto buffer = client.receive(8192);
        
        if (buffer.empty()) {
            utils::logWarn(std::format("Empty request from {}", client_addr.toString()));
            return;
        }
        
        // Parse request
        http::HTTPRequest request;
        std::string_view raw_request(buffer.data(), buffer.size());
        
        if (!request.parse(raw_request)) {
            utils::logWarn(std::format("Invalid request from {}: {}", 
                                      client_addr.toString(), 
                                      request.getError()));
            
            auto response = http::HTTPResponse().badRequest();
            client.send(response.build());
            return;
        }
        
        utils::logInfo(std::format("{} {} from {}", 
                                  http::methodToString(request.getMethod()),
                                  request.getPath(),
                                  client_addr.toString()));
        
        // Handle request
        auto response = handleRequest(request);
        
        // Send response
        client.send(response.build());
        
        utils::logInfo(std::format("Responded {} to {}", 
                                  response.getStatus(),
                                  client_addr.toString()));
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Error handling client {}: {}", 
                                   client_addr.toString(), 
                                   e.what()));
        
        try {
            auto response = http::HTTPResponse().internalError();
            client.send(response.build());
        } catch (...) {
            // Best effort
        }
    }
}

http::HTTPResponse Server::handleRequest(const http::HTTPRequest& request) {
    // Use custom handler if provided
    if (custom_handler_) {
        return custom_handler_(request);
    }
    
    // Default: serve static files
    return serveStaticFile(request);
}

http::HTTPResponse Server::serveStaticFile(const http::HTTPRequest& request) {
    // Only support GET and HEAD
    if (request.getMethod() != http::Method::GET && 
        request.getMethod() != http::Method::HEAD) {
        return http::HTTPResponse()
            .setStatus(405, "Method Not Allowed")
            .setBody("<h1>405 - Method Not Allowed</h1>")
            .setContentType("text/html");
    }
    
    // Get requested path
    std::string requested_path(request.getPath());
    
    // If path ends with /, append default file
    if (requested_path.ends_with('/')) {
        requested_path += default_file_;
    }
    
    // Security check: resolve path safely
    auto safe_path = utils::FileSystemUtils::securePath(document_root_, requested_path);
    
    if (!safe_path) {
        utils::logWarn(std::format("Path traversal attempt: {}", requested_path));
        return http::HTTPResponse().forbidden(
            "<h1>403 - Forbidden</h1><p>Path traversal detected.</p>"
        );
    }
    
    // Check if file exists
    if (!std::filesystem::exists(*safe_path)) {
        utils::logWarn(std::format("File not found: {}", safe_path->string()));
        return http::HTTPResponse().notFound();
    }
    
    // Check if it's a regular file
    if (!std::filesystem::is_regular_file(*safe_path)) {
        return http::HTTPResponse().forbidden(
            "<h1>403 - Forbidden</h1><p>Not a regular file.</p>"
        );
    }
    
    // Read file
    auto content = utils::FileSystemUtils::readFile(*safe_path);
    
    if (!content) {
        utils::logError(std::format("Failed to read file: {}", safe_path->string()));
        return http::HTTPResponse().internalError();
    }
    
    // Detect MIME type
    auto mime_type = http::MimeTypes::fromPath(*safe_path);
    
    return http::HTTPResponse()
        .ok(std::move(*content))
        .setContentType(mime_type);
}

} // namespace frqs::core