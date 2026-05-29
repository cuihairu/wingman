#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace wingman::platform {

/**
 * @brief File change type
 */
enum class FileChangeType : uint8_t {
    Added,      // File/directory added
    Removed,    // File/directory removed
    Modified,   // File/directory modified
    RenamedOld, // Renamed (old name)
    RenamedNew  // Renamed (new name)
};

/**
 * @brief File change information
 */
struct FileChange {
    FileChangeType type;
    std::string path;           // File path
    std::string oldPath;        // Old path when renamed
    uint64_t timestamp;         // Timestamp (milliseconds)

    FileChange() : type(FileChangeType::Modified), timestamp(0) {}
};

/**
 * @brief File change callback
 */
using FileChangeCallback = std::function<void(const FileChange&)>;

/**
 * @brief File watcher interface
 *
 * Monitor directory or file changes, supports recursive watching.
 */
class IFileWatcher {
public:
    virtual ~IFileWatcher() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize file watcher
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown file watcher
     */
    virtual void shutdown() = 0;

    // ========== Watch management ==========

    /**
     * @brief Add watch path
     * @param path Directory or file path to watch
     * @param recursive Whether to recursively watch subdirectories
     * @param callback Change callback function
     * @return Returns watch ID on success, 0 on failure
     */
    virtual uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) = 0;

    /**
     * @brief Remove watch
     * @param watchId Watch ID
     * @return Returns true on success
     */
    virtual bool unwatch(uint64_t watchId) = 0;

    /**
     * @brief Remove all watches for a path
     * @param path Path
     * @return Number of watches removed
     */
    virtual size_t unwatchPath(const std::string& path) = 0;

    // ========== Status query ==========

    /**
     * @brief Get active watch count
     */
    virtual size_t getWatchCount() const = 0;

    /**
     * @brief Check if there are active watches
     */
    virtual bool hasWatches() const = 0;

    // ========== Backend information ==========

    /**
     * @brief Get backend name
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief Get backend info
     */
    virtual BackendInfo getBackendInfo() const = 0;
};

} // namespace wingman::platform
