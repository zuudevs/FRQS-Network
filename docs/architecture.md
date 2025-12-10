# FRQS Network - Modular Architecture Design

## 📁 Proposed Project Structure

```
FRQS_Network/
├── CMakeLists.txt
├── README.md
├── ARCHITECTURE.md          # Architecture documentation
├── SECURITY.md              # Security guidelines
├── LICENSE
│
├── include/
│   ├── frqs/
│   │   ├── core/            # Core server (mandatory)
│   │   │   ├── server.hpp
│   │   │   ├── router.hpp
│   │   │   ├── middleware.hpp
│   │   │   └── context.hpp
│   │   │
│   │   ├── http/            # HTTP protocol (mandatory)
│   │   │   ├── request.hpp
│   │   │   ├── response.hpp
│   │   │   ├── method.hpp
│   │   │   └── status.hpp
│   │   │
│   │   ├── net/             # Networking (mandatory)
│   │   │   ├── socket.hpp
│   │   │   ├── ipv4.hpp
│   │   │   └── sockaddr.hpp
│   │   │
│   │   ├── utils/           # Core utilities (mandatory)
│   │   │   ├── logger.hpp
│   │   │   ├── thread_pool.hpp
│   │   │   └── filesystem_utils.hpp
│   │   │
│   │   └── plugins/         # Plugin system (optional)
│   │       ├── plugin.hpp           # Base plugin interface
│   │       ├── static_files.hpp     # Static file serving
│   │       ├── file_upload.hpp      # File upload handler
│   │       ├── auth.hpp             # Authentication
│   │       ├── cors.hpp             # CORS middleware
│   │       ├── rate_limit.hpp       # Rate limiting
│   │       └── websocket.hpp        # WebSocket support
│   │
│   └── frqs-extensions/     # Optional extensions (separate)
│       ├── screen_share.hpp  # Screen sharing (lab use)
│       ├── remote_control.hpp # Remote input (dangerous!)
│       └── proxy.hpp         # Reverse proxy
│
├── src/
│   ├── core/
│   ├── http/
│   ├── net/
│   ├── utils/
│   ├── plugins/
│   └── main.cpp
│
├── extensions/              # Optional extension modules
│   ├── screen_share/
│   │   ├── CMakeLists.txt
│   │   ├── screen_capturer.cpp
│   │   └── README.md
│   └── remote_control/
│       ├── CMakeLists.txt
│       ├── input_injector.cpp
│       └── SECURITY_WARNING.md
│
├── examples/
│   ├── basic_server.cpp
│   ├── rest_api.cpp
│   ├── file_server.cpp
│   └── websocket_chat.cpp
│
├── tests/
│   ├── unit/
│   └── integration/
│
└── docs/
    ├── api/
    ├── plugins/
    └── security/
```

---

## 🔌 Plugin Architecture Design

### Base Plugin Interface

```cpp
// include/frqs/plugins/plugin.hpp
namespace frqs::plugins {

class Plugin {
public:
    virtual ~Plugin() = default;
    
    // Plugin metadata
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual std::string description() const = 0;
    
    // Lifecycle
    virtual bool initialize(core::Server& server) = 0;
    virtual void shutdown() = 0;
    
    // Optional: Register routes
    virtual void registerRoutes(core::Router& router) {}
    
    // Optional: Register middleware
    virtual void registerMiddleware(core::Server& server) {}
};

} // namespace frqs::plugins
```

### Example: Static Files Plugin

```cpp
// include/frqs/plugins/static_files.hpp
namespace frqs::plugins {

class StaticFilesPlugin : public Plugin {
public:
    explicit StaticFilesPlugin(
        std::filesystem::path root,
        std::string mount_path = "/"
    );
    
    std::string name() const override { return "StaticFiles"; }
    std::string version() const override { return "1.0.0"; }
    
    bool initialize(core::Server& server) override;
    void shutdown() override;
    void registerRoutes(core::Router& router) override;
    
private:
    std::filesystem::path root_;
    std::string mount_path_;
};

} // namespace frqs::plugins
```

### Example: Authentication Plugin

```cpp
// include/frqs/plugins/auth.hpp
namespace frqs::plugins {

enum class AuthMethod {
    BEARER_TOKEN,
    BASIC_AUTH,
    JWT,
    OAUTH2
};

class AuthPlugin : public Plugin {
public:
    explicit AuthPlugin(AuthMethod method);
    
    std::string name() const override { return "Authentication"; }
    
    bool initialize(core::Server& server) override;
    void registerMiddleware(core::Server& server) override;
    
    // Configure auth
    void addToken(std::string token);
    void addUser(std::string username, std::string password_hash);
    void setJWTSecret(std::string secret);
    
private:
    AuthMethod method_;
    std::unordered_map<std::string, std::string> tokens_;
};

} // namespace frqs::plugins
```

