#pragma once

#include <string>
#include <memory>

struct lua_State;

namespace wingman::debug {

// ========== EmmyLua 适配器 ==========

// EmmyLua 调试器状态
enum class DebuggerState {
    Stopped,
    Running,
    Paused,
    Waiting    // 等待 IDE 连接
};

// EmmyLua 适配器
// 负责加载和管理 EmmyLua 调试器
class EmmyAdapter {
public:
    EmmyAdapter();
    ~EmmyAdapter();

    // 初始化 EmmyLua
    // loadPath: emmy_core.dll 所在目录
    bool initialize(const std::string& loadPath = "");

    // 启动调试器监听
    // port: 监听端口，默认 9966
    bool startListen(int port = 9966);

    // 连接到 IDE
    // host: IDE 地址
    // port: IDE 端口
    bool connectToIDE(const std::string& host = "localhost", int port = 9966);

    // 等待 IDE 连接（阻塞）
    void waitForIDE();

    // 断开连接
    void disconnect();

    // 停止调试器
    void stop();

    // 获取状态
    DebuggerState getState() const { return state_; }

    // 是否已连接
    bool isConnected() const { return connected_; }

    // 设置断点（通过 Lua API）
    bool setBreakpoint(const std::string& file, int line);

    // 移除断点
    bool removeBreakpoint(const std::string& file, int line);

    // 在 Lua 中使用
    bool setupLuaState(lua_State* L);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    DebuggerState state_ = DebuggerState::Stopped;
    bool connected_ = false;
    bool initialized_ = false;
};

// ========== 便捷函数 ==========

// 启动调试器（全局单例）
bool startDebugger(int port = 9966);

// 停止调试器
void stopDebugger();

// 获取调试器实例
EmmyAdapter& getDebugger();

} // namespace wingman::debug
