#include "wingman/event.hpp"
#include <chrono>
#include <spdlog/spdlog.h>

namespace wingman {

EventHub& EventHub::instance() {
    static EventHub hub;
    return hub;
}

uint64_t EventHub::subscribe(const std::string& type,
                             EventHandler handler,
                             const std::string& name,
                             bool once) {
    // Validate event type
    if (type.empty()) {
        spdlog::warn("[EventHub] Cannot subscribe to empty event type");
        return 0;
    }

    if (type.length() > MAX_EVENT_TYPE_LENGTH) {
        spdlog::warn("[EventHub] Event type too long: {} chars (max: {})",
                     type.length(), MAX_EVENT_TYPE_LENGTH);
        return 0;
    }

    if (!name.empty() && name.length() > MAX_EVENT_NAME_LENGTH) {
        spdlog::warn("[EventHub] Subscription name too long: {} chars (max: {})",
                     name.length(), MAX_EVENT_NAME_LENGTH);
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check subscription limit
    auto& typeSubs = subscriptions_[type];
    if (typeSubs.size() >= MAX_SUBSCRIPTIONS_PER_EVENT) {
        spdlog::warn("[EventHub] Too many subscriptions for event '{}': {} (max: {})",
                     type, typeSubs.size(), MAX_SUBSCRIPTIONS_PER_EVENT);
        return 0;
    }

    uint64_t id = nextSubscriptionId_++;
    typeSubs[id] = Subscription{std::move(handler), name, once};
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
    // Validate event type
    if (type.empty()) {
        spdlog::warn("[EventHub] Cannot emit event with empty type");
        return;
    }

    if (type.length() > MAX_EVENT_TYPE_LENGTH) {
        spdlog::warn("[EventHub] Event type too long: {} chars (max: {})",
                     type.length(), MAX_EVENT_TYPE_LENGTH);
        return;
    }

    std::vector<std::pair<uint64_t, Subscription>> subscribers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(type);
        if (it == subscriptions_.end()) {
            return;
        }
        for (const auto& [id, sub] : it->second) {
            subscribers.emplace_back(id, sub);
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

    std::vector<uint64_t> successfulOnceIds;
    for (const auto& [id, sub] : subscribers) {
        if (sub.handler) {
            try {
                sub.handler(msg);
                // Only remove once subscription if callback succeeded
                if (sub.once) {
                    successfulOnceIds.push_back(id);
                }
            } catch (const std::exception& e) {
                spdlog::error("[EventHub] Handler exception for event '{}': {}", type, e.what());
            } catch (...) {
                spdlog::error("[EventHub] Unknown handler exception for event '{}'", type);
            }
        }
    }

    // Remove only successful once subscriptions
    if (!successfulOnceIds.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(type);
        if (it != subscriptions_.end()) {
            for (uint64_t id : successfulOnceIds) {
                it->second.erase(id);
                subscriptionTypes_.erase(id);
            }
            if (it->second.empty()) {
                subscriptions_.erase(it);
            }
        }
    }
}

void EventHub::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.clear();
    subscriptionTypes_.clear();
}

} // namespace wingman
