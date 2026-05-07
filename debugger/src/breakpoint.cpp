#include "wingman/debugger/breakpoint.hpp"
#include <spdlog/spdlog.h>

namespace wingman {

BreakpointManager::BreakpointManager() : nextId_(1) {}

size_t BreakpointManager::addBreakpoint(const std::string& file, int line,
                                       BreakpointType type,
                                       const std::string& condition) {
    Breakpoint bp;
    bp.id = nextId_++;
    bp.file = file;
    bp.line = line;
    bp.type = type;
    bp.state = BreakpointState::Enabled;
    bp.condition = condition;
    bp.hitCount = 0;
    bp.ignoreCount = 0;

    breakpoints_.push_back(bp);
    fileIndex_[file].push_back(bp.id);

    spdlog::debug("[Debugger] Added breakpoint {} at {}:{}", bp.id, file, line);
    return bp.id;
}

bool BreakpointManager::removeBreakpoint(size_t id) {
    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        if (it->id == id) {
            // 从文件索引中移除
            auto& fileBps = fileIndex_[it->file];
            fileBps.erase(std::remove(fileBps.begin(), fileBps.end(), id), fileBps.end());

            spdlog::debug("[Debugger] Removed breakpoint {}", id);
            breakpoints_.erase(it);
            return true;
        }
    }
    return false;
}

void BreakpointManager::removeBreakpointsForFile(const std::string& file) {
    auto it = breakpoints_.begin();
    while (it != breakpoints_.end()) {
        if (it->file == file) {
            spdlog::debug("[Debugger] Removed breakpoint {} for file {}", it->id, file);
            it = breakpoints_.erase(it);
        } else {
            ++it;
        }
    }
    fileIndex_.erase(file);
}

void BreakpointManager::clearAll() {
    breakpoints_.clear();
    fileIndex_.clear();
    nextId_ = 1;
    spdlog::debug("[Debugger] Cleared all breakpoints");
}

bool BreakpointManager::enableBreakpoint(size_t id) {
    for (auto& bp : breakpoints_) {
        if (bp.id == id) {
            bp.state = BreakpointState::Enabled;
            spdlog::debug("[Debugger] Enabled breakpoint {}", id);
            return true;
        }
    }
    return false;
}

bool BreakpointManager::disableBreakpoint(size_t id) {
    for (auto& bp : breakpoints_) {
        if (bp.id == id) {
            bp.state = BreakpointState::Disabled;
            spdlog::debug("[Debugger] Disabled breakpoint {}", id);
            return true;
        }
    }
    return false;
}

std::optional<Breakpoint*> BreakpointManager::findBreakpoint(const std::string& file, int line) {
    auto it = fileIndex_.find(file);
    if (it == fileIndex_.end()) {
        return std::nullopt;
    }

    for (size_t id : it->second) {
        for (auto& bp : breakpoints_) {
            if (bp.id == id && bp.line == line && bp.state == BreakpointState::Enabled) {
                return &bp;
            }
        }
    }

    return std::nullopt;
}

std::vector<Breakpoint*> BreakpointManager::getBreakpointsForFile(const std::string& file) {
    std::vector<Breakpoint*> result;

    auto it = fileIndex_.find(file);
    if (it == fileIndex_.end()) {
        return result;
    }

    for (size_t id : it->second) {
        for (auto& bp : breakpoints_) {
            if (bp.id == id) {
                result.push_back(&bp);
                break;
            }
        }
    }

    return result;
}

std::vector<Breakpoint> BreakpointManager::getAllBreakpoints() const {
    return breakpoints_;
}

bool BreakpointManager::shouldBreak(const std::string& file, int line) {
    auto bpOpt = findBreakpoint(file, line);
    if (!bpOpt.has_value()) {
        return false;
    }

    Breakpoint* bp = bpOpt.value();

    // 检查忽略次数
    if (bp->ignoreCount > 0) {
        bp->ignoreCount--;
        bp->hitCount++;
        return false;
    }

    bp->hitCount++;
    bp->state = BreakpointState::Hit;

    // TODO: 条件断点求值（需要 Lua 表达式求值器）
    // if (bp->type == BreakpointType::Condition && !bp->condition.empty()) {
    //     return evaluateCondition(bp->condition);
    // }

    return true;
}

void BreakpointManager::incrementHitCount(size_t id) {
    for (auto& bp : breakpoints_) {
        if (bp.id == id) {
            bp.hitCount++;
            break;
        }
    }
}

int BreakpointManager::getHitCount(size_t id) const {
    for (const auto& bp : breakpoints_) {
        if (bp.id == id) {
            return bp.hitCount;
        }
    }
    return 0;
}

} // namespace wingman
