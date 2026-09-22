#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// transport_module / inbox_module 胶水补测（2026-09-22 覆盖率收口）。
// 两模块此前仅 Windows stub 分支可达（胶水主体在 WINGMAN_HAS_TRANSPORT 下），
// Linux 上从未被触达。本文件在 127.0.0.1 上自建真实 TCP listener / UDP 对端，
// 经 ModuleDescriptor 直调胶水函数完成端到端触达。
// 仅在非 Windows 平台编译（POSIX socket 选端口 + tcp:// 语义），见 CMakeLists。

namespace {

ModuleDescriptor getModule(const std::string& name) {
    for (auto& mod : getAllModules()) {
        if (mod.name == name) return mod;
    }
    return {};
}

const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

// 向内核要一个临时空闲 TCP 端口（bind :0 后立即释放；竞态窗口在本机测试中可忽略）
int freeTcpPort() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(fd, 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    socklen_t len = sizeof(addr);
    EXPECT_EQ(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len), 0);
    int port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

int asInt(const ScriptValue& obj, const char* key) {
    const ScriptValue* v = obj.get(key);
    return v ? static_cast<int>(v->asInt()) : -1;
}

bool asBool(const ScriptValue& obj, const char* key) {
    const ScriptValue* v = obj.get(key);
    return v ? v->asBool() : false;
}

class TransportInboxCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = getModule("transport");
        inbox_ = getModule("inbox");
        ASSERT_FALSE(transport_.name.empty());
        ASSERT_FALSE(inbox_.name.empty());
    }

    // 起一个 localhost TCP listener，返回 {success,handle}
    ScriptValue listen(const std::string& id, int port) {
        const auto* fn = findFunction(transport_, "tcpListen");
        return (*fn)({ScriptValue::fromString(id), ScriptValue::fromString("127.0.0.1"),
                      ScriptValue::fromInt(port)});
    }

    ModuleDescriptor transport_;
    ModuleDescriptor inbox_;
};

// ========== TCP 客户端/服务端端到端 ==========

TEST_F(TransportInboxCoverageTest, TcpSelfConnectSendAndSessionManagement) {
    const int port = freeTcpPort();
    auto srv = listen("cov_tcp_srv", port);
    ASSERT_TRUE(asBool(srv, "success")) << "port=" << port;
    int srvHandle = asInt(srv, "handle");
    ASSERT_GT(srvHandle, 0);

    const auto* connectFn = findFunction(transport_, "tcpConnect");
    auto cli = (*connectFn)({ScriptValue::fromString("cov_cli"),
                             ScriptValue::fromString("127.0.0.1"), ScriptValue::fromInt(port)});
    ASSERT_TRUE(asBool(cli, "success"));
    int cliHandle = asInt(cli, "handle");

    const auto* isConnectedFn = findFunction(transport_, "tcpIsConnected");
    EXPECT_TRUE((*isConnectedFn)({ScriptValue::fromInt(cliHandle)}).asBool());

    const auto* sendFn = findFunction(transport_, "tcpSend");
    EXPECT_TRUE((*sendFn)({ScriptValue::fromInt(cliHandle), ScriptValue::fromString("hello-cov")}).asBool());

    // 等 server 侧 accept 并注册 session
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto* sessionsFn = findFunction(transport_, "tcpGetSessions");
    auto sessions = (*sessionsFn)({ScriptValue::fromInt(srvHandle)});
    // server 侧异步 accept + 注册 session：轮询等待（至多 ~3s）
    for (int i = 0; i < 30 && sessions.size() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sessions = (*sessionsFn)({ScriptValue::fromInt(srvHandle)});
    }
    ASSERT_TRUE(sessions.isArray());
    ASSERT_GE(sessions.size(), 1u);

    const auto* broadcastFn = findFunction(transport_, "tcpBroadcast");
    EXPECT_TRUE((*broadcastFn)({ScriptValue::fromInt(srvHandle), ScriptValue::fromString("hi-all")}).asBool());

    const auto* sendToFn = findFunction(transport_, "tcpSendTo");
    int sessionId = static_cast<int>(sessions.at(0).asInt());
    EXPECT_TRUE((*sendToFn)({ScriptValue::fromInt(srvHandle),
                             ScriptValue::fromInt(sessionId), ScriptValue::fromString("hi-you")}).asBool());

    const auto* closeSessionFn = findFunction(transport_, "tcpCloseSession");
    EXPECT_TRUE((*closeSessionFn)({ScriptValue::fromInt(srvHandle), ScriptValue::fromInt(sessionId)}).asBool());

    const auto* disconnectFn = findFunction(transport_, "tcpDisconnect");
    EXPECT_TRUE((*disconnectFn)({ScriptValue::fromInt(cliHandle)}).asBool());

    const auto* stopFn = findFunction(transport_, "tcpStop");
    EXPECT_TRUE((*stopFn)({ScriptValue::fromInt(srvHandle)}).asBool());
    // 断开后 isConnected 走 client 缺失分支
    EXPECT_FALSE((*isConnectedFn)({ScriptValue::fromInt(cliHandle)}).asBool());
}

