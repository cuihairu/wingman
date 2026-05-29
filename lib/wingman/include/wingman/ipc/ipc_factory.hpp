#pragma once

#include "wingman/ipc/iipc_channel.hpp"
#include <memory>
#include <string>

namespace wingman::ipc {

/**
 * @brief IPC configuration
 */
struct IpcConfig {
    IpcTransport preferredTransport = IpcTransport::Auto;
    std::string serverName;       // Server name (for naming)
    int tcpPort = 0;              // TCP port (0 = auto-select)
    int timeoutMs = 5000;         // Connection timeout
};

/**
 * @brief IPC factory
 *
 * Auto-selects best IPC transport based on platform.
 */
class IpcFactory {
public:
    /**
     * @brief Create server channel
     * @param config Configuration
     * @return IPC channel, returns nullptr on failure
     */
    static std::unique_ptr<IIpcChannel> createServer(const IpcConfig& config = {});

    /**
     * @brief Create client channel
     * @param config Configuration
     * @return IPC channel, returns nullptr on failure
     */
    static std::unique_ptr<IIpcChannel> createClient(const IpcConfig& config = {});

    /**
     * @brief Get default endpoint
     * @return Server endpoint name/path
     */
    static std::string getDefaultEndpoint();

    /**
     * @brief Get platform preferred transport
     * @return Transport type
     */
    static IpcTransport getPreferredTransport();

    /**
     * @brief Check if transport is available
     * @param transport Transport type
     * @return Returns true if available
     */
    static bool isTransportAvailable(IpcTransport transport);
};

} // namespace wingman::ipc
