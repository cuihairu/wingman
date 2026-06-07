#pragma once

#include <functional>
#include <mutex>
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

class EventHub {
public:
    static EventHub& instance();

    uint64_t subscribe(const std::string& type,
                       EventHandler handler,
                       const std::string& name = "",
                       bool once = false);

    void unsubscribe(uint64_t subscriptionId);
    void unsubscribe(const std::string& name);

    void emit(const std::string& type,
              const nlohmann::json& payload = nlohmann::json::object(),
              const std::string& source = "",
              const std::string& correlationId = "",
              int priority = 0);

    void clear();

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
