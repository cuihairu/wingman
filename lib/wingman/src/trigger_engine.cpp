#include "wingman/trigger_engine.hpp"

#include "wingman/trigger.hpp"
#include <spdlog/spdlog.h>
#include <fstream>

// Lua config parsing
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace wingman {

TriggerEngine::TriggerEngine()
    : m_manager(std::make_unique<TriggerManager>())
    , m_running(false)
{
}

TriggerEngine::~TriggerEngine() {
    stop();
}

bool TriggerEngine::loadFromLua(const std::string& filepath) {
    lua_State* L = luaL_newstate();
    if (!L) {
        spdlog::error("Failed to create Lua state");
        return false;
    }

    luaL_openlibs(L);

    if (luaL_dofile(L, filepath.c_str()) != LUA_OK) {
        spdlog::error("Failed to load trigger config: {}", lua_tostring(L, -1));
        lua_close(L);
        return false;
    }

    lua_getglobal(L, "triggers");
    if (!lua_istable(L, -1)) {
        spdlog::error("triggers table not found in config");
        lua_close(L);
        return false;
    }

    auto parseActionType = [](const char* type) -> std::optional<BasicTriggerAction> {
        if (!type) return std::nullopt;
        if (strcmp(type, "key") == 0 || strcmp(type, "KeyPress") == 0) return BasicTriggerAction::KeyPress;
        if (strcmp(type, "click") == 0 || strcmp(type, "Click") == 0) return BasicTriggerAction::Click;
        if (strcmp(type, "macro") == 0 || strcmp(type, "RunScript") == 0) return BasicTriggerAction::RunScript;
        if (strcmp(type, "Type") == 0) return BasicTriggerAction::Type;
        if (strcmp(type, "StopScript") == 0) return BasicTriggerAction::StopScript;
        if (strcmp(type, "PauseScript") == 0) return BasicTriggerAction::PauseScript;
        if (strcmp(type, "ShowMessage") == 0) return BasicTriggerAction::ShowMessage;
        if (strcmp(type, "PlayAudio") == 0) return BasicTriggerAction::PlayAudio;
        if (strcmp(type, "Log") == 0) return BasicTriggerAction::Log;
        return std::nullopt;
    };

    auto parseConditionType = [](const char* type) -> std::optional<TriggerType> {
        if (!type) return std::nullopt;
        if (strcmp(type, "pixel") == 0 || strcmp(type, "ColorFound") == 0) return TriggerType::ColorFound;
        if (strcmp(type, "ColorLost") == 0) return TriggerType::ColorLost;
        if (strcmp(type, "image") == 0 || strcmp(type, "ImageFound") == 0) return TriggerType::ImageFound;
        if (strcmp(type, "ImageLost") == 0) return TriggerType::ImageLost;
        if (strcmp(type, "WindowOpened") == 0) return TriggerType::WindowOpened;
        if (strcmp(type, "WindowClosed") == 0) return TriggerType::WindowClosed;
        if (strcmp(type, "ProcessStarted") == 0) return TriggerType::ProcessStarted;
        if (strcmp(type, "ProcessStopped") == 0) return TriggerType::ProcessStopped;
        if (strcmp(type, "TimeElapsed") == 0) return TriggerType::TimeElapsed;
        if (strcmp(type, "HotkeyPressed") == 0) return TriggerType::HotkeyPressed;
        if (strcmp(type, "PixelChanged") == 0) return TriggerType::PixelChanged;
        return std::nullopt;
    };

    // Parse a single action table at stack top
    auto parseAction = [&](lua_State* Ls) -> TriggerActionData {
        TriggerActionData action{};

        lua_getfield(Ls, -1, "type");
        auto at = parseActionType(lua_tostring(Ls, -1));
        if (at) action.type = *at;
        lua_pop(Ls, 1);

        lua_getfield(Ls, -1, "value");
        if (lua_isstring(Ls, -1)) action.value = lua_tostring(Ls, -1);
        lua_pop(Ls, 1);

        lua_getfield(Ls, -1, "key");
        if (lua_isstring(Ls, -1)) action.value = lua_tostring(Ls, -1);
        lua_pop(Ls, 1);

        lua_getfield(Ls, -1, "x");
        action.x = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);

        lua_getfield(Ls, -1, "y");
        action.y = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);

        lua_getfield(Ls, -1, "delay");
        action.delay = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);

        return action;
    };

    // Parse region: supports both array {x,y,w,h} and object {x=,y=,width=,height=}
    auto parseRegion = [](lua_State* Ls, Rect& region) {
        if (!lua_istable(Ls, -1)) return;

        // Try object format first
        lua_getfield(Ls, -1, "x");
        if (lua_isnumber(Ls, -1)) {
            region.x = static_cast<int>(lua_tointeger(Ls, -1));
            lua_pop(Ls, 1);
            lua_getfield(Ls, -1, "y");
            region.y = static_cast<int>(lua_tointeger(Ls, -1));
            lua_pop(Ls, 1);
            lua_getfield(Ls, -1, "width");
            region.width = static_cast<int>(lua_tointeger(Ls, -1));
            lua_pop(Ls, 1);
            lua_getfield(Ls, -1, "height");
            region.height = static_cast<int>(lua_tointeger(Ls, -1));
            lua_pop(Ls, 1);
            return;
        }
        lua_pop(Ls, 1);

        // Fallback to array format {x, y, w, h}
        lua_rawgeti(Ls, -1, 1);
        region.x = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);
        lua_rawgeti(Ls, -1, 2);
        region.y = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);
        lua_rawgeti(Ls, -1, 3);
        region.width = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);
        lua_rawgeti(Ls, -1, 4);
        region.height = static_cast<int>(lua_tointeger(Ls, -1));
        lua_pop(Ls, 1);
    };

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_istable(L, -1)) {
            TriggerConfig config;
            config.enabled = true;

            lua_getfield(L, -1, "name");
            if (lua_isstring(L, -1)) config.name = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "enabled");
            if (lua_isboolean(L, -1)) config.enabled = lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "oneShot");
            config.oneShot = lua_toboolean(L, -1);
            lua_pop(L, 1);

            // Parse condition
            lua_getfield(L, -1, "condition");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "type");
                auto ct = parseConditionType(lua_tostring(L, -1));
                if (ct) config.condition.type = *ct;
                lua_pop(L, 1);

                // Support both "value" and "color" field names
                lua_getfield(L, -1, "value");
                if (lua_isstring(L, -1)) config.condition.value = lua_tostring(L, -1);
                lua_pop(L, 1);

                if (config.condition.value.empty()) {
                    lua_getfield(L, -1, "color");
                    if (lua_isstring(L, -1)) config.condition.value = lua_tostring(L, -1);
                    lua_pop(L, 1);
                }

                lua_getfield(L, -1, "region");
                parseRegion(L, config.condition.region);
                lua_pop(L, 1);

                lua_getfield(L, -1, "tolerance");
                config.condition.tolerance = static_cast<int>(lua_tointeger(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "interval");
                config.condition.interval = static_cast<int>(lua_tointeger(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "enabled");
                if (lua_isboolean(L, -1)) config.condition.enabled = lua_toboolean(L, -1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            // Parse actions array
            lua_getfield(L, -1, "actions");
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2) != 0) {
                    if (lua_istable(L, -1)) {
                        config.actions.push_back(parseAction(L));
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            // Fallback: single "action" table
            if (config.actions.empty()) {
                lua_getfield(L, -1, "action");
                if (lua_istable(L, -1)) {
                    config.actions.push_back(parseAction(L));
                }
                lua_pop(L, 1);
            }

            lua_getfield(L, -1, "cooldown");
            config.cooldown = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);

            // Skip triggers with unrecognized or missing condition type
            if (config.condition.type == TriggerType{}) {
                spdlog::warn("Skipping trigger '{}': unknown or missing condition.type", config.name);
                lua_pop(L, 1);
                continue;
            }

            size_t id = m_manager->add(config);
            m_triggerNames.push_back(config.name);
            m_triggerIds.push_back(id);

            spdlog::info("Loaded trigger: {} (id: {})", config.name, id);
        }

        lua_pop(L, 1);
    }

    lua_close(L);
    return true;
}

