#include "wingman/event.hpp"
#include <chrono>

namespace wingman {

EventHub& EventHub::instance() {
    static EventHub hub;
    return hub;
}

uint64_t EventHub::subscribe(const std::string& type,
                             EventHandler handler,
                             const std::string& name,
                             bool once) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t id = nextSubscriptionId_++;
    subscriptions_[type][id] = Subscription{std::move(handler), name, once};
    subscriptionTypes_[id] = type;
    return id;
}

void EventHub::unsubscribe(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptionTypes_.find(subscriptionId);
    if (it == subscriptionTypes_.end()) {
        return;
    }
    auto typeIt = subscriptions_.find(it->second);
    if (typeIt != subscriptions_.end()) {
        typeIt->second.erase(subscriptionId);
        if (typeIt->second.empty()) {
            subscriptions_.erase(typeIt);
        }
    }
    subscriptionTypes_.erase(it);
}

void EventHub::unsubscribe(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint64_t> ids;
    for (const auto& [type, subs] : subscriptions_) {
        for (const auto& [id, sub] : subs) {
            if (sub.name == name) {
                ids.push_back(id);
            }
        }
    }
    for (uint64_t id : ids) {
        auto it = subscriptionTypes_.find(id);
        if (it == subscriptionTypes_.end()) continue;
        auto typeIt = subscriptions_.find(it->second);
        if (typeIt != subscriptions_.end()) {
            typeIt->second.erase(id);
            if (typeIt->second.empty()) {
                subscriptions_.erase(typeIt);
            }
        }
        subscriptionTypes_.erase(it);
    }
}

void EventHub::emit(const std::string& type,
                    const nlohmann::json& payload,
                    const std::string& source,
                    const std::string& correlationId,
                    int priority) {
    std::vector<Subscription> subscribers;
    std::vector<uint64_t> onceIds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(type);
        if (it == subscriptions_.end()) {
            return;
        }
        for (const auto& [id, sub] : it->second) {
            subscribers.push_back(sub);
            if (sub.once) {
                onceIds.push_back(id);
            }
        }
        for (uint64_t id : onceIds) {
            subscriptionTypes_.erase(id);
            it->second.erase(id);
        }
        if (it->second.empty()) {
            subscriptions_.erase(it);
        }
    }

    EventMessage msg;
    msg.type = type;
    msg.source = source;
    msg.correlationId = correlationId;
    msg.priority = priority;
    msg.payload = payload;
    msg.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (const auto& sub : subscribers) {
        if (sub.handler) {
            sub.handler(msg);
        }
    }
}

void EventHub::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.clear();
    subscriptionTypes_.clear();
}

} // namespace wingman
