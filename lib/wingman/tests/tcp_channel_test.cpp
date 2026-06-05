#include <gtest/gtest.h>
#include "wingman/ipc/tcp_channel.hpp"

using namespace wingman::ipc;

// ========== Construction ==========

TEST(TcpChannelTest, ConstructClientMode) {
    TcpChannel channel(false, "127.0.0.1", 12345);
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
    EXPECT_EQ(channel.getTransport(), IpcTransport::TcpPipe);
    EXPECT_EQ(channel.getBackendName(), "TCP");
    EXPECT_EQ(channel.getEndpoint(), "127.0.0.1:12345");
}

TEST(TcpChannelTest, ConstructServerMode) {
    TcpChannel channel(true, "0.0.0.0", 54321);
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
    EXPECT_EQ(channel.getTransport(), IpcTransport::TcpPipe);
    EXPECT_EQ(channel.getBackendName(), "TCP");
    EXPECT_EQ(channel.getEndpoint(), "0.0.0.0:54321");
}

// ========== Disconnect ==========

TEST(TcpChannelTest, DisconnectWhenDisconnectedIsNoOp) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_NO_THROW(channel.disconnect());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
}

// ========== Send When Disconnected ==========

TEST(TcpChannelTest, SendWhenDisconnectedReturnsFalse) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "test";
    EXPECT_FALSE(channel.send(msg));
}

TEST(TcpChannelTest, SendRequestWhenDisconnectedReturnsZero) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_EQ(channel.sendRequest("method", "payload"), 0u);
}

TEST(TcpChannelTest, SendEventWhenDisconnectedReturnsFalse) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_FALSE(channel.sendEvent("event", "data"));
}

// ========== Callbacks ==========

TEST(TcpChannelTest, SetMessageCallbackDoesNotCrash) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_NO_THROW(channel.setMessageCallback([](const IpcMessage&) {}));
}

TEST(TcpChannelTest, SetErrorCallbackDoesNotCrash) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_NO_THROW(channel.setErrorCallback([](const std::string&) {}));
}

// ========== Receive ==========

TEST(TcpChannelTest, StopReceivingWhenNotStartedDoesNotCrash) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    EXPECT_NO_THROW(channel.stopReceiving());
}

TEST(TcpChannelTest, StartReceivingWhenNotConnectedDoesNotCrash) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    // startReceiving without connect will launch a thread that quickly exits
    // because isConnected() returns false
    EXPECT_NO_THROW(channel.startReceiving());
    EXPECT_NO_THROW(channel.stopReceiving());
}

// ========== Message Construction ==========

TEST(TcpChannelTest, IpcMessageDefaults) {
    IpcMessage msg;
    EXPECT_EQ(msg.type, IpcMessageType::Request);
    EXPECT_TRUE(msg.method.empty());
    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.id, 0u);
    EXPECT_EQ(msg.timestamp, 0u);
}

TEST(TcpChannelTest, IpcMessageFieldAssignment) {
    IpcMessage msg;
    msg.type = IpcMessageType::Event;
    msg.method = "test.method";
    msg.payload = R"({"key":"value"})";
    msg.id = 42;
    msg.timestamp = 1234567890;

    EXPECT_EQ(msg.type, IpcMessageType::Event);
    EXPECT_EQ(msg.method, "test.method");
    EXPECT_EQ(msg.payload, R"({"key":"value"})");
    EXPECT_EQ(msg.id, 42u);
    EXPECT_EQ(msg.timestamp, 1234567890u);
}

// ========== Multiple Construct/Destruct ==========

TEST(TcpChannelTest, MultipleConstructDestructDoesNotCrash) {
    for (int i = 0; i < 10; ++i) {
        TcpChannel channel(false, "127.0.0.1", 10000 + i);
        EXPECT_EQ(channel.getState(), IpcState::Disconnected);
    }
}

// ========== Endpoint Formatting ==========

TEST(TcpChannelTest, GetEndpointFormatsHostPort) {
    TcpChannel channel(false, "192.168.1.1", 8080);
    EXPECT_EQ(channel.getEndpoint(), "192.168.1.1:8080");
}

TEST(TcpChannelTest, GetEndpointDifferentPorts) {
    TcpChannel c1(false, "127.0.0.1", 1);
    TcpChannel c2(false, "127.0.0.1", 65535);
    EXPECT_EQ(c1.getEndpoint(), "127.0.0.1:1");
    EXPECT_EQ(c2.getEndpoint(), "127.0.0.1:65535");
}

// ========== Multiple Send Attempts ==========

TEST(TcpChannelTest, MultipleSendWhenDisconnected) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "test";
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(channel.send(msg));
    }
}

TEST(TcpChannelTest, SendEventMultipleWhenDisconnected) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(channel.sendEvent("evt", "data"));
    }
}

TEST(TcpChannelTest, SendRequestMultipleWhenDisconnected) {
    TcpChannel channel(false, "127.0.0.1", 9999);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(channel.sendRequest("method", "payload"), 0u);
    }
}

// ========== Message Types ==========

