#pragma once

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/trigger.hpp"
#include "wingman/recorder.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>

// 前向声明Windows类型
typedef unsigned long long SOCKET;

namespace wingman {

// ========== 远程控制请求/响应 ==========

struct RemoteRequest {
    std::string action;      // 动作类型
    nlohmann::json params;   // 参数

    // 从JSON解析
    static RemoteRequest fromJson(const std::string& jsonStr);
    static RemoteRequest fromJson(const nlohmann::json& j);

    // 转换为JSON
    nlohmann::json toJson() const;
};

struct RemoteResponse {
    bool success = false;
    nlohmann::json data;
    std::string error;

    // 转换为JSON字符串
    std::string toJsonString() const;
    static RemoteResponse fromJson(const nlohmann::json& j);
};

// ========== 远程控制服务器 ==========

class RemoteServer {
public:
    RemoteServer();
    ~RemoteServer();

    // 启动服务器
    bool start(int port = 9999);

    // 停止服务器
    void stop();

    // 检查运行状态
    bool isRunning() const { return running_; }

    // 获取端口
    int getPort() const { return port_; }

    // 获取客户端连接数
    size_t getConnectionCount() const { return connectionCount_; }

private:
    // 处理客户端连接
    void handleClient(SOCKET clientSocket);

    // 处理请求
    RemoteResponse handleRequest(const RemoteRequest& req);

    // ========== 动作处理器 ==========

    // 屏幕操作
    RemoteResponse handleCaptureScreen(const nlohmann::json& params);
    RemoteResponse handleGetPixel(const nlohmann::json& params);
    RemoteResponse handleFindColor(const nlohmann::json& params);
    RemoteResponse handleFindImage(const nlohmann::json& params);

    // 输入模拟
    RemoteResponse handleClick(const nlohmann::json& params);
    RemoteResponse handleMove(const nlohmann::json& params);
    RemoteResponse handleKey(const nlohmann::json& params);
    RemoteResponse handleTypeText(const nlohmann::json& params);

    // 触发器管理
    RemoteResponse handleAddTrigger(const nlohmann::json& params);
    RemoteResponse handleRemoveTrigger(const nlohmann::json& params);
    RemoteResponse handleEnableTrigger(const nlohmann::json& params);
    RemoteResponse handleDisableTrigger(const nlohmann::json& params);
    RemoteResponse handleListTriggers(const nlohmann::json& params);

    // 宏操作
    RemoteResponse handleRecordMacro(const nlohmann::json& params);
    RemoteResponse handleStopMacroRecording(const nlohmann::json& params);
    RemoteResponse handlePlayMacro(const nlohmann::json& params);

    // 系统信息
    RemoteResponse handlePing(const nlohmann::json& params);
    RemoteResponse handleGetVersion(const nlohmann::json& params);

    // ========== 服务器状态 ==========
    bool running_ = false;
    int port_ = 9999;
    size_t connectionCount_ = 0;

    // ========== 核心组件 ==========
    std::unique_ptr<TriggerManager> triggerManager_;
    std::unique_ptr<MacroRecorder> macroRecorder_;
    std::mutex mutex_;

    // ========== 实现细节 ==========
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== 远程控制客户端 ==========

class RemoteClient {
public:
    RemoteClient();
    ~RemoteClient();

    // 连接到服务器
    bool connect(const std::string& host, int port = 9999);

    // 断开连接
    void disconnect();

    // 检查连接状态
    bool isConnected() const { return connected_; }

    // 发送请求
    RemoteResponse send(const RemoteRequest& req);
    RemoteResponse send(const std::string& action, const nlohmann::json& params = {});

    // ========== 便捷方法 ==========

    // 屏幕操作
    RemoteResponse captureScreen(int x = 0, int y = 0, int width = 0, int height = 0);
    RemoteResponse getPixel(int x, int y);
    RemoteResponse findColor(uint32_t color, int x, int y, int width, int height, int tolerance = 10);

    // 输入模拟
    RemoteResponse click(int x, int y, const std::string& button = "left");
    RemoteResponse move(int x, int y, int durationMs = 100);
    RemoteResponse key(int keyCode);
    RemoteResponse typeText(const std::string& text, int delayMs = 50);

    // 触发器
    RemoteResponse addTrigger(const nlohmann::json& config);
    RemoteResponse listTriggers();
    RemoteResponse enableTrigger(const std::string& id);
    RemoteResponse disableTrigger(const std::string& id);

    // 系统信息
    RemoteResponse ping();

private:
    bool connected_ = false;
    std::string host_;
    int port_ = 9999;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
