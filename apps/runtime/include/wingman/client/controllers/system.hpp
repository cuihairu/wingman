#pragma once

#include "websocket.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace wingman::client::controllers {

/// 系统控制器
/// 处理系统相关的 RPC 调用
class SystemCtrl {
public:
    /// 获取系统状态
    static RpcResponse getStatus(const RpcRequest& req);

    /// 获取版本信息
    static RpcResponse getVersion(const RpcRequest& req);

    /// 退出应用
    static RpcResponse quit(const RpcRequest& req);
};

} // namespace wingman::client::controllers
