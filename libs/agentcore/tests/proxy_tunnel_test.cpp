#include "agentcore_test_common.hpp"

// ========== ProxyTunnel ==========

// 代理数据面（proxy.* Notify 的裸 TCP 拨号分支）回环真连测试：
// 测试侧 acceptor 扮演 target，收集器扮演 server 侧中继。全部等待有界。

namespace {

// 测试内联 base64（独立参考实现，顺带核对内建实现的向量一致性）
std::string testBase64Encode(const std::string& in) {
    static const char* tab =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::uint32_t buf = 0;
    int bits = 0;
    for (unsigned char c : in) {
        buf = (buf << 8) | c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += tab[(buf >> bits) & 0x3F];
        }
    }
    if (bits > 0) out += tab[(buf << (6 - bits)) & 0x3F];
    out += std::string((4 - out.size() % 4) % 4, '=');
    return out;
}

// 收集 Tunnel 发出的 proxy.* 帧；waitFor 轮询等待命中谓词的帧。
class SendCollector {
public:
    bool send(const std::string& type, const nlohmann::json& fields) {
        std::lock_guard<std::mutex> lock(mu_);
        frames_.emplace_back(type, fields);
        return true;
    }

    // 返回命中的首个帧（type, fields），无命中返回 { "", null }。
    std::pair<std::string, nlohmann::json> waitFor(
        const std::function<bool(const std::string&, const nlohmann::json&)>& hit,
        std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            {
                std::lock_guard<std::mutex> lock(mu_);
                for (const auto& [type, fields] : frames_) {
                    if (hit(type, fields)) return {type, fields};
                }
            }
            if (std::chrono::steady_clock::now() > deadline) {
                return {"", nullptr};
            }
            std::this_thread::sleep_for(10ms);
        }
    }

private:
    std::mutex mu_;
    std::vector<std::pair<std::string, nlohmann::json>> frames_;
};

// target 端 acceptor：接受 Tunnel 的拨入并持有连接。
// 成员声明顺序即初始化顺序：io_ 必须先于 acceptor_（在其上开监听）。
class TargetServer {
public:
    TargetServer() {
        asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
        acceptor_.open(ep.protocol());
        acceptor_.bind(ep);
        acceptor_.listen();
        port_ = acceptor_.local_endpoint().port();
        acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket sock) {
            if (!ec) {
                sock_ = std::move(sock);
                connected_.store(true);
            }
        });
        thread_ = std::thread([this] { io_.run(); });
    }

    ~TargetServer() {
        asio::error_code ignored;
        acceptor_.close(ignored);
        if (sock_) {
            sock_->close(ignored);
        }
        if (thread_.joinable()) thread_.join();
    }

    uint16_t port() const { return port_; }
    bool waitConnected(std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!connected_.load()) {
            if (std::chrono::steady_clock::now() > deadline) return false;
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }

    // 关闭 target 侧连接（模拟目标服务断开 → Tunnel 读泵 EOF）
    void closePeer() {
        asio::error_code ignored;
        if (sock_) sock_->close(ignored);
        closed_ = true;
    }

    // target → tunnel 方向写（Tunnel 读泵收进 proxy.data）
    bool writeSome(const std::string& data) {
        if (!sock_ || closed_) return false;
        asio::error_code ec;
        asio::write(*sock_, asio::buffer(data), ec);
        return !ec;
    }

    // tunnel → target 方向读（handleData 的写入落点）
    std::string readSome(std::chrono::milliseconds timeout = 5000ms) {
        if (!sock_ || closed_) return "";
        asio::error_code ec;
        sock_->non_blocking(true, ec);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::string got;
        char buf[4096];
        while (std::chrono::steady_clock::now() < deadline) {
            std::size_t n = sock_->read_some(asio::buffer(buf), ec);
            if (n > 0) {
                got.append(buf, n);
                break;
            }
            if (ec && ec != asio::error::would_block) break;
            std::this_thread::sleep_for(10ms);
        }
        sock_->non_blocking(false, ec);
        return got;
    }

    // 对端读 EOF/错误（Tunnel 关闭后成立）
    bool waitEof(std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            if (closed_) return true;
            if (!sock_) return false;
            asio::error_code ec;
            sock_->non_blocking(true, ec);
            char buf[16];
            std::size_t n = sock_->read_some(asio::buffer(buf), ec);
            if (n == 0 && ec == asio::error::eof) return true;
            if (ec && ec != asio::error::would_block) return true; // reset/bad_descriptor 皆算收口
            if (std::chrono::steady_clock::now() > deadline) return false;
            std::this_thread::sleep_for(10ms);
        }
    }

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_{io_};
    std::optional<asio::ip::tcp::socket> sock_;
    std::thread thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> closed_{false};
    uint16_t port_ = 0;
};

} // namespace

