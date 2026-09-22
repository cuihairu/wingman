#pragma once

// 远程链路配置（自 apps/runtime/config.hpp 下沉，桌面 runtime 与 Android
// agent 同源使用）。命名空间保持 wingman::runtime，与既有调用方/文档一致。

#include <string>

namespace wingman::runtime {

struct RemoteClientConfig {
    std::string serverIp = "127.0.0.1";
    int serverPort = 8888;
    int reconnectInterval = 5;      // 秒（退避基数）
    int maxReconnectInterval = 60;  // 秒（退避上限）
    int heartbeatInterval = 30;     // 秒
    int connectTimeout = 10;        // 秒
    // 注册鉴权 token（[remote] register_token；空 = 不携带，server 鉴权默认关闭）
    std::string registerToken;
};

} // namespace wingman::runtime
