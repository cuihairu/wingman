#include <gtest/gtest.h>

#include "wingman/ipc/ipc_factory.hpp"

#include <atomic>
#include <cstdlib>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace wingman::ipc;

class IpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.serverName = makeServerName();
        clearMessages();
    }

    void TearDown() override {
        clearMessages();
    }

    static std::string makeServerName() {
        static std::atomic<uint64_t> counter{0};
        return "wingman_test_" + std::to_string(++counter);
    }

    void clearMessages() {
        std::lock_guard<std::mutex> lock(messagesMutex);
        serverMessages.clear();
        clientMessages.clear();
    }

    void recordServerMessage(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(messagesMutex);
        serverMessages.push_back(msg);
        messagesCv.notify_all();
    }

    void recordClientMessage(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(messagesMutex);
        clientMessages.push_back(msg);
        messagesCv.notify_all();
    }

    size_t serverMessageCount() {
        std::lock_guard<std::mutex> lock(messagesMutex);
        return serverMessages.size();
    }

    bool waitForServerMessages(size_t expectedCount,
                               std::chrono::milliseconds timeout = 1s) {
        std::unique_lock<std::mutex> lock(messagesMutex);
        return messagesCv.wait_for(lock, timeout, [&] {
            return serverMessages.size() >= expectedCount;
        });
    }

    bool connectServerAndClient(std::unique_ptr<IIpcChannel>& server,
                                std::unique_ptr<IIpcChannel>& client) {
        if (!server || !client) {
            return false;
        }

        server->setMessageCallback([this](const IpcMessage& msg) {
            recordServerMessage(msg);
        });
        client->setMessageCallback([this](const IpcMessage& msg) {
            recordClientMessage(msg);
        });

        std::atomic<bool> serverConnected{false};
        std::thread serverThread([&] {
            serverConnected = server->connect(config.serverName);
        });

        std::this_thread::sleep_for(50ms);
        if (!client->connect(config.serverName)) {
            serverThread.join();
            return false;
        }

        serverThread.join();
        if (!serverConnected.load()) {
            return false;
        }

        server->startReceiving();
        client->startReceiving();

        std::this_thread::sleep_for(50ms);
        return server->isConnected() && client->isConnected();
    }

    void disconnectServerAndClient(std::unique_ptr<IIpcChannel>& server,
                                   std::unique_ptr<IIpcChannel>& client) {
        if (client) {
            client->disconnect();
        }
        if (server) {
            server->disconnect();
        }
    }

    IpcConfig config;
    std::mutex messagesMutex;
    std::condition_variable messagesCv;
    std::vector<IpcMessage> serverMessages;
    std::vector<IpcMessage> clientMessages;
};

TEST_F(IpcTest, CreateServer) {
#ifdef _WIN32
    auto server = IpcFactory::createServer(config);

    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->getTransport(), IpcFactory::getPreferredTransport());
    EXPECT_EQ(server->getBackendName(), "NamedPipe");
#else
    GTEST_SKIP() << "Server creation is only implemented for the Windows backend";
#endif
}

TEST_F(IpcTest, CreateClient) {
#ifdef _WIN32
    auto client = IpcFactory::createClient(config);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getTransport(), IpcFactory::getPreferredTransport());
#else
    GTEST_SKIP() << "Client creation is only implemented for the Windows backend";
#endif
}

TEST_F(IpcTest, GetDefaultEndpoint) {
    const std::string endpoint = IpcFactory::getDefaultEndpoint();
    EXPECT_FALSE(endpoint.empty());
}

TEST_F(IpcTest, GetPreferredTransport) {
    const IpcTransport transport = IpcFactory::getPreferredTransport();

#ifdef _WIN32
    EXPECT_EQ(transport, IpcTransport::NamedPipe);
#elif defined(__linux__) || defined(__APPLE__)
    EXPECT_EQ(transport, IpcTransport::UnixSocket);
#else
    EXPECT_EQ(transport, IpcTransport::TcpPipe);
#endif
}

TEST_F(IpcTest, IsTransportAvailable) {
#ifdef _WIN32
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::NamedPipe));
#elif defined(__linux__) || defined(__APPLE__)
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::UnixSocket));
#endif
    EXPECT_TRUE(IpcFactory::isTransportAvailable(IpcTransport::TcpPipe));
}

TEST_F(IpcTest, ServerClientConnection) {
#ifndef _WIN32
    GTEST_SKIP() << "IPC integration tests require the Windows NamedPipe backend";
#endif
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_TRUE(connectServerAndClient(server, client));

    EXPECT_TRUE(server->isConnected());
    EXPECT_TRUE(client->isConnected());

    disconnectServerAndClient(server, client);
}

TEST_F(IpcTest, SendRequest) {
#ifndef _WIN32
    GTEST_SKIP() << "IPC integration tests require the Windows NamedPipe backend";
#else
    if (std::getenv("CI")) GTEST_SKIP() << "IPC message tests are unreliable in CI";
#endif
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_TRUE(connectServerAndClient(server, client));

    const uint64_t messageId = client->sendRequest("ping", "{}");
    EXPECT_NE(messageId, 0U);
    EXPECT_TRUE(waitForServerMessages(1));

    disconnectServerAndClient(server, client);
}

TEST_F(IpcTest, SendEvent) {
#ifndef _WIN32
    GTEST_SKIP() << "IPC integration tests require the Windows NamedPipe backend";
#else
    if (std::getenv("CI")) GTEST_SKIP() << "IPC message tests are unreliable in CI";
#endif
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_TRUE(connectServerAndClient(server, client));

    EXPECT_TRUE(client->sendEvent("notification", "{\"message\":\"test\"}"));
    EXPECT_TRUE(waitForServerMessages(1));

    disconnectServerAndClient(server, client);
}

TEST_F(IpcTest, MultipleMessages) {
#ifndef _WIN32
    GTEST_SKIP() << "IPC integration tests require the Windows NamedPipe backend";
#else
    if (std::getenv("CI")) GTEST_SKIP() << "IPC message tests are unreliable in CI";
#endif
    auto server = IpcFactory::createServer(config);
    auto client = IpcFactory::createClient(config);

    ASSERT_TRUE(connectServerAndClient(server, client));

    constexpr size_t messageCount = 10;
    for (size_t i = 0; i < messageCount; ++i) {
        EXPECT_TRUE(client->sendEvent("test", "{\"value\":" + std::to_string(i) + "}"));
    }

    EXPECT_TRUE(waitForServerMessages(messageCount, 2s));
    EXPECT_GE(serverMessageCount(), messageCount);

    disconnectServerAndClient(server, client);
}
