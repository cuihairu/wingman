#include "wingman/core/event_bus.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace wingman::core {

EventBus::EventBus() = default;

EventBus::~EventBus() {
    stop();
}

uint64_t EventBus::subscribe(EventType type,
                             GenericEventCallback callback,
                             const std::string& subscriberName,
                             bool once) {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscribeInternal(type, std::move(callback), subscriberName, once);
}

void EventBus::unsubscribe(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscriptionIdToType_.find(subscriptionId);
    if (it == subscriptionIdToType_.end()) {
        return;
    }

    EventType type = it->second;
    subscriptions_[type].erase(subscriptionId);

    if (subscriptions_[type].empty()) {
        subscriptions_.erase(type);
    }

    subscriptionIdToType_.erase(it);
}

void EventBus::unsubscribe(const std::string& subscriberName) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint64_t> toRemove;
    for (auto& [type, subs] : subscriptions_) {
        for (auto& [id, sub] : subs) {
            if (sub.subscriberName == subscriberName) {
                toRemove.push_back(id);
            }
        }
    }

    for (uint64_t id : toRemove) {
        unsubscribe(id);
    }
}

void EventBus::publish(const Event& event) {
    dispatchEvent(event);
}

void EventBus::publishAsync(std::unique_ptr<Event> event) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        eventQueue_.push(QueuedEvent{
            std::move(event),
            event->getTimestamp()
        });
    }
    queueCondition_.notify_one();
}

void EventBus::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    dispatchThread_ = std::thread(&EventBus::dispatchLoop, this);
}

void EventBus::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    queueCondition_.notify_all();
    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }
}

void EventBus::processEvents() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!eventQueue_.empty() && running_.load()) {
        auto queued = std::move(eventQueue_.front());
        eventQueue_.pop();

        lock.unlock();
        dispatchEvent(*queued.event);
        lock.lock();
    }
}

size_t EventBus::getQueueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return eventQueue_.size();
}

uint64_t EventBus::subscribeInternal(EventType type,
                                     GenericEventCallback callback,
                                     const std::string& subscriberName,
                                     bool once) {
    uint64_t id = nextSubscriptionId_++;
    subscriptions_[type][id] = EventSubscription{
        std::move(callback),
        subscriberName,
        once,
        EventPriority::Low
    };
    subscriptionIdToType_[id] = type;
    return id;
}

void EventBus::dispatchLoop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (running_.load()) {
        queueCondition_.wait(lock, [this] {
            return !eventQueue_.empty() || !running_.load();
        });

        while (!eventQueue_.empty() && running_.load()) {
            auto queued = std::move(eventQueue_.front());
            eventQueue_.pop();

            lock.unlock();
            dispatchEvent(*queued.event);
            lock.lock();
        }
    }
}

void EventBus::dispatchEvent(const Event& event) {
    std::vector<EventSubscription> subscribers;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(event.getType());
        if (it == subscriptions_.end()) {
            return;
        }

        for (auto& [id, sub] : it->second) {
            if (event.getPriority() >= sub.minPriority) {
                subscribers.push_back(sub);
            }
        }

        std::vector<uint64_t> onceSubscriptions;
        for (auto& [id, sub] : it->second) {
            if (sub.once) {
                onceSubscriptions.push_back(id);
            }
        }

        for (uint64_t id : onceSubscriptions) {
            subscriptions_[event.getType()].erase(id);
            subscriptionIdToType_.erase(id);
        }

        if (subscriptions_[event.getType()].empty()) {
            subscriptions_.erase(event.getType());
        }
    }

    for (const auto& sub : subscribers) {
        try {
            sub.callback(event);
        } catch (const std::exception& e) {
            spdlog::error("[EventBus] Exception in callback for subscriber '{}': {}",
                         sub.subscriberName, e.what());
        } catch (...) {
            spdlog::error("[EventBus] Unknown exception in callback for subscriber '{}'",
                         sub.subscriberName);
        }
    }
}

} // namespace wingman::core
