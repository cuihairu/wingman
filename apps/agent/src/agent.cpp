#include "wingman/runtime/agent.hpp"
#include "wingman/platform/app_paths.hpp"
#include "wingman/platform/screen_factory.hpp"
#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/rpc/asset_handler.hpp"
#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"
#include "wingman/update_poller.hpp"
#include "wingman/version.hpp"
#include "wingman/window.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/event.hpp"
#include "wingman/runtime/local_ipc_server.hpp"
#include "wingman/agentcore/remote_client.hpp"
#include "wingman/runtime/rpc/config_handler.hpp"
#include "wingman/runtime/rpc/screenshot_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace wingman::runtime {

namespace {

// desktopCapabilities 桌面 runtime 的设备能力词汇（ADR: Capability System）。
// 与 server 侧 KnownCapabilities（orchestrator/server/internal/agent/
// capabilities.go）对齐；只声明本构建确实实现的能力，宁可少报不虚报：
//   screen.capture  ← script/screen 模块（capture/captureRegion/getPixel）
//   input.mouse/keyboard ← script/input 模块（click/move/keyDown/key/type）
//   window.enumerate/activate ← script/window 模块（find/activate/getForeground）
//   process.spawn  ← script/process 模块（start/wait/terminate）
//   vision.image/color ← script/vision 模块（findImage/findColor）
//   ocr            ← script/ocr 模块（含 OCR_* 触发条件）
// ml.onnx 仅当启用 WINGMAN_ENABLE_ML 编译时声明（默认 OFF 走 ml_stub，
// 不谎报推理能力）；input.touch/screen.stream 桌面不提供，永不声明。
std::vector<std::string> desktopCapabilities() {
    std::vector<std::string> caps = {
        "screen.capture",
        "input.mouse",
        "input.keyboard",
        "window.enumerate",
        "window.activate",
        "process.spawn",
        "vision.image",
        "vision.color",
        "ocr",
    };
#ifdef WINGMAN_ENABLE_ML
    caps.push_back("ml.onnx");
#endif
    return caps;
}

std::string scriptStateToString(ScriptState state) {
    switch (state) {
    case ScriptState::Loaded:
        return "loaded";
    case ScriptState::Running:
        return "running";
    case ScriptState::Paused:
        return "paused";
    case ScriptState::Stopped:
        return "stopped";
    case ScriptState::Error:
        return "error";
    case ScriptState::Unknown:
    default:
        return "unknown";
    }
}

// dispatchViaRpcDispatcher 把远程命令转成 RPC 请求经共享 dispatcher 执行。
// 遵循架构约束的 Dispatcher Reuse：本地 IPC 与远程 agent 命令共用同一套 handler
//（如 trigger.* / screenshot.*），CommandData 的字符串值尽量按 JSON 解析。
CommandResult dispatchViaRpcDispatcher(rpc::RpcDispatcher& dispatcher, const std::string& command,
    const CommandData& data, const std::string& errorPrefix)
{
    nlohmann::json params = nlohmann::json::object();
    for (const auto& [key, value] : data) {
        try {
            params[key] = nlohmann::json::parse(value);
        } catch (const std::exception&) {
            params[key] = value;
        }
    }

    nlohmann::json request = {
        {"type", "call"},
        {"id", "remote-" + command},
        {"method", command},
        {"params", params}
    };

    try {
        auto response = nlohmann::json::parse(dispatcher.dispatch(request.dump()));
        auto payload = response.value("data", nlohmann::json::object());
        if (payload.value("success", true) == false) {
            return CommandResult::error(payload.value("error", errorPrefix + " failed"));
        }
        if (payload.contains("result")) {
            return CommandResult::okData(payload["result"].dump());
        }
        return CommandResult::okData(payload.dump());
    } catch (const std::exception& e) {
        return CommandResult::error(errorPrefix + " failed: " + e.what());
    }
}

// 架构检测：与服务端 HandleLatest 的默认 arch 口径一致（amd64 / arm64）。
std::string detectArch() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "amd64";
#endif
}

