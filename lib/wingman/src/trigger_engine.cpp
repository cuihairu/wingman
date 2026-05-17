#include "wingman/trigger_engine.hpp"

#include "wingman/trigger.hpp"
#include <spdlog/spdlog.h>
#include <fstream>

// Lua 配置解析
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

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_istable(L, -1)) {
            TriggerConfig config;

            lua_getfield(L, -1, "name");
            if (lua_isstring(L, -1)) {
                config.name = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "enabled");
            config.enabled = lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "condition");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "type");
                const char* type = lua_tostring(L, -1);
                if (type) {
                    if (strcmp(type, "pixel") == 0) {
                        config.condition.type = TriggerType::ColorFound;
                    } else if (strcmp(type, "image") == 0) {
                        config.condition.type = TriggerType::ImageFound;
                    }
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "color");
                if (lua_isstring(L, -1)) {
                    config.condition.value = lua_tostring(L, -1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "region");
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1);
                    config.condition.region.x = static_cast<int>(lua_tointeger(L, -1));
                    lua_pop(L, 1);

                    lua_rawgeti(L, -1, 2);
                    config.condition.region.y = static_cast<int>(lua_tointeger(L, -1));
                    lua_pop(L, 1);

                    lua_rawgeti(L, -1, 3);
                    config.condition.region.width = static_cast<int>(lua_tointeger(L, -1));
                    lua_pop(L, 1);

                    lua_rawgeti(L, -1, 4);
                    config.condition.region.height = static_cast<int>(lua_tointeger(L, -1));
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "tolerance");
                config.condition.tolerance = static_cast<int>(lua_tointeger(L, -1));
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "action");
            if (lua_istable(L, -1)) {
                TriggerActionData action;

                lua_getfield(L, -1, "type");
                const char* type = lua_tostring(L, -1);
                if (type) {
                    if (strcmp(type, "key") == 0) {
                        action.type = BasicTriggerAction::KeyPress;
                    } else if (strcmp(type, "click") == 0) {
                        action.type = BasicTriggerAction::Click;
                    } else if (strcmp(type, "macro") == 0) {
                        action.type = BasicTriggerAction::RunScript;
                    }
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "key");
                if (lua_isstring(L, -1)) {
                    action.value = lua_tostring(L, -1);
                }
                lua_pop(L, 1);

                config.actions.push_back(action);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "cooldown");
            config.cooldown = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);

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
