#include "wingman/ipc/ipc_factory.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "wingman/ipc/windows/named_pipe_channel.hpp"
using NamedPipeChannel = wingman::ipc::windows::NamedPipeChannel;
#endif

namespace wingman::ipc {

// ========== IpcFactory 实现 ==========

std::unique_ptr<IIpcChannel> IpcFactory::createServer(const IpcConfig& config) {
    std::string endpoint = config.serverName.empty() ? getDefaultEndpoint() : config.serverName;
    IpcTransport transport = config.preferredTransport;

    // 自动选择
    if (transport == IpcTransport::Auto) {
        transport = getPreferredTransport();
    }

    // 尝试创建通道
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

    // 回退到 TCP
    spdlog::warn("[IpcFactory] Preferred transport not available, falling back to TCP");
    // TODO: 实现 TCP 通道
    return nullptr;
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

    spdlog::warn("[IpcFactory] Preferred transport not available, falling back to TCP");
    return nullptr;
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
    // Windows: Named Pipe 优先
    return IpcTransport::NamedPipe;
#elif defined(__linux__)
    // Linux: Unix Socket 优先
    return IpcTransport::UnixSocket;
#elif defined(__APPLE__)
    // macOS: Unix Socket 优先
    return IpcTransport::UnixSocket;
#else
    return IpcTransport::TcpPipe;
#endif
}

bool IpcFactory::isTransportAvailable(IpcTransport transport) {
#ifdef _WIN32
    if (transport == IpcTransport::NamedPipe) {
        return true;  // Windows 总是支持 Named Pipe
    }
    // Windows 10 1803+ 支持 Unix Socket
    if (transport == IpcTransport::UnixSocket) {
        // 可以检查版本，这里简化处理
        return true;
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (transport == IpcTransport::UnixSocket) {
        return true;  // Unix 系统总是支持 Unix Socket
    }
#endif
    if (transport == IpcTransport::TcpPipe) {
        return true;  // TCP 总是可用
    }
    return false;
}

} // namespace wingman::ipc
