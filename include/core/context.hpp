#pragma once

#include "http/request.hpp"
#include "http/response.hpp"
#include <any>

namespace frqs::core {

/**
 * @brief Request context with state management
 * 
 * Context wraps request/response and provides:
 * - Easy access to request data
 * - Response builders
 * - State storage (for middleware)
 * - Path parameters
 * 
 * @example
 * ```cpp
 * router.get("/users/:id", [](Context& ctx) {
 *     auto id = ctx.param("id");
 *     return ctx.json({{"user_id", *id}});
 * });
 * ```
 */
class Context {
public:
    Context(const http::HTTPRequest& req, http::HTTPResponse& resp)
        : request_(req), response_(resp) {}
    
    // ========== REQUEST ACCESS ==========
    
    [[nodiscard]] const http::HTTPRequest& request() const noexcept {
        return request_;
    }
    
    [[nodiscard]] http::HTTPResponse& response() noexcept {
        return response_;
    }
    
    // ========== PATH PARAMETERS ==========
    
    /**
     * @brief Get path parameter
     * @param name Parameter name (from route like "/users/:id")
     * @return Parameter value if exists
     */
    [[nodiscard]] std::optional<std::string_view> param(std::string_view name) const {
        auto it = params_.find(std::string(name));
        if (it != params_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void setParam(std::string name, std::string value) {
        params_[std::move(name)] = std::move(value);
    }
    
    // ========== QUERY PARAMETERS ==========
    
    [[nodiscard]] std::optional<std::string_view> query(std::string_view name) const {
        return request_.getQueryParam(name);
    }
    
    // ========== RESPONSE BUILDERS ==========
    
    Context& status(int code) {
        response_.setStatus(code);
        return *this;
    }
    
    Context& header(std::string_view name, std::string_view value) {
        response_.setHeader(name, value);
        return *this;
    }
    
    Context& body(std::string content) {
        response_.setBody(std::move(content));
        return *this;
    }
    
    // ========== CONVENIENCE METHODS ==========
    
    /**
     * @brief Send JSON response
     * @example ctx.json({{"status", "ok"}})
     */
    Context& json(const std::string& data) {
        return status(200)
            .header("Content-Type", "application/json")
            .body(data);
    }
    
    Context& html(std::string_view content) {
        return status(200)
            .header("Content-Type", "text/html")
            .body(std::string(content));
    }
    
    Context& text(std::string_view content) {
        return status(200)
            .header("Content-Type", "text/plain")
            .body(std::string(content));
    }
    
    Context& redirect(std::string_view url, int code = 302) {
        return status(code).header("Location", url);
    }
    
    // ========== STATE MANAGEMENT ==========
    
    /**
     * @brief Store data in context (for middleware)
     * @example ctx.set("user_id", 123)
     */
    void set(std::string_view key, std::any value) {
        state_[std::string(key)] = std::move(value);
    }
    
    /**
     * @brief Get stored data
     * @example auto user_id = ctx.get<int>("user_id")
     */
    template<typename T>
    [[nodiscard]] std::optional<T> get(std::string_view key) const {
        auto it = state_.find(std::string(key));
        if (it != state_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    /**
     * @brief Check if key exists
     */
    [[nodiscard]] bool has(std::string_view key) const {
        return state_.contains(std::string(key));
    }

private:
    const http::HTTPRequest& request_;
    http::HTTPResponse& response_;
    std::unordered_map<std::string, std::string> params_;
    std::unordered_map<std::string, std::any> state_;
};

} // namespace frqs::core