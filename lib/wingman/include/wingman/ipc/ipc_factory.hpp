#pragma once

#include "wingman/ipc/iipc_channel.hpp"
#include <memory>
#include <string>

namespace wingman::ipc {

/**
 * @brief IPC 配置
 */
struct IpcConfig {
    IpcTransport preferredTransport = IpcTransport::Auto;
    std::string serverName;       // 服务器名（用于命名）
    int tcpPort = 0;              // TCP 端口（0 = 自动选择）
    int timeoutMs = 5000;         // 连接超时
};

/**
 * @brief IPC 工厂
 *
 * 根据平台自动选择最佳的 IPC 传输方式。
 */
class IpcFactory {
public:
    /**
     * @brief 创建服务端通道
     * @param config 配置
     * @return IPC 通道，失败返回 nullptr
     */
    static std::unique_ptr<IIpcChannel> createServer(const IpcConfig& config = {});

    /**
     * @brief 创建客户端通道
     * @param config 配置
     * @return IPC 通道，失败返回 nullptr
     */
    static std::unique_ptr<IIpcChannel> createClient(const IpcConfig& config = {});

    /**
     * @brief 获取默认服务端点
     * @return 服务端点名称/路径
     */
    static std::string getDefaultEndpoint();

    /**
     * @brief 获取平台首选传输方式
     * @return 传输类型
     */
    static IpcTransport getPreferredTransport();

    /**
     * @brief 检查传输方式是否可用
     * @param transport 传输类型
     * @return 可用返回 true
     */
    static bool isTransportAvailable(IpcTransport transport);
};

} // namespace wingman::ipc