bool TriggerEngine::loadFromYAML(const std::string& /*filepath*/) {
    spdlog::warn("YAML config loading not yet implemented, use Lua config");
    return false;
}

bool TriggerEngine::start() {
    if (m_running) {
        return true;
    }

    m_manager->start();
    m_running = true;
    spdlog::info("Trigger engine started");
    return true;
}

void TriggerEngine::stop() {
    if (!m_running) {
        return;
    }

    m_manager->stop();
    m_running = false;
    spdlog::info("Trigger engine stopped");
}

bool TriggerEngine::enableTrigger(const std::string& name) {
    for (size_t i = 0; i < m_triggerNames.size(); ++i) {
        if (m_triggerNames[i] == name) {
            m_manager->enable(m_triggerIds[i]);
            spdlog::info("Enabled trigger: {}", name);
            return true;
        }
    }
    return false;
}

bool TriggerEngine::disableTrigger(const std::string& name) {
    for (size_t i = 0; i < m_triggerNames.size(); ++i) {
        if (m_triggerNames[i] == name) {
            m_manager->disable(m_triggerIds[i]);
            spdlog::info("Disabled trigger: {}", name);
            return true;
        }
    }
    return false;
}

TriggerEngine::Stats TriggerEngine::getStats() const {
    Stats stats;

    auto instances = m_manager->getAllTriggerInstances();
    stats.totalTriggers = instances.size();

    stats.enabledTriggers = 0;
    for (const auto& instance : instances) {
        if (instance.config.enabled) {
            stats.enabledTriggers++;
        }
    }

    stats.totalTriggered = 0;
    return stats;
}

} // namespace wingman
