// Unix domain socket 通道测试：依赖 POSIX socket API（sys/un.h、::getpid 等），
// 仅在非 Windows 平台编译。CMake 侧同样按 NOT WIN32 排除，此处守卫为双保险。
#if !defined(_WIN32)

#include <gtest/gtest.h>
#include "platform/posix/unix_socket_channel.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace wingman::ipc;

namespace {

std::string makeSocketPath() {
    static std::atomic<uint64_t> counter{0};
    const char* tmpdir = std::getenv("TMPDIR");
    std::string base = (tmpdir && *tmpdir) ? tmpdir : "/tmp";
    return base + "/wingman_unix_test_" + std::to_string(::getpid()) + "_"
         + std::to_string(++counter) + ".sock";
}

class MessageCollector {
public:
    void record(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        messages.push_back(msg);
        cv_.notify_all();
    }

    bool waitFor(size_t expected, std::chrono::milliseconds timeout = 2s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return messages.size() >= expected; });
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages.size();
    }

    IpcMessage at(size_t idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages.at(idx);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<IpcMessage> messages;
};

} // namespace

// ========== 构造与状态 ==========

TEST(UnixSocketChannelTest, ConstructServerMode) {
    UnixSocketChannel channel(true, "/tmp/test_server.sock");
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
    EXPECT_EQ(channel.getTransport(), IpcTransport::UnixSocket);
    EXPECT_EQ(channel.getBackendName(), "UnixSocket");
    EXPECT_EQ(channel.getEndpoint(), "/tmp/test_server.sock");
}

TEST(UnixSocketChannelTest, ConstructClientMode) {
    UnixSocketChannel channel(false, "/tmp/test_client.sock");
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getBackendName(), "UnixSocket");
}

TEST(UnixSocketChannelTest, DisconnectWhenDisconnectedIsNoOp) {
    UnixSocketChannel channel(false, "/tmp/nowhere.sock");
    EXPECT_NO_THROW(channel.disconnect());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
}

TEST(UnixSocketChannelTest, SendWhenDisconnectedReturnsFalse) {
    UnixSocketChannel channel(false, "/tmp/nowhere.sock");
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "test";
    EXPECT_FALSE(channel.send(msg));
    EXPECT_EQ(channel.sendRequest("method", "{}"), 0u);
    EXPECT_FALSE(channel.sendEvent("event", "{}"));
}

TEST(UnixSocketChannelTest, ClientConnectToMissingSocketFails) {
    UnixSocketChannel channel(false, "/tmp/wingman_missing_" + std::to_string(::getpid()) + ".sock");
    EXPECT_FALSE(channel.connect(""));
    EXPECT_EQ(channel.getState(), IpcState::Error);
}

TEST(UnixSocketChannelTest, ClientConnectOverridesEndpoint) {
    std::string path = makeSocketPath();
    UnixSocketChannel server(true, path);
    ASSERT_TRUE(server.connect(""));

    UnixSocketChannel client(false, "/tmp/somewhere_else.sock");
    // 通过 endpoint 参数覆盖路径
    EXPECT_TRUE(client.connect(path));
    EXPECT_EQ(client.getEndpoint(), path);

    client.disconnect();
    server.disconnect();
}

TEST(UnixSocketChannelTest, ServerBindFailureSetsError) {
    // 绑定到一个位于不存在目录里的路径 -> bind 失败
    UnixSocketChannel server(true, "/tmp/wingman_no_such_dir_12345/x.sock");
    EXPECT_FALSE(server.connect(""));
    EXPECT_EQ(server.getState(), IpcState::Error);
}

TEST(UnixSocketChannelTest, ServerConnectWhenAlreadyConnectedReturnsTrue) {
    std::string path = makeSocketPath();
    UnixSocketChannel server(true, path);
    ASSERT_TRUE(server.connect(""));
    // 已连接时再次 connect 直接返回 true
    EXPECT_TRUE(server.connect(""));
    server.disconnect();
}

// ========== 双向通信 ==========

class UnixSocketChannelTestEnv : public ::testing::Test {
protected:
    void SetUp() override {
        path = makeSocketPath();
        server = std::make_unique<UnixSocketChannel>(true, path);
        client = std::make_unique<UnixSocketChannel>(false, path);

        serverCollector = std::make_unique<MessageCollector>();
        clientCollector = std::make_unique<MessageCollector>();

        server->setMessageCallback([this](const IpcMessage& msg) {
            serverCollector->record(msg);
        });
        client->setMessageCallback([this](const IpcMessage& msg) {
            clientCollector->record(msg);
        });
    }

