#pragma once
// wingman::agentcore 单元测试
//
// 覆盖两块此前 0% 行覆盖的核心：
//   - EventBuffer   本地 UI 事件缓冲（容量驱逐 / sink 转发 / drain pull 模型）
//   - RemoteClient  出站远程链路（注册 / 心跳 / 命令分发 / 断线 outbox / 重连）
//
// RemoteClient 经 loopback TcpServer 真连真收发（同 libs/transport 的
// TransportEnv 模式）；全部等待都有界，整体 suite 无外部依赖、headless 可跑。

#include <gtest/gtest.h>

#include "proxy_tunnel.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/agentcore/hmac_sha256.hpp"
#include "wingman/agentcore/remote_client.hpp"
#include "wingman/agentcore/remote_client_config.hpp"
#include "wingman/transport/transport.hpp"

#include <nlohmann/json.hpp>
#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace wingman::runtime;
using namespace wingman::transport;
using namespace std::chrono_literals;


// 测试内联 hex 小写（独立参考实现，核对 hmac_sha256 的向量一致性）
inline std::string testHexEncode(const std::string& in) {
    static const char* tab = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) {
        out += tab[c >> 4];
        out += tab[c & 0x0f];
    }
    return out;
}

// 选择一个空闲端口（通过绑定后释放来探测）
inline int findFreePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    auto port = acceptor.local_endpoint().port();
    acceptor.close();
    return static_cast<int>(port);
}

class RemoteClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        port_ = findFreePort();
        server_ = createTcpServer();
        server_->setMessageHandler([this](const MessagePtr& msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            serverMessages_.push_back(msg);
            cv_.notify_all();
        });
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
    }

    void TearDown() override {
        client_.reset();
        server_->stop();
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
    }

    RemoteClientConfig makeConfig(int heartbeatSeconds = 30) {
        RemoteClientConfig config;
        config.serverIp = "127.0.0.1";
        config.serverPort = port_;
        config.heartbeatInterval = heartbeatSeconds;
        config.reconnectInterval = 1;
        config.maxReconnectInterval = 1;
        return config;
    }

    bool startServer() {
        if (!server_->listen("127.0.0.1", port_)) return false;
        return server_->start();
    }

    bool waitServerMessages(std::size_t n, std::chrono::milliseconds timeout = 3000ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return serverMessages_.size() >= n; });
    }

    // 启动服务端与客户端，等 agent.register 到达并回注册确认（可选失败确认）。
    bool connectAndRegister(bool ackSuccess = true, const std::string& ackId = "srv-42") {
        if (!startServer()) return false;
        client_ = std::make_unique<RemoteClient>(makeConfig());
        if (!client_->start()) return false;
        if (!waitServerMessages(1)) return false;
        nlohmann::json ack = {
            {"type", "agent.register_ack"},
            {"success", ackSuccess},
            {"agentId", ackId},
        };
        auto msg = Message::create(MessageType::Notify, ack.dump());
        return server_->send(server_->getSessionIds()[0], msg);
    }

    bool waitClientState(RemoteClient& client, const std::string& stateName,
                         std::chrono::milliseconds timeout = 3000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (client.connectionStateName() == stateName) return true;
            std::this_thread::sleep_for(10ms);
        }
        return client.connectionStateName() == stateName;
    }

    int port_ = 0;
    TcpServerPtr server_;
    std::unique_ptr<RemoteClient> client_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<MessagePtr> serverMessages_;
};

