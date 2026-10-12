#include "wingman/androidagent/android_trigger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace wingman::android {

namespace {

constexpr int kMinCheckIntervalMs = 50;   // TriggerDef.checkIntervalMs 下界
constexpr int kWatchTickMs = 25;          // 共享监视线程滴答（响应 stop/新增）

bool colorNear(const Color& a, const Color& b, int tolerance) {
    return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= tolerance &&
           std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= tolerance &&
           std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= tolerance &&
           std::abs(static_cast<int>(a.a) - static_cast<int>(b.a)) <= tolerance;
}

} // namespace

AndroidTriggerManager& AndroidTriggerManager::instance() {
    static AndroidTriggerManager manager;
    return manager;
}

AndroidTriggerManager::AndroidTriggerManager() = default;

AndroidTriggerManager::~AndroidTriggerManager() {
    stopAll();
    watchStop_.store(true, std::memory_order_release);
    wake_.notify_all();
    if (watchThread_.joinable()) {
        watchThread_.join();
    }
}

void AndroidTriggerManager::setBridge(platform::android::AndroidHostBridge* bridge) {
    std::lock_guard lock(mutex_);
    bridge_ = bridge;
}

bool AndroidTriggerManager::add(const TriggerDef& def) {
    if (def.name.empty() || def.conditions.empty() || def.actions.empty()) {
        return false;
    }
    std::lock_guard lock(mutex_);
    if (triggers_.count(def.name) != 0) {
        return false;
    }
    Entry entry;
    entry.def = def;
    if (entry.def.checkIntervalMs < kMinCheckIntervalMs) {
        entry.def.checkIntervalMs = kMinCheckIntervalMs;
    }
    triggers_.emplace(def.name, std::move(entry));
    return true;
}

bool AndroidTriggerManager::remove(const std::string& name) {
    std::lock_guard lock(mutex_);
    return triggers_.erase(name) != 0;
}

bool AndroidTriggerManager::start(const std::string& name) {
    std::lock_guard lock(mutex_);
    auto it = triggers_.find(name);
    if (it == triggers_.end()) {
        return false;
    }
    if (!it->second.running.load(std::memory_order_acquire)) {
        it->second.startedAtMs = nowMs();
        it->second.running.store(true, std::memory_order_release);
    }
    ensureWatchThread();
    wake_.notify_all();
    return true;
}

bool AndroidTriggerManager::stop(const std::string& name) {
    std::lock_guard lock(mutex_);
    auto it = triggers_.find(name);
    if (it == triggers_.end()) {
        return false;
    }
    it->second.running.store(false, std::memory_order_release);
    return true;
}

int AndroidTriggerManager::startAll() {
    int changed = 0;
    {
        std::lock_guard lock(mutex_);
        for (auto& [_, entry] : triggers_) {
            if (!entry.running.load(std::memory_order_acquire)) {
                entry.startedAtMs = nowMs();
                entry.running.store(true, std::memory_order_release);
                ++changed;
            }
        }
        ensureWatchThread();
    }
    wake_.notify_all();
    return changed;
}

int AndroidTriggerManager::stopAll() {
    int changed = 0;
    std::lock_guard lock(mutex_);
    for (auto& [_, entry] : triggers_) {
        if (entry.running.exchange(false, std::memory_order_acq_rel)) {
            ++changed;
        }
    }
    return changed;
}

bool AndroidTriggerManager::isRunning(const std::string& name) const {
    std::lock_guard lock(mutex_);
    auto it = triggers_.find(name);
    return it != triggers_.end() && it->second.running.load(std::memory_order_acquire);
}

int AndroidTriggerManager::getTriggerCount(const std::string& name) const {
    std::lock_guard lock(mutex_);
    auto it = triggers_.find(name);
    return it == triggers_.end() ? -1 : it->second.fireCount.load(std::memory_order_acquire);
}

int AndroidTriggerManager::count() const {
    std::lock_guard lock(mutex_);
    return static_cast<int>(triggers_.size());
}

int64_t AndroidTriggerManager::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void AndroidTriggerManager::ensureWatchThread() {
    // 调用方持 mutex_；线程生命周期与实例一致，析构在锁外 join（见析构序）
    if (watchThread_.joinable()) {
        return;
    }
    watchStop_.store(false, std::memory_order_release);
    watchThread_ = std::thread([this] { watchLoop(); });
}

std::vector<AndroidTriggerManager::Entry> AndroidTriggerManager::snapshotActive() const {
    std::vector<Entry> out;
    std::lock_guard lock(mutex_);
    for (const auto& [_, entry] : triggers_) {
        if (entry.running.load(std::memory_order_acquire)) {
            out.push_back(entry);
        }
    }
    return out;
}

