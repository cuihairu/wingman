#include "proxy_tunnel.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <spdlog/spdlog.h>

namespace wingman::runtime {

// ========== base64（自含实现：agentcore 层禁止依赖 lib/wingman 的 crypt） ==========

namespace {

const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::uint8_t* data, std::size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < len) {
        std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kBase64Chars[(v >> 18) & 0x3F];
        out += kBase64Chars[(v >> 12) & 0x3F];
        out += kBase64Chars[(v >> 6) & 0x3F];
        out += kBase64Chars[v & 0x3F];
        i += 3;
    }
    if (i + 1 == len) {
        std::uint32_t v = data[i] << 16;
        out += kBase64Chars[(v >> 18) & 0x3F];
        out += kBase64Chars[(v >> 12) & 0x3F];
        out += "==";
    } else if (i + 2 == len) {
        std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8);
        out += kBase64Chars[(v >> 18) & 0x3F];
        out += kBase64Chars[(v >> 12) & 0x3F];
        out += kBase64Chars[(v >> 6) & 0x3F];
        out += "=";
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> base64Decode(const std::string& in) {
    auto valueOf = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve((in.size() / 4) * 3);
    std::uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int v = valueOf(c);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

// 目标地址解析："host:port"（host 允许 IPv6 字面量 [::1]:22 形式）。
std::optional<std::pair<std::string, std::string>> splitTarget(const std::string& target) {
    if (!target.empty() && target.front() == '[') {
        auto close = target.find(']');
        if (close == std::string::npos || close + 1 >= target.size() || target[close + 1] != ':') {
            return std::nullopt;
        }
        return std::make_pair(target.substr(1, close - 1), target.substr(close + 2));
    }
    auto colon = target.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= target.size()) {
        return std::nullopt;
    }
    return std::make_pair(target.substr(0, colon), target.substr(colon + 1));
}

} // anonymous namespace

// ========== 连接状态 ==========

// 线程模型：每连接一个 worker 线程跑完整连接生命周期的 io_context，socket
// 的全部操作（拨号/读泵/写入/关闭）都是该线程内的 async handler；其他线程
// 只经 post 投递或经 atomic 标志查询，绝不跨线程碰 asio socket。stop() 逐
// 连接 post 收口后 join 全部 worker——对象析构后不存在游离线程（退出期
// detached 线程与静态析构竞态是本模型要消灭的根问题）。
//
// proxy.data 可能先于拨号完成到达（guacd 的 connect 一成功就发协议首包）：
// 写入先进 per-conn 队列，worker 连接成功后按序冲刷——无需拨号等待原语。
struct ProxyTunnel::Conn : public std::enable_shared_from_this<Conn> {
    std::string proxyId;
    std::string connId;
    std::string target;

    std::shared_ptr<asio::io_context> io = std::make_shared<asio::io_context>();
    std::optional<asio::ip::tcp::socket> sock; // 仅 worker 线程创建/使用

    std::atomic<bool> dead{false}; // 已收口；重复收口与迟到投递全部忽略

    // ---- 以下字段仅 worker 线程访问 ----
    bool connected = false;
    bool writing = false;
    std::deque<std::vector<std::uint8_t>> outQueue;
    std::vector<std::uint8_t> writeBuf; // 串行写复用缓冲（单写在途）
    // 拨号超时定时器：teardown 须能 cancel（否则 stop() 要等 timer 到期才
    // join 得了 worker）。在 sock 之前构造/析构无关，随 Conn 生命周期。
    std::optional<asio::steady_timer> deadline;

    void startRead(const std::shared_ptr<ProxyTunnel>& self);
    void queueWrite(const std::shared_ptr<ProxyTunnel>& self, std::vector<std::uint8_t> payload);
    void flushWrites(const std::shared_ptr<ProxyTunnel>& self);
    // teardown 收口：置 dead + 关 socket + 摘表 + 可选回发 proxy.close。幂等。
    void teardown(const std::shared_ptr<ProxyTunnel>& self, const std::string& reason, bool notify);
};

void ProxyTunnel::Conn::teardown(const std::shared_ptr<ProxyTunnel>& self,
    const std::string& reason, bool notify) {
    if (dead.exchange(true)) {
        return;
    }
    asio::error_code ignored;
    if (deadline) {
        deadline->cancel(); // 拨号 timer 在途时立即失效，worker 不再等满 5s
        deadline.reset();
    }
    if (sock) {
        sock->shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        sock->close(ignored);
    }
    outQueue.clear();
    {
        std::lock_guard<std::mutex> lock(self->mu_);
        self->conns_.erase(proxyId + "/" + connId);
    }
    spdlog::info("ProxyTunnel: {} torn down ({})", connId, reason);
    if (notify) {
        self->send_("proxy.close",
            {{"proxyId", proxyId}, {"connId", connId}, {"reason", reason}});
    }
}

