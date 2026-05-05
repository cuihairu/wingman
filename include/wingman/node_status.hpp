#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace wingman {

// 节点状态
enum class NodeState {
    Online,      // 在线
    Busy,        // 忙碌（正在执行脚本）
    Idle,        // 空闲
    Error,       // 错误
    Offline      // 离线
};

// 游戏窗口状态
struct GameWindowStatus {
    std::string title;           // 窗口标题
    std::string processName;     // 进程名
    uintptr_t handle;            // 窗口句柄
    int x, y, width, height;    // 窗口位置和大小
    bool isForeground;           // 是否在前台

    std::string toJson() const;
    static GameWindowStatus fromJson(const std::string& json);
};

// 脚本执行状态
struct ScriptStatus {
    std::string name;            // 脚本名称
    std::string state;           // 状态：running, paused, stopped, error
    int64_t uptimeSeconds;       // 运行时长（秒）
    std::string lastError;       // 最后错误信息

    std::string toJson() const;
    static ScriptStatus fromJson(const std::string& json);
};

// 节点心跳数据
struct NodeHeartbeat {
    std::string nodeId;          // 节点 ID（自动生成或配置）
    std::string hostname;        // 主机名
    NodeState status;            // 节点状态
    int64_t timestamp;           // 时间戳（毫秒）

    // 系统信息
    double cpuUsage;             // CPU 使用率（0-100）
    double memoryUsage;          // 内存使用（MB）

    // 游戏状态
    std::vector<GameWindowStatus> games;      // 游戏窗口列表
    std::vector<ScriptStatus> scripts;        // 运行中的脚本

    // 版本信息
    std::string version;         // Wingman 版本

    std::string toJson() const;
    static NodeHeartbeat fromJson(const std::string& json);

    // 获取当前时间戳（毫秒）
    static int64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

// 服务器下发命令
enum class ServerCommand {
    None,
    StartScript,     // 启动脚本
    StopScript,      // 停止脚本
    PauseScript,     // 暂停脚本
    ResumeScript,    // 恢复脚本
    Restart,         // 重启节点
    Shutdown,        // 关闭节点
    UpdateConfig     // 更新配置
};

// 服务器命令数据
struct ServerCommandData {
    ServerCommand command;
    std::string scriptPath;      // 脚本路径（StartScript 时使用）
    std::string configData;      // 配置数据（UpdateConfig 时使用）
    int64_t timestamp;           // 命令时间戳

    std::string toJson() const;
    static ServerCommandData fromJson(const std::string& json);
};

} // namespace wingman
