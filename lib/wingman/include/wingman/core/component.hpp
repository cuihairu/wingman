#pragma once

#include <string>
#include <stdexcept>
#include <atomic>

namespace wingman::core {

/**
 * @brief 组件状态
 */
enum class ComponentState : uint8_t {
    Uninitialized,  // 未初始化
    Initializing,   // 初始化中
    Ready,          // 就绪
    Running,        // 运行中
    Paused,         // 暂停
    Stopping,       // 停止中
    Stopped,        // 已停止
    Error           // 错误状态
};

/**
 * @brief 获取状态名称
 */
inline const char* componentStateName(ComponentState state) {
    switch (state) {
        case ComponentState::Uninitialized:  return "Uninitialized";
        case ComponentState::Initializing:   return "Initializing";
        case ComponentState::Ready:          return "Ready";
        case ComponentState::Running:        return "Running";
        case ComponentState::Paused:         return "Paused";
        case ComponentState::Stopping:       return "Stopping";
        case ComponentState::Stopped:        return "Stopped";
        case ComponentState::Error:          return "Error";
        default:                             return "Unknown";
    }
}

/**
 * @brief 组件异常
 */
class ComponentException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    ComponentException(const std::string& what)
        : std::runtime_error(what) {}

    ComponentException(const std::string& component, const std::string& what)
        : std::runtime_error("[" + component + "] " + what) {}
};

/**
 * @brief 组件接口
 *
 * 所有需要生命周期管理的组件都应该实现此接口。
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    /**
     * @brief 初始化组件
     * @return 成功返回 true
     * @throws ComponentException 初始化失败
     */
    virtual bool initialize() = 0;

    /**
     * @brief 启动组件
     * @return 成功返回 true
     */
    virtual bool start() = 0;

    /**
     * @brief 暂停组件
     */
    virtual void pause() = 0;

    /**
     * @brief 恢复组件
     */
    virtual void resume() = 0;

    /**
     * @brief 停止组件
     */
    virtual void stop() = 0;

    /**
     * @brief 关闭组件，释放资源
     */
    virtual void shutdown() = 0;

    /**
     * @brief 获取组件状态
     */
    virtual ComponentState getState() const = 0;

    /**
     * @brief 检查组件是否就绪
     */
    bool isReady() const {
        auto s = getState();
        return s == ComponentState::Ready || s == ComponentState::Running;
    }

    /**
     * @brief 检查组件是否运行中
     */
    bool isRunning() const {
        return getState() == ComponentState::Running;
    }

    /**
     * @brief 获取组件名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取组件版本
     */
    virtual std::string getVersion() const { return "1.0.0"; }

protected:
    IComponent() = default;
};

/**
 * @brief 组件基类
 *
 * 提供默认的状态管理实现，子类只需实现核心逻辑。
 */
class ComponentBase : public IComponent {
public:
    ~ComponentBase() override {
        try {
            shutdown();
        } catch (...) {
            // 析构函数不应该抛出异常
        }
    }

    bool initialize() override;
    bool start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    void shutdown() override;

    ComponentState getState() const override { return state_.load(); }
    std::string getName() const override { return componentName_; }

    /**
     * @brief 设置组件名称
     */
    void setName(const std::string& name) { componentName_ = name; }

protected:
    /**
     * @brief 子类实现：初始化逻辑
     * @return 成功返回 true
     */
    virtual bool onInitialize() { return true; }

    /**
     * @brief 子类实现：启动逻辑
     * @return 成功返回 true
     */
    virtual bool onStart() { return true; }

    /**
     * @brief 子类实现：暂停逻辑
     */
    virtual void onPause() {}

    /**
     * @brief 子类实现：恢复逻辑
     */
    virtual void onResume() {}

    /**
     * @brief 子类实现：停止逻辑
     */
    virtual void onStop() {}

    /**
     * @brief 子类实现：关闭逻辑
     */
    virtual void onShutdown() {}

    /**
     * @brief 设置状态
     */
    void setState(ComponentState state);

private:
    std::atomic<ComponentState> state_{ComponentState::Uninitialized};
    std::string componentName_;
};

} // namespace wingman::core
