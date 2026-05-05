#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include "wingman/auth.hpp"

namespace wingman {

class HTTPServer {
public:
    HTTPServer(const std::string& dbPath = "wingman.db", int port = 8080);
    ~HTTPServer();

    void start();
    void stop();

    void setPort(int port) { port_ = port; }

private:
    int port_;
    std::unique_ptr<AuthManager> authManager_;
    crow::SimpleApp app_;

    void setupRoutes();

    // Helper to add CORS headers to responses
    void addCorsHeaders(crow::response& resp);

    // Middleware for authentication
    bool authenticate(const crow::request& req, User& user);

    // Route handlers
    crow::response handleLogin(const crow::request& req);
    crow::response handleLogout(const crow::request& req);
    crow::response handleStatus(const crow::request& req);
    crow::response handleScripts(const crow::request& req);
    crow::response handleWindows(const crow::request& req);
    crow::response handleSettings(const crow::request& req);

    // API response helpers
    static crow::response jsonResponse(const nlohmann::json& data);
    static crow::response errorResponse(const std::string& message);
};

} // namespace wingman
