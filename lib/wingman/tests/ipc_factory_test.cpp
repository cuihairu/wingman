#include <gtest/gtest.h>
#include "wingman/ipc/ipc_factory.hpp"

using namespace wingman::ipc;

// ========== IpcConfig ==========

TEST(IpcConfigTest, DefaultValues) {
    IpcConfig cfg;
    EXPECT_EQ(cfg.preferredTransport, IpcTransport::Auto);
    EXPECT_TRUE(cfg.serverName.empty());
    EXPECT_EQ(cfg.tcpPort, 0);
    EXPECT_EQ(cfg.timeoutMs, 5000);
}

// ========== IpcFactory 静态方法 ==========

TEST(IpcFactoryTest, GetDefaultEndpointNotEmpty) {
    std::string endpoint = IpcFactory::getDefaultEndpoint();
    EXPECT_FALSE(endpoint.empty());
}

TEST(IpcFactoryTest, GetPreferredTransportNotAuto) {
    IpcTransport transport = IpcFactory::getPreferredTransport();
    EXPECT_NE(transport, IpcTransport::Auto);
}

TEST(IpcFactoryTest, PreferredTransportIsAvailable) {
    IpcTransport preferred = IpcFactory::getPreferredTransport();
    EXPECT_TRUE(IpcFactory::isTransportAvailable(preferred));
}

TEST(IpcFactoryTest, TcpTransportAlwaysAvailable) {
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::TcpPipe));
}

TEST(IpcFactoryTest, IpcMessageDefaultValues) {
    IpcMessage msg;
    EXPECT_EQ(msg.type, IpcMessageType::Request);
    EXPECT_TRUE(msg.method.empty());
    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.id, 0u);
    EXPECT_EQ(msg.timestamp, 0u);
}

TEST(IpcFactoryTest, CreateServerWithDefaultConfig) {
    IpcConfig cfg;
    // On Windows, this should try NamedPipe and create a channel
    auto channel = IpcFactory::createServer(cfg);
    // Result depends on platform — just test it doesn't crash
    EXPECT_NO_THROW(IpcFactory::createServer(cfg));
}

TEST(IpcFactoryTest, CreateClientWithDefaultConfig) {
    IpcConfig cfg;
    EXPECT_NO_THROW(IpcFactory::createClient(cfg));
}

TEST(IpcFactoryTest, CreateServerWithExplicitTransport) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcFactory::getPreferredTransport();
    cfg.serverName = "test_ipc_server";
    EXPECT_NO_THROW(IpcFactory::createServer(cfg));
}

TEST(IpcFactoryTest, CreateClientWithExplicitTransport) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcFactory::getPreferredTransport();
    cfg.serverName = "test_ipc_client";
    EXPECT_NO_THROW(IpcFactory::createClient(cfg));
}

TEST(IpcFactoryTest, CreateServerWithTcpFallback) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcTransport::TcpPipe;
    // TCP is not fully implemented yet, should return nullptr
    auto channel = IpcFactory::createServer(cfg);
    EXPECT_EQ(channel, nullptr);
}

TEST(IpcFactoryTest, CreateClientWithTcpFallback) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcTransport::TcpPipe;
    auto channel = IpcFactory::createClient(cfg);
    EXPECT_EQ(channel, nullptr);
}
