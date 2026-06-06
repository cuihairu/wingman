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
    EXPECT_FALSE(cfg.allowTcpFallback);
}

// ========== IpcFactory Static Methods ==========

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
    auto channel = IpcFactory::createServer(cfg);
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(channel->getTransport(), IpcTransport::TcpPipe);
    EXPECT_EQ(channel->getBackendName(), "TCP");
}

TEST(IpcFactoryTest, CreateClientWithTcpFallback) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcTransport::TcpPipe;
    auto channel = IpcFactory::createClient(cfg);
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(channel->getTransport(), IpcTransport::TcpPipe);
    EXPECT_EQ(channel->getBackendName(), "TCP");
}

// ========== Additional IPC Tests ==========

TEST(IpcConfigTest, CustomValues) {
    IpcConfig cfg;
    cfg.preferredTransport = IpcTransport::NamedPipe;
    cfg.serverName = "my_server";
    cfg.tcpPort = 8080;
    cfg.timeoutMs = 10000;
    cfg.allowTcpFallback = true;

    EXPECT_EQ(cfg.preferredTransport, IpcTransport::NamedPipe);
    EXPECT_EQ(cfg.serverName, "my_server");
    EXPECT_EQ(cfg.tcpPort, 8080);
    EXPECT_EQ(cfg.timeoutMs, 10000);
    EXPECT_TRUE(cfg.allowTcpFallback);
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
    EXPECT_NE(IpcMessageType::Request, IpcMessageType::Event);
    EXPECT_NE(IpcMessageType::Request, IpcMessageType::Error);
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
    for (auto t : {IpcTransport::Auto, IpcTransport::NamedPipe, IpcTransport::TcpPipe, IpcTransport::UnixSocket}) {
        EXPECT_NO_THROW(IpcFactory::isTransportAvailable(t));
    }
}

TEST(IpcFactoryTest, DefaultEndpointIsConsistent) {
    std::string ep1 = IpcFactory::getDefaultEndpoint();
    std::string ep2 = IpcFactory::getDefaultEndpoint();
    EXPECT_EQ(ep1, ep2);
}
