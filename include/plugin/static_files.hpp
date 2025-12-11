#pragma once

/**
 * @file plugins/static_files.hpp
 * @brief Static file serving plugin
 * @version 1.1.0
 * 
 * Serves static files from a document root with:
 * - Security (path traversal protection)
 * - MIME type detection
 * - Configurable default file (index.html)
 * - Optional directory listing
 * 
 * @copyright Copyright (c) 2025
 */

#include "plugin.hpp"
#include "core/router.hpp"
#include "utils/filesystem_utils.hpp"
#include "utils/logger.hpp"
#include "http/mime_types.hpp"
#include <filesystem>

namespace frqs::plugins {

/**
 * @brief Configuration for static files plugin
 */
struct StaticFilesConfig : PluginConfig {
    /// Document root directory
    std::filesystem::path root = "public";
    
    /// Mount path (URL prefix)
    std::string mount_path = "/";
    
    /// Default file to serve for directories
    std::string default_file = "index.html";
    
    /// Enable directory listing (security risk!)
    bool enable_directory_listing = false;
    
    /// Cache-Control header value
    std::string cache_control = "public, max-age=3600";
    
    /// Maximum file size to serve (bytes)
    size_t max_file_size = 100 * 1024 * 1024;  // 100MB
    
    void validate() const override {
        if (!std::filesystem::exists(root)) {
            throw std::invalid_argument("Document root does not exist: " + root.string());
        }
        if (!std::filesystem::is_directory(root)) {
            throw std::invalid_argument("Document root is not a directory: " + root.string());
        }
        if (mount_path.empty()) {
            throw std::invalid_argument("Mount path cannot be empty");
        }
        if (!mount_path.starts_with('/')) {
            throw std::invalid_argument("Mount path must start with /");
        }
    }
};

/**
 * @brief Static file serving plugin
 * 
 * @example
 * ```cpp
 * // Basic usage
 * server.addPlugin(std::make_unique<StaticFilesPlugin>("public"));
 * 
 * // With configuration
 * StaticFilesConfig config;
 * config.root = "/var/www/html";
 * config.mount_path = "/static";
 * config.cache_control = "public, max-age=86400";
 * server.addPlugin(std::make_unique<StaticFilesPlugin>(config));
 * ```
 */
class StaticFilesPlugin : public Plugin {
public:
    /**
     * @brief Construct with document root
     */
    explicit StaticFilesPlugin(const std::filesystem::path& root) {
        config_.root = root;
    }
    
    /**
     * @brief Construct with full configuration
     */
    explicit StaticFilesPlugin(StaticFilesConfig config)
        : config_(std::move(config)) {}
    
    // ========== PLUGIN INTERFACE ==========
    
    [[nodiscard]] std::string name() const override {
        return "StaticFiles";
    }
    
    [[nodiscard]] std::string version() const override {
        return "2.0.0";
    }
    
    [[nodiscard]] std::string description() const override {
        return "Serves static files with security and MIME type detection";
    }
    
    [[nodiscard]] std::string author() const override {
        return "FRQS Network Team";
    }
    
    [[nodiscard]] bool initialize(core::Server& server) override {
        try {
            config_.validate();
            
            // Canonicalize root path
            config_.root = std::filesystem::canonical(config_.root);
            
            utils::logInfo(std::format("Static files plugin initialized: root={}, mount={}",
                config_.root.string(), config_.mount_path));
            
            return true;
            
        } catch (const std::exception& e) {
            utils::logError(std::format("Failed to initialize static files plugin: {}", e.what()));
            return false;
        }
    }
    
    void shutdown() override {
        utils::logInfo("Static files plugin shutdown");
    }
    
    void registerRoutes(core::Router& router) override {
        // Register catch-all route for static files
        std::string route_pattern = config_.mount_path;
        if (!route_pattern.ends_with('/')) {
            route_pattern += '/';
        }
        route_pattern += "*";
        
        router.get(route_pattern, [this](core::Context& ctx) {
            handleStaticFile(ctx);
        });
    }
    
    [[nodiscard]] int priority() const noexcept override {
        return 900;  // Load late (after dynamic routes)
    }

private:
    StaticFilesConfig config_;
    
