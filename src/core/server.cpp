/**
 * @file core/server.cpp
 * @brief Clean server implementation with router & middleware
 * @version 1.1.1
 * @copyright Copyright (c) 2025
 */

#include "core/server.hpp"
#include "plugin/plugin.hpp"
#include "utils/logger.hpp"
#include <format>
#include <algorithm>

namespace frqs::core {

Server::Server(uint16_t port, size_t thread_count)
    : port_(port)
    , thread_count_(thread_count)
    , thread_pool_(std::make_unique<utils::ThreadPool>(thread_count))
{
    utils::logInfo(std::format("Server initialized on port {} with {} threads", 
                                port_, thread_count));
}

Server::~Server() {
    stop();
}

bool Server::addPlugin(std::unique_ptr<plugins::Plugin> plugin) {
    if (!plugin) {
        utils::logWarn("Attempted to add null plugin");
        return false;
    }
    
    try {
        // Check if plugin already loaded
        for (const auto& p : plugins_) {
            if (p->name() == plugin->name()) {
                utils::logWarn(std::format("Plugin '{}' already loaded", plugin->name()));
                return false;
            }
        }
        
        // Initialize plugin
        if (!plugin->initialize(*this)) {
            utils::logError(std::format("Failed to initialize plugin '{}'", plugin->name()));
            return false;
        }
        
        // Register routes
        plugin->registerRoutes(router_);
        
        // Register middleware
        plugin->registerMiddleware(*this);
        
        utils::logInfo(std::format("Plugin '{}' v{} loaded successfully", 
            plugin->name(), plugin->version()));
        
        plugins_.push_back(std::move(plugin));
        
        // Sort plugins by priority
        std::sort(plugins_.begin(), plugins_.end(),
            [](const auto& a, const auto& b) {
                return a->priority() < b->priority();
            });
        
        return true;
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Exception loading plugin: {}", e.what()));
        return false;
    }
}

bool Server::removePlugin(std::string_view name) {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
        [name](const auto& p) { return p->name() == name; });
    
    if (it != plugins_.end()) {
        (*it)->shutdown();
        plugins_.erase(it);
        utils::logInfo(std::format("Plugin '{}' removed", name));
        return true;
    }
    
    return false;
}

plugins::Plugin* Server::getPlugin(std::string_view name) const {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
        [name](const auto& p) { return p->name() == name; });
    
    return (it != plugins_.end()) ? it->get() : nullptr;
}

void Server::use(Middleware middleware) {
    middlewares_.push_back(std::move(middleware));
}

void Server::start() {
    if (running_) {
        utils::logWarn("Server is already running");
        return;
    }
    
    try {
        // Call onServerStart for all plugins
        for (auto& plugin : plugins_) {
            if (!plugin->onServerStart()) {
                utils::logError(std::format("Plugin '{}' failed onServerStart", plugin->name()));
                throw std::runtime_error("Plugin initialization failed");
            }
        }
        
        server_socket_ = std::make_unique<net::Socket>();
        
        net::SockAddr bind_addr(net::IPv4(0u), port_);
        server_socket_->bind(bind_addr);
        server_socket_->listen();
        
        running_ = true;
        
        utils::logInfo(std::format("Server listening on {}", bind_addr.toString()));
        utils::logInfo(std::format("Loaded plugins: {}", plugins_.size()));
        utils::logInfo(std::format("Registered middleware: {}", middlewares_.size()));
        
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
    
    // Call onServerStop for all plugins
    for (auto& plugin : plugins_) {
        plugin->onServerStop();
    }
    
    // Shutdown plugins
    for (auto& plugin : plugins_) {
        plugin->shutdown();
    }
    
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
            
            active_connections_++;
            
            thread_pool_->submit([this, client = std::move(client), client_addr]() mutable {
                handleClient(std::move(client), client_addr);
                active_connections_--;
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
        
        total_requests_++;
        
        // Process request through middleware & router
        http::HTTPResponse response;
        processRequest(request, response);
        
        client.send(response.build());
        
    } catch (const std::exception& e) {
        utils::logError(std::format("Error handling client {}: {}", 
                                   client_addr.toString(), 
                                   e.what()));
    }
}

void Server::processRequest(const http::HTTPRequest& request, http::HTTPResponse& response) {
    Context ctx(request, response);
    
    // Execute middleware chain + router
    executeMiddlewareChain(ctx, 0);
}

void Server::executeMiddlewareChain(Context& ctx, size_t index) {
    if (index < middlewares_.size()) {
        // Execute current middleware
        middlewares_[index](ctx, [this, &ctx, index]() {
            // Next middleware
            executeMiddlewareChain(ctx, index + 1);
        });
    } else {
        // End of middleware chain - try routing
        if (!router_.route(ctx)) {
            // No route found - 404
            ctx.status(404)
               .header("Content-Type", "text/html")
               .body("<h1>404 - Not Found</h1><p>The requested resource was not found.</p>");
        }
    }
}

} // namespace frqs::core