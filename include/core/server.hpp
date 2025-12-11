#pragma once

/**
 * @file core/server.hpp
 * @brief Core HTTP server - Clean, general-purpose, ultra-fast
 * @version 2.0.0
 * 
 * Core server provides:
 * - HTTP/1.1 protocol support
 * - Zero-copy request parsing
 * - Thread pool for concurrency
 * - Plugin system
 * - Middleware pipeline
 * - Modern routing
 * 
 * What's NOT here (moved to plugins/extensions):
 * - Static file serving → StaticFilesPlugin
 * - Screen capture → ScreenShareExtension
 * - Input injection → RemoteControlExtension
 * - CORS → CORSPlugin
 * - Authentication → AuthPlugin
 * 
 * @copyright Copyright (c) 2025
 */

#include "net/socket.hpp"
#include "net/sockaddr.hpp"

#ifdef DELETE
	#undef DELETE
#endif

#include "http/request.hpp"
#include "http/response.hpp"
#include "utils/thread_pool.hpp"
#include "router.hpp"
#include "context.hpp"

#include <memory>
#include <atomic>
#include <vector>
#include <functional>

// Forward declaration
namespace frqs::plugins {
    class Plugin;
}

namespace frqs::core {

/**
 * @brief Core HTTP server
 * 
 * Features:
 * - Async request handling with thread pool
 * - Plugin system for modularity
 * - Middleware pipeline
 * - Modern routing with path parameters
 * - Zero-copy parsing
 * - Secure by default
 * 
 * @example
 * ```cpp
 * // Minimal server
 * Server server(8080);
 * server.router().get("/", [](auto& ctx) {
 *     ctx.html("<h1>Hello World</h1>");
 * });
 * server.start();
 * 
 * // With plugins
 * Server server(8080);
 * server.addPlugin(std::make_unique<StaticFilesPlugin>("public"));
 * server.addPlugin(std::make_unique<CORSPlugin>());
 * server.start();
 * ```
 */
class Server {
public:
    /**
     * @brief Construct server
     * @param port Port to listen on
     * @param thread_count Number of worker threads (0 = auto-detect)
     */
    explicit Server(
        uint16_t port = 8080,
        size_t thread_count = std::thread::hardware_concurrency()
    );
    
    ~Server();
    
    // Delete copy and move (server is not copyable/movable)
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;
    
    // ========== PLUGIN MANAGEMENT ==========
    
    /**
     * @brief Add plugin to server
     * 
     * Plugins are initialized in priority order (lower = first).
     * Initialization happens immediately.
     * 
     * @param plugin Plugin instance
     * @return true if plugin initialized successfully
     * 
     * @example
     * ```cpp
     * server.addPlugin(std::make_unique<StaticFilesPlugin>("public"));
     * server.addPlugin(std::make_unique<AuthPlugin>(JWT));
     * ```
     */
    bool addPlugin(std::unique_ptr<plugins::Plugin> plugin);
    
    /**
     * @brief Remove plugin by name
     * @param name Plugin name
     * @return true if plugin was found and removed
     */
    bool removePlugin(std::string_view name);
    
    /**
     * @brief Get plugin by name
     * @return Plugin pointer or nullptr if not found
     */
    [[nodiscard]] plugins::Plugin* getPlugin(std::string_view name) const;
    
    /**
     * @brief Get all loaded plugins
     */
    [[nodiscard]] const auto& plugins() const noexcept {
        return plugins_;
    }
    
    // ========== MIDDLEWARE ==========
    
    /**
     * @brief Add middleware to pipeline
     * 
     * Middleware is executed in registration order for each request.
     * 
     * @param middleware Middleware function
     * 
     * @example
     * ```cpp
     * // Logging middleware
     * server.use([](Context& ctx, Next next) {
     *     auto start = std::chrono::steady_clock::now();
     *     next();
     *     auto duration = std::chrono::steady_clock::now() - start;
     *     log("Request: {} - {}ms", ctx.request().getPath(), duration.count());
     * });
     * 
     * // Auth middleware
     * server.use([](Context& ctx, Next next) {
     *     auto token = ctx.request().getHeader("Authorization");
     *     if (!validateToken(token)) {
     *         ctx.status(401).json({{"error", "Unauthorized"}});
     *         return;  // Don't call next()
     *     }
     *     next();
     * });
     * ```
     */
    void use(Middleware middleware);
    
    // ========== ROUTING ==========
    