// [update] 配置 → 轮询器配置。enabled 且 base_url 非空才启用；platform/arch
// 未配置时按本机架构自动探测（服务端同名字段可覆盖）。
UpdatePollConfig makeUpdatePollConfig(const UpdateConfig& u) {
    UpdatePollConfig cfg;
    cfg.baseUrl = (u.enabled && !u.baseUrl.empty()) ? u.baseUrl : std::string();
    cfg.channel = u.channel.empty() ? std::string("stable") : u.channel;
    cfg.platform = u.platform.empty() ? platform::platformName() : u.platform;
    cfg.arch = u.arch.empty() ? detectArch() : u.arch;
    cfg.localVersion = WINGMAN_VERSION;
    // download_dir 未配置时回退到用户数据目录下的 updates/ 子目录，
    // 避免启用更新后因下载目录缺失导致下载恒失败。
    cfg.downloadDir = u.downloadDir;
    if (cfg.downloadDir.empty()) {
        const auto data = platform::appDataDir();
        cfg.downloadDir = data.empty() ? std::string() : (data / "updates").string();
    }
    cfg.pollIntervalSeconds = u.pollIntervalSeconds;
    return cfg;
}

} // namespace

// ========== Agent 实现 ==========

class Agent::Impl {
public:
    AgentConfig config;
    RunMode mode = RunMode::Unknown;
    // initialize(configPath) 记录的配置文件路径；config.setRemote 写回时使用。
    // 经 initialize(AgentConfig) 构造时为空（不落盘，如测试/嵌入式场景）。
    std::string configPath;

    std::unique_ptr<RemoteClient> remoteClient;
    std::unique_ptr<StandaloneMode> standaloneMode;
    std::unique_ptr<LocalIpcServer> localIpcServer;
    // 自动更新轮询器（[update]，出站到 Go server；状态经事件流投给 GUI）
    std::unique_ptr<UpdatePoller> updatePoller;
    std::unique_ptr<rpc::RpcDispatcher> remoteDispatcher;
    std::unique_ptr<platform::IScreen> screen;
    // 共享触发器管理器：本地 IPC 与远程 agent 通道看到同一份触发器
    std::unique_ptr<TriggerManager> triggerManager;
};

Agent::Agent() : impl_(std::make_unique<Impl>()) {}

Agent::~Agent() {
    shutdown();
}

bool Agent::initialize(const std::string& configPath) {
    impl_->configPath = configPath;
    try {
        impl_->config = AgentConfig::loadFromFile(configPath);
    } catch (const std::exception& e) {
        // 配置文件不存在或加载失败，创建默认配置
        spdlog::warn("Failed to load config: {}, using default configuration", e.what());
        impl_->config = AgentConfig{};  // 使用默认值

        // 尝试保存默认配置文件
        if (impl_->config.saveToFile(configPath)) {
            spdlog::info("Created default config file: {}", configPath);
        } else {
            spdlog::warn("Failed to create default config file: {}", configPath);
        }
    }
    return initialize(impl_->config);
}

bool Agent::initialize(const AgentConfig& config) {
    impl_->config = config;
    impl_->mode = config.getRunMode();

    // 共享触发器管理器：命中时推送到 EventBuffer，由本地 IPC 缓冲与远程 sink
    //（initRemoteClient 中注册）分别转发给 GUI 与 Go server。
    impl_->triggerManager = std::make_unique<TriggerManager>();
    impl_->triggerManager->setOnFired([](const TriggerInstance& t) {
        EventBuffer::instance().push("trigger.fired", {
            {"id", t.id},
            {"name", t.config.name},
            {"triggered", t.triggered},
            {"lastTriggerTime", t.lastTriggerTime},
        });
    });

    auto caps = config.getCapabilities();

    spdlog::info("Initializing Agent with capabilities:");
    spdlog::info("  - Remote outbound: {}",
        hasCapability(caps, RunCapability::RemoteOutbound) ? "enabled" : "disabled");
    spdlog::info("  - Local IPC: {}",
        hasCapability(caps, RunCapability::LocalIpc) ? "enabled" : "disabled");
    spdlog::info("  - Standalone script: {}",
        hasCapability(caps, RunCapability::StandaloneScript) ? "enabled" : "disabled");

    // 初始化远端客户端（基于能力）
    if (hasCapability(caps, RunCapability::RemoteOutbound)) {
        if (!initRemoteClient()) {
            spdlog::error("Failed to initialize remote client");
            return false;
        }
    }

    // 初始化单机模式（即使仅用于本地 IPC 也需要 StandaloneMode 实例）
    if (hasCapability(caps, RunCapability::LocalIpc) ||
        hasCapability(caps, RunCapability::StandaloneScript)) {
        if (!initStandaloneMode()) {
            spdlog::error("Failed to initialize standalone mode");
            return false;
        }
    }

    return true;
}

