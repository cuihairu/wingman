#include <gtest/gtest.h>
#include "wingman/ipc/ipc_factory.hpp"
#include <thread>
#include <chrono>

using namespace wingman::ipc;

class IpcTest : public ::testing::Test {
protected:
    IpcConfig config;
    std::vector<IpcMessage> serverMessages;
    std::vector<IpcMessage> clientMessages;

    void SetUp() override {
        config.serverName = "wingman_test";
        serverMessages.clear();
        clientMessages.clear();
    }

    void TearDown() override {
        // 清理
    }
};

// ========== 基本测试 ==========

TEST(IpcTest, CreateServer) {
    auto server = IpcFactory::createServer(config);

    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->getTransport(), IpcFactory::getPreferredTransport());
    EXPECT_EQ(server->getBackendName(), "NamedPipe");  // Windows
}

TEST(IpcTest, CreateClient) {
    auto client = IpcFactory::createClient(config);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getTransport(), IpcFactory::getPreferredTransport());
}

TEST(IpcTest, GetDefaultEndpoint) {
    std::string endpoint = IpcFactory::getDefaultEndpoint();

    EXPECT_FALSE(endpoint.empty());
}

TEST(IpcTest, GetPreferredTransport) {
    IpcTransport transport = IpcFactory::getPreferredTransport();

#ifdef _WIN32
    EXPECT_EQ(transport, IpcTransport::NamedPipe);
#elif defined(__linux__) || defined(__APPLE__)
    EXPECT_EQ(transport, IpcTransport::UnixSocket);
#endif
}

TEST(IpcTest, IsTransportAvailable) {
#ifdef _WIN32
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::NamedPipe));
#elif defined(__linux__) || defined(__APPLE__)
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::UnixSocket));
#endif
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::TcpPipe));
}

// ========== 连接测试 ==========

TEST(IpcTest, ServerClientConnection) {
    auto server = IpcFactory::createServer(config);
    ASSERT_NE(server, nullptr);

    // 设置回调
    server->setMessageCallback([&](const IpcMessage& msg) {
        serverMessages.push_back(msg);
    });

    // 启动服务端
    EXPECT_TRUE(server->connect("wingman_test"));
    server->startReceiving();

    // 创建客户端
    auto client = IpcFactory::createClient(config);
    ASSERT_NE(client, nullptr);

    client->setMessageCallback([&](const IpcMessage& msg) {
        clientMessages.push_back(msg);
    });

    // 连接到服务端
    EXPECT_TRUE(client->connect("wingman_test"));
    client->startReceiving();

    // 等待连接稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(server->isConnected());
    EXPECT_TRUE(client->isConnected());

    // 清理
    client->disconnect();
    server->disconnect();
}

// ========== 消息发送测试 ==========

TEST(IpcTest, SendRequest) {
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_NE(server, nullptr);
    ASSERT_NE(client, nullptr);

    server->setMessageCallback([&](const IpcMessage& msg) {
        serverMessages.push_back(msg);
    });

    EXPECT_TRUE(server->connect("wingman_test"));
    server->startReceiving();

    EXPECT_TRUE(client->connect("wingman_test"));

    // 发送请求
    uint64_t msgId = client->sendRequest("ping", "{}");
    EXPECT_NE(msgId, 0);

    // 等待消息
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GT(serverMessages.size(), 0);

    client->disconnect();
    server->disconnect();
}

TEST(IpcTest, SendEvent) {
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_NE(server, nullptr);
    ASSERT_NE(client, nullptr);

    server->setMessageCallback([&](const IpcMessage& msg) {
        serverMessages.push_back(msg);
    });

    EXPECT_TRUE(server->connect("wingman_test"));
    server->startReceiving();

    EXPECT_TRUE(client->connect("wingman_test"));

    // 发送事件
    EXPECT_TRUE(client->sendEvent("notification", "{\"message\":\"test\"}"));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GT(serverMessages.size(), 0);

    client->disconnect();
    server->disconnect();
}

// ========== 多个消息测试 ==========

TEST(IpcTest, MultipleMessages) {
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_NE(server, nullptr);
    ASSERT_NE(client, nullptr);

    server->setMessageCallback([&](const IpcMessage& msg) {
        serverMessages.push_back(msg);
    });

    EXPECT_TRUE(server->connect("wingman_test"));
    server->startReceiving();

    EXPECT_TRUE(client->connect("wingman_test"));

    // 发送多个消息
    for (int i = 0; i < 10; ++i) {
        client->sendEvent("test", "{\"value\":" + std::to_string(i) + "}");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(serverMessages.size(), 5);  // 至少收到部分消息

    client->disconnect();
    server->disconnect();
}
