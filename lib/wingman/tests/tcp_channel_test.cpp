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