void Agent::shutdown() {
    spdlog::info("Shutting down Agent");

    // 先摘除远程事件转发 sink：lambda 捕获 this，而 EventBuffer 是进程级
    // 单例——不摘除则 Agent 析构/重建后 push 事件会调用悬空回调
    EventBuffer::instance().setRemoteSink(nullptr);

    // 先停更新轮询器，避免 IPC 服务拆除期间仍有 update.* 事件入队
    if (impl_->updatePoller) {
        impl_->updatePoller->stop();
        impl_->updatePoller.reset();
    }

    if (impl_->remoteClient) {
        impl_->remoteClient->stop();
        impl_->remoteClient.reset();
    }

    if (impl_->localIpcServer) {
        impl_->localIpcServer->stop();
        impl_->localIpcServer.reset();
    }

    if (impl_->standaloneMode) {
        impl_->standaloneMode->stop();
        impl_->standaloneMode.reset();
    }

    running_.store(false);
}

bool Agent::start() {
    running_.store(true);

    auto caps = impl_->config.getCapabilities();
    bool success = true;

    // 启动远端客户端（基于能力）
    if (impl_->remoteClient && hasCapability(caps, RunCapability::RemoteOutbound)) {
        if (!impl_->remoteClient->start()) {
            spdlog::error("Failed to start remote client");
            success = false;
        }
    }

    // 启动单机模式（如果需要脚本执行）
    if (impl_->standaloneMode && hasCapability(caps, RunCapability::StandaloneScript)) {
        if (!impl_->standaloneMode->start()) {
            spdlog::error("Failed to start standalone mode");
            success = false;
        }
    }

    // 启动本地 IPC 服务器（基于能力；共享触发器管理器，远程/本地同一份触发器）
    if (impl_->standaloneMode && hasCapability(caps, RunCapability::LocalIpc)) {
        impl_->localIpcServer = std::make_unique<LocalIpcServer>(
            *impl_->standaloneMode, std::string(), impl_->triggerManager.get());
        // 注入 system.getStatus 上下文：GUI 心跳即可刷新远程链路徽标与 IPC 客户端状态
        auto* ipcServer = impl_->localIpcServer.get();
        auto* remoteClient = impl_->remoteClient.get();
        impl_->localIpcServer->setStatusProviders(rpc::RuntimeStatusProviders{
            [remoteClient]() -> bool {
                return remoteClient && remoteClient->isConnected();
            },
            [remoteClient]() -> std::string {
                return remoteClient ? remoteClient->connectionStateName() : "disabled";
            },
            [ipcServer]() -> bool {
                return ipcServer && ipcServer->isClientConnected();
            },
            [mode = impl_->mode]() -> int {
                return static_cast<int>(mode);
            },
        });
        // config.getRemote / config.setRemote：GUI 读写远程注册配置（地址 +
        // A3-P1 注册令牌），仅本地 IPC，无对应远程命令——远程配置不允许从
        // Go server 侧改写（见 docs/agent-token-auth-design.md §4.1）
        impl_->localIpcServer->setRemoteConfigAccess(rpc::RemoteConfigAccess{
            [this]() -> nlohmann::json {
                const auto& rc = impl_->config.remoteClient;
                return {
                    {"serverIp", rc.serverIp},
                    {"serverPort", rc.serverPort},
                    {"registerToken", rc.registerToken},
                    {"challengeAuth", rc.useChallengeAuth},
                };
            },
            [this](const nlohmann::json& req) -> std::string {
                const auto& cur = impl_->config.remoteClient;
                RemoteClientConfig next = cur;

                if (req.contains("serverIp")) {
                    if (!req["serverIp"].is_string() || req["serverIp"].get<std::string>().empty()) {
                        return "serverIp 必须为非空字符串";
                    }
                    next.serverIp = req["serverIp"].get<std::string>();
                }
                if (req.contains("serverPort")) {
                    if (!req["serverPort"].is_number_integer()) {
                        return "serverPort 必须为整数";
                    }
                    const int port = req["serverPort"].get<int>();
                    if (port < 1 || port > 65535) {
                        return "serverPort 超出范围 (1-65535)";
                    }
                    next.serverPort = port;
                }
                if (req.contains("registerToken")) {
                    if (!req["registerToken"].is_string()) {
                        return "registerToken 必须为字符串";
                    }
                    auto token = req["registerToken"].get<std::string>();
                    if (token.size() > 256) {
                        return "registerToken 过长（上限 256 字符）";
                    }
                    next.registerToken = std::move(token);
                }
                if (req.contains("challengeAuth")) {
                    if (!req["challengeAuth"].is_boolean()) {
                        return "challengeAuth 必须为布尔值";
                    }
                    next.useChallengeAuth = req["challengeAuth"].get<bool>();
                }
                return applyRemoteConfig(std::move(next));
            },
        });
        // 自动更新轮询（[update]，ROADMAP M8.1）：仅 outbound HTTP 到 Go
        // server，不新增任何监听端口。状态经 update.status_changed 事件投给
        // GUI 事件流（runtime → EventBuffer → events.drain 拉取）。
        impl_->updatePoller = std::make_unique<UpdatePoller>(makeUpdatePollConfig(impl_->config.update));
        impl_->updatePoller->setOnStateChanged([](const UpdateStatus& status) {
            EventBuffer::instance().push("update.status_changed", status.toJson());
        });
        impl_->localIpcServer->setUpdatePoller(impl_->updatePoller.get());
        if (!impl_->config.update.baseUrl.empty() && impl_->config.update.enabled) {
            impl_->updatePoller->start();
        }
        if (!impl_->localIpcServer->start()) {
            spdlog::error("Failed to start local IPC server");
            success = false;
        }
    }

    if (success) {
        spdlog::info("Agent started successfully");
    }

    return success;
}