    /**
     * @brief Get router instance
     * 
     * Use this to register routes directly on the server.
     * 
     * @example
     * ```cpp
     * server.router().get("/api/status", [](auto& ctx) {
     *     ctx.json({{"status", "ok"}});
     * });
     * 
     * server.router().post("/api/users", [](auto& ctx) {
     *     // Handle user creation
     *     ctx.status(201).json({{"id", 123}});
     * });
     * ```
     */
    [[nodiscard]] Router& router() noexcept {
        return router_;
    }
    
    // ========== SERVER CONTROL ==========
    
    /**
     * @brief Start server (blocking)
     * 
     * This will:
     * 1. Call onServerStart() on all plugins
     * 2. Bind to port and start listening
     * 3. Enter accept loop (blocks until stop() is called)
     * 
     * @throws std::runtime_error if bind/listen fails
     */
    void start();
    
    /**
     * @brief Stop server
     * 
     * This will:
     * 1. Stop accepting new connections
     * 2. Wait for active requests to complete
     * 3. Call onServerStop() on all plugins
     * 4. Shutdown plugins
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    [[nodiscard]] bool isRunning() const noexcept {
        return running_;
    }
    
    /**
     * @brief Get server port
     */
    [[nodiscard]] uint16_t getPort() const noexcept {
        return port_;
    }
    
    /**
     * @brief Get number of active connections
     */
    [[nodiscard]] size_t activeConnections() const noexcept {
        return active_connections_;
    }
    
    /**
     * @brief Get total requests handled
     */
    [[nodiscard]] uint64_t totalRequests() const noexcept {
        return total_requests_;
    }

private:
    // Server configuration
    uint16_t port_;
    size_t thread_count_;
    
    // Core components
    std::unique_ptr<net::Socket> server_socket_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    Router router_;
    
    // Plugin system
    std::vector<std::unique_ptr<plugins::Plugin>> plugins_;
    
    // Middleware pipeline
    std::vector<Middleware> middlewares_;
    
    // Server state
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_connections_{0};
    std::atomic<uint64_t> total_requests_{0};
    
    // Internal methods
    void acceptLoop();
    void handleClient(net::Socket client, net::SockAddr client_addr);
    void processRequest(const http::HTTPRequest& request, http::HTTPResponse& response);
    void executeMiddlewareChain(Context& ctx, size_t index);
};

/**
 * @brief Server builder for fluent configuration
 * 
 * @example
 * ```cpp
 * auto server = ServerBuilder()
 *     .port(8080)
 *     .threads(4)
 *     .plugin<StaticFilesPlugin>("public")
 *     .plugin<CORSPlugin>()
 *     .middleware([](auto& ctx, auto next) {
 *         // Logging
 *         next();
 *     })
 *     .route("GET", "/", [](auto& ctx) {
 *         ctx.html("<h1>Home</h1>");
 *     })
 *     .build();
 * 
 * server->start();
 * ```
 */
class ServerBuilder {
public:
    ServerBuilder() = default;
    
    ServerBuilder& port(uint16_t p) {
        port_ = p;
        return *this;
    }
    
    ServerBuilder& threads(size_t t) {
        threads_ = t;
        return *this;
    }
    
    template<typename PluginT, typename... Args>
    ServerBuilder& plugin(Args&&... args) {
        plugins_.push_back([args...](Server& s) {
            s.addPlugin(std::make_unique<PluginT>(std::forward<Args>(args)...));
        });
        return *this;
    }
    
    ServerBuilder& middleware(Middleware m) {
        middlewares_.push_back(std::move(m));
        return *this;
    }
    
    ServerBuilder& route(std::string_view method, std::string_view path, RouteHandler handler) {
        routes_.push_back({std::string(method), std::string(path), std::move(handler)});
        return *this;
    }
    
    std::unique_ptr<Server> build() {
        auto server = std::make_unique<Server>(port_, threads_);
        
        // Apply plugins
        for (auto& plugin_fn : plugins_) {
            plugin_fn(*server);
        }
        
        // Apply middleware
        for (auto& mw : middlewares_) {
            server->use(std::move(mw));
        }
        
        // Apply routes
        for (auto& [method, path, handler] : routes_) {
            if (method == "GET") {
                server->router().get(path, std::move(handler));
            } else if (method == "POST") {
                server->router().post(path, std::move(handler));
            }
            // Add other methods as needed
        }
        
        return server;
    }

private:
    uint16_t port_ = 8080;
    size_t threads_ = std::thread::hardware_concurrency();
    std::vector<std::function<void(Server&)>> plugins_;
    std::vector<Middleware> middlewares_;
    std::vector<std::tuple<std::string, std::string, RouteHandler>> routes_;
};

} // namespace frqs::core