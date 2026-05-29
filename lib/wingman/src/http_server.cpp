// Define WIN32_LEAN_AND_MEAN to avoid WinSock conflicts
#define WIN32_LEAN_AND_MEAN

#include "wingman/http_server.hpp"
#include "wingman/debugger/debugger.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace wingman {

HTTPServer::HTTPServer(const std::string& dbPath, int port, const std::string& staticDir)
    : port_(port),
      staticDir_(staticDir),
      authManager_(std::make_unique<AuthManager>(dbPath)),
      scriptManager_(std::make_unique<ScriptManager>()) {

    // Scan and load all scripts in the scripts directory (multi-language support)
    std::string scriptsDir = "scripts";
    if (std::filesystem::exists(scriptsDir) && std::filesystem::is_directory(scriptsDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir)) {
            if (entry.is_regular_file()) {
                std::string scriptName = entry.path().stem().string();
                std::string scriptPath = entry.path().string();
                std::string lang = scriptManager_->detectLanguage(scriptPath);
                scriptManager_->loadScript(scriptName, scriptPath);
                spdlog::info("Loaded script: {} from {} [{}]", scriptName, scriptPath, lang);
            }
        }
    }

    // Set debugger event callback
    auto& debugger = wingman::getDebugger();
    debugger.setEventCallback([this](wingman::DebuggerEvent event, const std::string& file, int line) {
        std::string eventType;
        nlohmann::json data;

        switch (event) {
            case wingman::DebuggerEvent::BreakpointHit:
                eventType = "stopped";
                data["reason"] = "breakpoint";
                data["file"] = file;
                data["line"] = line;
                break;
            case wingman::DebuggerEvent::StepComplete:
                eventType = "stopped";
                data["reason"] = "step";
                data["file"] = file;
                data["line"] = line;
                break;
            case wingman::DebuggerEvent::Paused:
                eventType = "paused";
                break;
            case wingman::DebuggerEvent::Error:
                eventType = "error";
                break;
        }

        broadcastDebuggerEvent(eventType, data);
    });

    spdlog::info("[Debugger] Event callback registered");
}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setupWebSocketRoutes() {
    // WebSocket route - for real-time push
    CROW_WEBSOCKET_ROUTE(app_, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            onWSOpen(conn.get_shared_this());
        })
        .onmessage([this](const crow::request& req, crow::websocket::connection& conn, const std::string& message, bool is_binary) {
            onWSMessage(conn.get_shared_this(), message, is_binary);
        })
        .onclose([this](const crow::websocket::connection& conn, const std::string& reason) {
            onWSClose(conn.get_shared_this(), reason);
        })
        .onerror([this](crow::websocket::connection& conn, const std::string& error) {
            spdlog::error("[WS] Error: {}", error);
        });
}

