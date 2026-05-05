// Define WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
#define WIN32_LEAN_AND_MEAN

#include "wingman/http_server.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace wingman {

HTTPServer::HTTPServer(const std::string& dbPath, int port, const std::string& staticDir)
    : port_(port), staticDir_(staticDir), authManager_(std::make_unique<AuthManager>(dbPath)) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setupRoutes() {
    // 静态文件服务 - 提供 dashboard 前端
    CROW_ROUTE(app_, "/")
    ([this](const crow::request& req) {
        crow::response resp;
        std::string indexPath = staticDir_ + "/index.html";
        std::ifstream indexFile(indexPath);
        if (indexFile.is_open()) {
            std::string content((std::istreambuf_iterator<char>(indexFile)), std::istreambuf_iterator<char>());
            resp.body = content;
            resp.set_header("Content-Type", "text/html; charset=utf-8");
            resp.code = 200;
        } else {
            resp.body = "<html><body><h3>Dashboard not found at: " + staticDir_ + "</h3></body></html>";
            resp.set_header("Content-Type", "text/html; charset=utf-8");
            resp.code = 404;
        }
        return resp;
    });

    // 静态资源路由
    CROW_ROUTE(app_, "/<path>")
    ([this](const crow::request& req, const std::string& path) {
        // 如果是 API 请求，返回 404
        if (path.find("api/") == 0) {
            crow::response resp("API endpoint not found: " + path);
            resp.code = 404;
            return resp;
        }

        // 尝试提供静态文件
        std::string filePath = staticDir_ + "/" + path;
        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            crow::response resp(content);

            // 设置正确的 Content-Type
            if (path.find(".html") != std::string::npos) {
                resp.set_header("Content-Type", "text/html; charset=utf-8");
            } else if (path.find(".js") != std::string::npos) {
                resp.set_header("Content-Type", "application/javascript; charset=utf-8");
            } else if (path.find(".css") != std::string::npos) {
                resp.set_header("Content-Type", "text/css; charset=utf-8");
            } else if (path.find(".svg") != std::string::npos) {
                resp.set_header("Content-Type", "image/svg+xml");
            } else if (path.find(".png") != std::string::npos) {
                resp.set_header("Content-Type", "image/png");
            } else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) {
                resp.set_header("Content-Type", "image/jpeg");
            } else {
                resp.set_header("Content-Type", "application/octet-stream");
            }

            return resp;
        }

        // 文件不存在，返回 index.html (SPA 路由支持)
        std::string indexPath = staticDir_ + "/index.html";
        std::ifstream indexFile(indexPath);
        if (indexFile.is_open()) {
            std::string content((std::istreambuf_iterator<char>(indexFile)), std::istreambuf_iterator<char>());
            crow::response resp(content);
            resp.set_header("Content-Type", "text/html; charset=utf-8");
            return resp;
        }

        crow::response resp("File not found: " + path);
        resp.code = 404;
        return resp;
    });

    // Public routes - /api/v1/
    CROW_ROUTE(app_, "/api/v1/auth/login").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleLogin(req);
            addCorsHeaders(resp);
            return resp;
        });

    // Protected routes - /api/v1/
    CROW_ROUTE(app_, "/api/v1/auth/logout").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleLogout(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/status").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleStatus(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/scripts").methods("GET"_method, "POST"_method, "DELETE"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScripts(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/windows").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleWindows(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/settings").methods("GET"_method, "PUT"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleSettings(req);
            addCorsHeaders(resp);
            return resp;
        });

    // Health check
    CROW_ROUTE(app_, "/api/v1/health").methods("GET"_method)(
        [](const crow::request& req) {
            nlohmann::json j;
            j["status"] = "ok";
            j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            crow::response resp = jsonResponse(j);
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

    // 兼容旧路径 /api/（重定向到 /api/v1/）
    CROW_ROUTE(app_, "/api/auth/login").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleLogin(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/status").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleStatus(req);
            addCorsHeaders(resp);
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
        data["httpPort"] = 9527;
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

    // Run in a separate thread, bind to 0.0.0.0 for network access
    std::thread([this]() {
        try {
            spdlog::info("HTTP Server thread starting, binding to 0.0.0.0:{}", port_);
            app_.bindaddr("0.0.0.0").port(port_).multithreaded().run();
            spdlog::info("HTTP Server thread exited normally");
        } catch (const std::exception& e) {
            spdlog::error("HTTP Server thread exception: {}", e.what());
        } catch (...) {
            spdlog::error("HTTP Server thread unknown exception");
        }
    }).detach();

    spdlog::info("HTTP Server started on http://0.0.0.0:{}", port_);
    spdlog::info("start() function returning to caller");
}

void HTTPServer::stop() {
    app_.stop();
}

} // namespace wingman
