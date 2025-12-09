#include "core/server.hpp"
#include "http/mime_types.hpp"
#include "http/multipart_parser.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "utils/filesystem_utils.hpp"
#include "utils/screen_capturer.hpp"
#include "utils/input_injector.hpp"
#include <format>
#include <thread>
#include <chrono>
#include <fstream>

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
        auto buffer = client.receive(8192);
        
        if (buffer.empty()) {
            utils::logWarn(std::format("Empty request from {}", client_addr.toString()));
            return;
        }
        
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
        
        auto response = handleRequest(request);
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
        } catch (...) {}
    }
}

http::HTTPResponse Server::handleRequest(const http::HTTPRequest& request) {
    auto& config = utils::Config::instance();
    std::string path(request.getPath());
    
    // CORS middleware - Handle OPTIONS
    if (request.getMethod() == http::Method::OPTIONS) {
        return http::HTTPResponse()
            .setStatus(204, "No Content")
            .setHeader("Access-Control-Allow-Origin", "*")
            .setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            .setHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    }
    
    // Add CORS headers to all responses
    auto addCorsHeaders = [](http::HTTPResponse& resp) -> http::HTTPResponse& {
        resp.setHeader("Access-Control-Allow-Origin", "*");
        return resp;
    };
    
    // Auth middleware for protected routes
    auto requireAuth = [&]() -> bool {
        if (path.starts_with("/api/") || path == "/stream" || path == "/upload") {
            auto auth_header = request.getHeader("Authorization");
            if (!auth_header) {
                return false;
            }
            
            std::string expected = "Bearer " + config.getAuthToken();
            return *auth_header == expected;
        }
        return true;
    };
    
    if (!requireAuth()) {
        utils::logWarn(std::format("Unauthorized access attempt to {}", path));
        auto resp = http::HTTPResponse()
            .setStatus(401, "Unauthorized")
            .setBody(R"({"error":"Unauthorized"})");
        return addCorsHeaders(resp);
    }
    
    // Route: GET /stream - MJPEG Stream
    if (path == "/stream" && request.getMethod() == http::Method::GET) {
        return handleStream(request);
    }
    
    // Route: POST /api/input - Remote Control
    if (path == "/api/input" && request.getMethod() == http::Method::POST) {
        return handleInputControl(request);
    }
    
    // Route: POST /upload - File Upload
    if (path == "/upload" && request.getMethod() == http::Method::POST) {
        return handleFileUpload(request);
    }
    
    // Route: GET /api/status - Server Status
    if (path == "/api/status" && request.getMethod() == http::Method::GET) {
        auto resp = http::HTTPResponse()
            .ok(R"({"status":"online","fps_limit":)" + 
                std::to_string(config.getFpsLimit()) + "}")
            .setContentType("application/json");
        return addCorsHeaders(resp);
    }
    
    // Default: Serve static files
    auto resp = serveStaticFile(request);
    return addCorsHeaders(resp);
}

http::HTTPResponse Server::handleStream(const http::HTTPRequest&) {
    // NOTE: This is a special case - we need to send headers first, then stream
    // For simplicity, we'll return an error here and handle streaming separately
    // In production, this would be handled with a custom response type
    
    auto& config = utils::Config::instance();
    int fps_limit = config.getFpsLimit();
    int scale_factor = config.getScaleFactor();
    
    utils::ScreenCapturer capturer;
    
    // MJPEG stream format
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
    response += "Cache-Control: no-cache\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    
    // Stream frames (this is a simplified example)
    // In production, this would need proper connection management
    int frame_delay_ms = 1000 / fps_limit;
    
    for (int i = 0; i < 100 && running_; ++i) {  // Limit for demo
        auto frame = capturer.captureFrame(scale_factor);
        if (!frame) {
            break;
        }
        
        auto bmp_data = utils::ScreenCapturer::frameToBMP(*frame);
        
        response += "--frame\r\n";
        response += "Content-Type: image/bmp\r\n";
        response += std::format("Content-Length: {}\r\n", bmp_data.size());
        response += "\r\n";
        response.append(reinterpret_cast<char*>(bmp_data.data()), bmp_data.size());
        response += "\r\n";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
    }
    
    // This is a hack - we return the full stream as body
    // In production, use chunked transfer or custom socket handling
    http::HTTPResponse resp;
    resp.setStatus(200, "OK");
    resp.setBody(response);
    return resp;
}

