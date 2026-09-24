#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/crypt.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <chrono>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// inbox 下行链补测（2026-09-23 覆盖率第七批）：inbox_module 此前仅有上行
// 生命周期覆盖（transport_inbox_coverage_test），server → client 方向的
// handleMessage/handleInboxMessage 整条消息处理链、consume 出队与 payload
// 五类型转换、pending 满/空 msgId 防御、connect 复用均为零覆盖。
// 本文件用 transport 胶水 tcpListen 起 server，tcpSendTo 向已注册 session
// 推送 JSON 帧驱动 client 的接收线程，端到端触达下行链。
// 仅在非 Windows 平台编译（POSIX socket 选端口），见 CMakeLists。

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

ScriptValue call(const ModuleDescriptor& mod, const std::string& name,
                 std::vector<ScriptValue> args = {}) {
    const auto* fn = findFunction(mod, name);
    EXPECT_NE(fn, nullptr) << "missing function: " << name;
    if (!fn) return ScriptValue::null();
    return (*fn)(std::move(args));
}

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

bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

class InboxDownlinkCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = getModule("transport");
        inbox_ = getModule("inbox");
        ASSERT_FALSE(transport_.name.empty());
        ASSERT_FALSE(inbox_.name.empty());

        port_ = freeTcpPort();
        auto srv = call(transport_, "tcpListen", {ScriptValue::fromString("wg7_srv"),
                                                  ScriptValue::fromString("127.0.0.1"),
                                                  ScriptValue::fromInt(port_)});
        ASSERT_TRUE(asBool(srv, "success")) << "port=" << port_;
        srvHandle_ = asInt(srv, "handle");
    }

    void TearDown() override {
        if (srvHandle_ > 0) {
            call(transport_, "tcpStop", {ScriptValue::fromInt(srvHandle_)});
        }
        call(inbox_, "disconnect"); // 幂等清理默认客户端
    }

    // client 连上 server 后轮询取回 server 侧 session id
    int64_t waitForSession() {
        int64_t sessionId = -1;
        waitUntil([&] {
            const auto sessions = call(transport_, "tcpGetSessions", {ScriptValue::fromInt(srvHandle_)});
            if (sessions.isArray() && !sessions.arrayVal.empty()) {
                sessionId = sessions.arrayVal[0].asInt();
                return true;
            }
            return false;
        });
        return sessionId;
    }

    // server → client 推送一帧 JSON 文本（帧封装由 transport 层完成）
    bool push(const std::string& body) {
        const int64_t sessionId = waitForSession();
        if (sessionId < 0) return false;
        return call(transport_, "tcpSendTo", {ScriptValue::fromInt(srvHandle_),
                                              ScriptValue::fromInt(sessionId),
                                              ScriptValue::fromString(body)}).asBool();
    }

    ScriptValue connectInbox(int heartbeatMs, int maxPending) {
        return call(inbox_, "connect", {ScriptValue::fromString("tcp://127.0.0.1:" + std::to_string(port_)),
                                        ScriptValue::fromObject({
                                            {"agentId", ScriptValue::fromString("wg7-agent")},
                                            {"heartbeatInterval", ScriptValue::fromInt(heartbeatMs)},
                                            {"maxPendingMessages", ScriptValue::fromInt(maxPending)},
                                        })});
    }

    ModuleDescriptor transport_;
    ModuleDescriptor inbox_;
    int port_ = 0;
    int srvHandle_ = -1;
};

// ========== 下行链全驱动：handleMessage 三类型 + 坏 JSON + 入队/防御 + consume 五类型转换 ==========

