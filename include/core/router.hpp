#pragma once

/**
 * @file core/router.hpp & core/context.hpp
 * @brief Modern routing and request context
 * @version 2.0.0
 * @copyright Copyright (c) 2025
 */

#include "middleware.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <regex>

namespace frqs::core {

/**
 * @brief Route handler function
 */
using RouteHandler = std::function<void(Context&)>;

/**
 * @brief HTTP router with path parameters
 * 
 * Features:
 * - Path parameters (/users/:id)
 * - Method-specific routes
 * - Route groups (prefixes)
 * - Middleware per route
 * 
 * @example
 * ```cpp
 * Router router;
 * 
 * // Simple route
 * router.get("/", [](auto& ctx) {
 *     ctx.html("<h1>Home</h1>");
 * });
 * 
 * // With path parameter
 * router.get("/users/:id", [](auto& ctx) {
 *     auto id = ctx.param("id");
 *     ctx.json({{"user_id", *id}});
 * });
 * 
 * // Route group
 * auto api = router.group("/api");
 * api.get("/status", [](auto& ctx) {
 *     ctx.json({{"status", "ok"}});
 * });
 * ```
 */
class Router {
public:
    Router() = default;
    
    // ========== ROUTE REGISTRATION ==========
    
    void get(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::GET, path, std::move(handler));
    }
    
    void post(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::POST, path, std::move(handler));
    }
    
    void put(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::PUT, path, std::move(handler));
    }
    
    void del(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::DELETE, path, std::move(handler));
    }
    
    void patch(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::PATCH, path, std::move(handler));
    }
    
    void options(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::OPTIONS, path, std::move(handler));
    }
    
    void head(std::string_view path, RouteHandler handler) {
        addRoute(http::Method::HEAD, path, std::move(handler));
    }
    
    // ========== ROUTE GROUPS ==========
    
    /**
     * @brief Create route group with prefix
     * 
     * @example
     * ```cpp
     * auto api = router.group("/api/v1");
     * api.get("/users", handler);  // Matches /api/v1/users
     * ```
     */
    Router group(std::string_view prefix) {
        Router child;
        child.prefix_ = prefix_ + std::string(prefix);
        child.parent_ = this;
        return child;
    }
    
    // ========== ROUTE MATCHING ==========
    
    /**
     * @brief Match request to route and execute handler
     * 
     * @param ctx Request context
     * @return true if route was found and executed
     */
    bool route(Context& ctx) {
        auto path = ctx.request().getPath();
        auto method = ctx.request().getMethod();
        
        for (auto& route : routes_) {
            if (route.method != method) continue;
            
            std::smatch matches;
            std::string path_str(path);
            
            if (std::regex_match(path_str, matches, route.pattern)) {
                // Extract path parameters
                for (size_t i = 1; i < matches.size(); ++i) {
                    ctx.setParam(route.param_names[i - 1], matches[i].str());
                }
                
                // Execute handler
                route.handler(ctx);
                return true;
            }
        }
        
        return false;  // No route found
    }
    
private:
    struct Route {
        http::Method method;
        std::string path;
        std::regex pattern;
        std::vector<std::string> param_names;
        RouteHandler handler;
    };
    
    std::vector<Route> routes_;
    std::string prefix_;
    Router* parent_ = nullptr;
    
    void addRoute(http::Method method, std::string_view path, RouteHandler handler) {
        std::string full_path = prefix_ + std::string(path);
        
        // Convert path to regex pattern
        // /users/:id/:action -> /users/([^/]+)/([^/]+)
        std::vector<std::string> param_names;
        std::string pattern_str = "^";
        
        size_t pos = 0;
        while (pos < full_path.size()) {
            if (full_path[pos] == ':') {
                // Found parameter
                size_t end = full_path.find('/', pos);
                if (end == std::string::npos) end = full_path.size();
                
                std::string param_name = full_path.substr(pos + 1, end - pos - 1);
                param_names.push_back(param_name);
                
                pattern_str += "([^/]+)";
                pos = end;
            } else {
                // Regular character
                char c = full_path[pos];
                if (c == '.' || c == '+' || c == '*' || c == '?' || 
                    c == '^' || c == '$' || c == '(' || c == ')' ||
                    c == '[' || c == ']' || c == '{' || c == '}' || c == '|') {
                    pattern_str += '\\';
                }
                pattern_str += c;
                pos++;
            }
        }
        
        pattern_str += "$";
        
        routes_.push_back({
            method,
            std::string(path),
            std::regex(pattern_str),
            std::move(param_names),
            std::move(handler)
        });
    }
};

} // namespace frqs::core