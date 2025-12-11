#pragma once

/**
 * @file plugins/plugin.hpp
 * @brief Base interface for FRQS Network plugins
 * @version 1.1.1
 * 
 * Plugin system allows modular extension of server capabilities.
 * Plugins can:
 * - Register custom routes
 * - Add middleware to request pipeline
 * - Hook into server lifecycle
 * 
 * @copyright Copyright (c) 2025
 */

#include <functional>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

// Forward declarations
namespace frqs::core {
    class Server;
    class Router;
    class Context;
}

namespace frqs::plugins {

/**
 * @brief Base plugin interface
 * 
 * All plugins must inherit from this interface and implement
 * the required lifecycle methods.
 * 
 * @example
 * ```cpp
 * class MyPlugin : public Plugin {
 * public:
 *     std::string name() const override { return "MyPlugin"; }
 *     std::string version() const override { return "1.0.0"; }
 *     
 *     bool initialize(core::Server& server) override {
 *         // Setup plugin
 *         return true;
 *     }
 *     
 *     void shutdown() override {
 *         // Cleanup
 *     }
 * };
 * ```
 */
class Plugin {
public:
    virtual ~Plugin() = default;
    
    // ========== PLUGIN METADATA ==========
    
    /**
     * @brief Get plugin name
     * @return Unique plugin identifier
     */
    [[nodiscard]] virtual std::string name() const = 0;
    
    /**
     * @brief Get plugin version
     * @return Semantic version string (e.g., "1.0.0")
     */
    [[nodiscard]] virtual std::string version() const = 0;
    
    /**
     * @brief Get plugin description
     * @return Human-readable description
     */
    [[nodiscard]] virtual std::string description() const {
        return "No description provided";
    }
    
    /**
     * @brief Get plugin author
     * @return Author name/email
     */
    [[nodiscard]] virtual std::string author() const {
        return "Unknown";
    }
    
    // ========== LIFECYCLE HOOKS ==========
    
    /**
     * @brief Initialize plugin
     * 
     * Called when plugin is loaded. This is where you should:
     * - Validate configuration
     * - Allocate resources
     * - Register routes/middleware
     * - Connect to external services
     * 
     * @param server Reference to server instance
     * @return true if initialization succeeded, false otherwise
     * 
     * @note If this returns false, the plugin will not be loaded
     *       and the server may refuse to start (depending on config)
     */
    [[nodiscard]] virtual bool initialize(core::Server& server) = 0;
    
    /**
     * @brief Shutdown plugin
     * 
     * Called when plugin is unloaded or server is stopping.
     * Clean up resources, close connections, etc.
     * 
     * @note This method must not throw exceptions
     */
    virtual void shutdown() = 0;
    
    // ========== OPTIONAL HOOKS ==========
    
    /**
     * @brief Register routes
     * 
     * Override this to add custom routes to the server.
     * 
     * @param router Router instance
     * 
     * @example
     * ```cpp
     * void registerRoutes(core::Router& router) override {
     *     router.get("/api/status", [](auto& ctx) {
     *         return ctx.json({{"status", "ok"}});
     *     });
     * }
     * ```
     */
    virtual void registerRoutes(core::Router& router) {
        // Default: no routes
        (void)router;
    }
    
    /**
     * @brief Register middleware
     * 
     * Override this to add middleware to request pipeline.
     * Middleware is executed in registration order.
     * 
     * @param server Server instance
     * 
     * @example
     * ```cpp
     * void registerMiddleware(core::Server& server) override {
     *     server.use([](auto& ctx, auto next) {
     *         // Before request
     *         next();  // Call next middleware/handler
     *         // After request
     *     });
     * }
     * ```
     */
    virtual void registerMiddleware(core::Server& server) {
        // Default: no middleware
        (void)server;
    }
    
    /**
     * @brief Called before server starts accepting connections
     * 
     * Use this for final initialization steps that require
     * all plugins to be loaded.
     * 
     * @return true to proceed, false to abort server start
     */
    [[nodiscard]] virtual bool onServerStart() {
        return true;
    }
    
    /**
     * @brief Called after server stops accepting connections
     * 
     * Use this for cleanup that must happen before other
     * plugins are shut down.
     */
    virtual void onServerStop() {
        // Default: no action
    }
    
    // ========== CONFIGURATION ==========
    
    /**
     * @brief Check if plugin has required dependencies
     * 
     * @param available_plugins List of loaded plugin names
     * @return true if all dependencies are met
     * 
     * @example
     * ```cpp
     * bool checkDependencies(const std::vector<std::string>& plugins) override {
     *     return std::find(plugins.begin(), plugins.end(), "AuthPlugin") 
     *            != plugins.end();
     * }
     * ```
     */
    [[nodiscard]] virtual bool checkDependencies(
        const std::vector<std::string>& available_plugins
    ) const {
        (void)available_plugins;
        return true;  // No dependencies by default
    }
    
    /**
     * @brief Get plugin load priority
     * 
     * Lower numbers load first. Use this to control initialization order.
     * 
     * @return Priority value (0-1000, default: 500)
     * 
     * Common priorities:
     * - 0-100: Core infrastructure (logging, monitoring)
     * - 100-300: Security (auth, rate limiting)
     * - 300-500: Business logic
     * - 500-700: Optional features
     * - 700-1000: UI/presentation
     */
    [[nodiscard]] virtual int priority() const noexcept {
        return 500;  // Default priority
    }
    
    /**
     * @brief Check if plugin is enabled
     * 
     * Plugins can implement runtime enable/disable logic here.
     * 
     * @return true if plugin should be active
     */
    [[nodiscard]] virtual bool isEnabled() const noexcept {
        return true;  // Enabled by default
    }
};

/**
 * @brief Plugin configuration base
 * 
 * Plugins can define their own config structs inheriting from this.
 * 
 * @example
 * ```cpp
 * struct MyPluginConfig : PluginConfig {
 *     std::string option1 = "default";
 *     int option2 = 42;
 *     
 *     void validate() const {
 *         if (option2 < 0) {
 *             throw std::invalid_argument("option2 must be >= 0");
 *         }
 *     }
 * };
 * ```
 */
struct PluginConfig {
    virtual ~PluginConfig() = default;
    
    /**
     * @brief Validate configuration
     * 
     * Throw std::invalid_argument if config is invalid.
     */
    virtual void validate() const {}
};

/**
 * @brief Plugin factory function type
 * 
 * Use this to register plugin creators.
 * 
 * @example
 * ```cpp
 * PluginFactory factory = []() -> std::unique_ptr<Plugin> {
 *     return std::make_unique<MyPlugin>();
 * };
 * ```
 */
using PluginFactory = std::function<std::unique_ptr<Plugin>()>;

} // namespace frqs::plugins