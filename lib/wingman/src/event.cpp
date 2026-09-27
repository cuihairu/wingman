#include "wingman/event.hpp"
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>

namespace wingman {

EventHub& EventHub::instance() {
    // 泄漏式单例：进程退出期（静态析构阶段）后台任务线程仍可能 emit
    // （如 TaskManager 析构 shutdown→join 等待期间的任务 cancel/completed 事件），
    // Meyer's 静态局部此时已按逆序析构，emit 访问悬空的 unordered_map 直接段错误。
    // EventHub 无自定义析构逻辑，内存交由 OS 回收。
    static EventHub& hub = *new EventHub();
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

std::optional<SubscriptionInfo> EventHub::subscription(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto typeIt = subscriptionTypes_.find(subscriptionId);
    if (typeIt == subscriptionTypes_.end()) {
        return std::nullopt;
    }
    auto typeSubsIt = subscriptions_.find(typeIt->second);
    if (typeSubsIt == subscriptions_.end()) {
        return std::nullopt;
    }
    auto subIt = typeSubsIt->second.find(subscriptionId);
    if (subIt == typeSubsIt->second.end()) {
        return std::nullopt;
    }
    return SubscriptionInfo{subscriptionId, typeIt->second, subIt->second.name, subIt->second.once};
}

std::optional<SubscriptionInfo> EventHub::subscriptionByName(const std::string& name) {
    if (name.empty()) {
        // 匿名订阅（name 为空串）没有可查询的名字
        return std::nullopt;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // 同名订阅可注册多个（跨事件或同事件）：unordered_map 遍历无序，
    // 固定取最早注册（ID 最小）的那个，保证结果确定。
    std::optional<SubscriptionInfo> best;
    for (const auto& [type, subs] : subscriptions_) {
        for (const auto& [id, sub] : subs) {
            if (sub.name != name) continue;
            if (!best || id < best->id) {
                best = SubscriptionInfo{id, type, sub.name, sub.once};
            }
        }
    }
    return best;
}

std::vector<SubscriptionInfo> EventHub::subscriptionsForType(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SubscriptionInfo> result;
    auto typeSubsIt = subscriptions_.find(type);
    if (typeSubsIt == subscriptions_.end()) {
        return result;
    }
    result.reserve(typeSubsIt->second.size());
    for (const auto& [id, sub] : typeSubsIt->second) {
        result.push_back(SubscriptionInfo{id, type, sub.name, sub.once});
    }
    std::sort(result.begin(), result.end(),
              [](const SubscriptionInfo& a, const SubscriptionInfo& b) { return a.id < b.id; });
    return result;
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

void EventHub::clear(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto typeSubsIt = subscriptions_.find(type);
    if (typeSubsIt == subscriptions_.end()) {
        return;
    }
    for (const auto& [id, sub] : typeSubsIt->second) {
        subscriptionTypes_.erase(id);
    }
    subscriptions_.erase(typeSubsIt);
}

} // namespace wingman
