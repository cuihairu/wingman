#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"

#include <algorithm>
#include <cctype>

namespace wingman::rpc {

namespace {

std::string normalizeName(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '_'), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

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

    const std::string type = normalizeName(value.is_string() ? value.get<std::string>() : "");
    if (type == "colorfound" || type == "pixel") return TriggerType::ColorFound;
    if (type == "colorlost") return TriggerType::ColorLost;
    if (type == "imagefound" || type == "image") return TriggerType::ImageFound;
    if (type == "imagelost") return TriggerType::ImageLost;
    if (type == "windowopened") return TriggerType::WindowOpened;
    if (type == "windowclosed") return TriggerType::WindowClosed;
    if (type == "processstarted") return TriggerType::ProcessStarted;
    if (type == "processstopped") return TriggerType::ProcessStopped;
    if (type == "timeelapsed") return TriggerType::TimeElapsed;
    if (type == "hotkeypressed") return TriggerType::HotkeyPressed;
    if (type == "pixelchanged") return TriggerType::PixelChanged;
    return TriggerType::ColorFound;
}

std::string actionTypeToString(BasicTriggerAction type) {
    switch (type) {
        case BasicTriggerAction::RunScript: return "RunScript";
        case BasicTriggerAction::Click: return "Click";
        case BasicTriggerAction::KeyPress: return "KeyPress";
        case BasicTriggerAction::Type: return "Type";
        case BasicTriggerAction::StopScript: return "StopScript";
        case BasicTriggerAction::PauseScript: return "PauseScript";
        case BasicTriggerAction::ShowMessage: return "ShowMessage";
        case BasicTriggerAction::PlayAudio: return "PlayAudio";
        case BasicTriggerAction::Log: return "Log";
        case BasicTriggerAction::Delay: return "Delay";
    }
    return "Log";
}

BasicTriggerAction actionTypeFromJson(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        return static_cast<BasicTriggerAction>(value.get<int>());
    }

    const std::string type = normalizeName(value.is_string() ? value.get<std::string>() : "");
    if (type == "runscript" || type == "macro") return BasicTriggerAction::RunScript;
    if (type == "click") return BasicTriggerAction::Click;
    if (type == "keypress" || type == "key") return BasicTriggerAction::KeyPress;
    if (type == "type") return BasicTriggerAction::Type;
    if (type == "stopscript") return BasicTriggerAction::StopScript;
    if (type == "pausescript") return BasicTriggerAction::PauseScript;
    if (type == "showmessage") return BasicTriggerAction::ShowMessage;
    if (type == "playaudio") return BasicTriggerAction::PlayAudio;
    if (type == "delay") return BasicTriggerAction::Delay;
    return BasicTriggerAction::Log;
}

nlohmann::json conditionToJson(const BasicTriggerCondition& condition) {
    return {
        {"type", triggerTypeToString(condition.type)},
        {"value", condition.value},
        {"region", {
            {"x", condition.region.x},
            {"y", condition.region.y},
            {"width", condition.region.width},
            {"height", condition.region.height}
        }},
        {"tolerance", condition.tolerance},
        {"interval", condition.interval},
        {"enabled", condition.enabled}
    };
}

nlohmann::json actionToJson(const TriggerActionData& action) {
    return {
        {"type", actionTypeToString(action.type)},
        {"value", action.value},
        {"x", action.x},
        {"y", action.y},
        {"delay", action.delay}
    };
}

void applyTriggerConfigJson(TriggerConfig& config, const nlohmann::json& source) {
    if (source.contains("name")) config.name = source["name"].get<std::string>();
    if (source.contains("enabled")) config.enabled = source["enabled"].get<bool>();
    if (source.contains("oneShot")) config.oneShot = source["oneShot"].get<bool>();
    if (source.contains("cooldown")) config.cooldown = source["cooldown"].get<int>();

    if (source.contains("condition") && source["condition"].is_object()) {
        const auto& cond = source["condition"];
        if (cond.contains("type")) config.condition.type = triggerTypeFromJson(cond["type"]);
        if (cond.contains("value")) config.condition.value = cond["value"].get<std::string>();
        if (cond.contains("tolerance")) config.condition.tolerance = cond["tolerance"].get<int>();
        if (cond.contains("interval")) config.condition.interval = cond["interval"].get<int>();
        if (cond.contains("enabled")) config.condition.enabled = cond["enabled"].get<bool>();
        if (cond.contains("region") && cond["region"].is_object()) {
            const auto& r = cond["region"];
            config.condition.region = Rect(
                r.value("x", 0), r.value("y", 0),
                r.value("width", 0), r.value("height", 0)
            );
        }
    }

    if (source.contains("actions") && source["actions"].is_array()) {
        config.actions.clear();
        for (const auto& a : source["actions"]) {
            TriggerActionData action;
            action.type = actionTypeFromJson(a.contains("type") ? a["type"] : nlohmann::json{8});
            action.value = a.value("value", "");
            action.x = a.value("x", 0);
            action.y = a.value("y", 0);
            action.delay = a.value("delay", 0);
            config.actions.push_back(action);
        }
    }
}

} // namespace

void registerTriggerHandlers(RpcDispatcher& dispatcher, TriggerManager& manager) {
    using json = nlohmann::json;

    dispatcher.registerHandler("trigger.list", [&manager](const json&) -> json {
        auto instances = manager.getAllTriggerInstances();
        json triggers = json::array();
        for (const auto& inst : instances) {
            json actions = json::array();
            for (const auto& action : inst.config.actions) {
                actions.push_back(actionToJson(action));
            }

            triggers.push_back({
                {"id", std::to_string(inst.id)},
                {"name", inst.config.name},
                {"enabled", inst.config.enabled},
                {"type", triggerTypeToString(inst.config.condition.type)},
                {"condition", conditionToJson(inst.config.condition)},
                {"actions", actions},
                {"oneShot", inst.config.oneShot},
                {"cooldown", inst.config.cooldown},
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
        config.name = "Unnamed Trigger";
        config.condition.type = TriggerType::ColorFound;
        config.condition.tolerance = 10;
        config.condition.interval = 1000;
        config.condition.enabled = true;
        config.enabled = true;
        config.oneShot = false;
        config.cooldown = 0;
        applyTriggerConfigJson(config, source);

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
        applyTriggerConfigJson(config, source);

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
