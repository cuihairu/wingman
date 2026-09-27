#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace wingman {

// Event system limits to prevent resource exhaustion
constexpr size_t MAX_EVENT_TYPE_LENGTH = 256;
constexpr size_t MAX_EVENT_NAME_LENGTH = 128;
constexpr size_t MAX_SUBSCRIPTIONS_PER_EVENT = 1000;

struct EventMessage {
    std::string type;
    std::string source;
    std::string correlationId;
    uint64_t timestamp = 0;
    int priority = 0;
    nlohmann::json payload = nlohmann::json::object();
};

using EventHandler = std::function<void(const EventMessage&)>;

// 订阅快照：查询接口返回的监听器描述（不含 handler 本体）。
struct SubscriptionInfo {
    uint64_t id = 0;
    std::string type; // 事件类型名
    std::string name; // 监听器名（匿名订阅为空串）
    bool once = false;
};

class EventHub {
public:
    static EventHub& instance();

    uint64_t subscribe(const std::string& type,
                       EventHandler handler,
                       const std::string& name = "",
                       bool once = false);

    void unsubscribe(uint64_t subscriptionId);
    void unsubscribe(const std::string& name);

    // 监听器查询：按订阅 ID 返回订阅快照，不存在时为空。
    std::optional<SubscriptionInfo> subscription(uint64_t subscriptionId);
    // 按监听器名查询（同名订阅可注册多个，取最早注册者；匿名订阅不可按名查询）。
    std::optional<SubscriptionInfo> subscriptionByName(const std::string& name);
    // 某事件的全部订阅快照，按订阅 ID 升序（注册顺序）。
    std::vector<SubscriptionInfo> subscriptionsForType(const std::string& type);

    void emit(const std::string& type,
              const nlohmann::json& payload = nlohmann::json::object(),
              const std::string& source = "",
              const std::string& correlationId = "",
              int priority = 0);

    void clear();
    // 只清理指定事件的全部订阅（事件不存在时为无副作用的空操作）。
    void clear(const std::string& type);

private:
    struct Subscription {
        EventHandler handler;
        std::string name;
        bool once = false;
    };

    EventHub() = default;

    uint64_t nextSubscriptionId_ = 1;
    std::mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<uint64_t, Subscription>> subscriptions_;
    std::unordered_map<uint64_t, std::string> subscriptionTypes_;
};

} // namespace wingman
