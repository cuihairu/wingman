#include "wingman/smart_trigger.hpp"

#include "wingman/input.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace wingman {

// ========== SmartTrigger 实现 ==========

SmartTrigger::SmartTrigger(const std::string& name) : name_(name) {}

SmartTrigger::~SmartTrigger() {
    stop();
}

void SmartTrigger::addCondition(const TriggerCondition& condition) {
    conditions_.push_back(condition);
}

void SmartTrigger::addAction(const TriggerAction& action) {
    actions_.push_back(action);
}

void SmartTrigger::setCheckInterval(int intervalMs) {
    checkIntervalMs_ = intervalMs;
}

void SmartTrigger::setMaxTriggers(int maxCount) {
    maxTriggers_ = maxCount;
}

bool SmartTrigger::start() {
    if (running_) {
        spdlog::warn("Trigger '{}' is already running", name_);
        return false;
    }

    if (conditions_.empty()) {
        spdlog::error("Trigger '{}' has no conditions", name_);
        return false;
    }

    if (actions_.empty()) {
        spdlog::warn("Trigger '{}' has no actions", name_);
    }

    running_ = true;
    watchThread_ = std::thread(&SmartTrigger::watchLoop, this);

    spdlog::info("Trigger '{}' started", name_);
    return true;
}

void SmartTrigger::stop() {
    if (!running_) return;

    running_ = false;

    bool isSelfStop = watchThread_.joinable() &&
                      std::this_thread::get_id() == watchThread_.get_id();

    if (isSelfStop) {
        // Cannot join self -- detach so the thread cleans up on exit
        if (watchThread_.joinable()) {
            watchThread_.detach();
        }
    } else {
        // External caller: join to ensure clean shutdown
        if (watchThread_.joinable()) {
            watchThread_.join();
        }
    }

    spdlog::info("Trigger '{}' stopped", name_);
}

bool SmartTrigger::checkConditions() {
    for (const auto& condition : conditions_) {
        bool conditionMet = false;

        switch (condition.type) {
            case TriggerConditionType::COLOR_FOUND: {
                auto pos = Vision::findColor(condition.targetColor, condition.tolerance, condition.searchRegion);
                conditionMet = pos.has_value();
                break;
            }
            case TriggerConditionType::COLOR_NOT_FOUND: {
                auto pos = Vision::findColor(condition.targetColor, condition.tolerance, condition.searchRegion);
                conditionMet = !pos.has_value();
                break;
            }
            case TriggerConditionType::IMAGE_FOUND: {
                auto match = Vision::findImage(condition.templatePath, condition.searchRegion, condition.threshold);
                conditionMet = match.found;
                break;
            }
            case TriggerConditionType::IMAGE_NOT_FOUND: {
                auto match = Vision::findImage(condition.templatePath, condition.searchRegion, condition.threshold);
                conditionMet = !match.found;
                break;
            }
            case TriggerConditionType::TEXT_FOUND: {
                auto result = OCR::recognize(condition.searchRegion);
                conditionMet = result.success && result.text.find(condition.targetText) != std::string::npos;
                break;
            }
            case TriggerConditionType::TEXT_NOT_FOUND: {
                auto result = OCR::recognize(condition.searchRegion);
                conditionMet = !result.success || result.text.find(condition.targetText) == std::string::npos;
                break;
            }
            case TriggerConditionType::OCR_CONTAINS: {
                auto result = OCR::recognize(condition.searchRegion);
                conditionMet = result.success && result.text.find(condition.targetText) != std::string::npos;
                break;
            }
            case TriggerConditionType::OCR_EQUALS: {
                auto result = OCR::recognize(condition.searchRegion);
                std::string text = result.text;
                text.erase(std::remove_if(text.begin(), text.end(), ::isspace), text.end());
                std::string target = condition.targetText;
                target.erase(std::remove_if(target.begin(), target.end(), ::isspace), target.end());
                conditionMet = text == target;
                break;
            }
            case TriggerConditionType::EDGE_DETECTED: {
                auto edges = Vision::detectEdges(condition.searchRegion);
                conditionMet = !edges.empty();
                break;
            }
            case TriggerConditionType::COLOR_CHANGED: {
                auto currentColor = Vision::getDominantColor(condition.searchRegion);
                if (condition.hasPreviousColor) {
                    bool match = Vision::isColorMatch(currentColor, condition.previousColor, condition.tolerance);
                    conditionMet = !match;
                }
                const_cast<TriggerCondition&>(condition).previousColor = currentColor;
                const_cast<TriggerCondition&>(condition).hasPreviousColor = true;
                break;
            }
        }

        if (!conditionMet) {
            return false;
        }
    }

    return true;
}

