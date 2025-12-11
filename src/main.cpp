/**
 * @file src/main.cpp
 * @brief Main server entry point with FRQS branding
 * @version 2.0.0
 * @copyright Copyright (c) 2025
 */

#include "frqs-net.hpp"
#include "plugin/static_files.hpp"
#include "utils/config.hpp"
#include <iostream>
#include <csignal>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
void SetupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}
#else
void SetupConsole() {}
#endif

namespace {
    frqs::core::Server* g_server = nullptr;

    void signalHandler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            frqs::utils::logInfo("\n🛑 Shutdown signal received...");
            if (g_server) {
                g_server->stop();
            }
        }
    }
    
    // FRQS Brand Landing Page (Default)
    constexpr const char* FRQS_LANDING_PAGE = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FRQS Network - High-Performance C++23 Web Server</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            overflow-x: hidden;
        }
        
        .container {
            max-width: 1200px;
            padding: 40px 20px;
            text-align: center;
        }
        
        .logo {
            font-size: 4em;
            font-weight: 900;
            letter-spacing: -2px;
            margin-bottom: 20px;
            animation: fadeInDown 0.8s ease-out;
            background: linear-gradient(135deg, #fff 0%, #e0e0e0 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        
        .tagline {
            font-size: 1.5em;
            margin-bottom: 40px;
            opacity: 0.95;
            animation: fadeInUp 0.8s ease-out 0.2s both;
        }
        
        .features {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 30px;
            margin: 60px 0;
            animation: fadeIn 1s ease-out 0.4s both;
        }
        
        .feature {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            padding: 30px;
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            transition: transform 0.3s, box-shadow 0.3s;
        }
        
        .feature:hover {
            transform: translateY(-10px);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.2);
        }
        
        .feature-icon {
            font-size: 3em;
            margin-bottom: 15px;
        }
        
        .feature-title {
            font-size: 1.3em;
            font-weight: 600;
            margin-bottom: 10px;
        }
        
        .feature-desc {
            opacity: 0.9;
            line-height: 1.6;
        }
        
        .stats {
            display: flex;
            justify-content: center;
            gap: 60px;
            margin: 60px 0;
            flex-wrap: wrap;
            animation: fadeIn 1s ease-out 0.6s both;
        }
        
        .stat {
            text-align: center;
        }
        
        .stat-value {
            font-size: 3em;
            font-weight: 900;
            display: block;
            margin-bottom: 5px;
        }
        
        .stat-label {
            opacity: 0.9;
            font-size: 0.9em;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        
        .cta {
            margin-top: 60px;
            animation: fadeIn 1s ease-out 0.8s both;
        }
        
        .cta-button {
            display: inline-block;
            padding: 15px 40px;
            background: white;
            color: #667eea;
            text-decoration: none;
            border-radius: 50px;
            font-weight: 600;
            font-size: 1.1em;
            transition: transform 0.3s, box-shadow 0.3s;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
        }
        
        .cta-button:hover {
            transform: translateY(-3px);
            box-shadow: 0 15px 40px rgba(0, 0, 0, 0.3);
        }
        
        .footer {
            margin-top: 80px;
            opacity: 0.8;
            font-size: 0.9em;
        }
        
        @keyframes fadeInDown {
            from {
                opacity: 0;
                transform: translateY(-30px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        
        @keyframes fadeInUp {
            from {
                opacity: 0;
                transform: translateY(30px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        
        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }
        
        @media (max-width: 768px) {
            .logo { font-size: 2.5em; }
            .tagline { font-size: 1.2em; }
            .stats { gap: 30px; }
            .stat-value { font-size: 2em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">FRQS NETWORK</div>
        <div class="tagline">High-Performance C++23 Web Server</div>
        
        <div class="features">
            <div class="feature">
                <div class="feature-icon">⚡</div>
                <div class="feature-title">Ultra Fast</div>
                <div class="feature-desc">Zero-copy parsing and optimized thread pool for maximum throughput</div>
            </div>
            
            <div class="feature">
                <div class="feature-icon">🔒</div>
                <div class="feature-title">Secure by Default</div>
                <div class="feature-desc">Path traversal protection, request size limits, and comprehensive security features</div>
            </div>
            
            <div class="feature">
                <div class="feature-icon">🧩</div>
                <div class="feature-title">Modular</div>
                <div class="feature-desc">Plugin-based architecture for maximum flexibility and extensibility</div>
            </div>
            
            <div class="feature">
                <div class="feature-icon">🚀</div>
                <div class="feature-title">Modern C++23</div>
                <div class="feature-desc">Built with cutting-edge C++23 features: concepts, ranges, coroutines</div>
            </div>
        </div>
        
        <div class="stats">
            <div class="stat">
                <span class="stat-value">100K+</span>
                <span class="stat-label">Requests/sec</span>
            </div>
            
            <div class="stat">
                <span class="stat-value">&lt;1ms</span>
                <span class="stat-label">Latency p50</span>
            </div>
            
            <div class="stat">
                <span class="stat-value">2KB</span>
                <span class="stat-label">Memory/conn</span>
            </div>
        </div>
        
        <div class="cta">
            <a href="https://github.com/zuudevs/frqs-network" class="cta-button">
                View Documentation →
            </a>
        </div>
        
        <div class="footer">
            <p>FRQS Network v2.0.0 • Built with ❤️ by zuudevs</p>
            <p style="margin-top: 10px; opacity: 0.7;">
                Server running on <span id="hostname"></span>
            </p>
        </div>
    </div>
    
    <script>
        document.getElementById('hostname').textContent = window.location.host;
    </script>
</body>
</html>)HTML";
    
    void createDefaultConfig(const std::filesystem::path& config_path) {
        std::ofstream config(config_path);
        config << "# FRQS Network Configuration v2.0\n"
               << "# General-Purpose Web Server\n\n"
               << "# Server Settings\n"
               << "PORT=8080\n"
               << "DOC_ROOT=public\n"
               << "THREAD_COUNT=4\n\n"
               << "# Security (if using auth plugin)\n"
               << "AUTH_TOKEN=change_this_secure_token\n\n";
        
        frqs::utils::logInfo("✅ Created default config: frqs.conf");
    }
    
    void createDefaultLandingPage(const std::filesystem::path& doc_root) {
        std::filesystem::create_directories(doc_root);
        
        std::ofstream html(doc_root / "index.html");
        html << FRQS_LANDING_PAGE;
        
        frqs::utils::logInfo("✅ Created FRQS landing page: public/index.html");
    }
}

int main(int argc, char* argv[]) {
    using namespace frqs;
    SetupConsole();
    
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   ███████╗██████╗  ██████╗ ███████╗    ███╗   ██╗███████╗║
║   ██╔════╝██╔══██╗██╔═══██╗██╔════╝    ████╗  ██║██╔════╝║
║   █████╗  ██████╔╝██║   ██║███████╗    ██╔██╗ ██║█████╗  ║
║   ██╔══╝  ██╔══██╗██║▄▄ ██║╚════██║    ██║╚██╗██║██╔══╝  ║
║   ██║     ██║  ██║╚██████╔╝███████║    ██║ ╚████║███████╗║
║   ╚═╝     ╚═╝  ╚═╝ ╚══▀▀═╝ ╚══════╝    ╚═╝  ╚═══╝╚══════╝║
║                                                           ║
║        High-Performance C++23 Web Server v2.0            ║
║              General-Purpose & Modular                   ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;
    
    try {
        // Setup logging
        utils::enableFileLogging("frqs_server.log");
        
        utils::logInfo("🚀 Starting FRQS Network Server v2.0...");
        utils::logInfo("📅 " + std::string(__DATE__) + " " + std::string(__TIME__));
        
        // Load config
        auto& config = utils::Config::instance();
        std::filesystem::path config_file = "frqs.conf";
        
        if (argc > 1) {
            config_file = argv[1];
        }
        
        if (!std::filesystem::exists(config_file)) {
            utils::logInfo("📝 Config file not found, creating default...");
            createDefaultConfig(config_file);
        }
        
        if (!config.load(config_file)) {
            utils::logWarn("⚠️  Could not load config, using defaults");
        } else {
            utils::logInfo("✅ Configuration loaded from: " + config_file.string());
        }
        
        // Setup directories
        std::filesystem::path doc_root = config.getDocRoot();
        
        // Create FRQS landing page if no index.html exists
        if (!std::filesystem::exists(doc_root / "index.html")) {
            utils::logInfo("📄 Creating default FRQS landing page...");
            createDefaultLandingPage(doc_root);
        }
        
        // Server configuration
        uint16_t port = config.getPort();
        size_t threads = static_cast<size_t>(config.getThreadCount());
        
        // Display configuration
        std::cout << "\n┌─────────────────────────────────────┐" << std::endl;
        std::cout << "│  Server Configuration               │" << std::endl;
        std::cout << "├─────────────────────────────────────┤" << std::endl;
        std::cout << "│  Version:       2.0.0               │" << std::endl;
        std::cout << "│  Port:          " << port << std::string(20 - std::to_string(port).length(), ' ') << "│" << std::endl;
        std::cout << "│  Threads:       " << threads << std::string(20 - std::to_string(threads).length(), ' ') << "│" << std::endl;
        std::cout << "│  Doc Root:      " << doc_root.string().substr(0, 17) << std::string(20 - std::min(17, (int)doc_root.string().length()), ' ') << "│" << std::endl;
        std::cout << "└─────────────────────────────────────┘\n" << std::endl;
        
        // Create server
        core::Server server(port, threads);
        g_server = &server;
        
        // ========== ADD PLUGINS ==========
        
        // Static files plugin
       plugins::StaticFilesConfig static_config;
        static_config.root = doc_root;
        static_config.mount_path = "/";
        static_config.default_file = "index.html";
        static_config.cache_control = "public, max-age=3600";

        server.addPlugin(std::make_unique<plugins::StaticFilesPlugin>(std::move(static_config)));
        
        // ========== ADD CUSTOM ROUTES ==========
        
        // API: Health check
        server.router().get("/api/health", [](auto& ctx) {
            ctx.json(R"({"status":"healthy","version":"2.0.0"})");
        });
        
        // API: Server info
        server.router().get("/api/info", [&server](auto& ctx) {
            auto info = std::format(
                R"({{"server":"FRQS Network","version":"2.0.0","port":{},"connections":{},"requests":{}}})",
                server.getPort(),
                server.activeConnections(),
                server.totalRequests()
            );
            ctx.json(info);
        });
        
        // ========== ADD MIDDLEWARE ==========
        
        // Logging middleware
        server.use([](auto& ctx, auto next) {
            auto start = std::chrono::steady_clock::now();
            
            next();
            
            auto duration = std::chrono::steady_clock::now() - start;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            
            utils::logInfo(std::format("{} {} - {} - {}ms", 
                http::methodToString(ctx.request().getMethod()),
                ctx.request().getPath(),
                ctx.response().getStatus(),
                ms
            ));
        });
        
        // CORS middleware (if needed)
        server.use([](auto& ctx, auto next) {
            ctx.response().setHeader("Access-Control-Allow-Origin", "*");
            ctx.response().setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            ctx.response().setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            
            // Handle preflight
            if (ctx.request().getMethod() == http::Method::OPTIONS) {
                ctx.status(204).body("");
                return;
            }
            
            next();
        });
        
        // Signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Display access info
        std::cout << "┌─────────────────────────────────────┐" << std::endl;
        std::cout << "│  🌐 Server is running!              │" << std::endl;
        std::cout << "├─────────────────────────────────────┤" << std::endl;
        std::cout << "│                                     │" << std::endl;
        std::cout << "│  Local:   http://localhost:" << port << "     │" << std::endl;
        std::cout << "│  Network: http://[YOUR_IP]:" << port << "  │" << std::endl;
        std::cout << "│                                     │" << std::endl;
        std::cout << "│  Routes:                            │" << std::endl;
        std::cout << "│  • GET  /                           │" << std::endl;
        std::cout << "│  • GET  /api/health                 │" << std::endl;
        std::cout << "│  • GET  /api/info                   │" << std::endl;
        std::cout << "│                                     │" << std::endl;
        std::cout << "│  Press Ctrl+C to stop               │" << std::endl;
        std::cout << "└─────────────────────────────────────┘\n" << std::endl;
        
        utils::logInfo("🎯 Server ready - accepting connections");
        
        // Start server (blocks)
        server.start();
        
        utils::logInfo("👋 Server shutdown complete");
        std::cout << "\n✅ Goodbye!\n" << std::endl;
        
    } catch (const std::exception& e) {
        utils::logError("❌ Fatal error: " + std::string(e.what()));
        std::cerr << "\n❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}