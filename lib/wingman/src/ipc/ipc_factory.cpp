#include "wingman/ipc/ipc_factory.hpp"
#include "wingman/ipc/tcp_channel.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "wingman/ipc/windows/named_pipe_channel.hpp"
using NamedPipeChannel = wingman::ipc::windows::NamedPipeChannel;
#endif

namespace wingman::ipc {

// ========== IpcFactory Implementation ==========

std::unique_ptr<IIpcChannel> IpcFactory::createServer(const IpcConfig& config) {
    std::string endpoint = config.serverName.empty() ? getDefaultEndpoint() : config.serverName;
    IpcTransport transport = config.preferredTransport;

    // Auto-select
    if (transport == IpcTransport::Auto) {
        transport = getPreferredTransport();
    }

    // Try to create channel
    switch (transport) {
#ifdef _WIN32
        case IpcTransport::NamedPipe:
            if (isTransportAvailable(IpcTransport::NamedPipe)) {
                auto channel = std::make_unique<NamedPipeChannel>(true, endpoint);
                spdlog::info("[IpcFactory] Created NamedPipe server: {}", endpoint);
                return channel;
            }
            break;
#elif defined(__unix__) || defined(__APPLE__)
        case IpcTransport::UnixSocket:
            spdlog::warn("[IpcFactory] UnixSocket transport is not implemented on this platform build");
            break;
#endif
        default:
            break;
    }

    // Fallback to TCP
    spdlog::info("[IpcFactory] Preferred transport not available, falling back to TCP");
    int port = config.tcpPort > 0 ? config.tcpPort : 9800;
    return std::make_unique<TcpChannel>(true, "127.0.0.1", port);
}

std::unique_ptr<IIpcChannel> IpcFactory::createClient(const IpcConfig& config) {
    std::string endpoint = config.serverName.empty() ? getDefaultEndpoint() : config.serverName;
    IpcTransport transport = config.preferredTransport;

    if (transport == IpcTransport::Auto) {
        transport = getPreferredTransport();
    }

    switch (transport) {
#ifdef _WIN32
        case IpcTransport::NamedPipe:
            if (isTransportAvailable(IpcTransport::NamedPipe)) {
                auto channel = std::make_unique<NamedPipeChannel>(false, endpoint);
                spdlog::info("[IpcFactory] Created NamedPipe client: {}", endpoint);
                return channel;
            }
            break;
#elif defined(__unix__) || defined(__APPLE__)
        case IpcTransport::UnixSocket:
            spdlog::warn("[IpcFactory] UnixSocket transport is not implemented on this platform build");
            break;
#endif
        default:
            break;
    }

    spdlog::info("[IpcFactory] Preferred transport not available, falling back to TCP");
    int port = config.tcpPort > 0 ? config.tcpPort : 9800;
    return std::make_unique<TcpChannel>(false, "127.0.0.1", port);
}

std::string IpcFactory::getDefaultEndpoint() {
#ifdef _WIN32
    return "wingman";
#elif defined(__linux__)
    return "/tmp/wingman.sock";
#elif defined(__APPLE__)
    return "/var/run/wingman.sock";
#else
    return "wingman";
#endif
}

IpcTransport IpcFactory::getPreferredTransport() {
#ifdef _WIN32
    // Windows: Named Pipe preferred
    return IpcTransport::NamedPipe;
#elif defined(__linux__)
    // Linux: Unix Socket preferred
    return IpcTransport::UnixSocket;
#elif defined(__APPLE__)
    // macOS: Unix Socket preferred
    return IpcTransport::UnixSocket;
#else
    return IpcTransport::TcpPipe;
#endif
}

bool IpcFactory::isTransportAvailable(IpcTransport transport) {
#ifdef _WIN32
    if (transport == IpcTransport::NamedPipe) {
        return true;  // Windows always supports Named Pipe
    }
    // Windows 10 1803+ supports Unix Socket
    if (transport == IpcTransport::UnixSocket) {
        // Could check version; simplified here
        return true;
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (transport == IpcTransport::UnixSocket) {
        return true;  // Unix systems always support Unix Socket
    }
#endif
    if (transport == IpcTransport::TcpPipe) {
        return true;  // TCP is always available
    }
    return false;
}

} // namespace wingman::ipc
