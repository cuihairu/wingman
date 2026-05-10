#pragma once

#include "websocket.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace wingman::client::controllers {

/// 脚本信息
struct ScriptInfo {
    std::string id;
    std::string name;
    std::string path;
    size_t size = 0;
    bool isRunning = false;
};

/// 脚本控制器
/// 处理脚本相关的 RPC 调用
class ScriptCtrl {
public:
    /// 启动脚本
    static RpcResponse start(const RpcRequest& req);

    /// 停止脚本
    static RpcResponse stop(const RpcRequest& req);

    /// 列出脚本
    static RpcResponse list(const RpcRequest& req);

    /// 获取脚本内容
    static RpcResponse getContent(const RpcRequest& req);

    /// 保存脚本
    static RpcResponse save(const RpcRequest& req);

private:
    /// 扫描脚本目录
    static std::vector<ScriptInfo> scanScripts(const std::string& dir = "scripts");
};

} // namespace wingman::client::controllers
