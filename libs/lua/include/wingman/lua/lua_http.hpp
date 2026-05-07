#pragma once

#include <crow.h>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "wingman/auth.hpp"

namespace wingman {

// HTTP Context for Lua handlers (similar to Go's gin.Context)
class LuaHttpContext {
public:
    const crow::request& request;
    crow::response response;
    std::optional<User> user;
    std::unordered_map<std::string, std::string> session;
    std::unordered_map<std::string, std::string> storage;  // Request-scoped storage

    LuaHttpContext(const crow::request& req) : request(req), response(200) {}

    // Request helpers
    std::string getBody() const { return request.body; }
    std::string getHeader(const std::string& key) const { return request.get_header_value(key); }
    std::string getMethod() const { return crow::method_name(request.method); }
    std::string getURL() const { return request.url; }
    std::string getQuery(const std::string& key) const {
        auto url = crow::query_string(request.url_params);
        return url.get(key);
    }

    nlohmann::json getJSON() const {
        if (request.body.empty()) return nlohmann::json{};
        return nlohmann::json::parse(request.body);
    }

    // Response helpers
    void setStatus(int code) { response.code = code; }
    void setHeader(const std::string& key, const std::string& value) {
        response.add_header(key, value);
    }
    void setJSON(const nlohmann::json& data) {
        response.body = data.dump();
        setHeader("Content-Type", "application/json");
    }
    void setString(const std::string& body) {
        response.body = body;
    }

    // Session helpers
    void setSession(const std::string& key, const std::string& value) {
        session[key] = value;
    }
    std::string getSession(const std::string& key) const {
        auto it = session.find(key);
        return it != session.end() ? it->second : "";
    }

    // Storage helpers (for middleware to pass data)
    void set(const std::string& key, const std::string& value) {
        storage[key] = value;
    }
    std::string get(const std::string& key) const {
        auto it = storage.find(key);
        return it != storage.end() ? it->second : "";
    }

    // User helpers
    bool isAuthenticated() const { return user.has_value(); }
    std::string getUsername() const {
        return user.has_value() ? user->username : "";
    }
    std::string getUserRole() const {
        return user.has_value() ? user->role : "";
    }

    // JSON response helpers (similar to ctx.JSON in Go)
    crow::response JSON(const nlohmann::json& data) {
        setJSON(data);
        return std::move(response);
    }

    crow::response Error(const std::string& message, int code = 400) {
        nlohmann::json j;
        j["success"] = false;
        j["error"] = message;
        setStatus(code);
        setJSON(j);
        return std::move(response);
    }

    crow::response OK(const nlohmann::json& data = nlohmann::json{}) {
        nlohmann::json j;
        j["success"] = true;
        if (!data.is_null()) {
            j["data"] = data;
        }
        setJSON(j);
        return std::move(response);
    }
};

// Lua HTTP route handler type
using LuaContextHandler = std::function<void(LuaHttpContext&)>;

// Middleware type
using LuaMiddleware = std::function<bool(LuaHttpContext&)>;  // return false to stop chain

// HTTP Server with Lua route support
class LuaHTTPServer {
public:
    explicit LuaHTTPServer(int port = 8081);
    ~LuaHTTPServer();

    void start();
    void stop();

    // Register routes (called during Lua script loading)
    void get(const std::string& path, LuaContextHandler handler);
    void post(const std::string& path, LuaContextHandler handler);
    void put(const std::string& path, LuaContextHandler handler);
    void delete_(const std::string& path, LuaContextHandler handler);

    // Register middleware
    void use(LuaMiddleware middleware);

private:
    int port_;
    crow::SimpleApp app_;

    std::vector<LuaMiddleware> middlewares_;
    std::unordered_map<std::string, LuaContextHandler> getRoutes_;
    std::unordered_map<std::string, LuaContextHandler> postRoutes_;
    std::unordered_map<std::string, LuaContextHandler> putRoutes_;
    std::unordered_map<std::string, LuaContextHandler> deleteRoutes_;

    void setupRoutes();
    crow::response handleRequest(const crow::request& req, LuaContextHandler handler);
};

} // namespace wingman
