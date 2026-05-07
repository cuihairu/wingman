#include "wingman/lua_http.hpp"
#include <spdlog/spdlog.h>

namespace wingman {

LuaHTTPServer::LuaHTTPServer(int port) : port_(port) {}

LuaHTTPServer::~LuaHTTPServer() {
    stop();
}

void LuaHTTPServer::get(const std::string& path, LuaContextHandler handler) {
    getRoutes_[path] = std::move(handler);
    spdlog::info("Registered Lua GET route: {}", path);
}

void LuaHTTPServer::post(const std::string& path, LuaContextHandler handler) {
    postRoutes_[path] = std::move(handler);
    spdlog::info("Registered Lua POST route: {}", path);
}

void LuaHTTPServer::put(const std::string& path, LuaContextHandler handler) {
    putRoutes_[path] = std::move(handler);
    spdlog::info("Registered Lua PUT route: {}", path);
}

void LuaHTTPServer::delete_(const std::string& path, LuaContextHandler handler) {
    deleteRoutes_[path] = std::move(handler);
    spdlog::info("Registered Lua DELETE route: {}", path);
}

void LuaHTTPServer::use(LuaMiddleware middleware) {
    middlewares_.push_back(std::move(middleware));
}

crow::response LuaHTTPServer::handleRequest(const crow::request& req, LuaContextHandler handler) {
    LuaHttpContext ctx(req);

    // Run middleware chain
    for (const auto& mw : middlewares_) {
        if (!mw(ctx)) {
            ctx.Error("Middleware stopped request", 403);
            return std::move(ctx.response);
        }
    }

    // Run handler
    handler(ctx);

    // Add CORS header
    ctx.response.add_header("Access-Control-Allow-Origin", "*");

    return std::move(ctx.response);
}

void LuaHTTPServer::setupRoutes() {
    // Root path handler (no parameters)
    CROW_ROUTE(app_, "/").methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method)(
        [this](const crow::request& req) {
            std::string method = crow::method_name(req.method);

            LuaContextHandler handler = nullptr;
            if (method == "GET") {
                auto it = getRoutes_.find("/");
                if (it != getRoutes_.end()) handler = it->second;
            } else if (method == "POST") {
                auto it = postRoutes_.find("/");
                if (it != postRoutes_.end()) handler = it->second;
            } else if (method == "PUT") {
                auto it = putRoutes_.find("/");
                if (it != putRoutes_.end()) handler = it->second;
            } else if (method == "DELETE") {
                auto it = deleteRoutes_.find("/");
                if (it != deleteRoutes_.end()) handler = it->second;
            }

            if (handler) {
                return handleRequest(req, handler);
            }

            crow::response resp(404, R"({"success":false,"error":"Not found"})");
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

    // Path-based handler
    CROW_ROUTE(app_, "/<path>").methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method)(
        [this](const crow::request& req, const std::string& path) {
            std::string fullPath = "/" + path;
            std::string method = crow::method_name(req.method);

            LuaContextHandler handler = nullptr;
            if (method == "GET") {
                auto it = getRoutes_.find(fullPath);
                if (it != getRoutes_.end()) handler = it->second;
            } else if (method == "POST") {
                auto it = postRoutes_.find(fullPath);
                if (it != postRoutes_.end()) handler = it->second;
            } else if (method == "PUT") {
                auto it = putRoutes_.find(fullPath);
                if (it != putRoutes_.end()) handler = it->second;
            } else if (method == "DELETE") {
                auto it = deleteRoutes_.find(fullPath);
                if (it != deleteRoutes_.end()) handler = it->second;
            }

            if (handler) {
                return handleRequest(req, handler);
            }

            crow::response resp(404, R"({"success":false,"error":"Not found"})");
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

    // OPTIONS for CORS
    CROW_ROUTE(app_, "/").methods("OPTIONS"_method)(
        [](const crow::request& req) {
            crow::response resp;
            resp.add_header("Access-Control-Allow-Origin", "*");
            resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp.code = 200;
            return resp;
        });

    CROW_ROUTE(app_, "/<path>").methods("OPTIONS"_method)(
        [](const crow::request& req, const std::string& /*path*/) {
            crow::response resp;
            resp.add_header("Access-Control-Allow-Origin", "*");
            resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp.code = 200;
            return resp;
        });
}

void LuaHTTPServer::start() {
    setupRoutes();

    spdlog::info("Lua HTTP Server starting on port {}", port_);

    std::thread([this]() {
        app_.port(port_).multithreaded().run();
    }).detach();

    spdlog::info("Lua HTTP Server started on http://localhost:{}", port_);
}

void LuaHTTPServer::stop() {
    app_.stop();
}

} // namespace wingman
