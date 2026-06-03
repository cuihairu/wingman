#pragma once

/**
 * @file storage_all.hpp
 * @brief Wingman Storage System - Unified include for all storage types
 *
 * Storage hierarchy design:
 * - SessionStorage: In-memory storage, cleared on process exit
 * - LocalStorage: File-based persistence, local machine only
 */

#include "wingman/storage/storage.hpp"
#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"

namespace wingman {

// Storage factory (optional, for simplified creation)
class StorageFactory {
public:
    // Create SessionStorage
    static std::unique_ptr<SessionStorage> createSession();

    // Create LocalStorage
    static std::unique_ptr<LocalStorage> createLocal(
        const std::filesystem::path& storageDir = "storage"
    );

};

} // namespace wingman