    void TearDown() override {
        if (client) client->disconnect();
        if (server) server->disconnect();
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    bool connectPair() {
        std::atomic<bool> serverOk{false};
        std::thread serverThread([&] {
            serverOk = server->connect("");
        });
        // 等服务器监听起来
        std::this_thread::sleep_for(50ms);
        if (!client->connect("")) {
            serverThread.join();
            return false;
        }
        serverThread.join();
        if (!serverOk.load()) return false;

        server->startReceiving();  // 触发 accept
        client->startReceiving();
        std::this_thread::sleep_for(100ms);
        return true;
    }

    std::string path;
    std::unique_ptr<UnixSocketChannel> server;
    std::unique_ptr<UnixSocketChannel> client;
    std::unique_ptr<MessageCollector> serverCollector;
    std::unique_ptr<MessageCollector> clientCollector;
};

TEST_F(UnixSocketChannelTestEnv, ConnectAndStates) {
    ASSERT_TRUE(connectPair());
    EXPECT_TRUE(server->isConnected());
    EXPECT_TRUE(client->isConnected());
}

TEST_F(UnixSocketChannelTestEnv, ClientToServerRequest) {
    ASSERT_TRUE(connectPair());

    uint64_t id = client->sendRequest("test.method", R"({"key":"value"})");
    EXPECT_GT(id, 0u);

    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Request);
    EXPECT_EQ(msg.method, "test.method");
    EXPECT_EQ(msg.id, id);
    EXPECT_NE(msg.payload.find("value"), std::string::npos);
}

TEST_F(UnixSocketChannelTestEnv, ClientToServerEvent) {
    ASSERT_TRUE(connectPair());

    EXPECT_TRUE(client->sendEvent("some.event", R"({"n":1})"));
    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Event);
    EXPECT_EQ(msg.method, "some.event");
    EXPECT_EQ(msg.id, 0u);
}

TEST_F(UnixSocketChannelTestEnv, ServerToClientResponse) {
    ASSERT_TRUE(connectPair());

    client->sendRequest("ping", "{}");

    IpcMessage response;
    response.type = IpcMessageType::Response;
    response.method = "ping";
    response.payload = R"({"ok":true})";
    response.id = 1;
    ASSERT_TRUE(server->send(response));

    ASSERT_TRUE(clientCollector->waitFor(1));
    auto msg = clientCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Response);
    EXPECT_EQ(msg.method, "ping");
    EXPECT_NE(msg.payload.find("ok"), std::string::npos);
}

TEST_F(UnixSocketChannelTestEnv, MultipleMessagesInOrder) {
    ASSERT_TRUE(connectPair());

    for (int i = 0; i < 10; ++i) {
        client->sendRequest("m" + std::to_string(i), "{}");
    }
    ASSERT_TRUE(serverCollector->waitFor(10));
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(serverCollector->at(i).method, "m" + std::to_string(i));
    }
}

TEST_F(UnixSocketChannelTestEnv, SequentialRequestIds) {
    ASSERT_TRUE(connectPair());

    uint64_t id1 = client->sendRequest("a", "{}");
    uint64_t id2 = client->sendRequest("b", "{}");
    EXPECT_EQ(id2, id1 + 1);
}

TEST_F(UnixSocketChannelTestEnv, PayloadWithStringFallback) {
    ASSERT_TRUE(connectPair());

    // 非 JSON payload 应原样序列化传输
    client->sendRequest("raw", "not-a-json-payload");
    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.method, "raw");
    EXPECT_EQ(msg.payload, "not-a-json-payload");
}

TEST_F(UnixSocketChannelTestEnv, DisconnectPropagatesToPeer) {
    ASSERT_TRUE(connectPair());

    client->disconnect();
    EXPECT_FALSE(client->isConnected());

    // 服务端接收线程应感知到连接关闭并退出
    std::this_thread::sleep_for(200ms);
    server->disconnect();
    EXPECT_FALSE(server->isConnected());
}

TEST_F(UnixSocketChannelTestEnv, StartReceivingIdempotent) {
    ASSERT_TRUE(connectPair());

    // 重复 startReceiving 不应启动第二个线程
    server->startReceiving();
    client->startReceiving();
    std::this_thread::sleep_for(50ms);

    client->sendRequest("x", "{}");
    EXPECT_TRUE(serverCollector->waitFor(1));
}

TEST_F(UnixSocketChannelTestEnv, StopReceivingWithoutStartIsSafe) {
    // 名副其实的「未 startReceiving 直接 stop」：仅建立连接，不启动接收线程。
    // 接收线程阻塞在无超时的 recv() 上，对端存活时裸调 stopReceiving 会永久
    // join 挂起——这正是 disconnect() 先 shutdown 再 stopReceiving 的原因，
    // 因此本用例只验证无接收线程时 stop 是安全 no-op。
    std::atomic<bool> serverOk{false};
    std::thread serverThread([&] {
        serverOk = server->connect("");
    });
    // 等服务器监听起来
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(client->connect(""));
    serverThread.join();
    ASSERT_TRUE(serverOk.load());

    EXPECT_NO_THROW(client->stopReceiving());
    // 断开后仍能安全析构
    client->disconnect();
    server->disconnect();
}

