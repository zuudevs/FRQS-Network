# FRQS Network - Security Guide

## 🔒 Table of Contents

1. [Security Overview](#security-overview)
2. [Threat Model](#threat-model)
3. [Security Features](#security-features)
4. [Deployment Security](#deployment-security)
5. [Dangerous Extensions](#dangerous-extensions)
6. [Incident Response](#incident-response)
7. [Security Checklist](#security-checklist)

---

## 1. Security Overview

### Security Philosophy

FRQS Network follows a **defense-in-depth** approach:

```
┌─────────────────────────────────────────┐
│   Network Layer (Firewall, WAF)        │
├─────────────────────────────────────────┤
│   TLS/HTTPS Encryption                  │
├─────────────────────────────────────────┤
│   Rate Limiting & DDoS Protection       │
├─────────────────────────────────────────┤
│   Authentication & Authorization        │
├─────────────────────────────────────────┤
│   Input Validation & Sanitization       │
├─────────────────────────────────────────┤
│   Secure Coding Practices               │
├─────────────────────────────────────────┤
│   Audit Logging & Monitoring            │
└─────────────────────────────────────────┘
```

### Security Principles

1. **Least Privilege**: Grant minimum necessary permissions
2. **Defense in Depth**: Multiple layers of security
3. **Fail Secure**: Fail to a safe state
4. **Complete Mediation**: Check every access
5. **Separation of Privilege**: Divide authority
6. **Open Design**: Security through design, not obscurity

---

## 2. Threat Model

### 2.1 Assets to Protect

| Asset | Value | Threat Level |
|-------|-------|--------------|
| Server availability | HIGH | DoS attacks |
| User data (if stored) | HIGH | Unauthorized access |
| System files | CRITICAL | Path traversal |
| Authentication tokens | CRITICAL | Token theft |
| Screen content (if enabled) | HIGH | Unauthorized viewing |
| Remote control (if enabled) | CRITICAL | Unauthorized access |

### 2.2 Threat Actors

**External Attackers**
- **Skill Level**: Script kiddies to advanced persistent threats
- **Motivation**: Data theft, system disruption, botnet recruitment
- **Methods**: SQL injection, XSS, DoS, credential stuffing

**Malicious Insiders**
- **Skill Level**: Variable, often with system knowledge
- **Motivation**: Data exfiltration, sabotage
- **Methods**: Privilege abuse, data manipulation

**Automated Bots**
- **Skill Level**: Automated scanners
- **Motivation**: Vulnerability scanning, spam, resource abuse
- **Methods**: Port scanning, brute force, enumeration

### 2.3 Attack Vectors

#### HIGH RISK
- **SQL Injection** (if database plugins used)
- **Path Traversal** (accessing files outside doc root)
- **Authentication Bypass** (weak token validation)
- **Remote Code Execution** (via input injection extension)

#### MEDIUM RISK
- **Cross-Site Scripting** (XSS)
- **Cross-Site Request Forgery** (CSRF)
- **Session Hijacking**
- **Brute Force Attacks**

#### LOW RISK
- **Information Disclosure** (verbose errors)
- **Clickjacking**
- **MIME Type Confusion**

### 2.4 Threat Scenarios

#### Scenario 1: Path Traversal Attack

```http
GET /../../../etc/passwd HTTP/1.1
Host: victim.com
```

**Mitigation**: Canonical path checking in `FileSystemUtils::securePath()`

```cpp
// ✅ Protected
auto safe_path = FileSystemUtils::securePath(root, requested_path);
if (!safe_path) {
    return http::HTTPResponse().forbidden();
}
```

#### Scenario 2: DoS via Large Requests

```http
POST /upload HTTP/1.1
Content-Length: 999999999999
[massive payload]
```

**Mitigation**: Request size limits

```cpp
static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB

if (raw_data.size() > MAX_REQUEST_SIZE) {
    return http::HTTPResponse().badRequest("Request too large");
}
```

#### Scenario 3: Authentication Bypass

```http
GET /admin/dashboard HTTP/1.1
Authorization: Bearer invalid_token
```

**Mitigation**: Proper token validation

```cpp
bool AuthPlugin::validateJWT(std::string_view token) {
    // ✅ Proper validation
    auto decoded = jwt::decode(token);
    auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{jwt_secret_})
        .with_issuer("frqs-server");
    
    try {
        verifier.verify(decoded);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
```

#### Scenario 4: Screen Capture Abuse

```javascript
// Attacker tries to access screen stream
fetch('https://victim.com/stream')
  .then(r => r.blob())
  .then(b => uploadToAttacker(b));
```

**Mitigation**: Strong authentication + audit logging

```cpp
void ScreenSharePlugin::handleStream(Context& ctx) {
    // ✅ Verify authentication
    auto user = ctx.get<User>("user");
    if (!user || !user->hasPermission("screen.view")) {
        audit_log("UNAUTHORIZED_SCREEN_ACCESS", {
            {"ip", ctx.request().getClientIP()},
            {"path", ctx.request().getPath()}
        });
        return ctx.status(403).json({{"error", "Forbidden"}});
    }
    
    // Log successful access
    audit_log("SCREEN_ACCESS", {
        {"user", user->username},
        {"ip", ctx.request().getClientIP()}
    });
    
    // Stream screen...
}
```

---

## 3. Security Features

### 3.1 Built-in Security (Core Server)

#### Path Traversal Protection

```cpp
namespace frqs::utils {

std::optional<std::filesystem::path> FileSystemUtils::securePath(
    const std::filesystem::path& root,
    std::string_view requested_path
) {
    // 1. Normalize path (remove .., .)
    auto normalized = normalizePath(requested_path);
    
    // 2. Construct full path
    auto full_path = canonical_root / normalized;
    
    // 3. Canonicalize if exists
    if (std::filesystem::exists(full_path)) {
        full_path = std::filesystem::canonical(full_path);
    }
    
    // 4. CRITICAL: Verify path is within root
    if (!isSafePath(canonical_root, full_path)) {
        audit_log("PATH_TRAVERSAL_ATTEMPT", {
            {"requested", requested_path},
            {"resolved", full_path.string()}
        });
        return std::nullopt;
    }
    
    return full_path;
}

} // namespace frqs::utils
```

#### Request Size Limits

```cpp
class HTTPRequest {
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
    static constexpr size_t MAX_HEADER_SIZE = 8192;         // 8KB
    static constexpr size_t MAX_HEADERS_COUNT = 100;
    
    bool parse(std::string_view raw_data) noexcept {
        if (raw_data.size() > MAX_REQUEST_SIZE) {
            error_message_ = "Request too large";
            return false;
        }
        // ...
    }
};
```

#### Input Validation

```cpp
class HTTPRequest {
    bool parseRequestLine(std::string_view line) noexcept {
        // Extract method
        auto method_str = line.substr(0, method_end);
        method_ = parseMethod(method_str);
        
        // ✅ Validate method
        if (method_ == Method::UNKNOWN) {
            error_message_ = "Unsupported HTTP method";
            return false;
        }
        
        // ✅ Validate version
        if (version_ != "HTTP/1.1" && version_ != "HTTP/1.0") {
            error_message_ = "Unsupported HTTP version";
            return false;
        }
        
        return true;
    }
};
```

### 3.2 TLS/HTTPS Plugin

```cpp
// include/frqs/plugins/tls.hpp

namespace frqs::plugins {

class TLSPlugin : public Plugin {
public:
    struct Config {
        std::filesystem::path cert_file;
        std::filesystem::path key_file;
        std::filesystem::path ca_file;  // For client cert validation
        
        std::string cipher_list = 
            "ECDHE-RSA-AES256-GCM-SHA384:"
            "ECDHE-RSA-AES128-GCM-SHA256:"
            "DHE-RSA-AES256-GCM-SHA384";
        
        bool require_client_cert = false;
        bool enable_hsts = true;
        std::chrono::seconds hsts_max_age{31536000}; // 1 year
        
        // TLS version
        int min_tls_version = TLS1_2_VERSION;
        int max_tls_version = TLS1_3_VERSION;
    };
    
    explicit TLSPlugin(Config config);
    
    bool initialize(Server& server) override;
    void shutdown() override;

private:
    Config config_;
    SSL_CTX* ssl_ctx_ = nullptr;
    
    void setupSSLContext();
    void loadCertificates();
    void configureCipherSuites();
};

} // namespace frqs::plugins
```

### 3.3 Authentication Plugin

```cpp
// include/frqs/plugins/auth.hpp

namespace frqs::plugins {

class AuthPlugin : public Plugin {
public:
    struct Config {
        AuthMethod method = AuthMethod::JWT;
        
        // JWT configuration
        std::string jwt_secret;
        std::string jwt_algorithm = "HS256";
        std::chrono::hours jwt_expiry{24};
        bool allow_refresh = true;
        
        // OAuth2 configuration
        std::string oauth2_provider;
        std::string oauth2_client_id;
        std::string oauth2_client_secret;
        std::string oauth2_redirect_uri;
        
        // Public paths (no auth required)
        std::unordered_set<std::string> public_paths{
            "/", "/health", "/login", "/register"
        };
        
        // Failed login policy
        int max_failed_attempts = 5;
        std::chrono::minutes lockout_duration{15};
        
        // Session configuration
        std::chrono::hours session_timeout{24};
        bool enable_session_refresh = true;
    };
    
    explicit AuthPlugin(Config config);
    
    bool initialize(Server& server) override;
    void registerMiddleware(Server& server) override;
    void registerRoutes(Router& router) override;
    
    // Token management
    void addToken(std::string token);
    void revokeToken(std::string token);
    
    // User management
    void addUser(std::string username, std::string password_hash);
    void removeUser(std::string username);
    
    // JWT methods
    std::string generateJWT(const User& user);
    std::optional<User> validateJWT(std::string_view token);

private:
    Config config_;
    
    // Token storage (consider using Redis in production)
    std::unordered_set<std::string> valid_tokens_;
    std::unordered_set<std::string> revoked_tokens_;
    
    // User storage
    std::unordered_map<std::string, User> users_;
    
    // Failed login tracking
    std::unordered_map<std::string, int> failed_attempts_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lockouts_;
    
    bool validateRequest(Context& ctx);
    void recordFailedAttempt(std::string_view username);
    bool isLockedOut(std::string_view username);
};

} // namespace frqs::plugins
```

### 3.4 Rate Limiting Plugin

```cpp
// include/frqs/plugins/rate_limit.hpp

namespace frqs::plugins {

class RateLimitPlugin : public Plugin {
public:
    struct Config {
        // Rate limits
        int requests_per_minute = 60;
        int burst_size = 10;
        
        // Ban policy
        bool ban_on_abuse = true;
        std::chrono::seconds ban_duration{300};
        int ban_threshold = 5; // Violations before ban
        
        // Whitelist
        std::unordered_set<std::string> whitelist_ips;
        
        // Per-endpoint limits
        std::unordered_map<std::string, int> endpoint_limits;
        
        // Storage backend (for distributed systems)
        enum class Backend { MEMORY, REDIS } backend = Backend::MEMORY;
        std::string redis_url;
    };
    
    explicit RateLimitPlugin(Config config);
    
    bool initialize(Server& server) override;
    void registerMiddleware(Server& server) override;

private:
    Config config_;
    
    struct RateLimitInfo {
        int count = 0;
        std::chrono::steady_clock::time_point window_start;
        int violations = 0;
        std::chrono::steady_clock::time_point ban_until;
    };
    
    std::unordered_map<std::string, RateLimitInfo> rate_limits_;
    std::mutex mutex_;
    
    bool checkRateLimit(std::string_view client_ip, std::string_view path);
    void recordRequest(std::string_view client_ip);
    void banClient(std::string_view client_ip);
};

} // namespace frqs::plugins
```

### 3.5 Security Headers Plugin

```cpp
// include/frqs/plugins/security_headers.hpp

namespace frqs::plugins {

class SecurityHeadersPlugin : public Plugin {
public:
    struct Config {
        // Content Security Policy
        std::string csp = "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'";
        
        // Other headers
        std::string x_frame_options = "DENY";
        std::string x_content_type_options = "nosniff";
        std::string x_xss_protection = "1; mode=block";
        std::string referrer_policy = "no-referrer";
        
        // HSTS
        bool enable_hsts = true;
        std::chrono::seconds hsts_max_age{31536000};
        bool hsts_include_subdomains = true;
        bool hsts_preload = false;
        
        // Permissions Policy (formerly Feature Policy)
        std::string permissions_policy = 
            "geolocation=(), microphone=(), camera=()";
    };
    
    explicit SecurityHeadersPlugin(Config config = {});
    
    bool initialize(Server& server) override;
    void registerMiddleware(Server& server) override;

private:
    Config config_;
    
    void addSecurityHeaders(http::HTTPResponse& response);
};

} // namespace frqs::plugins
```

---

## 4. Deployment Security

### 4.1 Production Deployment Checklist

```bash
#!/bin/bash
# deploy_secure.sh - Secure deployment script

# 1. System hardening
sudo ufw enable
sudo ufw allow 443/tcp
sudo ufw allow 80/tcp
sudo ufw deny 8080/tcp  # Block direct access

# 2. Create dedicated user
sudo useradd -r -s /bin/false frqs_server

# 3. Set file permissions
sudo chown -R frqs_server:frqs_server /opt/frqs_server
sudo chmod 750 /opt/frqs_server
sudo chmod 640 /opt/frqs_server/frqs.conf

# 4. Secure sensitive files
sudo chmod 600 /etc/ssl/private/server.key
sudo chown frqs_server:frqs_server /etc/ssl/private/server.key

# 5. Enable AppArmor/SELinux profile
sudo aa-enforce /etc/apparmor.d/frqs_server

# 6. Set up systemd service with security
sudo systemctl enable frqs_server
sudo systemctl start frqs_server
```

### 4.2 Systemd Service with Security

```ini
# /etc/systemd/system/frqs_server.service

[Unit]
Description=FRQS Network Server
After=network.target

[Service]
Type=simple
User=frqs_server
Group=frqs_server
WorkingDirectory=/opt/frqs_server

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/frqs_server/uploads /var/log/frqs

# Resource limits
LimitNOFILE=65536
LimitNPROC=512
MemoryLimit=2G
CPUQuota=80%

# Restart policy
Restart=on-failure
RestartSec=5s

ExecStart=/opt/frqs_server/bin/frqs_server /opt/frqs_server/frqs.conf

[Install]
WantedBy=multi-user.target
```

### 4.3 Nginx Reverse Proxy (Recommended)

```nginx
# /etc/nginx/sites-available/frqs_server

upstream frqs_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 443 ssl http2;
    server_name yourdomain.com;
    
    # SSL Configuration
    ssl_certificate /etc/letsencrypt/live/yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/yourdomain.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    ssl_prefer_server_ciphers on;
    
    # Security Headers
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Content-Security-Policy "default-src 'self'" always;
    
    # Rate Limiting
    limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;
    limit_req zone=api burst=20 nodelay;
    
    # DDoS Protection
    client_body_timeout 10s;
    client_header_timeout 10s;
    client_max_body_size 10m;
    
    # Proxy Settings
    location / {
        proxy_pass http://frqs_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }
    
    # Block common attack patterns
    location ~ /\. {
        deny all;
    }
    
    location ~ \.php$ {
        deny all;
    }
}

# Redirect HTTP to HTTPS
server {
    listen 80;
    server_name yourdomain.com;
    return 301 https://$server_name$request_uri;
}
```

### 4.4 Monitoring & Alerting

```cpp
// include/frqs/plugins/monitoring.hpp

namespace frqs::plugins {

class MonitoringPlugin : public Plugin {
public:
    struct Config {
        bool enable_prometheus = true;
        std::string prometheus_endpoint = "/metrics";
        
        bool enable_health_check = true;
        std::string health_check_endpoint = "/health";
        
        // Alert thresholds
        int alert_error_rate = 5;  // errors per minute
        int alert_response_time = 1000;  // ms
        
        // Alert destinations
        std::string slack_webhook;
        std::string email_recipient;
    };
    
    void registerRoutes(Router& router) override {
        // Prometheus metrics
        router.get(config_.prometheus_endpoint, [this](auto& ctx) {
            ctx.text(exportMetrics());
        });
        
        // Health check
        router.get(config_.health_check_endpoint, [this](auto& ctx) {
            auto health = checkHealth();
            ctx.status(health.healthy ? 200 : 503)
               .json(health.toJSON());
        });
    }
    
private:
    struct Metrics {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> total_errors{0};
        std::atomic<uint64_t> total_bytes_sent{0};
        std::atomic<double> avg_response_time{0.0};
    };
    
    Metrics metrics_;
    
    std::string exportMetrics();
    HealthStatus checkHealth();
    void sendAlert(const Alert& alert);
};

} // namespace frqs::plugins
```

---

## 5. Dangerous Extensions

### ⚠️ Screen Sharing Extension

**Risk Level**: HIGH  
**Reason**: Exposes visual screen content

**Security Requirements**:

```cpp
// extensions/screen_share/screen_share_plugin.hpp

class ScreenSharePlugin : public Plugin {
public:
    struct Config {
        // MANDATORY: Authentication
        bool auth_required = true;  // NEVER set to false!
        
        // Watermark (recommended)
        bool enable_watermark = true;
        std::string watermark_text = "MONITORED";
        
        // Audit logging (mandatory)
        bool enable_audit_log = true;
        
        // Access control
        std::vector<std::string> authorized_users;
        std::vector<std::string> authorized_ips;
        
        // Session limits
        std::chrono::minutes max_session_duration{30};
        int max_concurrent_viewers = 5;
        
        // Privacy features
        bool blur_sensitive_windows = true;
        std::vector<std::string> excluded_window_titles{
            "Password", "Private", "Confidential"
        };
    };
    
    bool initialize(Server& server) override {
        // ✅ Verify authentication is enabled
        if (!config_.auth_required) {
            log_error("Screen sharing MUST require authentication!");
            return false;
        }
        
        // ✅ Verify audit logging is enabled
        if (!config_.enable_audit_log) {
            log_error("Screen sharing MUST enable audit logging!");
            return false;
        }
        
        // Log security warning
        security_log("SCREEN_SHARE_ENABLED", {
            {"watermark", config_.enable_watermark},
            {"authorized_users", config_.authorized_users.size()}
        });
        
        return true;
    }
    
    void handleStream(Context& ctx) {
        // ✅ Check authentication
        auto user = ctx.get<User>("user");
        if (!user || !isAuthorized(*user)) {
            audit_log("SCREEN_ACCESS_DENIED", {
                {"ip", ctx.request().getClientIP()},
                {"reason", "unauthorized"}
            });
            return ctx.status(403).json({{"error", "Forbidden"}});
        }
        
        // ✅ Check IP whitelist
        if (!config_.authorized_ips.empty()) {
            auto client_ip = ctx.request().getClientIP();
            if (std::find(config_.authorized_ips.begin(), 
                         config_.authorized_ips.end(), 
                         client_ip) == config_.authorized_ips.end()) {
                audit_log("SCREEN_ACCESS_DENIED", {
                    {"ip", client_ip},
                    {"reason", "ip_not_whitelisted"}
                });
                return ctx.status(403).json({{"error", "Forbidden"}});
            }
        }
        
        // ✅ Log successful access
        audit_log("SCREEN_ACCESS_GRANTED", {
            {"user", user->username},
            {"ip", ctx.request().getClientIP()},
            {"session_id", ctx.get<std::string>("session_id")}
        });
        
        // ✅ Enforce session limits
        auto session_start = std::chrono::steady_clock::now();
        
        // Stream with watermark
        streamScreen(ctx, [&]() {
            auto duration = std::chrono::steady_clock::now() - session_start;
            return duration < config_.max_session_duration;
        });
        
        // ✅ Log session end
        audit_log("SCREEN_SESSION_END", {
            {"user", user->username},
            {"duration_seconds", 
             std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::steady_clock::now() - session_start
             ).count()}
        });
    }
};
```

### ⚠️⚠️⚠️ Remote Control Extension

**Risk Level**: CRITICAL  
**Reason**: Allows remote command execution

**WARNING**: This extension is EXTREMELY DANGEROUS. Only use in:
- Isolated test environments
- Strictly controlled networks
- With multiple layers of authentication

```cpp
// extensions/remote_control/SECURITY_WARNING.md

# ⚠️⚠️⚠️ CRITICAL SECURITY WARNING ⚠️⚠️⚠️

## THIS EXTENSION IS EXTREMELY DANGEROUS

The Remote Control extension allows remote code execution on the host system.
**Unauthorized access could lead to complete system compromise.**

## NEVER USE IN PRODUCTION

This extension should ONLY be used in:
- Isolated test environments
- Air-gapped networks
- Strictly controlled lab environments

## REQUIRED SECURITY MEASURES

1. ✅ Multi-factor authentication (2FA/MFA)
2. ✅ IP whitelist (strict)
3. ✅ Certificate-based authentication
4. ✅ Session timeout (< 5 minutes)
5. ✅ Comprehensive audit logging
6. ✅ Real-time monitoring
7. ✅ Intrusion detection
8. ✅ Network segmentation

## INCIDENT RESPONSE

If unauthorized access is suspected:
1. Immediately disable the extension
2. Terminate all active sessions
3. Review audit logs
4. Scan for compromise
5. Notify security team
6. Consider system re-imaging

## LEGAL DISCLAIMER

Use of this extension may have legal implications. Ensure:
- You have explicit authorization
- Usage complies with local laws
- Proper consent is obtained
- Activity is documented
```

**Secure Implementation** (if you MUST use it):

```cpp
// extensions/remote_control/remote_control_plugin.hpp

class RemoteControlPlugin : public Plugin {
public:
    struct Config {
        // ✅ MANDATORY: Strong authentication
        bool require_2fa = true;  // NEVER disable!
        bool require_certificate = true;  // Client cert required
        
        // ✅ MANDATORY: IP whitelist
        std::vector<std::string> whitelist_ips;  // Must not be empty!
        
        // ✅ MANDATORY: Session security
        std::chrono::minutes session_timeout{5};  // Short timeout
        bool require_session_reauth = true;
        
        // ✅ MANDATORY: Audit logging
        bool enable_audit_log = true;  // NEVER disable!
        bool enable_realtime_alerts = true;
        
        // ✅ MANDATORY: Command restrictions
        std::unordered_set<std::string> allowed_commands;
        std::unordered_set<std::string> blocked_commands{
            "rm", "del", "format", "shutdown", "reboot"
        };
        
        // Alert destinations
        std::string alert_webhook;
        std::string alert_email;
    };
    
    bool initialize(Server& server) override {
        // ✅ Verify security requirements
        if (!config_.require_2fa) {
            log_error("Remote Control REQUIRES 2FA!");
            return false;
        }
        
        if (config_.whitelist_ips.empty()) {
            log_error("Remote Control REQUIRES IP whitelist!");
            return false;
        }
        
        if (!config_.enable_audit_log) {
            log_error("Remote Control REQUIRES audit logging!");
            return false;
        }
        
        // ✅ Log CRITICAL security warning
        security_log("REMOTE_CONTROL_ENABLED", {
            {"severity", "CRITICAL"},
            {"2fa_enabled", config_.require_2fa},
            {"whitelist_ips", config_.whitelist_ips},
            {"session_timeout_minutes", config_.session_timeout.count()}
        });
        
        // ✅ Send real-time alert
        sendAlert(Alert{
            .level = AlertLevel::CRITICAL,
            .title = "Remote Control Extension Enabled",
            .message = "High-risk extension activated. Monitor closely.",
            .metadata = {{"whitelist_count", config_.whitelist_ips.size()}}
        });
        
        return true;
    }
    
    void handleInputCommand(Context& ctx) {
        // ✅ Multi-layer security checks
        
        // 1. Check IP whitelist
        auto client_ip = ctx.request().getClientIP();
        if (!isWhitelisted(client_ip)) {
            audit_log("REMOTE_CONTROL_BLOCKED", {
                {"reason", "ip_not_whitelisted"},
                {"ip", client_ip}
            });
            sendAlert(Alert{
                .level = AlertLevel::HIGH,
                .title = "Unauthorized Remote Control Attempt",
                .message = std::format("Blocked access from {}", client_ip)
            });
            return ctx.status(403).json({{"error", "Forbidden"}});
        }
        
        // 2. Check authentication
        auto user = ctx.get<User>("user");
        if (!user) {
            audit_log("REMOTE_CONTROL_BLOCKED", {
                {"reason", "not_authenticated"},
                {"ip", client_ip}
            });
            return ctx.status(401).json({{"error", "Unauthorized"}});
        }
        
        // 3. Check 2FA
        if (config_.require_2fa && !user->has_valid_2fa) {
            audit_log("REMOTE_CONTROL_BLOCKED", {
                {"reason", "2fa_required"},
                {"user", user->username}
            });
            return ctx.status(403).json({{"error", "2FA required"}});
        }
        
        // 4. Check client certificate
        if (config_.require_certificate && !ctx.hasValidCertificate()) {
            audit_log("REMOTE_CONTROL_BLOCKED", {
                {"reason", "invalid_certificate"},
                {"user", user->username}
            });
            return ctx.status(403).json({{"error", "Invalid certificate"}});
        }
        
        // 5. Parse command
        auto command_json = ctx.request().getBody();
        auto command = parseCommand(command_json);
        
        // 6. Validate command
        if (!isCommandAllowed(command)) {
            audit_log("REMOTE_CONTROL_BLOCKED", {
                {"reason", "command_not_allowed"},
                {"user", user->username},
                {"command", command.type}
            });
            sendAlert(Alert{
                .level = AlertLevel::HIGH,
                .title = "Blocked Dangerous Command",
                .message = std::format("User {} attempted: {}", 
                    user->username, command.type)
            });
            return ctx.status(403).json({{"error", "Command not allowed"}});
        }
        
        // ✅ Log BEFORE execution
        audit_log("REMOTE_CONTROL_EXECUTE", {
            {"user", user->username},
            {"ip", client_ip},
            {"command", command.type},
            {"parameters", command.params}
        });
        
        // Execute command (sandboxed)
        auto result = executeCommandSandboxed(command);
        
        // ✅ Log AFTER execution
        audit_log("REMOTE_CONTROL_COMPLETE", {
            {"user", user->username},
            {"command", command.type},
            {"success", result.success}
        });
        
        ctx.json(result.toJSON());
    }
};
```

---

## 6. Incident Response

### 6.1 Security Incident Response Plan

**Phase 1: Detection**
- Monitor logs for anomalies
- Set up automated alerts
- Review access patterns

**Phase 2: Containment**
- Isolate affected systems
- Disable dangerous extensions
- Terminate suspicious sessions
- Enable additional logging

**Phase 3: Investigation**
- Analyze audit logs
- Identify attack vector
- Determine scope of compromise
- Preserve evidence

**Phase 4: Eradication**
- Remove malicious access
- Patch vulnerabilities
- Update security policies
- Strengthen authentication

**Phase 5: Recovery**
- Restore from clean backups
- Verify system integrity
- Re-enable services gradually
- Monitor closely

**Phase 6: Lessons Learned**
- Document incident
- Update procedures
- Train team
- Implement preventive measures

### 6.2 Emergency Response Contacts

```yaml
# incident_response.yaml

primary_contact:
  name: "Security Team"
  email: "security@company.com"
  phone: "+1-555-0123"
  on_call: 24/7

escalation:
  level_1:
    - "System Administrator"
    - "DevOps Team"
  level_2:
    - "Security Manager"
    - "IT Director"
  level_3:
    - "CISO"
    - "CTO"

external_support:
  incident_response: "ir@securityfirm.com"
  legal: "legal@company.com"
  law_enforcement: "cybercrime@agency.gov"
```

---

## 7. Security Checklist

### 7.1 Pre-Deployment Checklist

- [ ] HTTPS/TLS enabled with strong ciphers
- [ ] Strong authentication configured (JWT/OAuth2)
- [ ] Rate limiting enabled
- [ ] Security headers plugin active
- [ ] Dangerous extensions disabled
- [ ] Firewall rules configured
- [ ] Systemd security settings applied
- [ ] Reverse proxy configured (Nginx/HAProxy)
- [ ] Audit logging enabled
- [ ] Monitoring and alerting set up
- [ ] Backup and recovery plan documented
- [ ] Incident response plan in place
- [ ] Security team trained
- [ ] Penetration testing completed
- [ ] Code review performed
- [ ] Dependencies up to date
- [ ] Security documentation complete

### 7.2 Regular Security Maintenance

**Daily**
- [ ] Review access logs
- [ ] Check alert dashboard
- [ ] Monitor system resources

**Weekly**
- [ ] Review audit logs
- [ ] Check for security updates
- [ ] Verify backup integrity

**Monthly**
- [ ] Update dependencies
- [ ] Review access permissions
- [ ] Security awareness training
- [ ] Test incident response procedures

**Quarterly**
- [ ] Penetration testing
- [ ] Security audit
- [ ] Policy review
- [ ] Disaster recovery drill

### 7.3 Security Metrics to Track

```cpp
struct SecurityMetrics {
    // Authentication
    uint64_t failed_login_attempts = 0;
    uint64_t successful_logins = 0;
    uint64_t blocked_ips = 0;
    
    // Attack detection
    uint64_t path_traversal_attempts = 0;
    uint64_t rate_limit_violations = 0;
    uint64_t suspicious_requests = 0;
    
    // System health
    double avg_response_time_ms = 0.0;
    uint64_t active_connections = 0;
    uint64_t total_requests = 0;
    
    // Alert statistics
    uint64_t critical_alerts = 0;
    uint64_t high_alerts = 0;
    uint64_t medium_alerts = 0;
};
```

---

## Summary

### Key Takeaways

1. **Defense in Depth**: Multiple layers of security
2. **Least Privilege**: Grant minimum necessary permissions
3. **Audit Everything**: Comprehensive logging is critical
4. **Dangerous Extensions**: Only use in controlled environments
5. **Stay Updated**: Regular patches and updates

### For General-Purpose Server

✅ **Enable by Default:**
- TLS/HTTPS
- Rate limiting
- Security headers
- Authentication
- Audit logging

❌ **Disable by Default:**
- Screen sharing
- Remote control
- Any extension that could be abused

### Remember

Security is not a feature—it's a fundamental requirement. Always err on the side of caution, especially with extensions that expose system resources or allow remote access.

**When in doubt, disable dangerous features and consult security experts.**