void HTTPServer::setupRoutes() {
    // Set up WebSocket routes
    setupWebSocketRoutes();

    // Static file serving - provide dashboard frontend
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

    // Static resource route
    CROW_ROUTE(app_, "/<path>")
    ([this](const crow::request& req, const std::string& path) {
        // If API request, return 404
        if (path.find("api/") == 0) {
            crow::response resp("API endpoint not found: " + path);
            resp.code = 404;
            return resp;
        }

        // Try to serve static file
        std::string filePath = staticDir_ + "/" + path;
        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            crow::response resp(content);

            // Set correct Content-Type
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

        // File not found, return index.html (SPA routing support)
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

    // Script content and execution routes
    CROW_ROUTE(app_, "/api/v1/scripts/content").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScriptContent(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/scripts/save").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScriptSave(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/scripts/run").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScriptRun(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/scripts/stop").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScriptStop(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/v1/scripts/logs").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleScriptLogs(req);
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

    // ========== Debugger API Routes ==========
    // Use /api/debugger prefix (without v1) for VS Code extension compatibility

    CROW_ROUTE(app_, "/api/debugger/connect").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerConnect(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/command").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerCommand(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/breakpoints").methods("POST"_method, "GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerBreakpoints(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/stacktrace").methods("GET"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerStackTrace(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/variables").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerVariables(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/evaluate").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerEvaluate(req);
            addCorsHeaders(resp);
            return resp;
        });

    CROW_ROUTE(app_, "/api/debugger/setvariable").methods("POST"_method)(
        [this](const crow::request& req) {
            crow::response resp = handleDebuggerSetVariable(req);
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

    // Compatible with legacy /api/ path (redirect to /api/v1/)
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
        // Get script list
        nlohmann::json j;
        j["success"] = true;
        nlohmann::json arr = nlohmann::json::array();

        auto scriptNames = scriptManager_->getScriptNames();
        for (const auto& name : scriptNames) {
            auto* info = scriptManager_->getScriptInfo(name);
            if (info) {
                nlohmann::json script;
                script["id"] = name;
                script["name"] = name + ".lua";
                script["path"] = info->config.path;
                script["description"] = "";
                script["size"] = std::filesystem::file_size(info->config.path);
                script["modifiedTime"] = info->lastModified;
                script["isRunning"] = (info->state == ScriptState::running);
                arr.push_back(script);
            }
        }

        j["data"] = arr;
        return jsonResponse(j);
    } else if (req.method == "POST"_method) {
        // Create new script
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string name = body.value("name", "");
            std::string description = body.value("description", "");

            if (name.empty() || name.find(".lua") == std::string::npos) {
                return errorResponse("Script name must end with .lua");
            }

            std::string scriptName = name.substr(0, name.length() - 4); // Remove .lua
            std::string scriptPath = "scripts/" + name;

            // Create script file
            std::ofstream scriptFile(scriptPath);
            if (!scriptFile.is_open()) {
                return errorResponse("Failed to create script file");
            }

            scriptFile << "-- " << name << "\n";
            scriptFile << "-- " << description << "\n\n";
            scriptFile << "function main()\n";
            scriptFile << "    print(\"Hello, Wingman!\")\n";
            scriptFile << "end\n\n";
            scriptFile << "main()\n";
            scriptFile.close();

            // Load script
            if (scriptManager_->loadScript(scriptName, scriptPath)) {
                nlohmann::json j;
                j["success"] = true;
                j["data"] = {
                    {"id", scriptName},
                    {"name", name},
                    {"path", scriptPath}
                };
                return jsonResponse(j);
            } else {
                return errorResponse("Failed to load script");
            }
        } catch (const std::exception& e) {
            return errorResponse(e.what());
        }
    } else if (req.method == "DELETE"_method) {
        // Delete script
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string path = body.value("path", "");

            if (path.empty()) {
                return errorResponse("Path is required");
            }

            // Unload script
            std::string scriptName = std::filesystem::path(path).stem().string();
            scriptManager_->unloadScript(scriptName);

            // Delete file
            if (std::filesystem::remove(path)) {
                nlohmann::json j;
                j["success"] = true;
                return jsonResponse(j);
            } else {
                return errorResponse("Failed to delete script file");
            }
        } catch (const std::exception& e) {
            return errorResponse(e.what());
        }
    }

    return errorResponse("Method not allowed");
}

