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
#include <charconv>

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
        // Pre-allocate buffer untuk performa
        static thread_local std::vector<char> buffer(16384);
        buffer.resize(16384);
        
        size_t received = client.receive(buffer.data(), buffer.size());
        
        if (received == 0) {
            return;
        }
        
        http::HTTPRequest request;
        std::string_view raw_request(buffer.data(), received);
        
        if (!request.parse(raw_request)) {
            utils::logWarn(std::format("Invalid request from {}: {}", 
                                      client_addr.toString(), 
                                      request.getError()));
            
            auto response = http::HTTPResponse().badRequest();
            client.send(response.build());
            return;
        }
        
        // Special handling untuk streaming endpoint
        if (std::string(request.getPath()) == "/stream" && 
            request.getMethod() == http::Method::GET) {
            handleStreamDirect(std::move(client), request);
            return;
        }
        
        auto response = handleRequest(request);
        client.send(response.build());
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Error handling client {}: {}", 
                                   client_addr.toString(), 
                                   e.what()));
    }
}

void Server::handleStreamDirect(net::Socket client, const http::HTTPRequest& request) {
    auto& config = utils::Config::instance();
    
    // Authentication
    bool authenticated = false;
    auto auth_header = request.getHeader("Authorization");
    std::string expected = "Bearer " + config.getAuthToken();
    if (auth_header && *auth_header == expected) {
        authenticated = true;
    }
    
    if (!authenticated) {
        auto token_param = request.getQueryParam("token");
        if (token_param && *token_param == config.getAuthToken()) {
            authenticated = true;
        }
    }
    
    if (!authenticated) {
        utils::logWarn("Streaming request rejected: Invalid auth");
        auto response = http::HTTPResponse()
            .setStatus(401, "Unauthorized")
            .setHeader("Access-Control-Allow-Origin", "*")
            .setBody("Invalid or missing auth token");
        client.send(response.build());
        return;
    }
    
    // ========== OPTIMIZED STREAMING CONFIGURATION ==========
    int fps_limit = config.getFpsLimit();
    int scale_factor = config.getScaleFactor();
    int frame_delay_ms = 1000 / fps_limit;
    
    // Get quality from query param or use config default
    int jpeg_quality = 75;  // Default: balanced
    auto quality_param = request.getQueryParam("quality");
    if (quality_param) {
        try {
            jpeg_quality = std::stoi(std::string(*quality_param));
            jpeg_quality = std::clamp(jpeg_quality, 50, 95);
        } catch (...) {}
    }
    
    utils::ScreenCapturer capturer;
    capturer.setMotionThreshold(10);  // Pixel difference threshold
    
    // Set quality based on scale for bandwidth optimization
    if (scale_factor <= 1) {
        capturer.setQuality(utils::ScreenCapturer::Quality::HIGH);
    } else if (scale_factor == 2) {
        capturer.setQuality(utils::ScreenCapturer::Quality::MEDIUM);
    } else {
        capturer.setQuality(utils::ScreenCapturer::Quality::LOW);
    }
    
    utils::logInfo(std::format(
        "Optimized stream started: {}x scale, {}% quality, {} FPS, frame-diff enabled",
        scale_factor, jpeg_quality, fps_limit
    ));
    
    // Send MJPEG headers with JPEG content type
    std::string headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "X-Stream-Optimized: true\r\n"
        "X-Compression: jpeg\r\n"
        "Connection: close\r\n\r\n";
    
    try {
        client.send(headers);
        
        // Statistics
        uint64_t frames_sent = 0;
        uint64_t frames_skipped = 0;
        uint64_t total_bytes = 0;
        auto stream_start = std::chrono::steady_clock::now();
        auto last_stats_log = stream_start;
        
        // Stream loop with frame differencing
        while (running_) {
            auto start_time = std::chrono::steady_clock::now();
            
            // ========== FRAME DIFF: Only capture if changed ==========
            auto frame = capturer.captureFrame(scale_factor, false);
            
            if (!frame) {
                // No significant changes detected
                frames_skipped++;
                
                // Still respect frame rate for consistency
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto sleep_time = std::chrono::milliseconds(frame_delay_ms) - elapsed;
                if (sleep_time.count() > 0) {
                    std::this_thread::sleep_for(sleep_time);
                }
                continue;
            }
            
            // ========== JPEG COMPRESSION ==========
            auto jpeg_data = utils::ScreenCapturer::frameToJPEG(*frame, jpeg_quality);
            
            if (jpeg_data.empty()) {
                utils::logError("JPEG compression failed");
                break;
            }
            
            // Send frame with boundary
            std::string frame_header = 
                "--frame\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: " + std::to_string(jpeg_data.size()) + "\r\n"
                "X-Frame-Width: " + std::to_string(frame->width) + "\r\n"
                "X-Frame-Height: " + std::to_string(frame->height) + "\r\n"
                "\r\n";
            
            client.send(frame_header);
            client.send(jpeg_data.data(), jpeg_data.size());
            client.send("\r\n");
            
            frames_sent++;
            total_bytes += jpeg_data.size();
            
            // Log statistics every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_log).count() >= 5) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stream_start).count();
                double fps = static_cast<double>(frames_sent) / elapsed;
                double avg_frame_kb = static_cast<double>(total_bytes) / frames_sent / 1024.0;
                double bandwidth_kbps = static_cast<double>(total_bytes * 8) / elapsed / 1000.0;
                double skip_rate = static_cast<double>(frames_skipped) / (frames_sent + frames_skipped) * 100.0;
                
                utils::logInfo(std::format(
                    "Stream stats: {:.1f} FPS | {:.1f} KB/frame | {:.1f} Kbps | {:.1f}% skipped",
                    fps, avg_frame_kb, bandwidth_kbps, skip_rate
                ));
                
                last_stats_log = now;
            }
            
            // Frame rate limiting
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto sleep_time = std::chrono::milliseconds(frame_delay_ms) - elapsed;
            if (sleep_time.count() > 0) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
        
        // Final statistics
        auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stream_start
        ).count();
        
        utils::logInfo(std::format(
            "Stream ended: {} frames sent, {} skipped, {} KB total, {} seconds",
            frames_sent, frames_skipped, total_bytes / 1024, total_time
        ));
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Streaming error: {}", e.what()));
    }
}

