#pragma once

#include "context.hpp"
#include <functional>

namespace frqs::core {

/**
 * @brief Middleware function signature
 * 
 * Middleware can:
 * - Inspect/modify request
 * - Short-circuit by not calling next()
 * - Modify response after handler
 * 
 * @example
 * ```cpp
 * server.use([](Context& ctx, Next next) {
 *     // Before handler
 *     auto start = std::chrono::steady_clock::now();
 *     
 *     next();  // Call next middleware or handler
 *     
 *     // After handler
 *     auto duration = std::chrono::steady_clock::now() - start;
 *     log("Request took {}ms", duration.count());
 * });
 * ```
 */
using Next = std::function<void()>;
using Middleware = std::function<void(Context&, Next)>;

} // namespace frqs::core