TEST_F(UnixSocketChannelTestEnv, SocketFileRemovedOnServerDisconnect) {
    ASSERT_TRUE(connectPair());
    client->disconnect();
    server->disconnect();
    EXPECT_FALSE(std::filesystem::exists(path));
}

// ========== 第十批补测：错误回调 / 序列化边界 / 原始帧注入 / SIGPIPE ==========

namespace {

// 绕过 IpcMessage 封装的裸 socket 客户端：直接发长度前缀帧，用于注入
// 协议层无法构造的畸形输入（0 长度帧 / 非 JSON body）。
class RawSocketClient {
public:
    explicit RawSocketClient(const std::string& path) {
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    ~RawSocketClient() {
        if (fd_ >= 0) ::close(fd_);
    }
    bool valid() const { return fd_ >= 0; }

    // 4 字节小端长度前缀 + body
    void sendFrame(const std::string& body) const {
        uint32_t len = static_cast<uint32_t>(body.size());
        std::string frame(reinterpret_cast<const char*>(&len), sizeof(len));
        frame += body;
        size_t off = 0;
        while (off < frame.size()) {
            ssize_t n = ::send(fd_, frame.data() + off, frame.size() - off, 0);
            if (n <= 0) break;
            off += static_cast<size_t>(n);
        }
    }

private:
    int fd_ = -1;
};

} // namespace

TEST_F(UnixSocketChannelTestEnv, ErrorCallbackFiresOnConnectFailure) {
    // setErrorCallback 此前零调用方；连接失败 setState(Error) 时回调触发
    UnixSocketChannel dead(false, path + ".missing");
    std::atomic<bool> fired{false};
    std::string received;
    dead.setErrorCallback([&](const std::string& msg) {
        fired = true;
        received = msg;
    });
    EXPECT_FALSE(dead.connect(""));
    EXPECT_EQ(dead.getState(), IpcState::Error);
    EXPECT_TRUE(fired.load());
    EXPECT_NE(received.find(".missing"), std::string::npos);
}

TEST_F(UnixSocketChannelTestEnv, EmptyPayloadSerializedAsObject) {
    // serializeMessage 空 payload → j["payload"]=object 分支此前未触达
    ASSERT_TRUE(connectPair());
    EXPECT_GT(client->sendRequest("empty.payload", ""), 0u);
    ASSERT_TRUE(serverCollector->waitFor(1));
    EXPECT_EQ(serverCollector->at(0).method, "empty.payload");
}

TEST_F(UnixSocketChannelTestEnv, MalformedJsonYieldsErrorMessage) {
    // 坏 JSON 帧 → deserializeMessage 异常分支 → Error 消息进回调
    server->connect("");
    RawSocketClient raw(path);
    ASSERT_TRUE(raw.valid());
    server->startReceiving();  // accept 裸客户端
    std::this_thread::sleep_for(100ms);

    raw.sendFrame("{{{not-json-at-all");
    ASSERT_TRUE(serverCollector->waitFor(1));
    EXPECT_EQ(serverCollector->at(0).type, IpcMessageType::Error);
}

TEST_F(UnixSocketChannelTestEnv, ZeroLengthFrameDropsConnection) {
    // 0 长度帧 → receiveLoop 长度校验 break → 通道转 Disconnected
    server->connect("");
    RawSocketClient raw(path);
    ASSERT_TRUE(raw.valid());
    server->startReceiving();
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(server->isConnected());

    raw.sendFrame("");
    for (int i = 0; i < 40 && server->isConnected(); ++i) {
        std::this_thread::sleep_for(25ms);
    }
    EXPECT_FALSE(server->isConnected());
}

TEST_F(UnixSocketChannelTestEnv, SendAfterPeerDisconnectFailsGracefully) {
    // SIGPIPE 修复回归：对端断开后 send 必须返回 false 并置 Error
    // （MSG_NOSIGNAL / SO_NOSIGPIPE 屏蔽），而不是默认 SIGPIPE 终止整个
    // 测试进程。全程不启动接收线程：接收线程阻塞在无超时 recv() 上，对端
    // 存活时裸调 stopReceiving 会永久挂起（既有 StopReceivingWithoutStartIsSafe
    // 用例注释），此处 server disconnect 内部先 shutdown 再 stop，接收线程
    // 经 EOF 干净退出；client 无接收线程故不会把状态抢先改成 Disconnected。
    ASSERT_TRUE(server->connect(""));
    ASSERT_TRUE(client->connect(""));
    server->startReceiving();  // 仅 accept，client 侧保持 Connected
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(client->isConnected());

    server->disconnect();

    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "after.peer.close";
    EXPECT_FALSE(client->send(msg));
    EXPECT_EQ(client->getState(), IpcState::Error);
}

#endif // !defined(_WIN32)
