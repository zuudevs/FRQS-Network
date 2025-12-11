/**
 * @file core/router.cpp
 * @brief Router implementation
 * @version 1.1.1
 * @copyright Copyright (c) 2025
 */

#include "core/router.hpp"
#include <format>

namespace frqs::core {

void Router::addRoute(http::Method method, std::string_view path, RouteHandler handler) {
    std::string full_path = prefix_ + std::string(path);
    
    // Convert path to regex pattern
    // /users/:id/:action -> ^/users/([^/]+)/([^/]+)$
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
            // Regular character - escape special regex chars
            char c = full_path[pos];
            if (c == '.' || c == '+' || c == '*' || c == '?' || 
                c == '^' || c == '$' || c == '(' || c == ')' ||
                c == '[' || c == ']' || c == '{' || c == '}' || c == '|' || c == '\\') {
                pattern_str += '\\';
            }
            pattern_str += c;
            pos++;
        }
    }
    
    pattern_str += "$";
    
    try {
        routes_.push_back({
            method,
            std::string(path),
            std::regex(pattern_str),
            std::move(param_names),
            std::move(handler)
        });
    } catch (const std::regex_error& e) {
        // Log error but don't crash
        throw std::runtime_error(std::format("Invalid route pattern '{}': {}", path, e.what()));
    }
}

bool Router::route(Context& ctx) {
    auto path = ctx.request().getPath();
    auto method = ctx.request().getMethod();
    
    for (auto& route_entry : routes_) {
        if (route_entry.method != method) continue;
        
        std::smatch matches;
        std::string path_str(path);
        
        if (std::regex_match(path_str, matches, route_entry.pattern)) {
            // Extract path parameters
            for (size_t i = 1; i < matches.size(); ++i) {
                ctx.setParam(route_entry.param_names[i - 1], matches[i].str());
            }
            
            // Execute handler
            route_entry.handler(ctx);
            return true;
        }
    }
    
    return false;  // No route found
}

Router Router::group(std::string_view prefix) {
    Router child;
    child.prefix_ = prefix_ + std::string(prefix);
    child.parent_ = this;
    return child;
}

void Router::get(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::GET, path, std::move(handler));
}

void Router::post(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::POST, path, std::move(handler));
}

void Router::put(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::PUT, path, std::move(handler));
}

void Router::del(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::DELETE, path, std::move(handler));
}

void Router::patch(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::PATCH, path, std::move(handler));
}

void Router::options(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::OPTIONS, path, std::move(handler));
}

void Router::head(std::string_view path, RouteHandler handler) {
    addRoute(http::Method::HEAD, path, std::move(handler));
}

} // namespace frqs::core