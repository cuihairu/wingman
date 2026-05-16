#include "wingman/trigger.hpp"

#ifdef _WIN32
#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Undefine PlaySound macros before including mmsystem.h
#ifdef PlaySound
#undef PlaySound
#endif
#ifdef PlaySoundA
#undef PlaySoundA
#endif
#ifdef PlaySoundW
#undef PlaySoundW
#endif

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <chrono>
#include <thread>
#include <algorithm>

namespace wingman {

TriggerManager::TriggerManager() : m_running(false), m_thread(nullptr), m_luaState(nullptr) {
    m_logger = spdlog::default_logger();
    InitializeCriticalSection(&m_cs);
}

TriggerManager::TriggerManager(std::shared_ptr<spdlog::logger> logger)
    : m_running(false), m_thread(nullptr), m_luaState(nullptr), m_logger(logger) {
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }
    InitializeCriticalSection(&m_cs);
}

TriggerManager::~TriggerManager() {
    stop();
    DeleteCriticalSection(&m_cs);
}

void TriggerManager::setLuaState(lua_State* L) {
    Lock();
    m_luaState = L;
    Unlock();
}

void TriggerManager::setLogger(std::shared_ptr<spdlog::logger> logger) {
    Lock();
    m_logger = logger ? logger : spdlog::default_logger();
    Unlock();
}

size_t TriggerManager::add(const TriggerConfig& config) {
    Lock();

    static size_t nextId = 1;
    TriggerInstance instance;
    instance.id = nextId++;
    instance.config = config;
    instance.startTime = GetTickCount();
    instance.lastTriggerTime = 0;
    instance.triggered = false;

    m_triggers.push_back(instance);
    Unlock();

    return instance.id;
}

bool TriggerManager::update(size_t id, const TriggerConfig& config) {
    Lock();
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config = config;
            // 重置触发状态
            t.lastTriggerTime = 0;
            t.triggered = false;
            Unlock();
            return true;
        }
    }
    Unlock();
    return false;
}

void TriggerManager::remove(size_t id) {
    Lock();
    m_triggers.erase(
        std::remove_if(m_triggers.begin(), m_triggers.end(),
            [id](const TriggerInstance& t) { return t.id == id; }),
        m_triggers.end()
    );
    Unlock();
}

void TriggerManager::enable(size_t id) {
    Lock();
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config.enabled = true;
            break;
        }
    }
    Unlock();
}

void TriggerManager::disable(size_t id) {
    Lock();
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config.enabled = false;
            break;
        }
    }
    Unlock();
}

void TriggerManager::start() {
    if (m_running) return;

    m_running = true;
    m_thread = CreateThread(nullptr, 0, checkThread, this, 0, nullptr);
}

void TriggerManager::stop() {
    if (!m_running) return;

    m_running = false;

    if (m_thread) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
}

bool TriggerManager::isRunning(size_t id) const {
    Lock();
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            Unlock();
            return t.config.enabled;
        }
    }
    Unlock();
    return false;
}

std::vector<TriggerConfig> TriggerManager::getAllTriggerConfigs() const {
    std::vector<TriggerConfig> result;
    Lock();
    result.reserve(m_triggers.size());
    for (const auto& t : m_triggers) {
        result.push_back(t.config);
    }
    Unlock();
    return result;
}

std::vector<TriggerInstance> TriggerManager::getAllTriggerInstances() const {
    std::vector<TriggerInstance> result;
    Lock();
    result = m_triggers;
    Unlock();
    return result;
}

std::optional<TriggerConfig> TriggerManager::getTriggerConfig(size_t id) const {
    Lock();
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            auto config = t.config;
            Unlock();
            return config;
        }
    }
    Unlock();
    return std::nullopt;
}

bool TriggerManager::hasTrigger(size_t id) const {
    Lock();
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            Unlock();
            return true;
        }
    }
    Unlock();
    return false;
}

size_t TriggerManager::getTriggerCount() const {
    Lock();
    size_t count = m_triggers.size();
    Unlock();
    return count;
}

