#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"

namespace wingman::rpc {

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
                {"type", static_cast<int>(inst.config.condition.type)},
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
            config.condition.type = static_cast<TriggerType>(cond.value("type", 0));
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
