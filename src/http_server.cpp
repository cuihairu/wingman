// Define WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
#define WIN32_LEAN_AND_MEAN

#include "wingman/http_server.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace wingman {

HTTPServer::HTTPServer(const std::string& dbPath, int port)
    : port_(port), authManager_(std::make_unique<AuthManager>(dbPath)) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setupRoutes() {
    // Public routes
    CROW_ROUTE(app_, "/api/auth/login").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleLogin(req);
            addCorsHeaders(resp);
            return resp;
        });

    // Protected routes
    CROW_ROUTE(app_, "/api/auth/logout").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleLogout(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/status").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleStatus(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/scripts").methods("GET"_method, "POST"_method, "DELETE"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScripts(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/windows").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleWindows(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/settings").methods("GET"_method, "PUT"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleSettings(req);
            addCorsHeaders(resp);
            return resp;
        });

    // Health check
    CROW_ROUTE(app_, "/api/health").methods("GET"_method)(
        [](const crow::request& req) {
            nlohmann::json j;
            j["status"] = "ok";
            j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            crow::response resp = jsonResponse(j);
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

    // 404 handler
    CROW_ROUTE(app_, ".*")(
        [](const crow::request& req) {
            crow::response resp = errorResponse("Not found");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });
}

void HTTPServer::addCorsHeaders(crow::response& resp) {
    resp.add_header("Access-Control-Allow-Origin", "*");
    resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
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

        auto userOpt = authManager_->login(username, password);
        if (!userOpt) {
            nlohmann::json j;
            j["success"] = false;
            j["message"] = "Invalid credentials";
            crow::response resp(401, j.dump());
            return resp;
        }

        User user = *userOpt;
        std::string token = authManager_->generateToken(user);

        nlohmann::json j;
        j["success"] = true;
        j["token"] = token;
        j["user"] = {
            {"id", user.id},
            {"username", user.username},
            {"role", user.role}
        };

        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleLogout(const crow::request& req) {
    nlohmann::json j;
    j["success"] = true;
    j["message"] = "Logged out";
    return jsonResponse(j);
}

crow::response HTTPServer::handleStatus(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    nlohmann::json j;
    j["success"] = true;
    nlohmann::json data;
    data["totalScripts"] = 5;
    data["runningScripts"] = 2;
    data["totalWindows"] = 12;
    data["cpuUsage"] = 15;
    data["memoryUsage"] = 256;
    j["data"] = data;

    return jsonResponse(j);
}

crow::response HTTPServer::handleScripts(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    if (req.method == "GET"_method) {
        nlohmann::json j;
        j["success"] = true;
        nlohmann::json arr = nlohmann::json::array();
        nlohmann::json script1;
        script1["name"] = "auto_heal.lua";
        script1["path"] = "scripts/auto_heal.lua";
        script1["status"] = "running";
        script1["lastRun"] = 1234567890;
        arr.push_back(script1);

        nlohmann::json script2;
        script2["name"] = "auto_farm.lua";
        script2["path"] = "scripts/auto_farm.lua";
        script2["status"] = "stopped";
        arr.push_back(script2);

        j["data"] = arr;
        return jsonResponse(j);
    } else if (req.method == "POST"_method) {
        nlohmann::json j;
        j["success"] = true;
        return jsonResponse(j);
    } else if (req.method == "DELETE"_method) {
        nlohmann::json j;
        j["success"] = true;
        return jsonResponse(j);
    }

    return errorResponse("Method not allowed");
}

crow::response HTTPServer::handleWindows(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    nlohmann::json j;
    j["success"] = true;
    nlohmann::json arr = nlohmann::json::array();
    nlohmann::json win;
    win["handle"] = 12345;
    win["title"] = "Notepad";
    nlohmann::json bounds;
    bounds["x"] = 100;
    bounds["y"] = 100;
    bounds["width"] = 800;
    bounds["height"] = 600;
    win["bounds"] = bounds;
    win["isForeground"] = true;
    arr.push_back(win);
    j["data"] = arr;

    return jsonResponse(j);
}

crow::response HTTPServer::handleSettings(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    if (req.method == "GET"_method) {
        nlohmann::json j;
        j["success"] = true;
        nlohmann::json data;
        data["serverPort"] = 8888;
        data["httpPort"] = 8080;
        data["logLevel"] = "info";
        data["maxScripts"] = 10;
        j["data"] = data;
        return jsonResponse(j);
    } else if (req.method == "PUT"_method) {
        nlohmann::json j;
        j["success"] = true;
        return jsonResponse(j);
    }

    return errorResponse("Method not allowed");
}

crow::response HTTPServer::jsonResponse(const nlohmann::json& data) {
    std::string body = data.dump();
    crow::response resp(body);
    resp.add_header("Content-Type", "application/json");
    return resp;
}

crow::response HTTPServer::errorResponse(const std::string& message) {
    nlohmann::json j;
    j["success"] = false;
    j["error"] = message;
    std::string body = j.dump();
    crow::response resp(body);
    resp.add_header("Content-Type", "application/json");
    return resp;
}

void HTTPServer::start() {
    if (!authManager_->init()) {
        spdlog::error("Failed to initialize auth manager");
        return;
    }

    setupRoutes();

    spdlog::info("HTTP Server starting on port {}", port_);

    // Run in a separate thread
    std::thread([this]() {
        app_.port(port_).multithreaded().run();
    }).detach();

    spdlog::info("HTTP Server started on http://localhost:{}", port_);
}

void HTTPServer::stop() {
    app_.stop();
}

} // namespace wingman
