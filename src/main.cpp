#include "frqs-net.hpp"
#include <iostream>
#include <fstream>
#include <csignal>

namespace {
    frqs::core::Server* g_server = nullptr ;

    void signalHandler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            frqs::utils::logInfo("Received shutdown signal, stopping server...") ;
            if (g_server) {
                g_server->stop() ;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    using namespace frqs ;
    
    try {
        // Enable file logging
        utils::enableFileLogging("zhttp_server.log") ;
        
        utils::logInfo("=== ZHTTP Server v1.0.0 ===") ;
        utils::logInfo("High-Performance C++23 HTTP Server") ;
        
        // Parse command line arguments
        uint16_t port = 8080 ;
        std::filesystem::path doc_root = "public" ;
        size_t thread_count = std::thread::hardware_concurrency() ;
        
        if (argc > 1) {
            try {
                port = static_cast<uint16_t>(std::stoi(argv[1])) ;
            } catch (...) {
                utils::logWarn("Invalid port specified, using default: 8080") ;
            }
        }
        
        if (argc > 2) {
            doc_root = argv[2] ;
        }
        
        if (argc > 3) {
            try {
                thread_count = std::stoul(argv[3]) ;
                if (thread_count == 0) thread_count = 1 ;
            } catch (...) {
                utils::logWarn("Invalid thread count, using hardware concurrency") ;
            }
        }
        
        // Create document root if it doesn't exist
        if (!std::filesystem::exists(doc_root)) {
            std::filesystem::create_directories(doc_root) ;
            utils::logInfo("Created document root directory: " + doc_root.string()) ;
            
            // Create a default index.html
            std::ofstream index(doc_root / "index.html") ;
            if (index) {
                index << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ZHTTP Server</title>
    <style>
        body {
            font-family: system-ui, -apple-system, sans-serif ;
            max-width: 800px ;
            margin: 50px auto ;
            padding: 20px ;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%) ;
            color: white ;
        }
        h1 { font-size: 3em ; margin-bottom: 0.2em ; }
        p { font-size: 1.2em ; line-height: 1.6 ; }
        .container {
            background: rgba(255, 255, 255, 0.1) ;
            padding: 40px ;
            border-radius: 15px ;
            backdrop-filter: blur(10px) ;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3) ;
        }
        .feature {
            background: rgba(255, 255, 255, 0.15) ;
            padding: 15px ;
            margin: 10px 0 ;
            border-radius: 8px ;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 ZHTTP Server</h1>
        <p>Your high-performance C++23 web server is up and running!</p>
        <div class="feature">✨ Zero-Copy Request Parsing</div>
        <div class="feature">⚡ Multi-threaded Thread Pool</div>
        <div class="feature">🔒 Path Traversal Protection</div>
        <div class="feature">🎯 Modern C++23 Architecture</div>
    </div>
</body>
</html>)" ;
                utils::logInfo("Created default index.html") ;
            }
        }
        
        // Create and configure server
        core::Server server(port, thread_count) ;
        server.setDocumentRoot(doc_root) ;
        
        g_server = &server ;
        
        // Install signal handlers
        std::signal(SIGINT, signalHandler) ;
        std::signal(SIGTERM, signalHandler) ;
        
        // Start server (blocks until stopped)
        server.start() ;
        
        utils::logInfo("Server shutdown complete") ;
        
    } catch (const std::exception& e) {
        utils::logError("Fatal error: " + std::string(e.what())) ;
        return 1 ;
    }
    
    return 0 ;
}