void ProxyTunnel::Conn::startRead(const std::shared_ptr<ProxyTunnel>& self) {
    if (dead.load() || !sock) {
        return;
    }
    auto buf = std::make_shared<std::array<std::uint8_t, 32 * 1024>>();
    sock->async_read_some(asio::buffer(*buf),
        [self, conn = shared_from_this(), buf](asio::error_code ec, std::size_t n) {
            if (conn->dead.load()) {
                return; // stop/server 关闭所致：不回发
            }
            if (n > 0) {
                // base64 在 handler 内完成：EncodeToString 立即拷贝，buf 复用安全
                if (!self->send_("proxy.data", {
                        {"proxyId", conn->proxyId},
                        {"connId", conn->connId},
                        {"data", base64Encode(buf->data(), n)},
                    })) {
                    // 远程链路失效：代理流不可缓冲重放，收口等 server 侧清理
                    conn->teardown(self, "agent link lost", false);
                    return;
                }
            }
            if (ec) {
                conn->teardown(self, ec == asio::error::eof ? "target closed"
                                                            : "read error: " + ec.message(),
                    true);
                return;
            }
            conn->startRead(self); // 继续读
        });
}

void ProxyTunnel::Conn::queueWrite(const std::shared_ptr<ProxyTunnel>& self,
    std::vector<std::uint8_t> payload) {
    if (dead.load()) {
        return;
    }
    // 统一入队 + 冲刷：未拨通时 flushWrites 静默返回，connected 后由拨号
    // 成功 handler 冲刷——保序且无「拨号中丢首包」窗口
    outQueue.push_back(std::move(payload));
    flushWrites(self);
}

void ProxyTunnel::Conn::flushWrites(const std::shared_ptr<ProxyTunnel>& self) {
    if (dead.load() || !connected || writing || outQueue.empty() || !sock) {
        return;
    }
    writeBuf = std::move(outQueue.front());
    outQueue.pop_front();
    writing = true;
    asio::async_write(*sock, asio::buffer(writeBuf),
        [self, conn = shared_from_this()](asio::error_code ec, std::size_t) {
            conn->writing = false;
            if (conn->dead.load()) {
                return;
            }
            if (ec) {
                conn->teardown(self, "write error: " + ec.message(), true);
                return;
            }
            conn->flushWrites(self);
        });
}

// ========== 生命周期 ==========

ProxyTunnel::ProxyTunnel(SendFunc send) : send_(std::move(send)) {}

ProxyTunnel::~ProxyTunnel() {
    stop();
}

void ProxyTunnel::stop() {
    std::vector<std::shared_ptr<Conn>> conns;
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!stopped_) {
            stopped_ = true;
            for (auto& [key, conn] : conns_) {
                conns.push_back(conn);
            }
            conns_.clear();
        }
        workers.swap(workers_); // 只允许一个线程收集并 join
    }
    for (auto& conn : conns) {
        // 全部 socket 操作收在 worker 线程：post 收口 handler。若 worker 尚
        // 未进入 run()，该 handler 排在队首，连接建立前即被关闭（后续
        // async_connect 对已关 socket 立即失败，走同一条失败收口）。
        asio::post(*conn->io, [self = shared_from_this(), conn] {
            conn->teardown(self, "shutting down", false);
        });
    }
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    // 未登记 worker 的迟到 handleNew：stopped_ 已置位，会在锁内被拒
}

// ========== server → runtime 指令 ==========

void ProxyTunnel::handleNotify(const std::string& type, const nlohmann::json& msg) {
    if (type == "proxy.new") {
        handleNew(msg);
    } else if (type == "proxy.data") {
        handleData(msg);
    } else if (type == "proxy.close") {
        handleClose(msg);
    } else {
        spdlog::debug("ProxyTunnel: ignore notify type={}", type);
    }
}

void ProxyTunnel::handleNew(const nlohmann::json& msg) {
    std::string proxyId = msg.value("proxyId", "");
    std::string connId = msg.value("connId", "");
    std::string target = msg.value("target", "");
    if (proxyId.empty() || connId.empty() || target.empty()) {
        spdlog::warn("ProxyTunnel: proxy.new missing proxyId/connId/target");
        return;
    }

    auto conn = std::make_shared<Conn>();
    conn->proxyId = proxyId;
    conn->connId = connId;
    conn->target = target;

    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stopped_) {
            // 链路收口后的迟到指令：回 error 让 server 中继立即拆链
            send_("proxy.error", {{"proxyId", proxyId}, {"connId", connId}, {"error", "shutting down"}});
            return;
        }
        auto key = proxyId + "/" + connId;
        if (!conns_.emplace(std::move(key), conn).second) {
            spdlog::warn("ProxyTunnel: duplicate proxy.new for {}/{}", proxyId, connId);
            return;
        }
        workers_.emplace_back([self = shared_from_this(), conn] { self->runConn(conn); });
    }
}