TEST(TcpChannelTest, IpcMessageTypeValues) {
    EXPECT_EQ(static_cast<int>(IpcMessageType::Request), 0);
    EXPECT_EQ(static_cast<int>(IpcMessageType::Response), 1);
    EXPECT_EQ(static_cast<int>(IpcMessageType::Event), 2);
}

TEST(TcpChannelTest, IpcStateValues) {
    EXPECT_EQ(static_cast<int>(IpcState::Disconnected), 0);
    EXPECT_EQ(static_cast<int>(IpcState::Connecting), 1);
    EXPECT_EQ(static_cast<int>(IpcState::Connected), 2);
    EXPECT_EQ(static_cast<int>(IpcState::Disconnecting), 3);
    EXPECT_EQ(static_cast<int>(IpcState::Error), 4);
}

TEST(TcpChannelTest, IpcTransportValues) {
    // Verify enum values are accessible (exact values depend on header definition)
    EXPECT_NO_THROW(static_cast<int>(IpcTransport::NamedPipe));
    EXPECT_NO_THROW(static_cast<int>(IpcTransport::TcpPipe));
    EXPECT_NE(static_cast<int>(IpcTransport::NamedPipe),
              static_cast<int>(IpcTransport::TcpPipe));
}

// ========== Message With Various Fields ==========

TEST(TcpChannelTest, IpcMessageWithEmptyMethod) {
    IpcMessage msg;
    msg.type = IpcMessageType::Response;
    msg.method = "";
    msg.payload = "data";
    msg.id = 999;
    EXPECT_EQ(msg.type, IpcMessageType::Response);
    EXPECT_TRUE(msg.method.empty());
    EXPECT_EQ(msg.id, 999u);
}

TEST(TcpChannelTest, IpcMessageWithLargePayload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Event;
    msg.method = "bulk";
    msg.payload = std::string(10000, 'x');
    EXPECT_EQ(msg.payload.size(), 10000u);
}

// ========== Server-Client Connection ==========

TEST(TcpChannelTest, ServerClientConnectAndDisconnect) {
    // Use a random high port to avoid conflicts
    int port = 19000 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    // Start server in background (accept blocks)
    std::thread serverThread([&server]() {
        server.connect("");
    });

    // Give server time to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client connects
    EXPECT_TRUE(client.connect(""));
    EXPECT_TRUE(client.isConnected());

    // Server should now be connected too
    serverThread.join();
    EXPECT_TRUE(server.isConnected());

    // Disconnect both
    client.disconnect();
    EXPECT_FALSE(client.isConnected());

    server.disconnect();
    EXPECT_FALSE(server.isConnected());
}

TEST(TcpChannelTest, SendAndReceiveMessage) {
    int port = 19500 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    bool received = false;
    server.setMessageCallback([&](const IpcMessage&) {
        received = true;
    });

    // Connect
    std::thread serverThread([&server]() { server.connect(""); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(client.connect(""));
    serverThread.join();

    // Start receiving on server
    server.startReceiving();

    // Client sends a message
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "test.method";
    msg.payload = R"({"key":"value"})";
    msg.id = 1;
    EXPECT_TRUE(client.send(msg));

    // Wait for message to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Callback should have been invoked (deserialization is stub, so fields may be empty)
    EXPECT_TRUE(received);

    // Disconnect client first so server's blocking recv() returns 0
    client.disconnect();
    server.stopReceiving();
    server.disconnect();
}

TEST(TcpChannelTest, SendRequestReturnsNonZeroId) {
    int port = 19600 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    std::thread serverThread([&server]() { server.connect(""); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(client.connect(""));
    serverThread.join();

    uint64_t id = client.sendRequest("test.method", "payload");
    EXPECT_GT(id, 0u);

    client.disconnect();
    server.disconnect();
}

TEST(TcpChannelTest, SendEventReturnsTrue) {
    int port = 19700 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    std::thread serverThread([&server]() { server.connect(""); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(client.connect(""));
    serverThread.join();

    EXPECT_TRUE(client.sendEvent("test.event", "event data"));

    client.disconnect();
    server.disconnect();
}

TEST(TcpChannelTest, ConnectWithEndpointString) {
    int port = 19800 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    std::thread serverThread([&server]() {
        server.connect("");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect with endpoint string
    std::string endpoint = "127.0.0.1:" + std::to_string(port);
    EXPECT_TRUE(client.connect(endpoint));
    EXPECT_TRUE(client.isConnected());

    serverThread.join();
    client.disconnect();
    server.disconnect();
}

TEST(TcpChannelTest, ConnectAlreadyConnectedReturnsTrue) {
    int port = 19900 + (rand() % 1000);

    TcpChannel server(true, "127.0.0.1", port);
    TcpChannel client(false, "127.0.0.1", port);

    std::thread serverThread([&server]() { server.connect(""); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(client.connect(""));
    serverThread.join();

    // Second connect should return true
    EXPECT_TRUE(client.connect(""));

    client.disconnect();
    server.disconnect();
}

// Note: ErrorCallbackInvokedOnBindFailure is not testable on Windows because
// SO_REUSEADDR allows multiple sockets to bind the same port.
// Note: ClientConnectToNonExistentServerTimesOut removed — connect() retries
// 50 times with ~2s system timeout each, causing ~100s test duration.
