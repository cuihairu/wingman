// 文件监视平台工厂薄层接口：各平台目录导出 createPlatformFileWatcher()
// （内部完成 initialize 与失败回退/日志，语义与历史 ifdef 分支一致）。
// 平台分支由 CMake 按目录选源收敛，本头文件与调用点保持零平台宏
// （薄层纪律，见 docs/platform-abstraction-design.md §8）。
#pragma once

#include <memory>
#include "wingman/platform/ifilewatcher.hpp"

namespace wingman::platform {

// 兜底实现（无平台后端或工厂失败时使用）
class NullFileWatcher final : public IFileWatcher {
public:
    bool initialize() override { return true; }
    void shutdown() override {}

    uint64_t watch(const std::string&, bool, FileChangeCallback) override { return 0; }
    bool unwatch(uint64_t) override { return false; }
    size_t unwatchPath(const std::string&) override { return 0; }

    size_t getWatchCount() const override { return 0; }
    bool hasWatches() const override { return false; }

    std::string getBackendName() const override { return "Null"; }
    BackendInfo getBackendInfo() const override {
        return BackendInfo{"Null", "1.0", true, "No-op file watcher backend"};
    }
};

std::unique_ptr<IFileWatcher> createPlatformFileWatcher();

} // namespace wingman::platform
