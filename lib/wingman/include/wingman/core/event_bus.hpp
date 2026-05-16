#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <string>

namespace wingman::core {

/**
 * @brief 事件类型标识符
 */
using EventType = uint32_t;

/**
 * @brief 事件优先级
 */
enum class EventPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief 事件基类
 */
class Event {
public:
    virtual ~Event() = default;

    EventType getType() const { return type_; }
    EventPriority getPriority() const { return priority_; }
    uint64_t getTimestamp() const { return timestamp_; }
    uint64_t getSequenceId() const { return sequenceId_; }

    void setPriority(EventPriority priority) { priority_ = priority; }

protected:
    Event(EventType type, EventPriority priority = EventPriority::Normal)
        : type_(type)
        , priority_(priority)
        , timestamp_(getCurrentTimestamp())
        , sequenceId_(nextSequenceId()++) {}

private:
    EventType type_;
    EventPriority priority_;
    uint64_t timestamp_;
    uint64_t sequenceId_;

    static uint64_t getCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
    }

    static uint64_t& nextSequenceId() {
        static uint64_t id = 0;
        return id;
    }
};

/**
 * @brief 泛型事件回调函数
 */
using GenericEventCallback = std::function<void(const Event&)>;

/**
 * @brief 事件订阅信息
 */
struct EventSubscription {
    GenericEventCallback callback;
    std::string subscriberName;
    bool once = false;
    EventPriority minPriority = EventPriority::Low;
};

/**
 * @brief 事件总线
 *
 * 负责事件的发布、订阅和分发。
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    // 禁止拷贝
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * @brief 订阅事件
     * @param type 事件类型
     * @param callback 回调函数
     * @param subscriberName 订阅者名称
     * @param once 是否只触发一次
     * @return 订阅ID
     */
    uint64_t subscribe(EventType type,
                      GenericEventCallback callback,
                      const std::string& subscriberName = "",
                      bool once = false);

    /**
     * @brief 取消订阅
     * @param subscriptionId 订阅ID
     */
    void unsubscribe(uint64_t subscriptionId);

    /**
     * @brief 取消订阅（按订阅者名称）
     */
    void unsubscribe(const std::string& subscriberName);

    /**
     * @brief 发布事件（同步）
     */
    void publish(const Event& event);

    /**
     * @brief 发布事件（异步，加入队列）
     */
    void publishAsync(std::unique_ptr<Event> event);

    /**
     * @brief 启动事件分发线程
     */
    void start();

    /**
     * @brief 停止事件分发
     */
    void stop();

    /**
     * @brief 处理所有待处理事件（同步模式）
     */
    void processEvents();

    /**
     * @brief 获取队列大小
     */
    size_t getQueueSize() const;

    /**
     * @brief 检查是否运行中
     */
    bool isRunning() const { return running_.load(); }

private:
    uint64_t subscribeInternal(EventType type,
                               GenericEventCallback callback,
                               const std::string& subscriberName,
                               bool once);

    struct QueuedEvent {
        std::unique_ptr<Event> event;
        uint64_t timestamp;
    };

    mutable std::mutex mutex_;
    std::unordered_map<EventType, std::unordered_map<uint64_t, EventSubscription>> subscriptions_;
    std::unordered_map<uint64_t, EventType> subscriptionIdToType_;
    uint64_t nextSubscriptionId_ = 1;

    // 异步分发
    std::queue<QueuedEvent> eventQueue_;
    std::condition_variable queueCondition_;
    std::thread dispatchThread_;
    std::atomic<bool> running_{false};

    void dispatchLoop();
    void dispatchEvent(const Event& event);
};

} // namespace wingman::core
