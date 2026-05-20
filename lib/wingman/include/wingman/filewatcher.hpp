#pragma once

#include "wingman/platform/ifilewatcher.hpp"
#include <string>
#include <vector>
#include <functional>

namespace wingman {

// 前向声明
namespace platform {
    class IFileWatcher;
    struct FileChange;
}

/**
 * @brief 文件监控管理类
 *
 * 提供跨平台的文件变化监控功能。
 */
class FileWatcher {
public:
    /**
     * @brief 获取监控器单例
     */
    static platform::IFileWatcher& instance();

    // ========== 便捷静态方法 ==========

    /**
     * @brief 监控单个目录
     * @param path 目录路径
     * @param callback 变化回调
     * @param recursive 是否递归监控子目录
     * @return 监控 ID，失败返回 0
     */
    static uint64_t watch(const std::string& path,
                          std::function<void(const platform::FileChange&)> callback,
                          bool recursive = true);

    /**
     * @brief 取消监控
     * @param watchId 监控 ID
     * @return 成功返回 true
     */
    static bool unwatch(uint64_t watchId);

    /**
     * @brief 取消路径的所有监控
     * @param path 路径
     * @return 取消的监控数量
     */
    static size_t unwatchPath(const std::string& path);

    /**
     * @brief 获取活跃监控数量
     */
    static size_t getWatchCount();

    /**
     * @brief 检查是否有活跃监控
     */
    static bool hasWatches();
};

} // namespace wingman
