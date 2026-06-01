#include "wingman/trigger.hpp"
#include "wingman/script_manager.hpp"
#include "wingman/script/script_engine_factory.hpp"
#include "wingman/script/module_registry.hpp"

#ifdef _WIN32

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include <spdlog/spdlog.h>

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

static uint64_t getTickCount() {
    return GetTickCount64();
}

TriggerManager::TriggerManager() : m_running(false), m_scriptManager(nullptr) {
    m_logger = spdlog::default_logger();
}

TriggerManager::TriggerManager(std::shared_ptr<spdlog::logger> logger)
    : m_running(false), m_scriptManager(nullptr), m_logger(logger) {
    if (!m_logger) {
        m_logger = spdlog::default_logger();
    }
}

TriggerManager::~TriggerManager() {
    stop();
}

void TriggerManager::setScriptManager(ScriptManager* mgr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scriptManager = mgr;
}

void TriggerManager::setLogger(std::shared_ptr<spdlog::logger> logger) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logger = logger ? logger : spdlog::default_logger();
}

size_t TriggerManager::add(const TriggerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    static size_t nextId = 1;
    TriggerInstance instance;
    instance.id = nextId++;
    instance.config = config;
    instance.startTime = getTickCount();
    instance.lastTriggerTime = 0;
    instance.triggered = false;

    m_triggers.push_back(instance);
    return instance.id;
}

bool TriggerManager::update(size_t id, const TriggerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config = config;
            t.lastTriggerTime = 0;
            t.triggered = false;
            return true;
        }
    }
    return false;
}

void TriggerManager::remove(size_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_triggers.erase(
        std::remove_if(m_triggers.begin(), m_triggers.end(),
            [id](const TriggerInstance& t) { return t.id == id; }),
        m_triggers.end()
    );
}

void TriggerManager::enable(size_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config.enabled = true;
            break;
        }
    }
}

void TriggerManager::disable(size_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& t : m_triggers) {
        if (t.id == id) {
            t.config.enabled = false;
            break;
        }
    }
}

void TriggerManager::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;
    }
    m_thread = std::thread(&TriggerManager::checkThread, this);
}

void TriggerManager::stop() {
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool TriggerManager::isRunning(size_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            return t.config.enabled;
        }
    }
    return false;
}

std::vector<TriggerConfig> TriggerManager::getAllTriggerConfigs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TriggerConfig> result;
    result.reserve(m_triggers.size());
    for (const auto& t : m_triggers) {
        result.push_back(t.config);
    }
    return result;
}

std::vector<TriggerInstance> TriggerManager::getAllTriggerInstances() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_triggers;
}

std::optional<TriggerConfig> TriggerManager::getTriggerConfig(size_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            return t.config;
        }
    }
    return std::nullopt;
}

bool TriggerManager::hasTrigger(size_t id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_triggers) {
        if (t.id == id) {
            return true;
        }
    }
    return false;
}

size_t TriggerManager::getTriggerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_triggers.size();
}

void TriggerManager::checkThread() {
    while (m_running.load()) {
        std::vector<TriggerInstance> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot.reserve(m_triggers.size());
            for (const auto& trigger : m_triggers) {
                if (trigger.config.enabled) {
                    snapshot.push_back(trigger);
                }
            }
        }

        for (auto& trigger : snapshot) {
            if (!m_running.load()) {
                break;
            }

            if (!checkTrigger(trigger)) {
                continue;
            }

            std::vector<TriggerActionData> actions;
            const uint64_t now = getTickCount();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                    [id = trigger.id](const TriggerInstance& candidate) {
                        return candidate.id == id;
                    });

                if (it == m_triggers.end() || !it->config.enabled) {
                    continue;
                }

                if (now - it->lastTriggerTime < static_cast<uint64_t>(it->config.cooldown)) {
                    continue;
                }

                it->lastTriggerTime = now;
                actions = it->config.actions;

                if (it->config.oneShot) {
                    it->config.enabled = false;
                    it->triggered = true;
                }
            }

            executeActions(actions);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
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
            uint64_t elapsed = getTickCount() - trigger.startTime;
            return elapsed >= static_cast<uint64_t>(cond.interval);
        }

        case TriggerType::PixelChanged: {
            static Color lastPixel{};
            static bool firstCheck = true;
            Color currentPixel = Screen::getPixel(cond.region.x, cond.region.y);

            if (firstCheck) {
                lastPixel = currentPixel;
                firstCheck = false;
                return false;
            }

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
                if (!action.value.empty()) {
                    // Execute script via ScriptManager (language-agnostic)
                    // Create a temporary engine to execute script snippet
                    auto& factory = script::ScriptEngineFactory::instance();
                    std::string lang = factory.detectLanguage("script.lua"); // Default Lua
                    auto engine = factory.createEngine(lang);
                    if (engine && engine->initialize()) {
                        script::modules::registerAllModules(*engine);
                        if (engine->executeString(action.value)) {
                            if (m_logger) m_logger->info("Trigger action: executed script successfully");
                        } else {
                            if (m_logger) m_logger->error("Trigger action: script error: {}", engine->getLastError());
                        }
                        engine->shutdown();
                    } else {
                        if (m_logger) m_logger->warn("Trigger action: cannot create script engine");
                    }
                } else {
                    if (m_logger) m_logger->warn("Trigger action: empty script");
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
                showMessage(action.value);
                if (m_logger) m_logger->info("Trigger action: showed message '{}'", action.value);
                break;

            case BasicTriggerAction::PlayAudio:
                playAudio(action.value);
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

void TriggerManager::showMessage(const std::string& message) {
    MessageBoxA(nullptr, message.c_str(), "Wingman Trigger", MB_OK);
}

void TriggerManager::playAudio(const std::string& filepath) {
    ::PlaySoundA(filepath.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
}

} // namespace wingman

#endif // _WIN32
