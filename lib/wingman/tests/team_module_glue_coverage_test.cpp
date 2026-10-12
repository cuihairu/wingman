#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// team_module.cpp 胶水层补测（2026-09-22 覆盖率第二批）：现有
// script_function_test.cpp 的 TeamModuleFunctionsTest 为浅触达，本文件补齐
// 参数分支（缺参/类型回退/已加入拒绝）与 joined 状态下的完整路径。
// TeamManager 是进程级单例且胶水固定用 handle 1，现有测试已触碰过该句柄，
// 因此每个用例自带 ensureJoined/ensureLeft 准备状态，不依赖用例执行顺序。
// getVoteResult 命中 votes_ 的分支曾因 handleServerMessage 无胶水入口而
// 不可达；team.inbound 桥（2026-10-13）接通后由 InboundBridgeDrives* 用例触达。

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

class TeamGlueCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        mod_ = getModule("team");
        ASSERT_FALSE(mod_.name.empty());
    }

    bool isJoined() {
        const auto* fn = findFunction(mod_, "isJoined");
        return (*fn)({}).asBool();
    }

    // 无论此前处于什么状态，保证最终为已加入（自定义 memberId 便于断言）
    void ensureJoined() {
        const auto* joinFn = findFunction(mod_, "joinTeam");
        if (!isJoined()) {
            ASSERT_TRUE((*joinFn)({ScriptValue::fromString("cov-team"),
                                   ScriptValue::fromString("cov-member")}).asBool());
        }
    }

    void ensureLeft() {
        const auto* leaveFn = findFunction(mod_, "leaveTeam");
        if (isJoined()) {
            ASSERT_TRUE((*leaveFn)({}).asBool());
        }
    }

    ModuleDescriptor mod_;
};

// ========== joinTeam 分支 ==========

TEST_F(TeamGlueCoverageTest, JoinWithoutArgsRejected) {
    ensureLeft();
    const auto* joinFn = findFunction(mod_, "joinTeam");
    EXPECT_FALSE((*joinFn)({}).asBool());
}

TEST_F(TeamGlueCoverageTest, JoinWhileJoinedRejectedAndLeaveTwiceRejected) {
    ensureLeft();
    const auto* joinFn = findFunction(mod_, "joinTeam");
    const auto* leaveFn = findFunction(mod_, "leaveTeam");
    ASSERT_TRUE((*joinFn)({ScriptValue::fromString("cov-team"),
                           ScriptValue::fromString("cov-member")}).asBool());
    // 已加入时再次 join 被拒绝
    EXPECT_FALSE((*joinFn)({ScriptValue::fromString("other-team"),
                            ScriptValue::fromString("other-member")}).asBool());
    // memberId 生效
    const auto* memberIdFn = findFunction(mod_, "getMemberId");
    EXPECT_EQ((*memberIdFn)({}).asString(), "cov-member");
    ASSERT_TRUE((*leaveFn)({}).asBool());
    // 未加入时 leave 拒绝
    EXPECT_FALSE((*leaveFn)({}).asBool());
}

// ========== createVote / castVote ==========

TEST_F(TeamGlueCoverageTest, VotePathsWithJoinStateAndArgs) {
    const auto* voteFn = findFunction(mod_, "createVote");
    const auto* castFn = findFunction(mod_, "castVote");

    // 未加入：投票创建/投票均拒绝
    ensureLeft();
    EXPECT_FALSE((*voteFn)({ScriptValue::fromString("topic-a")}).asBool());
    EXPECT_FALSE((*castFn)({ScriptValue::fromString("v1"), ScriptValue::fromString("yes")}).asBool());

    // 缺参分支（优先于连接状态判断）
    EXPECT_FALSE((*voteFn)({}).asBool());
    EXPECT_FALSE((*castFn)({ScriptValue::fromString("v1")}).asBool());

    // 已加入：默认超时与显式超时都通过；投票发送成功
    ensureJoined();
    EXPECT_TRUE((*voteFn)({ScriptValue::fromString("topic-b")}).asBool());
    EXPECT_TRUE((*voteFn)({ScriptValue::fromString("topic-c"), ScriptValue::fromInt(5000)}).asBool());
    EXPECT_TRUE((*castFn)({ScriptValue::fromString("vote-x"), ScriptValue::fromString("yes")}).asBool());
}