DWORD WINAPI TriggerManager::checkThread(LPVOID param) {
    auto* manager = static_cast<TriggerManager*>(param);

    while (manager->m_running) {
        manager->Lock();

        for (auto& trigger : manager->m_triggers) {
            if (!trigger.config.enabled) continue;

            if (manager->checkTrigger(trigger)) {
                // 检查冷却时间
                DWORD now = GetTickCount();
                if (now - trigger.lastTriggerTime >= static_cast<DWORD>(trigger.config.cooldown)) {
                    trigger.lastTriggerTime = now;
                    manager->executeActions(trigger.config.actions);

                    if (trigger.config.oneShot) {
                        trigger.config.enabled = false;
                        trigger.triggered = true;
                    }
                }
            }
        }

        manager->Unlock();
        Sleep(50); // 检查间隔
    }

    return 0;
}

bool TriggerManager::checkTrigger(TriggerInstance& trigger) {
    const auto& cond = trigger.config.condition;

    switch (cond.type) {
        case TriggerType::ColorFound: {
            Point result;
            return Screen::findColor(
                Color::fromRGB(std::stoul(cond.value)),
                cond.region,
                cond.tolerance,
                result
            );
        }

        case TriggerType::ImageFound: {
            Point result;
            return Screen::findImage(cond.value, cond.region, cond.tolerance / 100.0, result);
        }

        case TriggerType::WindowOpened: {
            return Window::find(cond.value) != nullptr;
        }

        case TriggerType::WindowClosed: {
            return Window::find(cond.value) == nullptr;
        }

        case TriggerType::ProcessStarted: {
            return Process::find(cond.value) != 0;
        }

        case TriggerType::ProcessStopped: {
            return Process::find(cond.value) == 0;
        }

        case TriggerType::TimeElapsed: {
            // 特殊处理：检查自启动以来是否经过了指定的时间间隔
            // 使用 trigger.instance.startTime 作为起点
            DWORD elapsed = GetTickCount() - trigger.startTime;
            return elapsed >= static_cast<DWORD>(cond.interval);
        }

        case TriggerType::PixelChanged: {
            static Color lastPixel;
            Color currentPixel = Screen::getPixel(cond.region.x, cond.region.y);

            if (lastPixel.distance(currentPixel) > cond.tolerance * cond.tolerance) {
                lastPixel = currentPixel;
                return true;
            }
            return false;
        }

        default:
            return false;
    }
}

void TriggerManager::executeActions(const std::vector<TriggerActionData>& actions) {
    for (const auto& action : actions) {
        switch (action.type) {
            case BasicTriggerAction::Click:
                Input::click(action.x, action.y);
                if (m_logger) m_logger->debug("Trigger action: click at ({}, {})", action.x, action.y);
                break;

            case BasicTriggerAction::KeyPress:
                Input::key(std::stoi(action.value));
                if (m_logger) m_logger->debug("Trigger action: key press '{}'", action.value);
                break;

            case BasicTriggerAction::Type:
                Input::type(action.value, action.delay);
                if (m_logger) m_logger->debug("Trigger action: type text");
                break;

            case BasicTriggerAction::RunScript: {
                if (m_luaState && !action.value.empty()) {
                    if (luaL_dostring(m_luaState, action.value.c_str()) == LUA_OK) {
                        if (m_logger) m_logger->info("Trigger action: executed Lua script successfully");
                    } else {
                        const char* errorMsg = lua_tostring(m_luaState, -1);
                        if (m_logger) m_logger->error("Trigger action: Lua script error: {}", errorMsg ? errorMsg : "unknown");
                        lua_pop(m_luaState, 1);
                    }
                } else {
                    if (m_logger) m_logger->warn("Trigger action: cannot run Lua script - no Lua state or empty script");
                }
                break;
            }

            case BasicTriggerAction::StopScript:
                if (m_logger) m_logger->info("Trigger action: stop script requested");
                break;

            case BasicTriggerAction::PauseScript:
                if (m_logger) m_logger->info("Trigger action: pause script requested");
                break;

            case BasicTriggerAction::ShowMessage:
                MessageBoxA(nullptr, action.value.c_str(), "Wingman Trigger", MB_OK);
                if (m_logger) m_logger->info("Trigger action: showed message '{}'", action.value);
                break;

            case BasicTriggerAction::PlayAudio:
                ::PlaySoundA(action.value.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
                if (m_logger) m_logger->info("Trigger action: played sound '{}'", action.value);
                break;

            case BasicTriggerAction::Log:
                if (m_logger) {
                    m_logger->info("Trigger log: {}", action.value);
                }
                break;
        }

        if (action.delay > 0) {
            Input::delay(action.delay);
        }
    }
}

} // namespace wingman

#endif // _WIN32
