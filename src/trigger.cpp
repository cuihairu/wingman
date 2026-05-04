#include "wingman/trigger.hpp"
#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"

#include <chrono>
#include <thread>

namespace wingman {

TriggerManager::TriggerManager() : m_running(false), m_thread(nullptr) {
    InitializeCriticalSection(&m_cs);
}

TriggerManager::~TriggerManager() {
    stop();
    DeleteCriticalSection(&m_cs);
}

size_t TriggerManager::add(const TriggerConfig& config) {
    Lock();

    static size_t nextId = 1;
    TriggerInstance instance;
    instance.id = nextId++;
    instance.config = config;
    instance.lastTriggerTime = 0;
    instance.triggered = false;

    m_triggers.push_back(instance);
    Unlock();

    return instance.id;
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

DWORD WINAPI TriggerManager::checkThread(LPVOID param) {
    auto* manager = static_cast<TriggerManager*>(param);

    while (manager->m_running) {
        manager->Lock();

        for (auto& trigger : manager->m_triggers) {
            if (!trigger.config.enabled) continue;

            if (manager->checkTrigger(trigger)) {
                // 检查冷却时间
                DWORD now = GetTickCount();
                if (now - trigger.lastTriggerTime >= trigger.config.cooldown) {
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
            // 特殊处理：value 是毫秒数
            return GetTickCount() >= std::stoul(cond.value);
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
            case TriggerAction::Click:
                Input::click(action.x, action.y);
                break;

            case TriggerAction::KeyPress:
                Input::key(std::stoi(action.value));
                break;

            case TriggerAction::Type:
                Input::type(action.value, action.delay);
                break;

            case TriggerAction::RunScript: {
                // TODO: 实现脚本运行
                break;
            }

            case TriggerAction::StopScript:
                // TODO: 实现脚本停止
                break;

            case TriggerAction::PauseScript:
                // TODO: 实现脚本暂停
                break;

            case TriggerAction::ShowMessage:
                MessageBoxA(nullptr, action.value.c_str(), "Wingman Trigger", MB_OK);
                break;

            case TriggerAction::PlaySound:
                PlaySoundA(action.value.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
                break;

            case TriggerAction::Log:
                // TODO: 实现日志记录
                break;
        }

        if (action.delay > 0) {
            Input::delay(action.delay);
        }
    }
}

} // namespace wingman