void Agent::stop() {
    spdlog::info("Stopping Agent");
    running_.store(false);

    if (impl_->remoteClient) impl_->remoteClient->stop();
    if (impl_->updatePoller) impl_->updatePoller->stop();
    if (impl_->localIpcServer) impl_->localIpcServer->stop();
    if (impl_->standaloneMode) impl_->standaloneMode->stop();
}

std::string Agent::applyRemoteConfig(RemoteClientConfig next) {
    if (!impl_->remoteClient) {
        return "远程出站未启用，无法应用远程配置";
    }

    // 整体重建远程客户端（serverIp/serverPort 是构造参数，token 需 start 前
    // 设置）。initRemoteClient 可重入：remoteDispatcher/screen 均为整体替换，
    // EventBuffer::setRemoteSink 为覆盖语义，无重复注册风险。
    const bool wasRunning = running_.load();
    impl_->remoteClient->stop();
    impl_->screen.reset();
    impl_->remoteClient.reset();
    impl_->remoteDispatcher.reset();

    impl_->config.remoteClient = std::move(next);
    if (!initRemoteClient()) {
        return "远程客户端重建失败";
    }
    if (wasRunning && !impl_->remoteClient->start()) {
        // server 暂时不可达是常态：配置保留（下方照常落盘），重启后继续重连
        if (impl_->configPath.empty() || impl_->config.saveToFile(impl_->configPath)) {
            return "远程客户端已按新配置写入，但连接启动失败（将在重启后重试）";
        }
        return "远程客户端连接启动失败，且写入配置文件失败";
    }
    spdlog::info("Remote config applied: {}:{} (token {})",
        impl_->config.remoteClient.serverIp, impl_->config.remoteClient.serverPort,
        impl_->config.remoteClient.registerToken.empty() ? "cleared" : "set");

    // 写回配置文件，runtime 重启后配置保持
    if (!impl_->configPath.empty() && !impl_->config.saveToFile(impl_->configPath)) {
        return "配置已应用（本次运行生效），但写入配置文件失败";
    }
    return "";
}

