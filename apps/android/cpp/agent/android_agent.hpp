#pragma once

// Android Agent 装配层（A1，docs/android-agent-design.md §5.2）。
//
// 组合两块既有能力：
//  - runtime::RemoteClient（复用桌面 runtime，重连/outbox/心跳/register 现成）
//  - ScriptRunner（本目录，sol2 执行器）
//
// 职责：注册身份（agentId/hostname/platform/capabilities）、命令分发
// （run_script/stop_script）、脚本日志回传（agent.event "script_output"，
// 字段与桌面一致：scriptId/message/level，server 落 ExecutionLog 并转发
// Dashboard，无需 Dashboard 改动）。
//
// 本层不触碰 JNI：jni_bridge.cpp 是唯一 JNIEnv 翻译层。

#include <memory>
#include <mutex>
#include <string>

#include "platform/android/android_host_bridge.hpp"
#include "wingman/agentcore/remote_client.hpp"
#include "wingman/androidagent/script_runner.hpp"

namespace wingman::android {

class AndroidAgent {
public:
    struct Config {
        std::string serverIp = "127.0.0.1";
        int serverPort = 8888;
        std::string agentId;    // 空：RemoteClient 自动生成
        std::string hostname;   // 空：取系统主机名
        std::string platform = "android";
        // 附加注册字段 JSON 对象字符串（可空），如 {"apiLevel":34,"abi":"arm64-v8a"}，
        // 合并进 agent.register 的 capabilities 键
        std::string capabilitiesJson;
        // 注册鉴权 token（可空；server 侧 WINGMAN_AGENT_TOKENS 开启鉴权时必填，
        // 见 docs/agent-token-auth-design.md §4.2）
        std::string authToken;
        // 模板图根目录（A2：wingman.vision.findImage 相对路径解析根；
        // 经 configJson 由 jni_bridge 传入）
        std::string filesDir;
    };

    AndroidAgent() = default;
    ~AndroidAgent();

    AndroidAgent(const AndroidAgent&) = delete;
    AndroidAgent& operator=(const AndroidAgent&) = delete;

    // 启动（幂等：已在跑则返回 true）
    bool start(const Config& config);
    // 停止（先停脚本再停链路；幂等）
    void stop();

    bool isRunning() const;
    bool isConnected() const;
    bool isScriptRunning() const;

    // 状态 JSON（供 JNI nativeStatus / Kotlin UI）：
    // {"running":bool,"connected":bool,"script":{"running":bool,"executionId":str}}
    std::string statusJson() const;

private:
    runtime::CommandResult onCommand(const std::string& command,
                                     const runtime::CommandData& data);
    void sendScriptOutput(const std::string& scriptId, const std::string& message,
                          const std::string& level);
    std::string nextExecutionId();

    std::unique_ptr<runtime::RemoteClient> client_;
    ScriptRunner runner_;
    Config config_;
    mutable std::mutex mutex_;
    // 当前脚本回传用的 scriptId（onCommand 启动成功时设置；单脚本模型）
    std::string activeScriptId_;
    std::uint64_t executionSeq_ = 0;
};

} // namespace wingman::android