---

## 🔒 Security Improvements

### 1. Rate Limiting Plugin

```cpp
// include/frqs/plugins/rate_limit.hpp
namespace frqs::plugins {

class RateLimitPlugin : public Plugin {
public:
    struct Config {
        int requests_per_minute = 60;
        int burst_size = 10;
        bool ban_on_abuse = true;
        std::chrono::seconds ban_duration{300};
    };
    
    explicit RateLimitPlugin(Config config);
    
    bool initialize(core::Server& server) override;
    void registerMiddleware(core::Server& server) override;
};

} // namespace frqs::plugins
```

### 2. HTTPS/TLS Support

```cpp
// include/frqs/core/server.hpp (enhanced)
namespace frqs::core {

struct TLSConfig {
    std::filesystem::path cert_file;
    std::filesystem::path key_file;
    std::string cipher_list = "HIGH:!aNULL:!MD5";
    bool require_client_cert = false;
};

class Server {
public:
    // Enable HTTPS
    void enableTLS(const TLSConfig& config);
    
    // ... existing methods
};

} // namespace frqs::core
```

### 3. Security Headers Plugin

```cpp
// include/frqs/plugins/security_headers.hpp
namespace frqs::plugins {

class SecurityHeadersPlugin : public Plugin {
public:
    bool initialize(core::Server& server) override;
    void registerMiddleware(core::Server& server) override;
    
private:
    void addSecurityHeaders(http::HTTPResponse& response);
    // Adds: X-Frame-Options, X-Content-Type-Options,
    //       Strict-Transport-Security, CSP, etc.
};

} // namespace frqs::plugins
```

---

## 📝 Usage Examples

### Example 1: Basic Web Server

```cpp
#include "frqs/core/server.hpp"
#include "frqs/plugins/static_files.hpp"
#include "frqs/plugins/cors.hpp"

int main() {
    frqs::core::Server server(8080);
    
    // Add CORS support
    auto cors = std::make_unique<frqs::plugins::CORSPlugin>();
    cors->allowOrigin("*");
    server.addPlugin(std::move(cors));
    
    // Serve static files
    auto static_files = std::make_unique<frqs::plugins::StaticFilesPlugin>(
        "public", "/"
    );
    server.addPlugin(std::move(static_files));
    
    server.start();
}
```

### Example 2: REST API with Auth

```cpp
#include "frqs/core/server.hpp"
#include "frqs/plugins/auth.hpp"
#include "frqs/plugins/rate_limit.hpp"

int main() {
    frqs::core::Server server(8080);
    
    // Add rate limiting
    auto rate_limit = std::make_unique<frqs::plugins::RateLimitPlugin>(
        frqs::plugins::RateLimitPlugin::Config{
            .requests_per_minute = 100,
            .burst_size = 20
        }
    );
    server.addPlugin(std::move(rate_limit));
    
    // Add JWT authentication
    auto auth = std::make_unique<frqs::plugins::AuthPlugin>(
        frqs::plugins::AuthMethod::JWT
    );
    auth->setJWTSecret("your-secret-key");
    server.addPlugin(std::move(auth));
    
    // Define routes
    server.router().get("/api/users", [](auto& ctx) {
        // Protected endpoint
        return ctx.json({{"users", {}}});
    });
    
    server.start();
}
```

### Example 3: File Upload Server

```cpp
#include "frqs/core/server.hpp"
#include "frqs/plugins/file_upload.hpp"
#include "frqs/plugins/auth.hpp"

int main() {
    frqs::core::Server server(8080);
    
    // Add token auth
    auto auth = std::make_unique<frqs::plugins::AuthPlugin>(
        frqs::plugins::AuthMethod::BEARER_TOKEN
    );
    auth->addToken("secure_token_123");
    server.addPlugin(std::move(auth));
    
    // Configure file upload
    auto upload = std::make_unique<frqs::plugins::FileUploadPlugin>(
        frqs::plugins::FileUploadPlugin::Config{
            .upload_dir = "uploads",
            .max_file_size = 50 * 1024 * 1024,  // 50MB
            .allowed_types = {".jpg", ".png", ".pdf"}
        }
    );
    server.addPlugin(std::move(upload));
    
    server.start();
}
```

### Example 4: Screen Sharing (Extension)

```cpp
#include "frqs/core/server.hpp"
#include "frqs-extensions/screen_share.hpp"

int main() {
    frqs::core::Server server(8080);
    
    // Screen sharing is OPTIONAL extension
    auto screen_share = std::make_unique<frqs::extensions::ScreenSharePlugin>(
        frqs::extensions::ScreenShareConfig{
            .fps_limit = 15,
            .scale_factor = 2,
            .quality = 75,
            .auth_required = true
        }
    );
    server.addPlugin(std::move(screen_share));
    
    server.start();
}
```

