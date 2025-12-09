#include "frqs-net.hpp"
#include "utils/config.hpp"
#include <iostream>
#include <csignal>
#include <fstream>

namespace {
    frqs::core::Server* g_server = nullptr;

    void signalHandler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            frqs::utils::logInfo("Received shutdown signal, stopping server...");
            if (g_server) {
                g_server->stop();
            }
        }
    }
}

int main(int argc, char* argv[]) {
    using namespace frqs;
    
    try {
        utils::enableFileLogging("frqs_server.log");
        
        utils::logInfo("=== FRQS Lab Monitor v2.0.0 ===");
        utils::logInfo("Production-Grade Lab Monitoring System");
        
        // Load configuration
        auto& config = utils::Config::instance();
        std::filesystem::path config_file = "frqs.conf";
        
        if (argc > 1) {
            config_file = argv[1];
        }
        
        if (!config.load(config_file)) {
            utils::logWarn(std::format("Could not load config from {}, using defaults", 
                                       config_file.string()));
        } else {
            utils::logInfo(std::format("Loaded configuration from {}", config_file.string()));
        }
        
        // Create required directories
        std::filesystem::path doc_root = config.getDocRoot();
        std::filesystem::path upload_dir = config.getUploadDir();
        
        if (!std::filesystem::exists(doc_root)) {
            std::filesystem::create_directories(doc_root);
            utils::logInfo(std::format("Created document root: {}", doc_root.string()));
            
            // Create dashboard HTML
            std::ofstream dashboard(doc_root / "dashboard.html");
            if (dashboard) {
                dashboard << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FRQS Lab Monitor - Teacher Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: white;
            padding: 20px;
        }
        h1 { text-align: center; margin-bottom: 30px; font-size: 2.5em; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .student-screen {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 10px;
            padding: 15px;
            backdrop-filter: blur(10px);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
        }
        .student-screen h3 {
            margin-bottom: 10px;
            color: #ffd700;
        }
        .student-screen img {
            width: 100%;
            border-radius: 8px;
            border: 2px solid rgba(255, 255, 255, 0.3);
        }
        .control-panel {
            background: rgba(255, 255, 255, 0.15);
            padding: 20px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
        }
        .control-panel h2 {
            margin-bottom: 15px;
            color: #ffd700;
        }
        button {
            background: #4CAF50;
            color: white;
            border: none;
            padding: 12px 24px;
            font-size: 16px;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
            transition: background 0.3s;
        }
        button:hover { background: #45a049; }
        input[type="file"] {
            padding: 10px;
            background: rgba(255, 255, 255, 0.2);
            border: 1px solid rgba(255, 255, 255, 0.3);
            border-radius: 5px;
            color: white;
            margin: 5px;
        }
        .status {
            position: fixed;
            top: 20px;
            right: 20px;
            background: rgba(76, 175, 80, 0.9);
            padding: 10px 20px;
            border-radius: 5px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="status" id="status">● Online</div>
    <h1>🎓 FRQS Lab Monitor - Teacher Dashboard</h1>
    
    <div class="grid" id="studentGrid">
        <div class="student-screen">
            <h3>Student PC 1</h3>
            <img src="/stream?auth=TOKEN" alt="Student Screen 1">
        </div>
        <!-- Add more student screens as needed -->
    </div>
    
    <div class="control-panel">
        <h2>Control Panel</h2>
        <div>
            <h3>File Upload</h3>
            <input type="file" id="fileInput" multiple>
            <button onclick="uploadFiles()">Upload to Students</button>
        </div>
        <div style="margin-top: 20px;">
            <h3>Remote Control (Student 1)</h3>
            <button onclick="sendInput('click')">Left Click</button>
            <button onclick="sendInput('rightclick')">Right Click</button>
            <button onclick="sendInput('key', 13)">Press Enter</button>
        </div>
    </div>
    
    <script>
        const AUTH_TOKEN = 'secure_lab_token_2025';
        
        async function uploadFiles() {
            const fileInput = document.getElementById('fileInput');
            const formData = new FormData();
            
            for (const file of fileInput.files) {
                formData.append('files', file);
            }
            
            try {
                const response = await fetch('/upload', {
                    method: 'POST',
                    headers: { 'Authorization': `Bearer ${AUTH_TOKEN}` },
                    body: formData
                });
                const result = await response.json();
                alert(`Uploaded ${result.uploaded} files`);
            } catch (error) {
                alert('Upload failed: ' + error.message);
            }
        }
        
        async function sendInput(type, key = null) {
            const body = key ? { type, key } : { type };
            
            try {
                const response = await fetch('/api/input', {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${AUTH_TOKEN}`,
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(body)
                });
                const result = await response.json();
                console.log('Input sent:', result);
            } catch (error) {
                console.error('Input failed:', error);
            }
        }
        
        // Check server status
        setInterval(async () => {
            try {
                const response = await fetch('/api/status', {
                    headers: { 'Authorization': `Bearer ${AUTH_TOKEN}` }
                });
                const status = await response.json();
                document.getElementById('status').textContent = '● Online';
            } catch {
                document.getElementById('status').textContent = '● Offline';
            }
        }, 5000);
    </script>
</body>
</html>)";
                utils::logInfo("Created dashboard.html");
            }
        }
        
        if (!std::filesystem::exists(upload_dir)) {
            std::filesystem::create_directories(upload_dir);
            utils::logInfo(std::format("Created upload directory: {}", upload_dir.string()));
        }
        
        // Create and configure server
        uint16_t port = config.getPort();
        size_t thread_count = static_cast<size_t>(config.getThreadCount());
        
        core::Server server(port, thread_count);
        server.setDocumentRoot(doc_root);
        
        g_server = &server;
        
        // Install signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        utils::logInfo(std::format("Auth Token: {}", config.getAuthToken()));
        utils::logInfo(std::format("FPS Limit: {}", config.getFpsLimit()));
        utils::logInfo(std::format("Scale Factor: {}", config.getScaleFactor()));
        utils::logInfo("Access dashboard at: http://localhost:" + std::to_string(port) + "/dashboard.html");
        
        // Start server (blocks until stopped)
        server.start();
        
        utils::logInfo("Server shutdown complete");
        
    } catch (const std::exception& e) {
        utils::logError("Fatal error: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}