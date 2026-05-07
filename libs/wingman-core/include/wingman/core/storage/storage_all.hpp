#pragma once

/**
 * @file storage_all.hpp
 * @brief Wingman Storage System - 统一引入所有存储类型
 *
 * 存储层级设计：
 * - SessionStorage: 内存存储，进程退出清空
 * - LocalStorage: 文件持久化，仅本机可见
 * - TeamStorage: 团队/编排组可见 (orchestrator team 成员)
 * - ServerStorage: 服务器全局可见 (所有连接的客户端)
 */

#include "wingman/storage/storage.hpp"
#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"

// 远程存储需要 server 模块
#ifdef WINGMAN_BUILD_SERVER
#include "wingman/storage/team_storage.hpp"
#include "wingman/storage/server_storage.hpp"
#endif

namespace wingman {

// 存储工厂（可选，用于简化创建）
class StorageFactory {
public:
    // 创建 SessionStorage
    static std::unique_ptr<SessionStorage> createSession();

    // 创建 LocalStorage
    static std::unique_ptr<LocalStorage> createLocal(
        const std::filesystem::path& storageDir = "storage"
    );

#ifdef WINGMAN_BUILD_SERVER
    // 创建 TeamStorage (需要 server 模块)
    static std::unique_ptr<TeamStorage> createTeam(
        std::shared_ptr<server::Client> client
    );

    // 创建 ServerStorage (需要 server 模块)
    static std::unique_ptr<ServerStorage> createServer(
        std::shared_ptr<server::Client> client
    );
#endif
};

} // namespace wingman

