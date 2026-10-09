#pragma once

// 远程链路配置（自 apps/agent/config.hpp 下沉，桌面 runtime 与 Android
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
    // challenge-response 注册鉴权（[remote] challenge_auth，A3-P2 §6.1；
    // 默认关 = P1 明文 token 兼容模式）。开启后注册携带 challenge:true 且
    // 明文 token 绝不过网，server 以 auth.challenge 下发 nonce 完成校验。
    // 旧 server（P1）不理解 challenge 字段会按缺 token 拒绝，故显式 opt-in。
    bool useChallengeAuth = false;
};

} // namespace wingman::runtime
