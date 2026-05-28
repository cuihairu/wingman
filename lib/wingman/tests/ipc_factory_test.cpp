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

// ========== Additional IPC Tests ==========

TEST(IpcConfigTest, CustomValues) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcTransport::NamedPipe;
    cfg.serverName = "my_server";
    cfg.tcpPort = 8080;
    cfg.timeoutMs = 10000;

    EXPECT_EQ(cfg.preferredTransport, IpcTransport::NamedPipe);
    EXPECT_EQ(cfg.serverName, "my_server");
    EXPECT_EQ(cfg.tcpPort, 8080);
    EXPECT_EQ(cfg.timeoutMs, 10000);
}

TEST(IpcFactoryTest, IpcMessageFieldAssignment) {
    IpcMessage msg;
    msg.type = IpcMessageType::Response;
    msg.method = "test.method";
    msg.payload = "{\"key\":\"value\"}";
    msg.id = 42;
    msg.timestamp = 1700000000;

    EXPECT_EQ(msg.type, IpcMessageType::Response);
    EXPECT_EQ(msg.method, "test.method");
    EXPECT_EQ(msg.payload, "{\"key\":\"value\"}");
    EXPECT_EQ(msg.id, 42u);
    EXPECT_EQ(msg.timestamp, 1700000000u);
}

TEST(IpcFactoryTest, IpcMessageTypes) {
    EXPECT_NE(IpcMessageType::Request, IpcMessageType::Response);
    EXPECT_NE(IpcMessageType::Request, IpcMessageType::Notification);
}

TEST(IpcFactoryTest, IpcTransportValues) {
    EXPECT_NE(IpcTransport::Auto, IpcTransport::NamedPipe);
    EXPECT_NE(IpcTransport::Auto, IpcTransport::TcpPipe);
}

TEST(IpcFactoryTest, CreateServerWithEmptyName) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcFactory::getPreferredTransport();
    cfg.serverName = "";
    EXPECT_NO_THROW(IpcFactory::createServer(cfg));
}

TEST(IpcFactoryTest, CreateClientWithEmptyName) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcFactory::getPreferredTransport();
    cfg.serverName = "";
    EXPECT_NO_THROW(IpcFactory::createClient(cfg));
}

TEST(IpcFactoryTest, AllTransportAvailabilityChecks) {
    for (auto t : {IpcTransport::Auto, IpcTransport::NamedPipe, IpcTransport::TcpPipe}) {
        EXPECT_NO_THROW(IpcFactory::isTransportAvailable(t));
    }
}

TEST(IpcFactoryTest, DefaultEndpointIsConsistent) {
    std::string ep1 = IpcFactory::getDefaultEndpoint();
    std::string ep2 = IpcFactory::getDefaultEndpoint();
    EXPECT_EQ(ep1, ep2);
}
