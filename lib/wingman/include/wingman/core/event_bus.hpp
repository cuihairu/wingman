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
 * @brief Event type identifier
 */
using EventType = uint32_t;

/**
 * @brief Event priority
 */
enum class EventPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Event base class
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
        static uint64_t id = 1;
        return id;
    }
};

/**
 * @brief Generic event callback
 */
using GenericEventCallback = std::function<void(const Event&)>;

/**
 * @brief Event subscription info
 */
struct EventSubscription {
    GenericEventCallback callback;
    std::string subscriberName;
    bool once = false;
    EventPriority minPriority = EventPriority::Low;
};

/**
 * @brief Event bus
 *
 * Responsible for event publishing, subscribing, and dispatching.
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    // Non-copyable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * @brief Subscribe to event
     * @param type Event type
     * @param callback Callback function
     * @param subscriberName Subscriber name
     * @param once Whether to trigger only once
     * @return Subscription ID
     */
    uint64_t subscribe(EventType type,
                      GenericEventCallback callback,
                      const std::string& subscriberName = "",
                      bool once = false);

    /**
     * @brief Unsubscribe
     * @param subscriptionId Subscription ID
     */
    void unsubscribe(uint64_t subscriptionId);

    /**
     * @brief Unsubscribe (by subscriber name)
     */
    void unsubscribe(const std::string& subscriberName);

    /**
     * @brief Publish event (synchronous)
     */
    void publish(const Event& event);

    /**
     * @brief Publish event (async, enqueue)
     */
    void publishAsync(std::unique_ptr<Event> event);

    /**
     * @brief Start event dispatch thread
     */
    void start();

    /**
     * @brief Stop event dispatch
     */
    void stop();

    /**
     * @brief Process all pending events (synchronous mode)
     */
    void processEvents();

    /**
     * @brief Get queue size
     */
    size_t getQueueSize() const;

    /**
     * @brief Check if running
     */
    bool isRunning() const { return running_.load(); }

private:
    uint64_t subscribeInternal(EventType type,
                               GenericEventCallback callback,
                               const std::string& subscriberName,
                               bool once);

    // Internal unsubscribe without lock (must be called with lock held)
    void unsubscribeInternal(uint64_t subscriptionId);

    struct QueuedEvent {
        std::unique_ptr<Event> event;
        uint64_t timestamp;
    };

    mutable std::mutex mutex_;
    std::unordered_map<EventType, std::unordered_map<uint64_t, EventSubscription>> subscriptions_;
    std::unordered_map<uint64_t, EventType> subscriptionIdToType_;
    uint64_t nextSubscriptionId_ = 1;

    // Async dispatch
    std::queue<QueuedEvent> eventQueue_;
    std::condition_variable queueCondition_;
    std::thread dispatchThread_;
    std::atomic<bool> running_{false};

    void dispatchLoop();
    void dispatchEvent(const Event& event);
};

} // namespace wingman::core