// ========== getVoteResult：缺参 null + 未找到投票 error JSON ==========
// （votes_ 命中分支胶水不可达，见文件头注释）

TEST_F(TeamGlueCoverageTest, GetVoteResultMissingArgsAndUnknownVote) {
    const auto* fn = findFunction(mod_, "getVoteResult");
    EXPECT_TRUE((*fn)({}).isNull());

    // ctest 独立进程（gtest_discover_tests）下 TeamManager 是干净单例，
    // 胶水 getClient(1) 判空提前返回 null，触达不了 votes_ 查找——按文件头
    // 约定自带准备状态，不依赖其他用例先建过 client。
    ensureJoined();
    std::string result = (*fn)({ScriptValue::fromString("no-such-vote")}).asString();
    EXPECT_NE(result.find("Vote not found"), std::string::npos);
    ensureLeft();
}

// ========== reportStatus：JSON 解析三分支 + joined 守卫 ==========

TEST_F(TeamGlueCoverageTest, ReportStatusBranches) {
    const auto* fn = findFunction(mod_, "reportStatus");

    // 缺参 / 未加入拒绝
    EXPECT_FALSE((*fn)({}).asBool());
    ensureLeft();
    EXPECT_FALSE((*fn)({ScriptValue::fromString("{\"cpu\":1}")}).asBool());

    ensureJoined();
    // 合法 JSON 字符串原样携带
    EXPECT_TRUE((*fn)({ScriptValue::fromString("{\"cpu\":1}")}).asBool());
    // 非法 JSON 字符串回退为 {"value": ...} 而非失败
    EXPECT_TRUE((*fn)({ScriptValue::fromString("not-json")}).asBool());
    // Object 分支简化为 {"reported": true}
    EXPECT_TRUE((*fn)({ScriptValue::fromObject({
        {"k", ScriptValue::fromString("v")}})}).asBool());
}

// ========== broadcast：JSON 解析三分支 + joined 守卫 ==========

TEST_F(TeamGlueCoverageTest, BroadcastBranches) {
    const auto* fn = findFunction(mod_, "broadcast");

    EXPECT_FALSE((*fn)({}).asBool());
    ensureLeft();
    EXPECT_FALSE((*fn)({ScriptValue::fromString("\"hi\"")}).asBool());

    ensureJoined();
    EXPECT_TRUE((*fn)({ScriptValue::fromString("\"hi\"")}).asBool());
    EXPECT_TRUE((*fn)({ScriptValue::fromString("plain text")}).asBool()); // 回退 {"text": ...}
    EXPECT_TRUE((*fn)({ScriptValue::fromObject({
        {"data", ScriptValue::fromInt(1)}})}).asBool());
}

// ========== getTeamStatus：joined 后字段完整 ==========

TEST_F(TeamGlueCoverageTest, TeamStatusFieldsWhenJoined) {
    ensureJoined();
    const auto* fn = findFunction(mod_, "getTeamStatus");
    std::string status = (*fn)({}).asString();
    // joinTeam 乐观更新：teamId 与成员列表本地立即可见
    EXPECT_NE(status.find("\"teamId\":\"cov-team\""), std::string::npos);
    EXPECT_NE(status.find("cov-member"), std::string::npos);
    EXPECT_NE(status.find("\"leaderId\""), std::string::npos);
    EXPECT_NE(status.find("\"state\""), std::string::npos);
}

// ========== on：事件订阅分支 ==========

TEST_F(TeamGlueCoverageTest, SubscribeBranches) {
    const auto* fn = findFunction(mod_, "on");

    // 缺参 / 回调不可调用
    EXPECT_FALSE((*fn)({}).asBool());
    EXPECT_FALSE((*fn)({ScriptValue::fromString("vote_started")}).asBool());
    EXPECT_FALSE((*fn)({ScriptValue::fromString("vote_started"),
                        ScriptValue::fromString("not-callable")}).asBool());

    // 合法订阅：回调异常被吞掉（不向事件线程传播）
    auto boom = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        throw std::runtime_error("cov-team-cb");
    });
    EXPECT_TRUE((*fn)({ScriptValue::fromString("vote_started"), boom}).asBool());

    // 订阅生效：emit 后回调被调（异常路径一并触达）
    int called = 0;
    auto counter = ScriptValue::fromCallable([&called](const std::vector<ScriptValue>& args) -> ScriptValue {
        ++called;
        EXPECT_FALSE(args.empty());
        EXPECT_FALSE(args[0].asString().empty());
        return ScriptValue::fromBool(true);
    });
    ASSERT_TRUE((*fn)({ScriptValue::fromString("vote_started"), counter}).asBool());
    EventHub::instance().emit("team.vote_started", {{"voteId", "v-cov"}});
    EXPECT_EQ(called, 1);
}