void SmartTrigger::executeActions() {
    for (const auto& action : actions_) {
        switch (action.type) {
            case TriggerActionType::CLICK:
                Input::click(action.clickPosition.x, action.clickPosition.y);
                spdlog::debug("Trigger '{}': clicked ({}, {})", name_, action.clickPosition.x, action.clickPosition.y);
                break;

            case TriggerActionType::KEY_PRESS:
                Input::key(action.keyCode);
                spdlog::debug("Trigger '{}': pressed key {}", name_, action.keyCode);
                break;

            case TriggerActionType::WAIT:
                std::this_thread::sleep_for(std::chrono::milliseconds(action.waitMs));
                spdlog::debug("Trigger '{}': waited {}ms", name_, action.waitMs);
                break;

            case TriggerActionType::LUA_SCRIPT:
                spdlog::debug("Trigger '{}': executing Lua script", name_);
                break;

            case TriggerActionType::CUSTOM_CALLBACK:
                if (action.callback) {
                    action.callback();
                }
                break;

            case TriggerActionType::STOP:
                stop();
                spdlog::info("Trigger '{}': stopped by action", name_);
                break;

            case TriggerActionType::LOG:
                spdlog::info("Trigger '{}': {}", name_, action.logMessage);
                break;
        }
    }
}

void SmartTrigger::watchLoop() {
    while (running_) {
        if (checkConditions()) {
            triggerCount_++;

            spdlog::info("Trigger '{}' triggered (count: {})", name_, triggerCount_.load());

            executeActions();

            // Fast exit after self-stop via STOP action
            if (!running_) break;

            if (maxTriggers_ > 0 && triggerCount_ >= maxTriggers_) {
                spdlog::info("Trigger '{}' reached max triggers ({})", name_, maxTriggers_);
                stop();
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs_));
    }
}

// ========== SmartTriggerManager 实现 ==========

SmartTriggerManager& SmartTriggerManager::instance() {
    static SmartTriggerManager instance;
    return instance;
}

std::shared_ptr<SmartTrigger> SmartTriggerManager::createTrigger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (triggers_.find(name) != triggers_.end()) {
        spdlog::warn("Trigger '{}' already exists", name);
        return triggers_[name];
    }

    auto trigger = std::make_shared<SmartTrigger>(name);
    triggers_[name] = trigger;

    spdlog::info("Created trigger '{}'", name);
    return trigger;
}

std::shared_ptr<SmartTrigger> SmartTriggerManager::getTrigger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = triggers_.find(name);
    if (it != triggers_.end()) {
        return it->second;
    }

    return nullptr;
}

void SmartTriggerManager::removeTrigger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = triggers_.find(name);
    if (it != triggers_.end()) {
        it->second->stop();
        triggers_.erase(it);
        spdlog::info("Removed trigger '{}'", name);
    }
}

void SmartTriggerManager::startAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [name, trigger] : triggers_) {
        trigger->start();
    }

    spdlog::info("Started {} triggers", triggers_.size());
}

void SmartTriggerManager::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [name, trigger] : triggers_) {
        trigger->stop();
    }

    spdlog::info("Stopped {} triggers", triggers_.size());
}

std::vector<std::shared_ptr<SmartTrigger>> SmartTriggerManager::getAllTriggers() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<SmartTrigger>> result;
    for (const auto& [name, trigger] : triggers_) {
        result.push_back(trigger);
    }

    return result;
}

} // namespace wingman