crow::response HTTPServer::handleScriptContent(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        std::string path = body.value("path", "");

        if (path.empty()) {
            return errorResponse("Path is required");
        }

        // Read file content
        std::ifstream file(path);
        if (!file.is_open()) {
            return errorResponse("Failed to open script file");
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        nlohmann::json j;
        j["success"] = true;
        j["data"] = content;
        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleScriptSave(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        std::string path = body.value("path", "");
        std::string content = body.value("content", "");

        if (path.empty()) {
            return errorResponse("Path is required");
        }

        // Write file
        std::ofstream file(path);
        if (!file.is_open()) {
            return errorResponse("Failed to open script file for writing");
        }

        file << content;
        file.close();

        // Reload script
        std::string scriptName = std::filesystem::path(path).stem().string();
        scriptManager_->reloadScript(scriptName);

        nlohmann::json j;
        j["success"] = true;
        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleScriptRun(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        std::string path = body.value("path", "");

        if (path.empty()) {
            return errorResponse("Path is required");
        }

        std::string scriptName = std::filesystem::path(path).stem().string();

        if (scriptManager_->runScript(scriptName)) {
            nlohmann::json j;
            j["success"] = true;
            j["data"] = {
                {"executionId", scriptName}
            };
            return jsonResponse(j);
        } else {
            return errorResponse("Failed to run script");
        }
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleScriptStop(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    try {
        auto body = nlohmann::json::parse(req.body);
        std::string executionId = body.value("executionId", "");

        if (executionId.empty()) {
            return errorResponse("Execution ID is required");
        }

        if (scriptManager_->stopScript(executionId)) {
            nlohmann::json j;
            j["success"] = true;
            return jsonResponse(j);
        } else {
            return errorResponse("Failed to stop script");
        }
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleScriptLogs(const crow::request& req) {
    User user;
    if (!authenticate(req, user)) {
        return crow::response(401);
    }

    // TODO: Implement log collection functionality
    // Currently returns empty array
    nlohmann::json j;
    j["success"] = true;
    j["data"] = nlohmann::json::array();
    return jsonResponse(j);
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
    // Stop heartbeat thread
    wsHeartbeatRunning_ = false;
    if (wsHeartbeatThread_.joinable()) {
        wsHeartbeatThread_.join();
    }

    // Close all WebSocket connections
    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        for (auto& ws : wsConnections_) {
            ws->close();
        }
        wsConnections_.clear();
    }

    app_.stop();
}

// ========== WebSocket Implementation ==========

void HTTPServer::onWSOpen(std::shared_ptr<crow::websocket::connection> conn) {
    std::string connId = "ws_" + std::to_string(++wsConnectionIdCounter_);
    auto wsConn = std::make_shared<WSConnection>(conn, connId);

    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        wsConnections_.insert(wsConn);
    }

    spdlog::info("[WS] Connection opened: {} (total: {})", connId, wsConnections_.size());

    // Send welcome message
    nlohmann::json welcome;
    welcome["type"] = "connected";
    welcome["connectionId"] = connId;
    welcome["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    wsConn->send(welcome.dump());

    // Start heartbeat thread (if not already started)
    if (!wsHeartbeatRunning_) {
        wsHeartbeatRunning_ = true;
        wsHeartbeatThread_ = std::thread([this]() {
            spdlog::info("[WS] Heartbeat thread started");
            while (wsHeartbeatRunning_) {
                std::this_thread::sleep_for(std::chrono::seconds(30));

                std::lock_guard<std::mutex> lock(wsMutex_);
                auto now = std::chrono::system_clock::now();
                std::vector<std::shared_ptr<WSConnection>> toRemove;

                for (auto& ws : wsConnections_) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ws->lastPing).count();
                    if (elapsed > 60) { // 60-second timeout
                        spdlog::warn("[WS] Connection {} timeout, closing", ws->id);
                        ws->close();
                        toRemove.push_back(ws);
                    } else {
                        // Send heartbeat
                        nlohmann::json ping;
                        ping["type"] = "ping";
                        ping["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
                        ws->send(ping.dump());
                    }
                }

                // Remove timed-out connections
                for (auto& ws : toRemove) {
                    wsConnections_.erase(ws);
                }
            }
            spdlog::info("[WS] Heartbeat thread stopped");
        });
    }
}

void HTTPServer::onWSMessage(std::shared_ptr<crow::websocket::connection> conn, const std::string& message, bool is_binary) {
    try {
        auto j = nlohmann::json::parse(message);
        std::string type = j.value("type", "");

        spdlog::debug("[WS] Received message type: {}", type);

        // Get connection ID
        std::string connId;
        {
            std::lock_guard<std::mutex> lock(wsMutex_);
            for (auto& ws : wsConnections_) {
                if (ws->connection.get() == conn.get()) {
                    connId = ws->id;
                    break;
                }
            }
        }

        if (connId.empty()) {
            spdlog::warn("[WS] Message from unknown connection");
            return;
        }

        // Handle pong response
        if (type == "pong") {
            std::lock_guard<std::mutex> lock(wsMutex_);
            for (auto& ws : wsConnections_) {
                if (ws->connection.get() == conn.get()) {
                    ws->lastPing = std::chrono::system_clock::now();
                    break;
                }
            }
        }
        // Join room
        else if (type == "join_room") {
            std::string roomId = j.value("roomId", "");
            if (!roomId.empty()) {
                joinRoom(connId, roomId);
            }
        }
        // Leave room
        else if (type == "leave_room") {
            leaveRoom(connId);
        }
        // Room message
        else if (type == "room_message") {
            std::string roomId = j.value("roomId", "");
            std::string action = j.value("action", "");
            nlohmann::json data = j.value("data", nlohmann::json::object());

            if (!roomId.empty()) {
                nlohmann::json msg;
                msg["type"] = "room";
                msg["event"] = "message";
                msg["roomId"] = roomId;
                msg["action"] = action;
                msg["data"] = data;
                msg["from"] = connId;
                msg["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

                // Broadcast to other room members
                broadcastToRoom(roomId, connId, msg);

                // Also send back to sender (acknowledgment)
                {
                    std::lock_guard<std::mutex> lock(wsMutex_);
                    for (auto& ws : wsConnections_) {
                        if (ws->connection.get() == conn.get()) {
                            nlohmann::json ack;
                            ack["type"] = "room";
                            ack["event"] = "message_sent";
                            ack["roomId"] = roomId;
                            ack["action"] = action;
                            ws->send(ack.dump());
                            break;
                        }
                    }
                }

                spdlog::info("[Room] Message from {} to room {}: action={}", connId, roomId, action);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("[WS] Message parse error: {}", e.what());
    }
}

void HTTPServer::onWSClose(std::shared_ptr<crow::websocket::connection> conn, const std::string& reason) {
    std::string connId;
    std::string currentRoom;

    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        for (auto it = wsConnections_.begin(); it != wsConnections_.end(); ++it) {
            if ((*it)->connection.get() == conn.get()) {
                connId = (*it)->id;
                currentRoom = (*it)->currentRoom;
                spdlog::info("[WS] Connection closed: {} (reason: {}, total: {})",
                    (*it)->id, reason, wsConnections_.size() - 1);
                wsConnections_.erase(it);
                break;
            }
        }
    }

    // Leave room
    if (!connId.empty() && !currentRoom.empty()) {
        leaveRoom(connId);
    }
    }
}

void HTTPServer::broadcast(const WSMessage& message) {
    std::string msgStr = message.toString();
    std::lock_guard<std::mutex> lock(wsMutex_);

    for (auto& ws : wsConnections_) {
        if (ws->isOpen()) {
            try {
                ws->send(msgStr);
            } catch (const std::exception& e) {
                spdlog::error("[WS] Broadcast error to {}: {}", ws->id, e.what());
            }
        }
    }
}

void HTTPServer::broadcastAgentEvent(const std::string& eventType, const nlohmann::json& agentData) {
    nlohmann::json j;
    j["type"] = "agent";
    j["event"] = eventType;
    j["data"] = agentData;
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    std::string msgStr = j.dump();
    std::lock_guard<std::mutex> lock(wsMutex_);

    for (auto& ws : wsConnections_) {
        if (ws->isOpen()) {
            try {
                ws->send(msgStr);
            } catch (const std::exception& e) {
                spdlog::error("[WS] Broadcast error to {}: {}", ws->id, e.what());
            }
        }
    }
}

void HTTPServer::broadcastWorkflowEvent(const std::string& eventType, const nlohmann::json& workflowData) {
    nlohmann::json j;
    j["type"] = "workflow";
    j["event"] = eventType;
    j["data"] = workflowData;
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    std::string msgStr = j.dump();
    std::lock_guard<std::mutex> lock(wsMutex_);

    for (auto& ws : wsConnections_) {
        if (ws->isOpen()) {
            try {
                ws->send(msgStr);
            } catch (const std::exception& e) {
                spdlog::error("[WS] Broadcast error to {}: {}", ws->id, e->what());
            }
        }
    }
}

void HTTPServer::broadcastDebuggerEvent(const std::string& eventType, const nlohmann::json& data) {
    nlohmann::json j;
    j["type"] = "debugger";
    j["event"] = eventType;
    j["data"] = data;
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    std::string msgStr = j.dump();
    std::lock_guard<std::mutex> lock(wsMutex_);

    for (auto& ws : wsConnections_) {
        if (ws->isOpen()) {
            try {
                ws->send(msgStr);
            } catch (const std::exception& e) {
                spdlog::error("[WS] Broadcast error to {}: {}", ws->id, e.what());
            }
        }
    }

    spdlog::debug("[Debugger] Broadcast event: {}", eventType);
}

    std::string msgStr = j.dump();
    std::lock_guard<std::mutex> lock(wsMutex_);

    for (auto& ws : wsConnections_) {
        if (ws->isOpen()) {
            try {
                ws->send(msgStr);
            } catch (const std::exception& e) {
                spdlog::error("[WS] Broadcast error to {}: {}", ws->id, e.what());
            }
        }
    }
}

// ========== Room Management ==========

void HTTPServer::joinRoom(const std::string& connId, const std::string& roomId) {
    std::lock_guard<std::mutex> wsLock(wsMutex_);
    std::lock_guard<std::mutex> roomLock(roomMutex_);

    // Find connection
    std::shared_ptr<WSConnection> targetConn;
    for (auto& ws : wsConnections_) {
        if (ws->id == connId) {
            targetConn = ws;
            break;
        }
    }

    if (!targetConn) {
        spdlog::warn("[Room] Connection {} not found", connId);
        return;
    }

    // If previously in a room, leave first
    if (!targetConn->currentRoom.empty()) {
        leaveRoom(connId);
    }

    // Join new room
    auto& room = rooms_[roomId];
    room.roomId = roomId;
    room.connectionIds.insert(connId);
    targetConn->currentRoom = roomId;

    spdlog::info("[Room] Connection {} joined room {} (size: {})", connId, roomId, room.size());

    // Send join success message
    nlohmann::json msg;
    msg["type"] = "room";
    msg["event"] = "joined";
    msg["roomId"] = roomId;
    msg["connectionId"] = connId;
    msg["roomSize"] = static_cast<int>(room.size());
    targetConn->send(msg.dump());

    // Notify other room members
    nlohmann::json notify;
    notify["type"] = "room";
    notify["event"] = "user_joined";
    notify["roomId"] = roomId;
    notify["connectionId"] = connId;
    broadcastToRoom(roomId, connId, notify);
}

void HTTPServer::leaveRoom(const std::string& connId) {
    std::lock_guard<std::mutex> wsLock(wsMutex_);
    std::lock_guard<std::mutex> roomLock(roomMutex_);

    // Find connection
    std::shared_ptr<WSConnection> targetConn;
    for (auto& ws : wsConnections_) {
        if (ws->id == connId) {
            targetConn = ws;
            break;
        }
    }

    if (!targetConn || targetConn->currentRoom.empty()) {
        return;
    }

    std::string roomId = targetConn->currentRoom;

    // Remove from room
    auto it = rooms_.find(roomId);
    if (it != rooms_.end()) {
        it->second.connectionIds.erase(connId);

        // Notify other room members
        nlohmann::json notify;
        notify["type"] = "room";
        notify["event"] = "user_left";
        notify["roomId"] = roomId;
        notify["connectionId"] = connId;
        broadcastToRoom(roomId, connId, notify);

        // If room is empty, delete it
        if (it->second.empty()) {
            rooms_.erase(it);
            spdlog::info("[Room] Room {} removed (empty)", roomId);
        }
    }

    targetConn->currentRoom = "";
    spdlog::info("[Room] Connection {} left room {}", connId, roomId);
}

void HTTPServer::sendToRoom(const std::string& roomId, const nlohmann::json& message) {
    std::lock_guard<std::mutex> wsLock(wsMutex_);
    std::lock_guard<std::mutex> roomLock(roomMutex_);

    auto it = rooms_.find(roomId);
    if (it == rooms_.end()) {
        spdlog::warn("[Room] Room {} not found", roomId);
        return;
    }

    std::string msgStr = message.dump();
    for (const auto& connId : it->second.connectionIds) {
        for (auto& ws : wsConnections_) {
            if (ws->id == connId && ws->isOpen()) {
                try {
                    ws->send(msgStr);
                } catch (const std::exception& e) {
                    spdlog::error("[Room] Send error to {}: {}", ws->id, e.what());
                }
                break;
            }
        }
    }
}

void HTTPServer::broadcastToRoom(const std::string& roomId, const std::string& excludeConnId, const nlohmann::json& message) {
    std::lock_guard<std::mutex> wsLock(wsMutex_);
    std::lock_guard<std::mutex> roomLock(roomMutex_);

    auto it = rooms_.find(roomId);
    if (it == rooms_.end()) {
        return;
    }

    std::string msgStr = message.dump();
    for (const auto& connId : it->second.connectionIds) {
        if (connId == excludeConnId) continue;  // Skip sender

        for (auto& ws : wsConnections_) {
            if (ws->id == connId && ws->isOpen()) {
                try {
                    ws->send(msgStr);
                } catch (const std::exception& e) {
                    spdlog::error("[Room] Broadcast error to {}: {}", ws->id, e.what());
                }
                break;
            }
        }
    }
}

std::vector<std::string> HTTPServer::getRoomConnections(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(roomMutex_);

    auto it = rooms_.find(roomId);
    if (it == rooms_.end()) {
        return {};
    }

    std::vector<std::string> result;
    for (const auto& connId : it->second.connectionIds) {
        result.push_back(connId);
    }
    return result;
}

// ========== Debugger API Handlers ==========

crow::response HTTPServer::handleDebuggerConnect(const crow::request& req) {
    // Debugger connection request (no authentication required, simplified VS Code extension connection)
    nlohmann::json j;
    j["success"] = true;
    j["message"] = "Debugger connected";
    j["version"] = "0.1.0";
    spdlog::info("[Debugger] Client connected from {}", req.remote_address);
    return jsonResponse(j);
}

crow::response HTTPServer::handleDebuggerCommand(const crow::request& req) {
    try {
        auto body = nlohmann::json::parse(req.body);
        std::string command = body.value("command", "continue");

        auto& debugger = wingman::getDebugger();

        if (command == "continue") {
            debugger.continueExecution();
        } else if (command == "stepOver") {
            debugger.stepOver();
        } else if (command == "stepIn") {
            debugger.stepIn();
        } else if (command == "pause") {
            debugger.pause();
        } else {
            return errorResponse("Unknown command: " + command);
        }

        nlohmann::json j;
        j["success"] = true;
        j["state"] = debugStateToString(debugger.getState());
        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleDebuggerBreakpoints(const crow::request& req) {
    try {
        auto& debugger = wingman::getDebugger();
        auto& bpManager = debugger.breakpoints();

        if (req.method == "GET"_method) {
            // Get all breakpoints
            auto bps = bpManager.getAllBreakpoints();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& bp : bps) {
                nlohmann::json j;
                j["id"] = bp.id;
                j["file"] = bp.file;
                j["line"] = bp.line;
                j["enabled"] = (bp.state == BreakpointState::Enabled);
                j["hitCount"] = bp.hitCount;
                arr.push_back(j);
            }

            nlohmann::json response;
            response["success"] = true;
            response["breakpoints"] = arr;
            return jsonResponse(response);
        } else {
            // POST - Set breakpoints
            auto body = nlohmann::json::parse(req.body);
            std::string file = body.value("file", "");
            auto breakpoints = body.value("breakpoints", nlohmann::json::array());

            // Clear old breakpoints for this file
            if (!file.empty()) {
                bpManager.removeBreakpointsForFile(file);
            }

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& bp : breakpoints) {
                int line = bp.value("line", 0);
                if (line > 0 && !file.empty()) {
                    size_t id = bpManager.addBreakpoint(file, line);

                    nlohmann::json j;
                    j["id"] = id;
                    j["verified"] = true;
                    j["line"] = line;
                    arr.push_back(j);
                }
            }

            nlohmann::json response;
            response["success"] = true;
            response["breakpoints"] = arr;
            return jsonResponse(response);
        }
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleDebuggerStackTrace(const crow::request& req) {
    try {
        auto& debugger = wingman::getDebugger();
        auto frames = debugger.getStackTrace();

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& frame : frames) {
            arr.push_back(frame.toJson());
        }

        nlohmann::json j;
        j["success"] = true;
        j["stackFrames"] = arr;
        j["totalFrames"] = frames.size();
        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleDebuggerVariables(const crow::request& req) {
    try {
        auto body = nlohmann::json::parse(req.body);
        size_t reference = body.value("reference", 1);

        auto& debugger = wingman::getDebugger();
        auto vars = debugger.getVariables(reference);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& var : vars) {
            arr.push_back(var.toJson());
        }

        nlohmann::json j;
        j["success"] = true;
        j["variables"] = arr;
        return jsonResponse(j);
    } catch (const std::exception& e) {
        return errorResponse(e.what());
    }
}

crow::response HTTPServer::handleDebuggerEvaluate(const crow::request& req) {
    // Simplified version does not support expression evaluation
    nlohmann::json j;
    j["success"] = false;
    j["message"] = "Expression evaluation not supported in basic debugger";
    return jsonResponse(j);
}

crow::response HTTPServer::handleDebuggerSetVariable(const crow::request& req) {
    // Simplified version does not support variable modification
    nlohmann::json j;
    j["success"] = false;
    j["message"] = "Variable modification not supported in basic debugger";
    return jsonResponse(j);
}

} // namespace wingman
