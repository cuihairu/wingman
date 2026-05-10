#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <functional>

// Forward declarations
namespace wingman::client::controllers {
    struct RpcRequest;
    struct RpcResponse;
    class ScriptCtrl;
    class SystemCtrl;
}

namespace wingman::client::controllers {

/// HTTP RESTful API 控制器
/// 提供 HTTP 接口供 Tauri UI 调用
class ApiCtrl : public drogon::HttpController<ApiCtrl> {
public:
    METHOD_LIST_BEGIN
        // 系统状态相关
        ADD_METHOD_TO(ApiCtrl::index, "/", drogon::Get);
        ADD_METHOD_TO(ApiCtrl::getStatus, "/api/status", drogon::Get);
        ADD_METHOD_TO(ApiCtrl::getVersion, "/api/version", drogon::Get);
        ADD_METHOD_TO(ApiCtrl::health, "/api/health", drogon::Get);

        // 脚本管理相关
        ADD_METHOD_TO(ApiCtrl::listScripts, "/api/scripts", drogon::Get);
        ADD_METHOD_TO(ApiCtrl::getScript, "/api/scripts/{id}", drogon::Get);
        ADD_METHOD_TO(ApiCtrl::startScript, "/api/scripts/{id}/start", drogon::Post);
        ADD_METHOD_TO(ApiCtrl::stopScript, "/api/scripts/{id}/stop", drogon::Post);
        ADD_METHOD_TO(ApiCtrl::saveScript, "/api/scripts/{id}", drogon::Put);
        ADD_METHOD_TO(ApiCtrl::deleteScript, "/api/scripts/{id}", drogon::Delete);
    METHOD_LIST_END

    /// 首页
    void index(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /// 健康检查
    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /// 获取系统状态
    void getStatus(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /// 获取版本信息
    void getVersion(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /// 列出所有脚本
    void listScripts(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /// 获取单个脚本详情
    void getScript(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& id);

    /// 启动脚本
    void startScript(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& id);

    /// 停止脚本
    void stopScript(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    /// 保存脚本
    void saveScript(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& id);

    /// 删除脚本
    void deleteScript(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& id);

private:
    /// 构建成功响应
    static drogon::HttpResponsePtr success(const nlohmann::json& data);

    /// 构建错误响应
    static drogon::HttpResponsePtr error(int code, const std::string& message);

    /// 从请求获取 JSON body
    static std::optional<nlohmann::json> getJson(const drogon::HttpRequestPtr& req);
};

} // namespace wingman::client::controllers
