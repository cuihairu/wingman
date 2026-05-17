#pragma once

#include <string>
#include <vector>
#include <memory>
#include "wingman/trigger.hpp"

namespace wingman {

// 触发器引擎
// 负责从配置文件加载触发器并管理生命周期
class TriggerEngine {
public:
    TriggerEngine();
    ~TriggerEngine();

    // 从 Lua 配置文件加载触发器
    bool loadFromLua(const std::string& filepath);

    // 从 YAML 配置文件加载触发器
    bool loadFromYAML(const std::string& filepath);

    // 启动引擎
    bool start();

    // 停止引擎
    void stop();

    // 是否运行中
    bool isRunning() const { return m_running; }

    // 获取触发器管理器
    TriggerManager* getManager() { return m_manager.get(); }

    // 启用/禁用触发器
    bool enableTrigger(const std::string& name);
    bool disableTrigger(const std::string& name);

    // 获取统计信息
    struct Stats {
        size_t totalTriggers = 0;
        size_t enabledTriggers = 0;
        size_t totalTriggered = 0;
    };
    Stats getStats() const;

private:
    std::unique_ptr<TriggerManager> m_manager;
    std::vector<std::string> m_triggerNames;
    std::vector<size_t> m_triggerIds;
    bool m_running = false;
};

} // namespace wingman