// OPTIMIZED: Fast JSON value extraction
static std::string_view fastJsonGet(std::string_view json, std::string_view key) {
    std::string search = "\"";
    search += key;
    search += "\":";
    
    size_t pos = json.find(search);
    if (pos == std::string_view::npos) return "";
    
    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    
    if (pos >= json.size()) return "";
    
    if (json[pos] == '"') {
        size_t start = ++pos;
        size_t end = json.find('"', start);
        if (end == std::string_view::npos) return "";
        return json.substr(start, end - start);
    } else {
        size_t start = pos;
        while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
            ++pos;
        }
        return json.substr(start, pos - start);
    }
}

http::HTTPResponse Server::handleRequest(const http::HTTPRequest& request) {
    auto& config = utils::Config::instance();
    std::string path(request.getPath());
    
    // CORS preflight
    if (request.getMethod() == http::Method::OPTIONS) {
        return http::HTTPResponse()
            .setStatus(204, "No Content")
            .setHeader("Access-Control-Allow-Origin", "*")
            .setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            .setHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    }
    
    // Helper function untuk add CORS headers
    auto addCors = [](http::HTTPResponse resp) -> http::HTTPResponse {
        return resp.setHeader("Access-Control-Allow-Origin", "*");
    };
    
    // Auth check untuk protected routes
    if (path.starts_with("/api/") || path == "/upload") {
        auto auth_header = request.getHeader("Authorization");
        if (!auth_header) {
            return addCors(http::HTTPResponse()
                .setStatus(401, "Unauthorized")
                .setBody(R"({"error":"Unauthorized"})"));
        }
        
        std::string expected = "Bearer " + config.getAuthToken();
        if (*auth_header != expected) {
            return addCors(http::HTTPResponse()
                .setStatus(401, "Unauthorized")
                .setBody(R"({"error":"Invalid token"})"));
        }
    }
    
    // Route: POST /api/input - Remote Control
    if (path == "/api/input" && request.getMethod() == http::Method::POST) {
        return addCors(handleInputControl(request));
    }
    
    // Route: POST /upload - File Upload
    if (path == "/upload" && request.getMethod() == http::Method::POST) {
        return addCors(handleFileUpload(request));
    }
    
    // Route: GET /api/status
    if (path == "/api/status" && request.getMethod() == http::Method::GET) {
        return addCors(http::HTTPResponse()
            .ok(R"({"status":"online","fps_limit":)" + 
                std::to_string(config.getFpsLimit()) + "}")
            .setContentType("application/json"));
    }
    
    // Default: Static files
    return addCors(serveStaticFile(request));
}