void ProxyTunnel::handleData(const nlohmann::json& msg) {
    std::string proxyId = msg.value("proxyId", "");
    std::string connId = msg.value("connId", "");
    std::string dataB64 = msg.value("data", "");
    if (proxyId.empty() || connId.empty()) {
        return;
    }
    std::shared_ptr<Conn> conn;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = conns_.find(proxyId + "/" + connId);
        if (it == conns_.end()) {
            return; // 与 proxy.close 竞态：连接已拆，静默忽略（同 cockpit 幂等语义）
        }
        conn = it->second;
    }
    auto data = base64Decode(dataB64);
    if (!data) {
        spdlog::warn("ProxyTunnel: proxy.data bad base64 for {}", connId);
        return;
    }
    // 解码在调用线程完成；入队与写入收在 worker 线程（保序、无跨线程 socket）
    asio::post(*conn->io, [self = shared_from_this(), conn, payload = std::move(*data)]() mutable {
        conn->queueWrite(self, std::move(payload));
    });
}

void ProxyTunnel::handleClose(const nlohmann::json& msg) {
    std::string proxyId = msg.value("proxyId", "");
    std::string connId = msg.value("connId", "");
    if (proxyId.empty() || connId.empty()) {
        return;
    }
    std::shared_ptr<Conn> conn;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = conns_.find(proxyId + "/" + connId);
        if (it == conns_.end()) {
            return;
        }
        conn = it->second;
    }
    asio::post(*conn->io, [self = shared_from_this(), conn] {
        conn->teardown(self, "server closed", false);
    });
}

// ========== worker：拨号 + 读泵生命周期 ==========

void ProxyTunnel::runConn(std::shared_ptr<Conn> conn) {
    // DNS 解析为同步调用（注册表上报地址通常为 IP 字面量；失败立即收口）
    asio::error_code resolveEc;
    asio::ip::tcp::resolver resolver(*conn->io);
    asio::ip::tcp::resolver::results_type endpoints;
    if (auto parsed = splitTarget(conn->target)) {
        endpoints = resolver.resolve(parsed->first, parsed->second, resolveEc);
    } else {
        resolveEc = asio::error::invalid_argument;
    }
    if (resolveEc || endpoints.begin() == endpoints.end()) {
        std::string err = resolveEc ? resolveEc.message() : "bad target";
        spdlog::warn("ProxyTunnel: resolve {} for {} failed: {}", conn->target, conn->connId, err);
        send_("proxy.error", {{"proxyId", conn->proxyId}, {"connId", conn->connId}, {"error", err}});
        conn->teardown(shared_from_this(), "dial failed: " + err, false);
        return;
    }

    conn->sock.emplace(*conn->io);
    conn->deadline.emplace(*conn->io);
    auto self = shared_from_this();

    // 拨号：async_connect 与 5s 定时器竞速；两个 handler 都收口到同一条
    // 失败路径（connect 被 timer 关闭后以 operation_aborted 回调）。
    // settled/delay 引用的生命周期 = runConn 栈帧 ⊇ io->run() 执行期。
    bool settled = false;
    asio::steady_timer& deadline = *conn->deadline;
    // 单端点 connect（本 asio 版本 async_connect 完成签名为 void(error_code)；
    // 多地址回退语义未启用——注册表上报地址为单宿主字面量）
    conn->sock->async_connect(*endpoints.begin(),
        [self, conn, &settled](asio::error_code ec) {
            if (conn->dead.load()) {
                return; // 收口先行：失败原因由收口方负责
            }
            settled = true;
            if (ec) {
                std::string err = ec == asio::error::operation_aborted ? "dial timeout" : ec.message();
                spdlog::warn("ProxyTunnel: dial {} -> {} failed: {}",
                    conn->connId, conn->target, err);
                self->send_("proxy.error",
                    {{"proxyId", conn->proxyId}, {"connId", conn->connId}, {"error", err}});
                conn->teardown(self, "dial failed: " + err, false);
                return;
            }
            conn->connected = true;
            spdlog::info("ProxyTunnel: {} connected to {}", conn->connId, conn->target);
            conn->flushWrites(self);   // 冲刷拨号期间积压的 proxy.data
            conn->startRead(self);     // 读泵：target → server
        });
    deadline.expires_after(std::chrono::seconds(5));
    deadline.async_wait([self, conn, &settled](asio::error_code) {
        if (!settled && !conn->dead.load() && conn->sock) {
            asio::error_code ignored;
            conn->sock->close(ignored); // 中止 connect → 其 handler 以 aborted 收口
        }
    });

    // run() 持续到连接生命周期结束（读/写/收口 handler 全部完成）。
    // 一切异常路径都以 teardown 关闭 socket、清空在途操作，run 有界返回。
    conn->io->run();

    // 兜底：run 返回即连接已收口（handler 内 teardown 或 stop 收口）。
    // 这里只保证 socket 资源最终释放（如 stop 收口先于 sock 创建的窗口）。
    asio::error_code ignored;
    if (conn->sock) {
        conn->sock->close(ignored);
    }
}

} // namespace wingman::runtime