TEST_F(InboxDownlinkCoverageTest, DownlinkChainAndConsumePayloadTypes) {
    auto conn = connectInbox(40, 1); // 40ms 心跳驱动 heartbeatLoop；maxPending=1 驱动上限拒绝
    ASSERT_TRUE(asBool(conn, "success")) << "port=" << port_;
    const int handle = asInt(conn, "handle");
    ASSERT_GT(handle, 0);
    EXPECT_TRUE(call(inbox_, "isConnected", {ScriptValue::fromInt(handle)}).asBool());
    ASSERT_TRUE(waitUntil([&] { return waitForSession() >= 0; })) << "server never saw the session";

    // 下行控制帧三类型 + 坏 JSON（handleMessage 分发/异常分支）
    EXPECT_TRUE(push(R"({"type":"inbox.register_ack"})"));
    EXPECT_TRUE(push(R"({"type":"inbox.heartbeat_ack"})"));
    EXPECT_TRUE(push(R"({"type":"inbox.some_unknown"})"));
    EXPECT_TRUE(push("not-json{{{"));

    // inbox.message + object payload → 入队（InboxMessage 四参构造 + push + 空回调跳过）
    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m1","messageType":"task",)"
                     R"("payload":{"k":"v"},"timestamp":1234})"));

    // consume 取出：object payload 转换（kv 值为 dump 字符串）
    ScriptValue got;
    ASSERT_TRUE(waitUntil([&] {
        got = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !got.isNull();
    })) << "m1 never arrived";
    EXPECT_EQ(got.get("msgId")->asString(), "m1");
    EXPECT_EQ(got.get("type")->asString(), "task");
    EXPECT_EQ(got.get("payload")->get("k")->asString(), "\"v\"");
    EXPECT_EQ(got.get("timestamp")->asInt(), 1234);

    // ack 命中 pending（标记 acked，best-effort 上行）
    EXPECT_TRUE(call(inbox_, "ack", {ScriptValue::fromInt(handle),
                                     ScriptValue::fromString("m1")}).asBool());

    // pending 已满（m1 consume 后仍在 pending，maxPending=1）→ 第二条被拒不入队。
    // push 只保证写入 server→client 链路，client IO 线程何时处理是异步的：
    // 先等 300ms 让该帧处理定格（loopback 实测往返 <5ms，窗口两个数量级冗余），
    // 再断言拒绝，最后才 report 释放 pending——避免与 report 竞态。
    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m2","payload":"dropped"})"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(call(inbox_, "consume", {ScriptValue::fromInt(handle),
                                         ScriptValue::fromInt(50)}).isNull())
        << "m2 should have been dropped (pending full), not queued";

    // report 移除 pending → 队列恢复接收
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("m1"),
                                        ScriptValue::fromString(R"({"ok":true})")}).asBool());

    // 依次推 string/number/boolean/array payload 各一条，按序 consume 验证五类转换
    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m3","payload":"hello"})"));
    ScriptValue m3;
    ASSERT_TRUE(waitUntil([&] {
        m3 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !m3.isNull();
    })) << "m3 never arrived";
    EXPECT_EQ(m3.get("msgId")->asString(), "m3");
    EXPECT_EQ(m3.get("payload")->asString(), "hello"); // string 原样
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("m3")}).asBool());

    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m4","payload":42})"));
    ScriptValue m4;
    ASSERT_TRUE(waitUntil([&] {
        m4 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !m4.isNull();
    })) << "m4 never arrived";
    EXPECT_EQ(m4.get("payload")->asInt(), 42);
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("m4")}).asBool());

    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m5","payload":true})"));
    ScriptValue m5;
    ASSERT_TRUE(waitUntil([&] {
        m5 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !m5.isNull();
    })) << "m5 never arrived";
    EXPECT_EQ(m5.get("payload")->asBool(), true);
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("m5")}).asBool());

    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"m6","payload":[1,2]})"));
    ScriptValue m6;
    ASSERT_TRUE(waitUntil([&] {
        m6 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !m6.isNull();
    })) << "m6 never arrived";
    ASSERT_TRUE(m6.get("payload")->isArray());
    EXPECT_EQ(m6.get("payload")->arrayVal.size(), 2u);
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("m6")}).asBool());

    // 缺 msgId 的 inbox.message → 防御丢弃，不入队
    ASSERT_TRUE(push(R"({"type":"inbox.message","payload":"no-id"})"));
    EXPECT_TRUE(call(inbox_, "consume", {ScriptValue::fromInt(handle),
                                         ScriptValue::fromInt(80)}).isNull());

    EXPECT_TRUE(call(inbox_, "disconnect", {ScriptValue::fromInt(handle)}).asBool());
    EXPECT_FALSE(call(inbox_, "isConnected", {ScriptValue::fromInt(handle)}).asBool());
}