---

## 🛡️ Security Considerations

### ⚠️ Dangerous Features (Move to Extensions)

1. **Input Injector** - EXTREME SECURITY RISK
   - Should be in separate extension
   - Require explicit opt-in
   - Add security warnings
   - Log all injection attempts

2. **Screen Capture** - Privacy Concern
   - Optional extension only
   - Require strong authentication
   - Add watermark/notification
   - Audit logging

### ✅ Recommended Security Stack

```cpp
// Secure production setup
frqs::core::Server server(443);

// 1. Enable HTTPS
server.enableTLS({
    .cert_file = "/etc/ssl/certs/server.crt",
    .key_file = "/etc/ssl/private/server.key"
});

// 2. Add security headers
server.addPlugin(std::make_unique<SecurityHeadersPlugin>());

// 3. Rate limiting
server.addPlugin(std::make_unique<RateLimitPlugin>(
    RateLimitPlugin::Config{.requests_per_minute = 60}
));

// 4. Authentication
auto auth = std::make_unique<AuthPlugin>(AuthMethod::JWT);
auth->setJWTSecret(getenv("JWT_SECRET"));
server.addPlugin(std::move(auth));

// 5. CORS (if needed)
auto cors = std::make_unique<CORSPlugin>();
cors->allowOrigin("https://yourdomain.com");
server.addPlugin(std::move(cors));

server.start();
```

---

## 📊 Performance Considerations

### Core Server (Always Loaded)
- Zero-copy parsing
- Thread pool (minimal overhead)
- Efficient routing
- **Target: 100k+ req/s**

### Plugins (Optional Load)
- Load only what you need
- Minimal performance impact
- **Example: Static files adds ~5% overhead**

### Extensions (Heavy Features)
- Screen capture: High CPU/bandwidth
- WebSocket: Persistent connections
- **Use only when needed**

---

## 🔄 Migration Path

### Phase 1: Core Refactor
1. Extract routing logic → `core/router.hpp`
2. Create middleware system → `core/middleware.hpp`
3. Plugin interface → `plugins/plugin.hpp`

### Phase 2: Plugin Migration
1. Static files → Plugin
2. File upload → Plugin
3. CORS → Plugin
4. Auth → Plugin

### Phase 3: Extension Separation
1. Screen capture → Extension
2. Input injector → Extension (with warnings)
3. WebSocket → Extension

### Phase 4: Security Hardening
1. Add TLS support
2. Rate limiting
3. Security headers
4. Audit logging

---

## 📖 Documentation Structure

```
docs/
├── getting-started.md
├── core-concepts.md
├── plugins/
│   ├── creating-plugins.md
│   ├── static-files.md
│   ├── authentication.md
│   └── rate-limiting.md
├── extensions/
│   ├── screen-share.md
│   └── remote-control-security.md
├── security/
│   ├── best-practices.md
│   ├── tls-setup.md
│   └── threat-model.md
└── api/
    ├── server-api.md
    ├── router-api.md
    └── middleware-api.md
```

---

## 🎯 Conclusion

### ✅ Benefits of Modular Design

1. **Flexibility**: Use only what you need
2. **Security**: Isolate dangerous features
3. **Maintainability**: Clear separation of concerns
4. **Performance**: Load minimal components
5. **Extensibility**: Easy to add new features

### 🔒 Security Improvements

1. Move dangerous features to extensions
2. Add TLS/HTTPS support
3. Implement rate limiting
4. Better authentication (JWT, OAuth2)
5. Security headers by default

### 🚀 General Purpose Usage

Core server is now truly general-purpose:
- Web server ✅
- REST API ✅
- File server ✅
- Static hosting ✅
- WebSocket server ✅ (plugin)

Lab-specific features (screen share, remote control) are **optional extensions**.

---

## 🤔 Answer to Your Questions

> **Apakah screen capture melenceng dari tujuan awal?**

**YA**, untuk general-purpose server. **SOLUSI**: Jadikan optional extension.

> **Agar flexibel usage**

Plugin architecture memungkinkan:
- Basic web server (core only)
- File server (core + static files plugin)
- API server (core + auth + rate limit)
- Lab monitoring (core + screen share extension)

Semua dari codebase yang sama, tinggal pilih plugin yang dibutuhkan!

---

**Next Steps:**
1. Refactor core server dengan plugin system
2. Create plugin interface
3. Migrate existing features to plugins
4. Move dangerous features to extensions
5. Add comprehensive documentation
6. Implement security features (TLS, rate limiting)