#pragma once

/**
 * @file storage_all.hpp
 * @brief Wingman Storage System - Unified include for all storage types
 *
 * Storage hierarchy design:
 * - SessionStorage: In-memory storage, cleared on process exit
 * - LocalStorage: File-based persistence, local machine only
 * - TeamStorage: Visible to team/orchestrator group members
 * - ServerStorage: Globally visible on server (all connected clients)
 */

#include "wingman/storage/storage.hpp"
#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"

// Remote storage requires server module
#ifdef WINGMAN_BUILD_SERVER
#include "wingman/storage/team_storage.hpp"
#include "wingman/storage/server_storage.hpp"
#endif

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

#ifdef WINGMAN_BUILD_SERVER
    // Create TeamStorage (requires server module)
    static std::unique_ptr<TeamStorage> createTeam(
        std::shared_ptr<server::Client> client
    );

    // Create ServerStorage (requires server module)
    static std::unique_ptr<ServerStorage> createServer(
        std::shared_ptr<server::Client> client
    );
#endif
};

} // namespace wingman

