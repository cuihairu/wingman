#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"

namespace wingman::rpc {

namespace {

std::string triggerTypeToString(TriggerType type) {
    switch (type) {
        case TriggerType::ColorFound: return "ColorFound";
        case TriggerType::ColorLost: return "ColorLost";
        case TriggerType::ImageFound: return "ImageFound";
        case TriggerType::ImageLost: return "ImageLost";
        case TriggerType::WindowOpened: return "WindowOpened";
        case TriggerType::WindowClosed: return "WindowClosed";
        case TriggerType::ProcessStarted: return "ProcessStarted";
        case TriggerType::ProcessStopped: return "ProcessStopped";
        case TriggerType::TimeElapsed: return "TimeElapsed";
        case TriggerType::HotkeyPressed: return "HotkeyPressed";
        case TriggerType::PixelChanged: return "PixelChanged";
    }
    return "ColorFound";
}

TriggerType triggerTypeFromJson(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        return static_cast<TriggerType>(value.get<int>());
    }

    const std::string type = value.is_string() ? value.get<std::string>() : "";
    if (type == "ColorFound" || type == "pixel") return TriggerType::ColorFound;
    if (type == "ColorLost") return TriggerType::ColorLost;
    if (type == "ImageFound" || type == "image") return TriggerType::ImageFound;
    if (type == "ImageLost") return TriggerType::ImageLost;
    if (type == "WindowOpened") return TriggerType::WindowOpened;
    if (type == "WindowClosed") return TriggerType::WindowClosed;
    if (type == "ProcessStarted") return TriggerType::ProcessStarted;
    if (type == "ProcessStopped") return TriggerType::ProcessStopped;
    if (type == "TimeElapsed") return TriggerType::TimeElapsed;
    if (type == "HotkeyPressed") return TriggerType::HotkeyPressed;
    if (type == "PixelChanged") return TriggerType::PixelChanged;
    return TriggerType::ColorFound;
}

} // namespace

void registerTriggerHandlers(RpcDispatcher& dispatcher, TriggerManager& manager) {
    using json = nlohmann::json;

    dispatcher.registerHandler("trigger.list", [&manager](const json&) -> json {
        auto instances = manager.getAllTriggerInstances();
        json triggers = json::array();
        for (const auto& inst : instances) {
            triggers.push_back({
                {"id", std::to_string(inst.id)},
                {"name", inst.config.name},
                {"enabled", inst.config.enabled},
                {"type", triggerTypeToString(inst.config.condition.type)},
                {"lastTriggered", inst.triggered}
            });
        }
        return {{"triggers", triggers}};
    });

    dispatcher.registerHandler("trigger.add", [&manager](const json& params) -> json {
        const json& source = params.contains("config") && params["config"].is_object()
            ? params["config"]
            : params;

        TriggerConfig config;
        config.name = source.value("name", "Unnamed Trigger");
        config.enabled = source.value("enabled", true);
        config.oneShot = source.value("oneShot", false);
        config.cooldown = source.value("cooldown", 0);

        if (source.contains("condition")) {
            const auto& cond = source["condition"];
            config.condition.type = triggerTypeFromJson(cond.value("type", json{0}));
            config.condition.value = cond.value("value", "");
            config.condition.tolerance = cond.value("tolerance", 10);
            config.condition.interval = cond.value("interval", 1000);
            config.condition.enabled = true;
            if (cond.contains("region")) {
                const auto& r = cond["region"];
                config.condition.region = Rect(
                    r.value("x", 0), r.value("y", 0),
                    r.value("width", 0), r.value("height", 0)
                );
            }
        }

        if (source.contains("actions")) {
            for (const auto& a : source["actions"]) {
                TriggerActionData action;
                action.type = static_cast<BasicTriggerAction>(a.value("type", 0));
                action.value = a.value("value", "");
                action.x = a.value("x", 0);
                action.y = a.value("y", 0);
                action.delay = a.value("delay", 0);
                config.actions.push_back(action);
            }
        }

        size_t id = manager.add(config);
        return {{"id", std::to_string(id)}};
    });

    dispatcher.registerHandler("trigger.remove", [&manager](const json& params) -> json {
        std::string idStr = params.value("id", "0");
        size_t id = std::stoull(idStr);
        manager.remove(id);
        return {{"success", true}};
    });

    dispatcher.registerHandler("trigger.update", [&manager](const json& params) -> json {
        std::string idStr = params.value("id", "0");
        size_t id = std::stoull(idStr);
        const json& source = params.contains("config") && params["config"].is_object()
            ? params["config"]
            : params;

        auto existing = manager.getTriggerConfig(id);
        if (!existing) {
            return {{"success", false}, {"error", "Trigger not found"}};
        }

        TriggerConfig config = *existing;
        if (source.contains("name")) config.name = source["name"];
        if (source.contains("enabled")) config.enabled = source["enabled"];
        if (source.contains("oneShot")) config.oneShot = source["oneShot"];
        if (source.contains("cooldown")) config.cooldown = source["cooldown"];

        manager.update(id, config);
        return {{"success", true}};
    });

    dispatcher.registerHandler("trigger.toggle", [&manager](const json& params) -> json {
        std::string idStr = params.value("id", "0");
        size_t id = std::stoull(idStr);

        auto config = manager.getTriggerConfig(id);
        if (!config) {
            return {{"success", false}, {"error", "Trigger not found"}};
        }

        if (config->enabled) {
            manager.disable(id);
        } else {
            manager.enable(id);
        }

        return {{"enabled", !config->enabled}};
    });
}

} // namespace wingman::rpc