http::HTTPResponse Server::handleInputControl(const http::HTTPRequest& request) {
    // Parse JSON body: {"type":"move", "x":0.5, "y":0.5}
    std::string_view body = request.getBody();
    
    // Simple JSON parser (production would use a proper library)
    auto findValue = [&](std::string_view key) -> std::string {
        std::string search = "\"" + std::string(key) + "\":";
        size_t pos = body.find(search);
        if (pos == std::string_view::npos) return "";
        
        pos += search.length();
        while (pos < body.size() && std::isspace(body[pos])) ++pos;
        
        if (pos >= body.size()) return "";
        
        if (body[pos] == '"') {
            // String value
            size_t start = ++pos;
            size_t end = body.find('"', start);
            if (end == std::string_view::npos) return "";
            return std::string(body.substr(start, end - start));
        } else {
            // Numeric value
            size_t start = pos;
            while (pos < body.size() && (std::isdigit(body[pos]) || body[pos] == '.' || body[pos] == '-')) {
                ++pos;
            }
            return std::string(body.substr(start, pos - start));
        }
    };
    
    std::string type = findValue("type");
    utils::InputInjector injector;
    
    bool success = false;
    
    if (type == "move") {
        double x = std::stod(findValue("x"));
        double y = std::stod(findValue("y"));
        success = injector.moveMouse(x, y);
    } else if (type == "click") {
        success = injector.clickLeft();
    } else if (type == "rightclick") {
        success = injector.clickRight();
    } else if (type == "key") {
        int keycode = std::stoi(findValue("key"));
        success = injector.typeKey(static_cast<uint16_t>(keycode));
    }
    
    std::string response_body = success 
        ? R"({"status":"success"})" 
        : R"({"status":"error"})";
    
    return http::HTTPResponse()
        .ok(response_body)
        .setContentType("application/json");
}

http::HTTPResponse Server::handleFileUpload(const http::HTTPRequest& request) {
    auto& config = utils::Config::instance();
    
    // Get Content-Type header
    auto content_type_header = request.getHeader("Content-Type");
    if (!content_type_header) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Missing Content-Type"})");
    }
    
    // Extract boundary from Content-Type
    std::string_view content_type = *content_type_header;
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string_view::npos) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Missing boundary"})");
    }
    
    std::string boundary(content_type.substr(boundary_pos + 9));
    
    // Parse multipart data
    http::MultipartParser parser;
    if (!parser.parse(request.getBody(), boundary)) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Failed to parse multipart data"})");
    }
    
    // Get all file parts
    auto files = parser.getFileParts();
    if (files.empty()) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"No files found"})");
    }
    
    // Create upload directory if needed
    std::filesystem::path upload_dir = config.getUploadDir();
    if (!std::filesystem::exists(upload_dir)) {
        std::filesystem::create_directories(upload_dir);
    }
    
    // Save files
    std::vector<std::string> saved_files;
    for (const auto& file : files) {
        if (file.data.size() > config.getMaxUploadSize()) {
            utils::logWarn(std::format("File {} too large, skipping", file.filename));
            continue;
        }
        
        std::filesystem::path file_path = upload_dir / file.filename;
        
        std::ofstream out(file_path, std::ios::binary);
        if (out) {
            out.write(reinterpret_cast<const char*>(file.data.data()), file.data.size());
            saved_files.push_back(file.filename);
            utils::logInfo(std::format("Saved file: {}", file_path.string()));
        }
    }
    
    std::string response = std::format(
        R"({{"status":"success","uploaded":{},"files":["{}"]}})",
        saved_files.size(),
        saved_files.empty() ? "" : saved_files[0]
    );
    
    return http::HTTPResponse()
        .ok(response)
        .setContentType("application/json");
}

http::HTTPResponse Server::serveStaticFile(const http::HTTPRequest& request) {
    if (request.getMethod() != http::Method::GET && 
        request.getMethod() != http::Method::HEAD) {
        return http::HTTPResponse()
            .setStatus(405, "Method Not Allowed")
            .setBody("<h1>405 - Method Not Allowed</h1>")
            .setContentType("text/html");
    }
    
    std::string requested_path(request.getPath());
    
    if (requested_path.ends_with('/')) {
        requested_path += default_file_;
    }
    
    auto safe_path = utils::FileSystemUtils::securePath(document_root_, requested_path);
    
    if (!safe_path) {
        utils::logWarn(std::format("Path traversal attempt: {}", requested_path));
        return http::HTTPResponse().forbidden(
            "<h1>403 - Forbidden</h1><p>Path traversal detected.</p>"
        );
    }
    
    if (!std::filesystem::exists(*safe_path)) {
        return http::HTTPResponse().notFound();
    }
    
    if (!std::filesystem::is_regular_file(*safe_path)) {
        return http::HTTPResponse().forbidden(
            "<h1>403 - Forbidden</h1><p>Not a regular file.</p>"
        );
    }
    
    auto content = utils::FileSystemUtils::readFile(*safe_path);
    
    if (!content) {
        utils::logError(std::format("Failed to read file: {}", safe_path->string()));
        return http::HTTPResponse().internalError();
    }
    
    auto mime_type = http::MimeTypes::fromPath(*safe_path);
    
    return http::HTTPResponse()
        .ok(std::move(*content))
        .setContentType(mime_type);
}

} // namespace frqs::core