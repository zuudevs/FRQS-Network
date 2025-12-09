#include "frqs-net.hpp"
#include "utils/config.hpp"
#include <iostream>
#include <csignal>
#include <fstream>

#undef min

void SetupConsole() {
    // 1. Set Output Code Page ke UTF-8
    if (!SetConsoleOutputCP(CP_UTF8)) {
        std::cerr << "Warning: Gagal set output CP ke UTF-8!" << std::endl;
    }

    // 2. Set Input Code Page ke UTF-8
    if (!SetConsoleCP(CP_UTF8)) {
        std::cerr << "Warning: Gagal set input CP ke UTF-8!" << std::endl;
    }

    // 3. Enable Virtual Terminal Processing
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        std::cerr << "Warning: Gagal enable Virtual Terminal Processing!" << std::endl;
    }
}

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
    
    void createDefaultConfig(const std::filesystem::path& config_path) {
        std::ofstream config(config_path);
        config << "# FRQS Network Configuration\n"
               << "# Lab Monitoring & Screen Sharing Server\n\n"
               << "# Server Settings\n"
               << "PORT=8080\n"
               << "DOC_ROOT=public\n"
               << "THREAD_COUNT=4\n\n"
               << "# Security\n"
               << "AUTH_TOKEN=secure_lab_token_2025\n\n"
               << "# Screen Capture Settings\n"
               << "FPS_LIMIT=15\n"
               << "SCALE_FACTOR=2\n\n"
               << "# Upload Settings\n"
               << "UPLOAD_DIR=uploads\n"
               << "MAX_UPLOAD_SIZE=52428800\n";  // 50MB
        
        frqs::utils::logInfo("✅ Created default config: frqs.conf");
    }
    
    void createViewerHTML(const std::filesystem::path& doc_root) {
        std::filesystem::create_directories(doc_root);
        
        std::ofstream html(doc_root / "index.html");
        html << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FRQS Screen Share - Remote Desktop</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', system-ui, sans-serif;
            background: #0a0e27;
            color: #e0e0e0;
            overflow: hidden;
        }
        .container {
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 15px 30px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        }
        .header h1 {
            font-size: 1.5em;
            font-weight: 600;
        }
        .status {
            display: flex;
            align-items: center;
            gap: 10px;
            background: rgba(255,255,255,0.1);
            padding: 8px 16px;
            border-radius: 20px;
            backdrop-filter: blur(10px);
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #4ade80;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .main-content {
            flex: 1;
            display: flex;
            gap: 20px;
            padding: 20px;
            overflow: hidden;
        }
        .screen-container {
            flex: 1;
            background: #1a1f3a;
            border-radius: 12px;
            overflow: hidden;
            position: relative;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4);
        }
        .screen-container img {
            width: 100%;
            height: 100%;
            object-fit: contain;
            cursor: crosshair;
        }
        .loading {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            text-align: center;
        }
        .spinner {
            border: 4px solid rgba(255,255,255,0.1);
            border-top: 4px solid #667eea;
            border-radius: 50%;
            width: 50px;
            height: 50px;
            animation: spin 1s linear infinite;
            margin: 0 auto 15px;
        }
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        .controls {
            width: 300px;
            background: #1a1f3a;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4);
            overflow-y: auto;
        }
        .controls h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.2em;
        }
        .control-section {
            background: rgba(255,255,255,0.05);
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 15px;
        }
        .control-section h3 {
            font-size: 0.9em;
            color: #a0a0a0;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        button {
            width: 100%;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
            margin-bottom: 8px;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
        }
        button:active {
            transform: translateY(0);
        }
        .btn-danger {
            background: linear-gradient(135deg, #f43f5e 0%, #dc2626 100%);
        }
        input[type="text"] {
            width: 100%;
            background: rgba(255,255,255,0.1);
            border: 1px solid rgba(255,255,255,0.2);
            color: white;
            padding: 10px;
            border-radius: 6px;
            margin-bottom: 10px;
            font-size: 14px;
        }
        input[type="text"]:focus {
            outline: none;
            border-color: #667eea;
        }
        .stats {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 15px;
        }
        .stat-box {
            background: rgba(102, 126, 234, 0.1);
            padding: 10px;
            border-radius: 6px;
            text-align: center;
        }
        .stat-value {
            font-size: 1.5em;
            font-weight: bold;
            color: #667eea;
        }
        .stat-label {
            font-size: 0.8em;
            color: #a0a0a0;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🖥️ FRQS Screen Share</h1>
            <div class="status">
                <div class="status-dot" id="statusDot"></div>
                <span id="statusText">Connected</span>
            </div>
        </div>
        
        <div class="main-content">
            <div class="screen-container">
                <div class="loading" id="loading">
                    <div class="spinner"></div>
                    <div>Connecting to remote screen...</div>
                </div>
                <img id="screenStream" style="display:none;" alt="Remote Screen">
            </div>
            
            <div class="controls">
                <h2>🎮 Remote Control</h2>
                
                <div class="control-section">
                    <h3>Connection</h3>
                    <input type="text" id="authToken" placeholder="Auth Token" value="secure_lab_token_2025">
                    <button onclick="connectStream()">🔗 Connect</button>
                    <button onclick="disconnectStream()" class="btn-danger">❌ Disconnect</button>
                </div>
                
                <div class="control-section">
                    <h3>Mouse Control</h3>
                    <p style="font-size: 0.85em; color: #a0a0a0; margin-bottom: 10px;">
                        Click on the screen to control mouse
                    </p>
                    <button onclick="sendClick('click')">🖱️ Left Click</button>
                    <button onclick="sendClick('rightclick')">🖱️ Right Click</button>
                </div>
                
                <div class="control-section">
                    <h3>Keyboard Shortcuts</h3>
                    <button onclick="sendKey(13)">⏎ Enter</button>
                    <button onclick="sendKey(27)">⎋ Escape</button>
                    <button onclick="sendKey(32)">␣ Space</button>
                </div>
                
                <div class="stats">
                    <div class="stat-box">
                        <div class="stat-value" id="fpsCounter">0</div>
                        <div class="stat-label">FPS</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-value" id="latency">0ms</div>
                        <div class="stat-label">Latency</div>
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        const SERVER_URL = window.location.origin;
        let isConnected = false;
        let frameCount = 0;
        let lastFrameTime = Date.now();
        
        const screenImg = document.getElementById('screenStream');
        const loading = document.getElementById('loading');
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        
        function updateStatus(connected) {
            isConnected = connected;
            statusDot.style.background = connected ? '#4ade80' : '#f43f5e';
            statusText.textContent = connected ? 'Connected' : 'Disconnected';
        }
        
        function connectStream() {
            const token = document.getElementById('authToken').value;
            if (!token) {
                alert('Please enter auth token');
                return;
            }
            
            loading.style.display = 'none';
            screenImg.style.display = 'block';
            
            // FIXED: Pass token as query parameter
            const streamUrl = `${SERVER_URL}/stream?token=${encodeURIComponent(token)}`;
            
            screenImg.onload = () => {
                frameCount++;
                const now = Date.now();
                if (now - lastFrameTime >= 1000) {
                    document.getElementById('fpsCounter').textContent = frameCount;
                    frameCount = 0;
                    lastFrameTime = now;
                }
                updateStatus(true);
            };
            
            screenImg.onerror = () => {
                updateStatus(false);
                loading.style.display = 'block';
                screenImg.style.display = 'none';
                alert('Connection failed: Check auth token or server status');
            };
            
            // Add timestamp to prevent caching
            screenImg.src = streamUrl + '&t=' + Date.now();
        }
        
        function disconnectStream() {
            screenImg.src = '';
            screenImg.style.display = 'none';
            loading.style.display = 'block';
            updateStatus(false);
        }
        
        // Click-to-control
        screenImg.addEventListener('click', (e) => {
            if (!isConnected) return;
            
            const rect = screenImg.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            sendInput({ type: 'move', x, y });
            setTimeout(() => sendInput({ type: 'click' }), 50);
        });
        
        async function sendInput(data) {
            const token = document.getElementById('authToken').value;
            const start = Date.now();
            
            try {
                const response = await fetch(`${SERVER_URL}/api/input`, {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${token}`,
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(data)
                });
                
                const latency = Date.now() - start;
                document.getElementById('latency').textContent = `${latency}ms`;
                
                return await response.json();
            } catch (err) {
                console.error('Input failed:', err);
            }
        }
        
        async function sendClick(type) {
            await sendInput({ type });
        }
        
        async function sendKey(keyCode) {
            await sendInput({ type: 'key', key: keyCode });
        }
        
        // Auto-connect on load
        window.onload = () => {
            setTimeout(connectStream, 500);
        };
    </script>
</body>
</html>)HTML";
        
        frqs::utils::logInfo("✅ Created viewer: public/index.html");
    }
}

int main(int argc, char* argv[]) {
    using namespace frqs;
    SetupConsole();
    
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   ███████╗██████╗  ██████╗ ███████╗    ███╗   ██╗███████╗╗
║   ██╔════╝██╔══██╗██╔═══██╗██╔════╝    ████╗  ██║██╔════╝║
║   █████╗  ██████╔╝██║   ██║███████╗    ██╔██╗ ██║█████╗  ║
║   ██╔══╝  ██╔══██╗██║▄▄ ██║╚════██║    ██║╚██╗██║██╔══╝  ║
║   ██║     ██║  ██║╚██████╔╝███████║    ██║ ╚████║███████╗║
║   ╚═╝     ╚═╝  ╚═╝ ╚══▀▀═╝ ╚══════╝    ╚═╝  ╚═══╝╚══════╝║
║                                                           ║
║        High-Performance Screen Sharing Server            ║
║              C++23 | Production Ready                    ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;
    
    try {
        // Setup logging
        utils::enableFileLogging("frqs_server.log");
        
        utils::logInfo("🚀 Starting FRQS Network Server...");
        utils::logInfo("📅 " + std::string(__DATE__) + " " + std::string(__TIME__));
        
        // FIXED: Config file harus di folder BIN (working directory)
        auto& config = utils::Config::instance();
        std::filesystem::path config_file = "frqs.conf";  // Relative to working directory
        
        // Override if provided via command line
        if (argc > 1) {
            config_file = argv[1];
        }
        
        // Create default config if doesn't exist
        if (!std::filesystem::exists(config_file)) {
            utils::logInfo("📝 Config file not found, creating default...");
            createDefaultConfig(config_file);
        }
        
        // Load config
        if (!config.load(config_file)) {
            utils::logWarn("⚠️  Could not load config, using defaults");
        } else {
            utils::logInfo("✅ Configuration loaded from: " + config_file.string());
        }
        
        // Verify AUTH_TOKEN is loaded
        std::string auth_token = config.getAuthToken();
        if (auth_token.empty()) {
            utils::logError("❌ AUTH_TOKEN is empty! Check your frqs.conf file!");
            std::cerr << "\n⚠️  WARNING: No auth token configured!\n";
            std::cerr << "   Please check frqs.conf and ensure AUTH_TOKEN is set.\n" << std::endl;
        }
        
        // Create directories
        std::filesystem::path doc_root = config.getDocRoot();
        std::filesystem::path upload_dir = config.getUploadDir();
        
        if (!std::filesystem::exists(doc_root / "index.html")) {
            utils::logInfo("📄 Creating default viewer page...");
            createViewerHTML(doc_root);
        }
        
        if (!std::filesystem::exists(upload_dir)) {
            std::filesystem::create_directories(upload_dir);
            utils::logInfo("📁 Created upload directory: " + upload_dir.string());
        }
        
        // Server configuration
        uint16_t port = config.getPort();
        size_t threads = static_cast<size_t>(config.getThreadCount());
        
        // Display configuration
        std::cout << "\n┌─────────────────────────────────────┐" << std::endl;
        std::cout << "│  Server Configuration               │" << std::endl;
        std::cout << "├─────────────────────────────────────┤" << std::endl;
        std::cout << "│  Port:          " << port << std::string(20 - std::to_string(port).length(), ' ') << "│" << std::endl;
        std::cout << "│  Threads:       " << threads << std::string(20 - std::to_string(threads).length(), ' ') << "│" << std::endl;
        std::cout << "│  Doc Root:      " << doc_root.string().substr(0, 17) << std::string(20 - std::min(17, (int)doc_root.string().length()), ' ') << "│" << std::endl;
        std::cout << "│  FPS Limit:     " << config.getFpsLimit() << std::string(20 - std::to_string(config.getFpsLimit()).length(), ' ') << "│" << std::endl;
        std::cout << "│  Scale Factor:  " << config.getScaleFactor() << std::string(20 - std::to_string(config.getScaleFactor()).length(), ' ') << "│" << std::endl;
        std::cout << "└─────────────────────────────────────┘\n" << std::endl;
        
        // Create server
        core::Server server(port, threads);
        server.setDocumentRoot(doc_root);
        g_server = &server;
        
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
        std::cout << "│  Auth Token:                        │" << std::endl;
        
        // Show token (truncate if too long)
        std::string token_display = auth_token.length() > 30 
            ? auth_token.substr(0, 27) + "..." 
            : auth_token;
        
        std::cout << "│  " << token_display << std::string(37 - token_display.length(), ' ') << "│" << std::endl;
        std::cout << "│                                     │" << std::endl;
        std::cout << "│  Press Ctrl+C to stop               │" << std::endl;
        std::cout << "└─────────────────────────────────────┘\n" << std::endl;
        
        utils::logInfo("🎯 Accepting connections...");
        
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