void AndroidTriggerManager::commitState(const std::string& name, const Entry& evaluated) {
    std::lock_guard lock(mutex_);
    auto it = triggers_.find(name);
    if (it == triggers_.end()) {
        return;
    }
    // 触发次数只增不减；running 以引擎侧结论为准（maxTriggers 自停/STOP 动作）
    it->second.fireCount.store(evaluated.fireCount.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
    it->second.running.store(evaluated.running.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
    it->second.startedAtMs = evaluated.startedAtMs;
    it->second.lastCheckedMs = evaluated.lastCheckedMs;
    it->second.def.conditions = evaluated.def.conditions;
}

void AndroidTriggerManager::watchLoop() {
    while (!watchStop_.load(std::memory_order_acquire)) {
        {
            // 停止/登记变更即刻响应的滴答等待
            std::unique_lock wakeLock(wakeMutex_);
            wake_.wait_for(wakeLock, std::chrono::milliseconds(kWatchTickMs));
        }
        if (watchStop_.load(std::memory_order_acquire)) {
            break;
        }

        auto entries = snapshotActive();
        if (entries.empty()) {
            continue;
        }
        const int64_t now = nowMs();

        for (auto& entry : entries) {
            if (!entry.running.load(std::memory_order_acquire) ||
                watchStop_.load(std::memory_order_acquire)) {
                continue;
            }
            if (now - entry.startedAtMs < entry.def.checkIntervalMs) {
                continue;
            }
            // checkIntervalMs 轮询门：距上次检查不足间隔则本轮跳过
            //（timer 条件的有效粒度 = max(checkIntervalMs, timerMs)）
            if (entry.lastCheckedMs != 0 &&
                now - entry.lastCheckedMs < entry.def.checkIntervalMs) {
                continue;
            }

            bool allMet = true;
            std::unique_ptr<Bitmap> frame;  // 同一触发器的一轮检查共用一帧
            vision::ImageAnalyzer analyzer;

            for (auto& condition : entry.def.conditions) {
                switch (condition.type) {
                    case TriggerConditionType::Timer: {
                        const int64_t base = condition.lastMetMs == 0
                            ? entry.startedAtMs : condition.lastMetMs;
                        if (now - base >= condition.timerMs) {
                            condition.lastMetMs = now;  // 边沿：一次满足只记一次
                        } else {
                            allMet = false;
                        }
                        break;
                    }
                    case TriggerConditionType::ColorFound:
                    case TriggerConditionType::ColorNotFound: {
                        if (!frame && bridge_) {
                            frame = bridge_->captureFrame();
                        }
                        bool found = false;
                        if (frame) {
                            found = analyzer.findColor(
                                *frame, condition.color, condition.region,
                                condition.tolerance).has_value();
                        }
                        allMet = allMet &&
                            (condition.type == TriggerConditionType::ColorFound
                                ? found : !found);
                        break;
                    }
                    case TriggerConditionType::PixelChanged: {
                        if (!frame && bridge_) {
                            frame = bridge_->captureFrame();
                        }
                        bool changed = false;
                        if (frame) {
                            const Color current = frame->getPixel(condition.x, condition.y);
                            if (!condition.hasPrevColor) {
                                // 首拍只建立基线，不算变化
                                condition.hasPrevColor = true;
                                condition.prevColor = current;
                            } else if (!colorNear(current, condition.prevColor,
                                                  condition.tolerance)) {
                                changed = true;
                                condition.prevColor = current;
                            }
                        }
                        allMet = allMet && changed;
                        break;
                    }
                }
                if (!allMet) {
                    break;
                }
            }

            if (allMet) {
                entry.fireCount.fetch_add(1, std::memory_order_relaxed);
                for (const auto& action : entry.def.actions) {
                    switch (action.type) {
                        case TriggerActionType::Tap:
                            if (bridge_) bridge_->tap(action.x, action.y, action.durationMs);
                            break;
                        case TriggerActionType::LongPress:
                            if (bridge_) bridge_->longPress(action.x, action.y, action.durationMs);
                            break;
                        case TriggerActionType::Swipe:
                            if (bridge_) bridge_->swipe(action.x, action.y,
                                                        action.x2, action.y2, action.durationMs);
                            break;
                        case TriggerActionType::Stop:
                            entry.running.store(false, std::memory_order_release);
                            break;
                    }
                }
                // maxTriggers 自停（与桌面 SmartTrigger 一致：停而不注销，
                // remove+add 或重新 start 前不再触发）
                if (entry.def.maxTriggers > 0 &&
                    entry.fireCount.load(std::memory_order_relaxed) >= entry.def.maxTriggers) {
                    entry.running.store(false, std::memory_order_release);
                }
            }

            entry.lastCheckedMs = now;
            commitState(entry.def.name, entry);
        }
    }
}

} // namespace wingman::android