    void handleStaticFile(core::Context& ctx) {
        std::string path(ctx.request().getPath());
        
        // Remove mount path prefix
        if (path.starts_with(config_.mount_path)) {
            path = path.substr(config_.mount_path.length());
        }
        
        // Default to index file for directories
        if (path.empty() || path.ends_with('/')) {
            path += config_.default_file;
        }
        
        // Security: Resolve path safely
        auto safe_path = utils::FileSystemUtils::securePath(config_.root, path);
        
        if (!safe_path) {
            utils::logWarn(std::format("Path traversal attempt blocked: {}", path));
            ctx.status(403)
               .header("Content-Type", "text/html")
               .body("<h1>403 Forbidden</h1><p>Access denied</p>");
            return;
        }
        
        // Check if file exists
        if (!std::filesystem::exists(*safe_path)) {
            ctx.status(404)
               .header("Content-Type", "text/html")
               .body("<h1>404 Not Found</h1>");
            return;
        }
        
        // Check if it's a directory
        if (std::filesystem::is_directory(*safe_path)) {
            if (config_.enable_directory_listing) {
                serveDirec toryListing(ctx, *safe_path);
            } else {
                ctx.status(403)
                   .header("Content-Type", "text/html")
                   .body("<h1>403 Forbidden</h1><p>Directory listing disabled</p>");
            }
            return;
        }
        
        // Check if it's a regular file
        if (!std::filesystem::is_regular_file(*safe_path)) {
            ctx.status(403)
               .header("Content-Type", "text/html")
               .body("<h1>403 Forbidden</h1>");
            return;
        }
        
        // Check file size
        auto file_size = std::filesystem::file_size(*safe_path);
        if (file_size > config_.max_file_size) {
            ctx.status(413)
               .header("Content-Type", "text/html")
               .body("<h1>413 Payload Too Large</h1>");
            return;
        }
        
        // Read file
        auto content = utils::FileSystemUtils::readFile(*safe_path, config_.max_file_size);
        
        if (!content) {
            ctx.status(500)
               .header("Content-Type", "text/html")
               .body("<h1>500 Internal Server Error</h1>");
            return;
        }
        
        // Determine MIME type
        auto mime_type = http::MimeTypes::fromPath(*safe_path);
        
        // Send response
        ctx.status(200)
           .header("Content-Type", mime_type)
           .header("Cache-Control", config_.cache_control)
           .header("Content-Length", std::to_string(content->size()))
           .body(std::move(*content));
    }
    
    void serveDirectoryListing(core::Context& ctx, const std::filesystem::path& dir) {
        std::string html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Directory Listing</title>
    <style>
        body { font-family: sans-serif; margin: 40px; }
        h1 { border-bottom: 2px solid #333; }
        ul { list-style: none; padding: 0; }
        li { padding: 10px; border-bottom: 1px solid #eee; }
        a { text-decoration: none; color: #0066cc; }
        a:hover { text-decoration: underline; }
        .dir { font-weight: bold; }
        .size { color: #666; float: right; }
    </style>
</head>
<body>
    <h1>Directory Listing</h1>
    <p><a href="../">📁 Parent Directory</a></p>
    <ul>
)HTML";
        
        try {
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                entries.push_back(entry);
            }
            
            // Sort: directories first, then files
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory();
                }
                return a.path().filename() < b.path().filename();
            });
            
            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                std::string icon = entry.is_directory() ? "📁" : "📄";
                std::string css_class = entry.is_directory() ? "dir" : "file";
                
                std::string size_str;
                if (!entry.is_directory()) {
                    auto size = entry.file_size();
                    if (size < 1024) {
                        size_str = std::format("{} B", size);
                    } else if (size < 1024 * 1024) {
                        size_str = std::format("{:.1f} KB", size / 1024.0);
                    } else {
                        size_str = std::format("{:.1f} MB", size / (1024.0 * 1024.0));
                    }
                }
                
                html += std::format(
                    "<li class=\"{}\"><a href=\"{}\">{} {}</a><span class=\"size\">{}</span></li>\n",
                    css_class, name, icon, name, size_str
                );
            }
            
        } catch (const std::exception& e) {
            html += std::format("<li>Error: {}</li>\n", e.what());
        }
        
        html += R"HTML(
    </ul>
</body>
</html>
)HTML";
        
        ctx.status(200)
           .header("Content-Type", "text/html")
           .body(html);
    }
};

} // namespace frqs::plugins