TEST_F(TransportInboxCoverageTest, TcpConnectRefusedReturnsErrorObject) {
    const auto* connectFn = findFunction(transport_, "tcpConnect");
    // 端口 1（tcpmux）几乎必然无监听 → connect 失败 + removeClient 清理分支
    auto result = (*connectFn)({ScriptValue::fromString("cov_refused"),
                                ScriptValue::fromString("127.0.0.1"), ScriptValue::fromInt(1)});
    EXPECT_FALSE(asBool(result, "success"));
    EXPECT_FALSE(result.get("error")->asString().empty());
}

TEST_F(TransportInboxCoverageTest, TcpListenPortConflictFails) {
    const int port = freeTcpPort();
    auto first = listen("cov_conflict_a", port);
    ASSERT_TRUE(asBool(first, "success"));

    // 同端口二次监听 → listen 失败 + stop/removeServer 清理分支
    auto second = listen("cov_conflict_b", port);
    EXPECT_FALSE(asBool(second, "success"));
    EXPECT_EQ(second.get("error")->asString(), "Failed to listen on port");

    const auto* stopFn = findFunction(transport_, "tcpStop");
    (*stopFn)({ScriptValue::fromInt(asInt(first, "handle"))});
}

TEST_F(TransportInboxCoverageTest, TcpInvalidHandleBranches) {
    const ScriptValue bogus = ScriptValue::fromInt(987654);
    const auto* sendFn = findFunction(transport_, "tcpSend");
    const auto* isConnectedFn = findFunction(transport_, "tcpIsConnected");
    const auto* sendToFn = findFunction(transport_, "tcpSendTo");
    const auto* broadcastFn = findFunction(transport_, "tcpBroadcast");
    const auto* closeSessionFn = findFunction(transport_, "tcpCloseSession");
    const auto* sessionsFn = findFunction(transport_, "tcpGetSessions");
    ASSERT_NE(sendFn, nullptr);
    ASSERT_NE(isConnectedFn, nullptr);
    ASSERT_NE(sendToFn, nullptr);
    ASSERT_NE(broadcastFn, nullptr);
    ASSERT_NE(closeSessionFn, nullptr);
    ASSERT_NE(sessionsFn, nullptr);

    EXPECT_FALSE((*sendFn)({bogus, ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*isConnectedFn)({bogus}).asBool());
    EXPECT_FALSE((*sendToFn)({bogus, ScriptValue::fromInt(1), ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*broadcastFn)({bogus, ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*closeSessionFn)({bogus, ScriptValue::fromInt(1)}).asBool());
    EXPECT_EQ((*sessionsFn)({bogus}).size(), 0u);
    // tcpStop 对无效 handle 也返回 true（幂等 remove 语义）
    const auto* stopFn = findFunction(transport_, "tcpStop");
    EXPECT_TRUE((*stopFn)({bogus}).asBool());
}

// ========== UDP 往返与错误分支 ==========

TEST_F(TransportInboxCoverageTest, UdpBindSendRecvRoundTrip) {
    const auto* socketFn = findFunction(transport_, "udpSocket");
    const auto* bindFn = findFunction(transport_, "udpBind");
    const auto* sendToFn = findFunction(transport_, "udpSendTo");
    const auto* recvFn = findFunction(transport_, "udpRecvFrom");
    const auto* closeFn = findFunction(transport_, "udpClose");
    ASSERT_NE(socketFn, nullptr);
    ASSERT_NE(bindFn, nullptr);
    ASSERT_NE(sendToFn, nullptr);
    ASSERT_NE(recvFn, nullptr);
    ASSERT_NE(closeFn, nullptr);

    int portA = freeTcpPort(); // UDP 端口与 TCP 独立，这里只要一个未被占用的端口号
    int portB = freeTcpPort();

    auto sockA = (*socketFn)({ScriptValue::fromString("cov_udp_a")});
    auto sockB = (*socketFn)({ScriptValue::fromString("cov_udp_b")});
    // udpSocket 返回 {success, handle, id}
    ASSERT_GT(asInt(sockA, "handle"), 0);
    ASSERT_GT(asInt(sockB, "handle"), 0);
    int handleA = asInt(sockA, "handle");
    int handleB = asInt(sockB, "handle");

    ASSERT_TRUE(asBool((*bindFn)({ScriptValue::fromInt(handleA), ScriptValue::fromString("127.0.0.1"),
                                  ScriptValue::fromInt(portA)}), "success"));
    ASSERT_TRUE(asBool((*bindFn)({ScriptValue::fromInt(handleB), ScriptValue::fromString("127.0.0.1"),
                                  ScriptValue::fromInt(portB)}), "success"));

    // B → A 发送（内核缓冲），A 随后 recvFrom 立即返回（receive_from 无超时，
    // 必须先发后收，否则阻塞）。udpSendTo 返回裸 bool
    EXPECT_TRUE((*sendToFn)({ScriptValue::fromInt(handleB), ScriptValue::fromString("127.0.0.1"),
                             ScriptValue::fromInt(portA), ScriptValue::fromString("udp-payload")}).asBool());
    auto received = (*recvFn)({ScriptValue::fromInt(handleA), ScriptValue::fromInt(500)});
    // 返回 {success, data}
    EXPECT_TRUE(asBool(received, "success"));
    EXPECT_EQ(received.get("data")->asString(), "udp-payload");

    EXPECT_TRUE((*closeFn)({ScriptValue::fromInt(handleA)}).asBool());
    EXPECT_TRUE((*closeFn)({ScriptValue::fromInt(handleB)}).asBool());
}

TEST_F(TransportInboxCoverageTest, UdpErrorBranches) {
    const auto* socketFn = findFunction(transport_, "udpSocket");
    const auto* bindFn = findFunction(transport_, "udpBind");
    const auto* sendToFn = findFunction(transport_, "udpSendTo");
    const auto* recvFn = findFunction(transport_, "udpRecvFrom");
    ASSERT_NE(socketFn, nullptr);
    ASSERT_NE(bindFn, nullptr);
    ASSERT_NE(sendToFn, nullptr);
    ASSERT_NE(recvFn, nullptr);

    // 未 bind 的 socket recvFrom → "Socket not bound" 分支（返回 {success:false, data:""}）
    auto raw = (*socketFn)({ScriptValue::fromString("cov_udp_raw")});
    int rawHandle = asInt(raw, "handle");
    auto unbound = (*recvFn)({ScriptValue::fromInt(rawHandle), ScriptValue::fromInt(10)});
    EXPECT_FALSE(asBool(unbound, "success"));
    EXPECT_EQ(unbound.get("data")->asString(), "");

    // 非法地址 bind 失败
    EXPECT_FALSE(asBool((*bindFn)({ScriptValue::fromInt(rawHandle), ScriptValue::fromString("not-an-ip"),
                                   ScriptValue::fromInt(9999)}), "success"));

    // 无效句柄分支
    const ScriptValue bogus = ScriptValue::fromInt(765432);
    EXPECT_FALSE(asBool((*bindFn)({bogus, ScriptValue::fromString("127.0.0.1"), ScriptValue::fromInt(1)}), "success"));
    EXPECT_FALSE((*sendToFn)({bogus, ScriptValue::fromString("127.0.0.1"), ScriptValue::fromInt(1),
                              ScriptValue::fromString("x")}).asBool());
    // 无效 handle 的 recvFrom → Invalid socket handle 分支
    EXPECT_FALSE(asBool((*recvFn)({bogus}), "success"));

    // 无参默认 id 分支
    auto defaulted = (*socketFn)({});
    EXPECT_TRUE(asBool(defaulted, "success"));
}

// ========== inbox 客户端（对真实 TCP listener 的生命周期） ==========

TEST_F(TransportInboxCoverageTest, InboxLifecycleAgainstTcpListener) {
    const int port = freeTcpPort();
    auto srv = listen("cov_inbox_srv", port);
    ASSERT_TRUE(asBool(srv, "success"));

    const auto* connectFn = findFunction(inbox_, "connect");
    const auto* isConnectedFn = findFunction(inbox_, "isConnected");
    const auto* heartbeatFn = findFunction(inbox_, "heartbeat");
    const auto* consumeFn = findFunction(inbox_, "consume");
    const auto* ackFn = findFunction(inbox_, "ack");
    const auto* reportFn = findFunction(inbox_, "report");
    const auto* disconnectFn = findFunction(inbox_, "disconnect");
    ASSERT_NE(connectFn, nullptr);
    ASSERT_NE(isConnectedFn, nullptr);
    ASSERT_NE(heartbeatFn, nullptr);
    ASSERT_NE(consumeFn, nullptr);
    ASSERT_NE(ackFn, nullptr);
    ASSERT_NE(reportFn, nullptr);
    ASSERT_NE(disconnectFn, nullptr);

    auto conn = (*connectFn)({ScriptValue::fromString("tcp://127.0.0.1:" + std::to_string(port)),
                              ScriptValue::fromObject({
                                  {"agentId", ScriptValue::fromString("cov-agent")},
                                  {"heartbeatInterval", ScriptValue::fromInt(30000)},
                              })});
    ASSERT_TRUE(asBool(conn, "success")) << "port=" << port;
    int handle = asInt(conn, "handle");
    ASSERT_GT(handle, 0);

    EXPECT_TRUE((*isConnectedFn)({ScriptValue::fromInt(handle)}).asBool());
    EXPECT_TRUE((*heartbeatFn)({ScriptValue::fromInt(handle)}).asBool());

    // 无消息 → consume 短超时后返回 null
    EXPECT_TRUE((*consumeFn)({ScriptValue::fromInt(handle), ScriptValue::fromInt(150)}).isNull());

    // ack/report 对未知 msgId 仍 best-effort 发送（pendingMessages_ 查找
    // 不影响返回值；本地 listener 可收 → 发送成功返回 true）
    EXPECT_TRUE((*ackFn)({ScriptValue::fromInt(handle), ScriptValue::fromString("no-such-msg")}).asBool());
    EXPECT_TRUE((*reportFn)({ScriptValue::fromInt(handle), ScriptValue::fromString("no-such-msg"),
                             ScriptValue::fromString("{}")}).asBool());

    EXPECT_TRUE((*disconnectFn)({ScriptValue::fromInt(handle)}).asBool());
    EXPECT_FALSE((*isConnectedFn)({ScriptValue::fromInt(handle)}).asBool());

    const auto* stopFn = findFunction(transport_, "tcpStop");
    (*stopFn)({ScriptValue::fromInt(asInt(srv, "handle"))});
}

TEST_F(TransportInboxCoverageTest, InboxErrorBranches) {
    const auto* connectFn = findFunction(inbox_, "connect");
    const auto* consumeFn = findFunction(inbox_, "consume");
    const auto* heartbeatFn = findFunction(inbox_, "heartbeat");
    const auto* isConnectedFn = findFunction(inbox_, "isConnected");
    const auto* ackFn = findFunction(inbox_, "ack");
    const auto* reportFn = findFunction(inbox_, "report");
    ASSERT_NE(connectFn, nullptr);

    // 缺 URL / 非 tcp 协议 / 连接拒绝
    EXPECT_FALSE(asBool((*connectFn)({}), "success"));
    EXPECT_EQ((*connectFn)({}).get("error")->asString(), "URL required");
    EXPECT_FALSE(asBool((*connectFn)({ScriptValue::fromString("http://127.0.0.1:8080")}), "success"));
    EXPECT_FALSE(asBool((*connectFn)({ScriptValue::fromString("tcp://127.0.0.1:1")}), "success"));

    // 无效/未连接句柄分支
    const ScriptValue bogus = ScriptValue::fromInt(654321);
    EXPECT_FALSE(asBool((*consumeFn)({bogus, ScriptValue::fromInt(10)}), "success"));
    EXPECT_FALSE((*heartbeatFn)({bogus}).asBool());
    EXPECT_FALSE((*isConnectedFn)({bogus}).asBool());
    EXPECT_FALSE((*ackFn)({bogus, ScriptValue::fromString("m")}).asBool());
    EXPECT_FALSE((*reportFn)({bogus, ScriptValue::fromString("m")}).asBool());
    // 缺参分支
    EXPECT_FALSE((*ackFn)({}).asBool());
    EXPECT_FALSE((*reportFn)({}).asBool());
    // disconnect 幂等
    const auto* disconnectFn = findFunction(inbox_, "disconnect");
    EXPECT_TRUE((*disconnectFn)({bogus}).asBool());
}

} // anonymous namespace
