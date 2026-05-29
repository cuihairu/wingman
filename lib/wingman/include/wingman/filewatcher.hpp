#pragma once

#include "wingman/platform/ifilewatcher.hpp"
#include <string>
#include <vector>
#include <functional>

namespace wingman {

// Forward declaration
namespace platform {
    class IFileWatcher;
    struct FileChange;
}

/**
 * @brief File watcher management class
 *
 * Provides cross-platform file change monitoring functionality.
 */
class FileWatcher {
public:
    /**
     * @brief Get watcher singleton
     */
    static platform::IFileWatcher& instance();

    // ========== Convenience static methods ==========

    /**
     * @brief Watch a single directory
     * @param path Directory path
     * @param callback Change callback
     * @param recursive Whether to recursively watch subdirectories
     * @return Watch ID, returns 0 on failure
     */
    static uint64_t watch(const std::string& path,
                          std::function<void(const platform::FileChange&)> callback,
                          bool recursive = true);

    /**
     * @brief Unwatch
     * @param watchId Watch ID
     * @return True on success
     */
    static bool unwatch(uint64_t watchId);

    /**
     * @brief Unwatch all watches for a path
     * @param path Path
     * @return Number of watches removed
     */
    static size_t unwatchPath(const std::string& path);

    /**
     * @brief Get active watch count
     */
    static size_t getWatchCount();

    /**
     * @brief Check if there are active watches
     */
    static bool hasWatches();
};

} // namespace wingman
