#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// 覆盖率收尾（2026-09-30 批次）：debugger / orchestration / security 三个
// 胶水模块的全部导出函数从未被任何测试调用（stub/纯函数面，无平台分支）。
// 经 ModuleDescriptor 直调锁死返回契约——debugger 是有意 stub（返回 false/
// 空集），orchestration 已落地依赖调度（拒绝路径 + 未知 ID 契约在此锁定，
// 行为面由 orchestration_module_test.cpp 覆盖），security 是 SecurityManager
// 薄封装。

namespace {

// 先按值拷贝模块再取函数指针：getAllModules() 返回临时 vector，直接在
// 循环里 return &f 是悬垂指针——Linux 上释放块内容未复用侥幸全绿，
// Windows Debug 堆复用/加毒后三用例全红（2026-10-01 CI 实测，与其他
// glue 用例的 getModule-by-value 模式对齐）
ModuleDescriptor getModule(const std::string& moduleName) {
    for (const auto& mod : getAllModules()) {
        if (mod.name == moduleName) return mod;
    }
    return {};
}

ScriptValue call(const std::string& moduleName, const std::string& fnName,
                 std::vector<ScriptValue> args = {}) {
    const auto mod = getModule(moduleName);
    const ModuleDescriptor::FunctionEntry* fn = nullptr;
    for (const auto& f : mod.functions) {
        if (f.name == fnName) { fn = &f; break; }
    }
    EXPECT_NE(fn, nullptr) << moduleName << "." << fnName;
    if (!fn) return ScriptValue::null();  // 缺函数时不断言后仍解引用（防升级为崩溃）
    return (*fn)(args);
}

} // namespace

TEST(GlueDebuggerModuleTest, StubContract) {
    // 有意 stub：start 恒 false、stop 恒 null
    EXPECT_FALSE(call("debugger", "start").asBool());
    EXPECT_TRUE(call("debugger", "stop").isNull());
    // breakpoint 只回显 "file:line" 标识（真实断点由 IDE 侧设置）
    EXPECT_EQ(call("debugger", "breakpoint", {ScriptValue::fromString("a.lua"),
                                              ScriptValue::fromInt(12)}).asString(),
              "a.lua:12");
    EXPECT_EQ(call("debugger", "breakHere").asString(), "DEBUG_BREAK_HERE");
}

TEST(GlueOrchestrationModuleTest, RejectionAndUnknownIdContract) {
    // 坏定义（缺参/非对象/无 tasks）与未知 ID 查询/取消：恒拒绝——与进程内
    // 是否已提交过工作流无关（全局管理器跨用例累积，不做空集断言）
    EXPECT_TRUE(call("orchestration", "submit_workflow").isNull());
    EXPECT_TRUE(call("orchestration", "submit_workflow",
                     {ScriptValue::fromString("nope")}).isNull());
    EXPECT_FALSE(call("orchestration", "cancel_workflow",
                      {ScriptValue::fromString("no-such-workflow")}).asBool());
    EXPECT_TRUE(call("orchestration", "get_workflow",
                     {ScriptValue::fromString("no-such-workflow")}).isNull());
}

TEST(GlueSecurityModuleTest, PassthroughContract) {
    const auto delay = call("security", "getRandomDelay");
    EXPECT_TRUE(delay.isInt() || delay.isFloat());
    EXPECT_GE(delay.asInt(), 0);

    const auto offset = call("security", "getRandomOffset");
    ASSERT_TRUE(offset.isArray());
    ASSERT_EQ(offset.size(), 2u);

    // 宿主环境探测：只锁类型（值取决于运行环境，不在测试里断言真假）
    EXPECT_TRUE(call("security", "isDebuggerPresent").isBool());
    EXPECT_TRUE(call("security", "isRunningInVM").isBool());
    EXPECT_TRUE(call("security", "verifyIntegrity").isBool());

    // hashString 走 crypt::sha256（64 位十六进制）；同一输入稳定
    const auto h1 = call("security", "hashString", {ScriptValue::fromString("wingman")}).asString();
    const auto h2 = call("security", "hashString", {ScriptValue::fromString("wingman")}).asString();
    EXPECT_EQ(h1.size(), 64u);
    EXPECT_EQ(h1, h2);

    EXPECT_EQ(call("security", "generateRandomString", {ScriptValue::fromInt(16)}).asString().size(),
              16u);
    // filterSensitive 把敏感键名整体替换为 "***"（security.cpp 模式表）
    const auto filtered = call("security", "filterSensitive",
                               {ScriptValue::fromString("password=abc123")}).asString();
    EXPECT_EQ(filtered.find("password"), std::string::npos);
    EXPECT_NE(filtered.find("***"), std::string::npos);
}