TEST(ProxyTunnelTest, LoopbackRoundTripBothDirections) {
    TargetServer target;
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"target", "127.0.0.1:" + std::to_string(target.port())},
    });
    ASSERT_TRUE(target.waitConnected());

    // target → tunnel 读泵 → proxy.data（base64）
    const std::string payload = "hello-tunnel-payload";
    ASSERT_TRUE(target.writeSome(payload));
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.data";
    });
    EXPECT_EQ(type, "proxy.data");
    if (type == "proxy.data") {
        EXPECT_EQ(fields.value("proxyId", ""), "guac:sess-1");
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-1-1");
        EXPECT_EQ(fields.value("data", ""), testBase64Encode(payload));
    }

    // server → tunnel handleData → 落到 target socket
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"data", testBase64Encode("reply-frame")},
    });
    EXPECT_EQ(target.readSome(), "reply-frame");

    // server 侧关闭 → tunnel 收口（对端读到 EOF），不回发
    tunnel->handleNotify("proxy.close", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"reason", "test"},
    });
    EXPECT_TRUE(target.waitEof());
    tunnel->stop();
}

TEST(ProxyTunnelTest, TargetEofNotifiesServerSideClose) {
    TargetServer target;
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-2"},
        {"connId", "guac:sess-2-1"},
        {"target", "127.0.0.1:" + std::to_string(target.port())},
    });
    ASSERT_TRUE(target.waitConnected());
    // target 主动断开（模拟目标服务关闭）→ tunnel 读泵 EOF → proxy.close 回发
    {
        asio::error_code ignored;
        target.closePeer();
    }
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.close";
    });
    EXPECT_EQ(type, "proxy.close");
    if (type == "proxy.close") {
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-2-1");
        EXPECT_FALSE(fields.value("reason", "").empty());
    }
    tunnel->stop();
}

TEST(ProxyTunnelTest, DialFailureReportsProxyError) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-3"},
        {"connId", "guac:sess-3-1"},
        {"target", "127.0.0.1:1"}, // 回环保留端口，必然拒绝
    });
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.error";
    });
    EXPECT_EQ(type, "proxy.error");
    if (type == "proxy.error") {
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-3-1");
        EXPECT_FALSE(fields.value("error", "").empty());
    }

    // 对已失败连接的 proxy.data 静默丢弃，不崩
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-3"},
        {"connId", "guac:sess-3-1"},
        {"data", testBase64Encode("stray")},
    });
    tunnel->stop();
}

TEST(ProxyTunnelTest, StopSendsProxyErrorForLateNew) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });
    tunnel->stop();
    tunnel->stop(); // 幂等

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-4"},
        {"connId", "guac:sess-4-1"},
        {"target", "127.0.0.1:1"},
    });
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.error";
    });
    EXPECT_EQ(type, "proxy.error");
    if (type == "proxy.error") {
        EXPECT_EQ(fields.value("error", ""), "shutting down");
    }
}

TEST(ProxyTunnelTest, BadTargetAndBadBase64AreRejectedGracefully) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    // 缺字段
    tunnel->handleNotify("proxy.new", {{"proxyId", "guac:x"}});
    // 非法 target
    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-5"}, {"connId", "guac:sess-5-1"}, {"target", "no-port"},
    });
    // 非法 base64
    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}, {"target", "127.0.0.1:1"},
    });
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}, {"data", "!!not-base64!!"},
    });
    // 未知 type 忽略；proxy.error 是 runtime→server 方向，收到即忽略
    tunnel->handleNotify("proxy.error", {{"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}});

    // 无拨号成功的连接、无崩溃即为通过（有界收尾）
    tunnel->stop();
}
