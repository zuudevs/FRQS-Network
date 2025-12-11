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
    
    void get(std::string_view path, RouteHandler handler) ;
    void post(std::string_view path, RouteHandler handler) ;
    void put(std::string_view path, RouteHandler handler) ;
    void del(std::string_view path, RouteHandler handler) ;
    void patch(std::string_view path, RouteHandler handler) ;
    void options(std::string_view path, RouteHandler handler) ;
    void head(std::string_view path, RouteHandler handler) ;
    
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
    Router group(std::string_view prefix) ;
    
    // ========== ROUTE MATCHING ==========
    
    /**
     * @brief Match request to route and execute handler
     * 
     * @param ctx Request context
     * @return true if route was found and executed
     */
    bool route(Context& ctx) ;
    
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
    
    void addRoute(http::Method method, std::string_view path, RouteHandler handler) ;
};

} // namespace frqs::core