#include "wingman/client/controllers/api.hpp"
#include "wingman/version.hpp"
#include <spdlog/spdlog.h>
#include <optional>
#include <chrono>

namespace wingman::client::controllers {

// ========== 辅助方法 ==========

drogon::HttpResponsePtr ApiCtrl::success(const nlohmann::json& data) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        nlohmann::json{{"success", true}, {"data", data}}
    );
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Content-Type", "application/json; charset=utf-8");
    return resp;
}

drogon::HttpResponsePtr ApiCtrl::error(int code, const std::string& message) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        nlohmann::json{{"success", false}, {"error", message}}
    );
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(code));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Content-Type", "application/json; charset=utf-8");
    return resp;
}

std::optional<nlohmann::json> ApiCtrl::getJson(const drogon::HttpRequestPtr& req) {
    try {
        auto body = req->body();
        if (body.empty()) {
            return nlohmann::json::object();
        }
        return nlohmann::json::parse(body);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse JSON: {}", e.what());
        return std::nullopt;
    }
}

// ========== API 端点实现 ==========

void ApiCtrl::index(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json info;
    info["name"] = "Wingman WebSocket Server";
    info["version"] = WINGMAN_VERSION;
    info["description"] = "Game Automation Programmable Control Engine";
    info["endpoints"] = {
        {"/api/status", "GET - 获取系统状态"},
        {"/api/version", "GET - 获取版本信息"},
        {"/api/health", "GET - 健康检查"},
        {"/api/scripts", "GET - 列出所有脚本"},
        {"/api/scripts/{id}", "GET - 获取脚本详情"},
        {"/api/scripts/{id}/start", "POST - 启动脚本"},
        {"/api/scripts/{id}/stop", "POST - 停止脚本"},
        {"/api/scripts/{id}", "PUT - 保存脚本"},
        {"/api/scripts/{id}", "DELETE - 删除脚本"},
        {"/ws", "WebSocket - WebSocket 连接端点"}
    };
    callback(success(info));
}

void ApiCtrl::health(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    nlohmann::json data;
    data["status"] = "ok";
    data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    callback(success(data));
}

void ApiCtrl::getStatus(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "system.getStatus";
    rpcReq.params = nlohmann::json::object();

    auto rpcResp = SystemCtrl::getStatus(rpcReq);

    if (rpcResp.success) {
        callback(success(rpcResp.result));
    } else {
        callback(500, rpcResp.error);
    }
}

void ApiCtrl::getVersion(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "system.getVersion";
    rpcReq.params = nlohmann::json::object();

    auto rpcResp = SystemCtrl::getVersion(rpcReq);

    if (rpcResp.success) {
        callback(success(rpcResp.result));
    } else {
        callback(500, rpcResp.error);
    }
}

void ApiCtrl::listScripts(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "script.list";
    rpcReq.params = nlohmann::json::object();

    auto rpcResp = ScriptCtrl::list(rpcReq);

    if (rpcResp.success) {
        callback(success(rpcResp.result));
    } else {
        callback(500, rpcResp.error);
    }
}

void ApiCtrl::getScript(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& id) {
    // 首先获取脚本列表找到对应路径
    RpcRequest listReq;
    listReq.id = "http_list";
    listReq.method = "script.list";
    listReq.params = nlohmann::json::object();

    auto listResp = ScriptCtrl::list(listReq);
    if (!listResp.success) {
        callback(error(500, "Failed to list scripts"));
        return;
    }

    // 查找匹配的脚本
    std::string scriptPath;
    if (listResp.result.contains("scripts")) {
        for (const auto& script : listResp.result["scripts"]) {
            if (script["id"] == id) {
                scriptPath = script["path"];
                break;
            }
        }
    }

    if (scriptPath.empty()) {
        callback(error(404, "Script not found: " + id));
        return;
    }

    // 获取脚本内容
    RpcRequest contentReq;
    contentReq.id = "http_content";
    contentReq.method = "script.getContent";
    contentReq.params = nlohmann::json{{"path", scriptPath}};

    auto contentResp = ScriptCtrl::getContent(contentReq);
    if (contentResp.success) {
        nlohmann::json data;
        data["id"] = id;
        data["path"] = scriptPath;
        data["content"] = contentResp.result["content"];
        callback(success(data));
    } else {
        callback(error(500, contentResp.error));
    }
}

void ApiCtrl::startScript(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          const std::string& id) {
    // 构建启动请求
    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "script.start";
    rpcReq.params = nlohmann::json{{"path", "scripts/" + id + ".lua"}};

    auto rpcResp = ScriptCtrl::start(rpcReq);

    if (rpcResp.success) {
        callback(success(rpcResp.result));
    } else {
        callback(error(500, rpcResp.error));
    }
}

void ApiCtrl::stopScript(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& id) {
    // 从路径参数获取 scriptId
    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "script.stop";
    rpcReq.params = nlohmann::json{{"scriptId", "script_" + id}};

    auto rpcResp = ScriptCtrl::stop(rpcReq);

    if (rpcResp.success) {
        callback(success(rpcResp.result));
    } else {
        callback(error(500, rpcResp.error));
    }
}

void ApiCtrl::saveScript(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& id) {
    auto jsonBody = getJson(req);
    if (!jsonBody) {
        callback(error(400, "Invalid JSON body"));
        return;
    }

    std::string content = jsonBody->value("content", "");
    std::string scriptPath = "scripts/" + id + ".lua";

    RpcRequest rpcReq;
    rpcReq.id = "http_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    rpcReq.method = "script.save";
    rpcReq.params = nlohmann::json{{"path", scriptPath}, {"content", content}};

    auto rpcResp = ScriptCtrl::save(rpcReq);

    if (rpcResp.success) {
        nlohmann::json data;
        data["id"] = id;
        data["path"] = scriptPath;
        callback(success(data));
    } else {
        callback(error(500, rpcResp.error));
    }
}

void ApiCtrl::deleteScript(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                           const std::string& id) {
    // TODO: 实现 ScriptCtrl::remove 方法
    callback(error(501, "Not implemented yet"));
}

} // namespace wingman::client::controllers