// ========== connect 二次调用：复用同 client（clientId 遍历命中 + "Already connected"） ==========

TEST_F(InboxDownlinkCoverageTest, ConnectTwiceReusesClientAndHandle) {
    auto first = connectInbox(30000, 100);
    ASSERT_TRUE(asBool(first, "success"));
    const int handle = asInt(first, "handle");

    // 同 id 二次 createClient：clientId 遍历命中返回同 handle；InboxClient::connect
    // 对已运行 client 短路返回 true → 胶水 success 且 handle 不变
    auto second = connectInbox(30000, 100);
    ASSERT_TRUE(asBool(second, "success"));
    EXPECT_EQ(asInt(second, "handle"), handle);

    call(inbox_, "disconnect", {ScriptValue::fromInt(handle)});
}

// ========== payload null / float 转换（第十一批补测：五类型转换仅剩的两型） ==========

TEST_F(InboxDownlinkCoverageTest, ConsumeNullAndFloatPayloadVariants) {
    auto conn = connectInbox(30000, 100);
    ASSERT_TRUE(asBool(conn, "success"));
    const int handle = asInt(conn, "handle");
    ASSERT_GT(handle, 0);
    ASSERT_TRUE(waitUntil([&] { return waitForSession() >= 0; })) << "server never saw the session";

    // payload:null → ScriptValue::null()（479）；消息对象仍带 payload 键（507）
    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"n1","payload":null,"timestamp":7})"));
    ScriptValue n1;
    ASSERT_TRUE(waitUntil([&] {
        n1 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !n1.isNull();  // 判据是消息对象非空，payload 本身应为 null
    })) << "n1 never arrived";
    EXPECT_EQ(n1.get("msgId")->asString(), "n1");
    ASSERT_NE(n1.get("payload"), nullptr);
    EXPECT_TRUE(n1.get("payload")->isNull());
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("n1")}).asBool());

    // payload:1.5 → fromFloat（487）
    ASSERT_TRUE(push(R"({"type":"inbox.message","msgId":"f1","payload":1.5})"));
    ScriptValue f1;
    ASSERT_TRUE(waitUntil([&] {
        f1 = call(inbox_, "consume", {ScriptValue::fromInt(handle), ScriptValue::fromInt(50)});
        return !f1.isNull();
    })) << "f1 never arrived";
    EXPECT_DOUBLE_EQ(f1.get("payload")->asFloat(), 1.5);
    EXPECT_TRUE(call(inbox_, "report", {ScriptValue::fromInt(handle),
                                        ScriptValue::fromString("f1")}).asBool());

    call(inbox_, "disconnect", {ScriptValue::fromInt(handle)});
}

// ========== crypt.deriveKey：iter=0 → PBKDF2 拒绝 → KDF 失败分支 ==========

TEST(CryptDeriveKeyFailureTest, ZeroIterationsFailsDerivation) {
    // OpenSSL PBKDF2 要求 iter >= 1：iter=0 使 EVP_KDF_derive 返回 ≤0，
    // 触达错误清理分支（ctx/kdf 释放 + 错误日志 + 空串）
    EXPECT_EQ(wingman::crypt::deriveKey("password", "aabbccdd", 0, 32), "");
    // 对照：合法 iter 正常派生（64 hex 字符 = 32 字节）
    const std::string key = wingman::crypt::deriveKey("password", "aabbccdd", 1000, 32);
    EXPECT_EQ(key.size(), 64u);
}

} // namespace