RemoteClient* Agent::getRemoteClient() {
    return impl_->remoteClient.get();
}

StandaloneMode* Agent::getStandaloneMode() {
    return impl_->standaloneMode.get();
}

RunMode Agent::getMode() const {
    return impl_->mode;
}

const AgentConfig& Agent::getConfig() const {
    return impl_->config;
}

bool Agent::initRemoteClient() {
    impl_->remoteClient = std::make_unique<RemoteClient>(impl_->config.remoteClient);
    // 注册鉴权 token（配置于 [remote] register_token；空 = 不携带，
    // 见 docs/agent-token-auth-design.md §4.1）
    if (!impl_->config.remoteClient.registerToken.empty()) {
        impl_->remoteClient->setAuthToken(impl_->config.remoteClient.registerToken);
    }

    // 设备能力上报（ADR: Capability System）：随 agent.register 携带
    // platform + capabilities 词汇；server 侧注册/持久化/展示（workflow
    // requires 调度匹配依赖此上报）。initRemoteClient 可重入（applyRemoteConfig
    // 热重建），每次重建重新注入。
    impl_->remoteClient->setRegisterMetadata({
        {"platform", "desktop"},
        {"capabilities", desktopCapabilities()},
    });

    impl_->remoteDispatcher = std::make_unique<rpc::RpcDispatcher>();
    impl_->screen = platform::createPlatformScreen();
    if (impl_->screen) {
        rpc::registerScreenshotHandlers(*impl_->remoteDispatcher, *impl_->screen);
    } else {
        rpc::registerScreenshotHandlers(*impl_->remoteDispatcher);
    }
    // trigger.* 复用本地 IPC 的 RPC handler（Dispatcher Reuse）
    if (impl_->triggerManager) {
        rpc::registerTriggerHandlers(*impl_->remoteDispatcher, *impl_->triggerManager);
    }
    // asset.sync：模板图片等资产落盘脚本目录的 assets/ 子目录（脚本以
    // assets/<name> 相对引用；响应 data 携带落盘绝对路径）
    rpc::registerAssetHandlers(*impl_->remoteDispatcher,
        (std::filesystem::path(impl_->config.standalone.scriptDir) / "assets").string());

    // 绑定命令回调，处理 server 下发的命令
    impl_->remoteClient->setCommandCallback([this](const std::string& command, const CommandData& data) {
        return handleRemoteCommand(command, data);
    });

    // 注册远程事件转发：EventBuffer 收到事件时，选择性地以 agent.event 推送到 Go server，
    // 由 server 广播到 Dashboard WS（listener.go handleEvent）。
    // 仅转发 server 关心的事件，避免高频 log.line 淹没 agent 上行链路。
    EventBuffer::instance().setRemoteSink([this](const std::string& method, const nlohmann::json& payload) {
        if (!impl_->remoteClient) return;
        if (method == "trigger.fired") {
            impl_->remoteClient->sendAgentEvent("trigger_fired", payload);
        } else if (method == "script.state_changed") {
            impl_->remoteClient->sendAgentEvent("script_state", payload);
        } else if (method == "script.output") {
            impl_->remoteClient->sendAgentEvent("script_output", payload);
        }
        // log.line / connection.state_changed 为本地相关，不转发
    });

    // 团队/收件箱桥（A4 多设备协同）：脚本 wingman.team.* 出站经 EventHub
    // team.outbound 事件转 agent 通道 Notify（team.join / team.leave / team.vote_*
    // / team.status_report / team.broadcast）；server 下行 inbox.message 转
    // team.inbound 事件交回 team 模块状态机（team_module 订阅），处理即 ack。
    // 订阅按名去重：initRemoteClient 可重入（applyRemoteConfig 热重建）不重复挂，
    // lambda 每次 fresh 读 impl_->remoteClient，重建后仍指向当前实例。
    if (!EventHub::instance().subscriptionByName("team.outbound_remote_bridge").has_value()) {
        EventHub::instance().subscribe("team.outbound", [this](const EventMessage& msg) {
            if (!impl_->remoteClient) return;
            const std::string type = msg.payload.value("type", "");
            if (type.empty()) return;
            impl_->remoteClient->sendNotify(type, msg.payload);
        }, "team.outbound_remote_bridge");
    }
    impl_->remoteClient->setNotifyHandler([this](const std::string& type, const nlohmann::json& body) {
        if (!impl_->remoteClient) return;
        if (type == "inbox.message") {
            // sendToInbox 的载荷自带 type 键；HTTP 团队推送的载荷没有，
            // 用 messageType 补齐后交 team 模块状态机分派（未知类型静默忽略）
            nlohmann::json payload = body.value("payload", nlohmann::json::object());
            if (!payload.contains("type")) {
                payload["type"] = body.value("messageType", "");
            }
            EventHub::instance().emit("team.inbound", payload);
            // 处理即确认：server GetMessages 只回未 ack 消息，不 ack 永久占缓冲
            impl_->remoteClient->sendNotify("inbox.ack", {
                {"msgId", body.value("msgId", "")},
            });
        }
        // team.error / inbox.*_ack 当前仅记录，无需分派
    });

    return true;
}

