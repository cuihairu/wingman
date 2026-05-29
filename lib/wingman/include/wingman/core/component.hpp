#pragma once

#include <string>
#include <stdexcept>
#include <atomic>

namespace wingman::core {

/**
 * @brief Component state
 */
enum class ComponentState : uint8_t {
    Uninitialized,  // Uninitialized
    Initializing,   // Initializing
    Ready,          // Ready
    Running,        // Running
    Paused,         // Paused
    Stopping,       // Stopping
    Stopped,        // Stopped
    Error           // Error state
};

/**
 * @brief Get state name
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
 * @brief Component exception
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
 * @brief Component interface
 *
 * All components requiring lifecycle management should implement this interface.
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    /**
     * @brief Initialize component
     * @return Returns true on success
     * @throws ComponentException Initialization failed
     */
    virtual bool initialize() = 0;

    /**
     * @brief Start component
     * @return Returns true on success
     */
    virtual bool start() = 0;

    /**
     * @brief Pause component
     */
    virtual void pause() = 0;

    /**
     * @brief Resume component
     */
    virtual void resume() = 0;

    /**
     * @brief Stop component
     */
    virtual void stop() = 0;

    /**
     * @brief Shutdown component, release resources
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get component state
     */
    virtual ComponentState getState() const = 0;

    /**
     * @brief Check if component is ready
     */
    bool isReady() const {
        auto s = getState();
        return s == ComponentState::Ready || s == ComponentState::Running;
    }

    /**
     * @brief Check if component is running
     */
    bool isRunning() const {
        return getState() == ComponentState::Running;
    }

    /**
     * @brief Get component name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get component version
     */
    virtual std::string getVersion() const { return "1.0.0"; }

protected:
    IComponent() = default;
};

/**
 * @brief Component base class
 *
 * Provides default state management implementation, subclasses only need to implement core logic.
 */
class ComponentBase : public IComponent {
public:
    ~ComponentBase() override {
        try {
            shutdown();
        } catch (...) {
            // Destructor should not throw exceptions
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
     * @brief Set component name
     */
    void setName(const std::string& name) { componentName_ = name; }

protected:
    /**
     * @brief Subclass implementation: initialization logic
     * @return Returns true on success
     */
    virtual bool onInitialize() { return true; }

    /**
     * @brief Subclass implementation: start logic
     * @return Returns true on success
     */
    virtual bool onStart() { return true; }

    /**
     * @brief Subclass implementation: pause logic
     */
    virtual void onPause() {}

    /**
     * @brief Subclass implementation: resume logic
     */
    virtual void onResume() {}

    /**
     * @brief Subclass implementation: stop logic
     */
    virtual void onStop() {}

    /**
     * @brief Subclass implementation: shutdown logic
     */
    virtual void onShutdown() {}

    /**
     * @brief Set status
     */
    void setState(ComponentState state);

private:
    std::atomic<ComponentState> state_{ComponentState::Uninitialized};
    std::string componentName_;
};

} // namespace wingman::core