// ========== team.inbound 桥下行：server 驱动的状态机与事件 ==========
// （apps/agent 把 agent 通道下行的 inbox.message 转成 team.inbound 事件，
//  team_module 按名去重挂接 handleServerMessage —— 测试直接 emit 模拟）

TEST_F(TeamGlueCoverageTest, InboundBridgeDrivesJoinedStateAndMemberEvents) {
    // 先经胶水建出默认客户端（handle 1），桥下行分派才有着落
    ensureJoined();

    // team.joined 载荷驱动本地状态（区别于 joinTeam 的乐观更新路径）
    EventHub::instance().emit("team.inbound", {
        {"type", "team.joined"},
        {"teamId", "bridge-team"},
        {"leaderId", "admin"},
        {"members", nlohmann::json::array({"bridge-member"})},
    });
    EXPECT_TRUE(isJoined());
    const auto* statusFn = findFunction(mod_, "getTeamStatus");
    const std::string status = (*statusFn)({}).asString();
    EXPECT_NE(status.find("\"teamId\":\"bridge-team\""), std::string::npos);
    EXPECT_NE(status.find("\"leaderId\":\"admin\""), std::string::npos);
    EXPECT_NE(status.find("bridge-member"), std::string::npos);

    // member_joined 事件经 on() 订阅触达（载荷 JSON 字符串透传给回调）
    const auto* onFn = findFunction(mod_, "on");
    std::string seenPayload;
    auto memberCb = ScriptValue::fromCallable([&seenPayload](const std::vector<ScriptValue>& args) -> ScriptValue {
        seenPayload = args.at(0).asString();
        return ScriptValue::fromBool(true);
    });
    ASSERT_TRUE((*onFn)({ScriptValue::fromString("member_joined"), memberCb}).asBool());
    EventHub::instance().emit("team.inbound", {
        {"type", "team.member_joined"},
        {"teamId", "bridge-team"},
        {"memberId", "late-member"},
    });
    EXPECT_NE(seenPayload.find("late-member"), std::string::npos);

    // 成员表同步追加
    const std::string after = (*statusFn)({}).asString();
    EXPECT_NE(after.find("late-member"), std::string::npos);

    // 收尾保持「已加入」终态：A11 空态用例按 teamId 非空 skip，若留下
    // 「client 存在但未加入」的中间态，其空态断言（state=idle）无从触达也不触发 skip
    ensureJoined();
}

TEST_F(TeamGlueCoverageTest, InboundBridgeDrivesVoteLifecycle) {
    ensureJoined();
    const auto* voteResultFn = findFunction(mod_, "getVoteResult");

    // vote_started 入本地投票表（votes_ 历史上仅 handleServerMessage 可填、
    // 胶水无入口而被判不可达 —— team.inbound 桥接通后该分支可达）
    EventHub::instance().emit("team.inbound", {
        {"type", "team.vote_started"},
        {"voteId", "v-bridge"},
        {"teamId", "cov-team"},
        {"proposerId", "admin"},
        {"subject", "switch map"},
        {"deadline", 1715299200000},
    });
    const std::string started = (*voteResultFn)({ScriptValue::fromString("v-bridge")}).asString();
    EXPECT_NE(started.find("\"voteId\":\"v-bridge\""), std::string::npos);
    EXPECT_NE(started.find("\"active\":true"), std::string::npos);

    // vote_ended 携结果回填 responses 并落定 active=false
    EventHub::instance().emit("team.inbound", {
        {"type", "team.vote_ended"},
        {"voteId", "v-bridge"},
        {"result", {{"cov-member", "yes"}}},
    });
    const std::string ended = (*voteResultFn)({ScriptValue::fromString("v-bridge")}).asString();
    EXPECT_NE(ended.find("cov-member"), std::string::npos);
    EXPECT_NE(ended.find("\"active\":false"), std::string::npos);

    // 同上：保持「已加入」终态，避免 A11 空态用例的盲区
    ensureJoined();
}

} // anonymous namespace
