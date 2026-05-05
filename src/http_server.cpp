#include "wingman/http_server.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace wingman {

HTTPServer::HTTPServer(const std::string& dbPath, int port)
    : port_(port), authManager_(std::make_unique<AuthManager>(dbPath)),
      app_(std::make_unique<crow::SimpleApp>()) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setupCors() {
    // CORS middleware
    auto& cors = app_->get_middleware<crow::CORSHandler>();

    // Allow all origins for development (restrict in production)
    cors.global
        .headers("Content-Type", "Authorization", "X-Requested-With")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .origin("*");
}

void HTTPServer::setupRoutes() {
    // Public routes
    CROW_ROUTE(*app_, "/api/auth/login").methods("POST"_method)(
        [this](const crow::request& req) { return handleLogin(req); });

    // Protected routes
    CROW_ROUTE(*app_, "/api/auth/logout").methods("POST"_method)(
        [this](const crow::request& req) { return handleLogout(req); });

    CROW_ROUTE(*app_, "/api/status").methods("GET"_method)(
        [this](const crow::request& req) { return handleStatus(req); });

    CROW_ROUTE(*app_, "/api/scripts").methods("GET"_method, "POST"_method, "DELETE"_method)(
        [this](const crow::request& req) { return handleScripts(req); });

    CROW_ROUTE(*app_, "/api/windows").methods("GET"_method)(
        [this](const crow::request& req) { return handleWindows(req); });

    CROW_ROUTE(*app_, "/api/settings").methods("GET"_method, "PUT"_method)(
        [this](const crow::request& req) { return handleSettings(req); });

    // Health check
    CROW_ROUTE(*app_, "/api/health").methods("GET"_method)(
        [](const crow::request& req) {
            nlohmann::json j;
            j["status"] = "ok";
            j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            return jsonResponse(j);
        });

    // 404 handler
    CROW_ROUTE(*app_, ".*")(
        [](const crow::request& req) {
            return errorResponse("Not found");
        });
}

bool HTTPServer::authenticate(const crow::request& req, User& user) {
    auto authHeader = req.get_header_value("Authorization");
    if (authHeader.empty()) {
        return false;
    }

    // Expected format: "Bearer <token>"
    if (authHeader.size() < 7 || authHeader.substr(0, 7) != "Bearer ") {
        return false;
    }

    std::string token = authHeader.substr(7);
    auto verifiedUser = authManager_->verifyToken(token);
    if (verifiedUser) {
        user = *verifiedUser;
        return true;
    }

    return false;
}

crow::response HTTPServer::handleLogin(const crow::request& req) {
    try {
        auto body = nlohmann::json::parse(req.body);
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        if (username.empty() || password.empty()) {
            return errorResponse("Username and password required");
        }

        auto user = authManager_->login(username, password);
        if (!user) {
            nlohmann::json j;
            j["success"] = false;
            j["message"] = "Invalid credentials";
            return crow::response(401, j.dump());
        }

        std::string token = authManager_->generateToken(*user);

        nlohmann::json j;
        j["success"] = true;
        j["token"] = token;
        j["user"] = {
            {"id", user->id},
            {"username", user->username},
            {"role", user->role}
        };

        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleLogout(const crow::request& req) {
    // In a real implementation, invalidate the token
    nlohmann::json j;
    j["success"] = true;
    j["message"] = "Logged out";
    return jsonResponse(j);
}

crow::response HTTPServer::handleStatus(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401, errorResponse("Unauthorized"));
    }

    nlohmann::json j;
    j["success"] = true;
    j["data"] = {
        {"totalScripts", 5},
        {"runningScripts", 2},
        {"totalWindows", 12},
        {"cpuUsage", 15},
        {"memoryUsage", 256}
    };

    return jsonResponse(j);
}

crow::response HTTPServer::handleScripts(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401, errorResponse("Unauthorized"));
    }

    if (req.method == "GET"_method) {
        nlohmann::json j;
        j["success"] = true;
        j["data"] = nlohmann::json::array();
        j["data"].push_back({
            {"name", "auto_heal.lua"},
            {"path", "scripts/auto_heal.lua"},
            {"status", "running"},
            {"lastRun", 1234567890}
        });
        j["data"].push_back({
            {"name", "auto_farm.lua"},
            {"path", "scripts/auto_farm.lua"},
            {"status", "stopped"}
        });
        return jsonResponse(j);
    } else if (req.method == "POST"_method) {
        // Start script
        return jsonResponse({{"success", true}});
    } else if (req.method == "DELETE"_method) {
        // Stop script
        return jsonResponse({{"success", true}});
    }

    return errorResponse("Method not allowed");
}

crow::response HTTPServer::handleWindows(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401, errorResponse("Unauthorized"));
    }

    nlohmann::json j;
    j["success"] = true;
    j["data"] = nlohmann::json::array();
    j["data"].push_back({
        {"handle", 12345},
        {"title", "Notepad"},
        {"bounds", {{"x", 100}, {"y", 100}, {"width", 800}, {"height", 600}}},
        {"isForeground", true}
    });

    return jsonResponse(j);
}

crow::response HTTPServer::handleSettings(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401, errorResponse("Unauthorized"));
    }

    if (req.method == "GET"_method) {
        nlohmann::json j;
        j["success"] = true;
        j["data"] = {
            {"serverPort", 8888},
            {"httpPort", 8080},
            {"logLevel", "info"},
            {"maxScripts", 10}
        };
        return jsonResponse(j);
    } else if (req.method == "PUT"_method) {
        // Update settings
        return jsonResponse({{"success", true}});
    }

    return errorResponse("Method not allowed");
}

crow::response HTTPServer::jsonResponse(const nlohmann::json& data) {
    crow::response resp(data.dump());
    resp.add_header("Content-Type", "application/json");
    return resp;
}

crow::response HTTPServer::errorResponse(const std::string& message) {
    nlohmann::json j;
    j["success"] = false;
    j["error"] = message;
    crow::response resp(j.dump());
    resp.add_header("Content-Type", "application/json");
    return resp;
}

void HTTPServer::start() {
    if (!authManager_->init()) {
        spdlog::error("Failed to initialize auth manager");
        return;
    }

    setupCors();
    setupRoutes();

    spdlog::info("HTTP Server starting on port {}", port_);

    // Run in a separate thread
    std::thread([this]() {
        app_->port(port_).multithreaded().run();
    }).detach();

    spdlog::info("HTTP Server started on http://localhost:{}", port_);
}

void HTTPServer::stop() {
    if (app_) {
        app_->stop();
    }
}

} // namespace wingman