bool Agent::initStandaloneMode() {
    impl_->standaloneMode = std::make_unique<StandaloneMode>(impl_->config.standalone);
    return true;
}

// ========== 远程命令处理 ==========

CommandResult Agent::handleRemoteCommand(const std::string& command, const CommandData& data) {
    spdlog::info("Received remote command: {}", command);

    if (command == "run_script") {
        // 获取脚本路径
        auto pathIt = data.find("path");
        if (pathIt == data.end() || pathIt->second.empty()) {
            spdlog::warn("run_script missing path parameter");
            return CommandResult::error("missing path parameter");
        }

        if (!impl_->standaloneMode || !impl_->standaloneMode->isRunning()) {
            spdlog::warn("StandaloneMode not available, cannot run script");
            return CommandResult::error("StandaloneMode not available");
        }

        // 加载并启动脚本
        std::string scriptId = impl_->standaloneMode->loadScript(pathIt->second);
        if (!scriptId.empty()) {
            if (impl_->standaloneMode->startScript(scriptId)) {
                spdlog::info("Script started: {} (path: {})", scriptId, pathIt->second);
                return CommandResult::ok("script started: " + scriptId);
            } else {
                spdlog::error("Failed to start script: {}", scriptId);
                return CommandResult::error("failed to start script: " + scriptId);
            }
        } else {
            spdlog::error("Failed to load script: {}", pathIt->second);
            return CommandResult::error("failed to load script: " + pathIt->second);
        }

    } else if (command == "system.shutdown") {
        spdlog::info("Shutting down agent due to remote command");
        // 命令回调内联运行在 RemoteClient 的消息处理线程上，同步 stop() 会
        // 停掉/回收正在执行本回调的线程（EDEADLK），ack 也无法发出——
        // 先回 ack，stop 移交独立线程收尾。
        // ack 的实际投递发生在本回调返回之后（消息循环统一 send）：高负载下
        // 分离线程可能抢占先跑到 RemoteClient 停机，传输先断导致 ack 丢失
        // （AgentLoopbackTest 全并行跑偶发超时的根因）。停机前留一个宽限窗，
        // 让 µs 级的 ack 投递确定性地先走完。
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stop();
        }).detach();
        return CommandResult::ok("agent shutting down");

    } else if (command == "list_windows") {
        nlohmann::json windows = nlohmann::json::array();
        for (const auto& info : Window::enumerate()) {
            windows.push_back({
                {"handle", static_cast<int64_t>(Window::key(info.handle))},
                {"title", info.title},
                {"isForeground", info.isForeground},
                {"bounds", {
                    {"x", info.bounds.x},
                    {"y", info.bounds.y},
                    {"width", info.bounds.width},
                    {"height", info.bounds.height}
                }}
            });
        }
        return CommandResult::okData(windows.dump());

    } else if (command == "get_status") {
        nlohmann::json scripts = nlohmann::json::array();
        if (impl_->standaloneMode) {
            for (const auto& script : impl_->standaloneMode->listScripts()) {
                scripts.push_back({
                    {"id", script.id},
                    {"path", script.path},
                    {"state", scriptStateToString(script.state)},
                    {"error", script.error},
                    {"uptime", script.uptime}
                });
            }
        }

        nlohmann::json status = {
            {"mode", static_cast<int>(impl_->mode)},
            {"remoteEnabled", impl_->config.enableRemote},
            {"localIpcEnabled", impl_->config.enableLocalIpc},
            {"standaloneScriptEnabled", impl_->config.enableStandaloneScript},
            {"standaloneRunning", impl_->standaloneMode ? impl_->standaloneMode->isRunning() : false},
            {"remoteConnected", impl_->remoteClient ? impl_->remoteClient->isConnected() : false},
            {"remoteState", impl_->remoteClient ? impl_->remoteClient->connectionStateName() : std::string("disabled")},
            {"ipcClientConnected", impl_->localIpcServer ? impl_->localIpcServer->isClientConnected() : false},
            {"scripts", scripts}
        };
        return CommandResult::okData(status.dump());

    } else if (command == "screenshot.capture") {
        if (!impl_->remoteDispatcher) {
            return CommandResult::error("remote dispatcher not available");
        }
        return dispatchViaRpcDispatcher(*impl_->remoteDispatcher, command, data, "screenshot capture");

    } else if (command.rfind("trigger.", 0) == 0) {
        // trigger.list / toggle / add / remove / update：远程通道复用本地 IPC 的
        // RPC handler（Dispatcher Reuse），Dashboard 经 Go server 透传访问。
        if (!impl_->remoteDispatcher || !impl_->triggerManager) {
            return CommandResult::error("trigger handlers not available");
        }
        return dispatchViaRpcDispatcher(*impl_->remoteDispatcher, command, data, "trigger command");

    } else if (command.rfind("asset.", 0) == 0) {
        // asset.sync：远程通道复用 RPC handler（Dispatcher Reuse）
        if (!impl_->remoteDispatcher) {
            return CommandResult::error("asset handlers not available");
        }
        return dispatchViaRpcDispatcher(*impl_->remoteDispatcher, command, data, "asset command");

    } else if (command == "stop_script") {
        auto scriptIdIt = data.find("script_id");
        if (scriptIdIt != data.end() && !scriptIdIt->second.empty()) {
            if (impl_->standaloneMode) {
                if (impl_->standaloneMode->stopScript(scriptIdIt->second)) {
                    spdlog::info("Script stopped: {}", scriptIdIt->second);
                    return CommandResult::ok("script stopped: " + scriptIdIt->second);
                } else {
                    spdlog::error("Failed to stop script: {}", scriptIdIt->second);
                    return CommandResult::error("failed to stop script: " + scriptIdIt->second);
                }
            } else {
                return CommandResult::error("StandaloneMode not available");
            }
        }
        return CommandResult::error("missing script_id parameter");
    } else {
        spdlog::warn("Unknown remote command: {}", command);
        return CommandResult::error("unknown command: " + command);
    }
}

} // namespace wingman::runtime