http::HTTPResponse Server::handleInputControl(const http::HTTPRequest& request) {
    std::string_view body = request.getBody();
    
    auto type = fastJsonGet(body, "type");
    utils::InputInjector injector;
    
    bool success = false;
    
    if (type == "move") {
        auto x_str = fastJsonGet(body, "x");
        auto y_str = fastJsonGet(body, "y");
        
        double x = 0.0, y = 0.0;
        std::from_chars(x_str.data(), x_str.data() + x_str.size(), x);
        std::from_chars(y_str.data(), y_str.data() + y_str.size(), y);
        
        success = injector.moveMouse(x, y);
    } 
    else if (type == "click") {
        success = injector.clickLeft();
    } 
    else if (type == "rightclick") {
        success = injector.clickRight();
    } 
    else if (type == "key") {
        auto key_str = fastJsonGet(body, "key");
        int keycode = 0;
        std::from_chars(key_str.data(), key_str.data() + key_str.size(), keycode);
        success = injector.typeKey(static_cast<uint16_t>(keycode));
    }
    
    return http::HTTPResponse()
        .ok(success ? R"({"status":"success"})" : R"({"status":"error"})")
        .setContentType("application/json");
}

http::HTTPResponse Server::handleFileUpload(const http::HTTPRequest& request) {
    auto& config = utils::Config::instance();
    
    auto content_type_header = request.getHeader("Content-Type");
    if (!content_type_header) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Missing Content-Type"})");
    }
    
    std::string_view content_type = *content_type_header;
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string_view::npos) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Missing boundary"})");
    }
    
    std::string boundary(content_type.substr(boundary_pos + 9));
    
    http::MultipartParser parser;
    if (!parser.parse(request.getBody(), boundary)) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"Failed to parse multipart data"})");
    }
    
    auto files = parser.getFileParts();
    if (files.empty()) {
        return http::HTTPResponse()
            .badRequest(R"({"error":"No files found"})");
    }
    
    std::filesystem::path upload_dir = config.getUploadDir();
    if (!std::filesystem::exists(upload_dir)) {
        std::filesystem::create_directories(upload_dir);
    }
    
    std::vector<std::string> saved_files;
    for (const auto& file : files) {
        if (file.data.size() > config.getMaxUploadSize()) {
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
        R"({{"status":"success","uploaded":{}}})",
        saved_files.size()
    );
    
    return http::HTTPResponse()
        .ok(response)
        .setContentType("application/json");
}

http::HTTPResponse Server::serveStaticFile(const http::HTTPRequest& request) {
    if (request.getMethod() != http::Method::GET && 
        request.getMethod() != http::Method::HEAD) {
        return http::HTTPResponse()
            .setStatus(405, "Method Not Allowed");
    }
    
    std::string requested_path(request.getPath());
    
    if (requested_path.ends_with('/')) {
        requested_path += default_file_;
    }
    
    auto safe_path = utils::FileSystemUtils::securePath(document_root_, requested_path);
    
    if (!safe_path) {
        utils::logWarn(std::format("Path traversal attempt: {}", requested_path));
        return http::HTTPResponse().forbidden();
    }
    
    if (!std::filesystem::exists(*safe_path)) {
        return http::HTTPResponse().notFound();
    }
    
    if (!std::filesystem::is_regular_file(*safe_path)) {
        return http::HTTPResponse().forbidden();
    }
    
    auto content = utils::FileSystemUtils::readFile(*safe_path);
    
    if (!content) {
        return http::HTTPResponse().internalError();
    }
    
    auto mime_type = http::MimeTypes::fromPath(*safe_path);
    
    return http::HTTPResponse()
        .ok(std::move(*content))
        .setContentType(mime_type);
}

} // namespace frqs::core