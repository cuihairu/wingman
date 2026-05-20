#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace wingman::platform {

/**
 * @brief 文件变化类型
 */
enum class FileChangeType : uint8_t {
    Added,      // 文件/目录添加
    Removed,    // 文件/目录删除
    Modified,   // 文件/目录修改
    RenamedOld, // 重命名（旧名称）
    RenamedNew  // 重命名（新名称）
};

/**
 * @brief 文件变化信息
 */
struct FileChange {
    FileChangeType type;
    std::string path;           // 文件路径
    std::string oldPath;        // 重命名时的旧路径
    uint64_t timestamp;         // 时间戳（毫秒）

    FileChange() : type(FileChangeType::Modified), timestamp(0) {}
};

/**
 * @brief 文件变化回调
 */
using FileChangeCallback = std::function<void(const FileChange&)>;

/**
 * @brief 文件监控接口
 *
 * 监控目录或文件的变化，支持递归监控。
 */
class IFileWatcher {
public:
    virtual ~IFileWatcher() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化文件监控器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭文件监控器
     */
    virtual void shutdown() = 0;

    // ========== 监控管理 ==========

    /**
     * @brief 添加监控路径
     * @param path 要监控的目录或文件路径
     * @param recursive 是否递归监控子目录
     * @param callback 变化回调函数
     * @return 成功返回监控 ID，失败返回 0
     */
    virtual uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) = 0;

    /**
     * @brief 移除监控
     * @param watchId 监控 ID
     * @return 成功返回 true
     */
    virtual bool unwatch(uint64_t watchId) = 0;

    /**
     * @brief 移除路径的所有监控
     * @param path 路径
     * @return 移除的监控数量
     */
    virtual size_t unwatchPath(const std::string& path) = 0;

    // ========== 状态查询 ==========

    /**
     * @brief 获取活跃监控数量
     */
    virtual size_t getWatchCount() const = 0;

    /**
     * @brief 检查是否有活跃监控
     */
    virtual bool hasWatches() const = 0;

    // ========== 后端信息 ==========

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取后端信息
     */
    virtual BackendInfo getBackendInfo() const = 0;
};

} // namespace wingman::platform
