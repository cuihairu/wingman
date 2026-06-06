#include "wingman/ipc/ipc_factory.hpp"
#include "wingman/ipc/tcp_channel.hpp"
#include <spdlog/spdlog.h>
#include <cstdlib>

#ifdef _WIN32
#include "wingman/ipc/windows/named_pipe_channel.hpp"
using NamedPipeChannel = wingman::ipc::windows::NamedPipeChannel;
#elif defined(__unix__) || defined(__APPLE__)
#include "wingman/ipc/unix_socket_channel.hpp"
#endif

namespace wingman::ipc {

namespace {

std::unique_ptr<IIpcChannel> createTcpChannel(bool serverMode, const IpcConfig& config) {
    if (!config.allowTcpFallback && config.preferredTransport != IpcTransport::TcpPipe) {
        spdlog::warn("[IpcFactory] TCP fallback disabled; refusing to create debug TCP IPC channel");
        return nullptr;
    }

    int port = config.tcpPort > 0 ? config.tcpPort : 9800;
    return std::make_unique<TcpChannel>(serverMode, "127.0.0.1", port);
}

} // namespace

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
            if (isTransportAvailable(IpcTransport::UnixSocket)) {
                auto channel = std::make_unique<UnixSocketChannel>(true, endpoint);
                spdlog::info("[IpcFactory] Created UnixSocket server: {}", endpoint);
                return channel;
            }
            break;
#endif
        default:
            break;
    }

    if (transport == IpcTransport::TcpPipe) {
        return createTcpChannel(true, config);
    }

    spdlog::warn("[IpcFactory] No supported IPC server transport available");
    return createTcpChannel(true, config);
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
            if (isTransportAvailable(IpcTransport::UnixSocket)) {
                auto channel = std::make_unique<UnixSocketChannel>(false, endpoint);
                spdlog::info("[IpcFactory] Created UnixSocket client: {}", endpoint);
                return channel;
            }
            break;
#endif
        default:
            break;
    }

    if (transport == IpcTransport::TcpPipe) {
        return createTcpChannel(false, config);
    }

    spdlog::warn("[IpcFactory] No supported IPC client transport available");
    return createTcpChannel(false, config);
}

std::string IpcFactory::getDefaultEndpoint() {
#ifdef _WIN32
    return "wingman";
#elif defined(__linux__)
    if (const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR")) {
        return std::string(runtimeDir) + "/wingman.sock";
    }
    return "/tmp/wingman.sock";
#elif defined(__APPLE__)
    if (const char* tmpDir = std::getenv("TMPDIR")) {
        return std::string(tmpDir) + "wingman.sock";
    }
    return "/tmp/wingman.sock";
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
    if (transport == IpcTransport::UnixSocket) {
        // Windows AF_UNIX support can be detected, but this build does not
        // provide a Windows UnixSocketChannel implementation yet.
        return false;
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
