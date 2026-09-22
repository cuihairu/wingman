// misc_modules.cpp 第六批缺口补测：uia 模块（Linux stub 下全查找路径）、
// bt action callable 的 RUNNING/FAILURE 映射 tick、bt.status/remove 未知树、
// bt.wait 构造、smarttrigger.setCheckInterval 存在路径、node.sendHeartbeat、
// ocr.recognize 文本字段。
// UIElement 12 方法闭包体与 uiaElementRegistry 在 Linux 上不可达（无平台
// UIA 后端产生真实元素实例，见 docs 记录），本文件覆盖全部可达部分。
#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

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
    return (*fn)(args);
}

ScriptValue callable(ScriptValue::CallableFunc fn, bool threadSafe = false) {
    return ScriptValue::fromCallable(std::move(fn), threadSafe);
}

} // anonymous namespace

// ========== uia：Linux 无后端时全部查找函数返回 null / 空数组 ==========

TEST(UiaModuleGlueTest, AllFindFunctionsReturnNullWithoutBackend) {
    const auto mod = getModule("uia");
    ASSERT_EQ(mod.name, "uia");

    EXPECT_TRUE(call(mod, "from_foreground").isNull());
    EXPECT_TRUE(call(mod, "from_point", {ScriptValue::fromInt(5), ScriptValue::fromInt(6)}).isNull());
    EXPECT_TRUE(call(mod, "from_point").isNull()); // 缺参默认 0,0
    EXPECT_TRUE(call(mod, "from_window", {ScriptValue::fromInt(123)}).isNull());
    EXPECT_TRUE(call(mod, "find_by_name", {ScriptValue::fromString("btn")}).isNull());
    EXPECT_TRUE(call(mod, "find_by_id", {ScriptValue::fromString("id-1")}).isNull());
    EXPECT_TRUE(call(mod, "find_button", {ScriptValue::fromString("OK")}).isNull());
    EXPECT_TRUE(call(mod, "find_edit", {ScriptValue::fromString("user")}).isNull());
    EXPECT_TRUE(call(mod, "find_text", {ScriptValue::fromString("hello")}).isNull());
    EXPECT_TRUE(call(mod, "wait_for_name",
                     {ScriptValue::fromString("never"), ScriptValue::fromInt(1)}).isNull());

    const auto all = call(mod, "find_all_by_control_type", {ScriptValue::fromInt(0)});
    EXPECT_TRUE(all.isArray());
    EXPECT_EQ(all.size(), 0u);
}

// ========== uia：事件监听全防御分支（含 threadSafe 通过后的无后端路径）==========

TEST(UiaModuleGlueTest, EventListenersAllBranches) {
    const auto mod = getModule("uia");
    ASSERT_EQ(mod.name, "uia");

    // 缺 callable / 非 callable → 0
    EXPECT_EQ(call(mod, "on_property_changed", {ScriptValue::fromString("x")}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_property_changed",
                   {ScriptValue::fromString("x"), ScriptValue::fromBool(true)}).asInt(), 0);
    // 非线程安全 callable（模拟 Lua）→ emit uia.error 后 0
    EXPECT_EQ(call(mod, "on_property_changed",
                   {ScriptValue::fromString("x"),
                    callable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); })}).asInt(), 0);
    // 线程安全 callable：Linux 无后端，listenerId 仍为 0 但覆盖包装闭包构造路径
    EXPECT_EQ(call(mod, "on_property_changed",
                   {ScriptValue::fromString("x"),
                    callable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, true)}).asInt(), 0);

    // on_structure_changed 同构四分支
    EXPECT_EQ(call(mod, "on_structure_changed", {ScriptValue::fromString("x")}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_structure_changed",
                   {ScriptValue::fromString("x"), ScriptValue::fromInt(7)}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_structure_changed",
                   {ScriptValue::fromString("x"),
                    callable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); })}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_structure_changed",
                   {ScriptValue::fromString("x"),
                    callable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, true)}).asInt(), 0);

    // remove_event_listener：无后端 → false；缺参默认 id=0 → false
    EXPECT_EQ(call(mod, "remove_event_listener", {ScriptValue::fromInt(42)}).asBool(), false);
    EXPECT_EQ(call(mod, "remove_event_listener").asBool(), false);
}

// ========== bt：action callable 的 RUNNING / FAILURE 映射 tick + wait/status/remove ==========

TEST(BehaviorTreeModuleGlueTest, ActionCallableStatusMappingsAndMissingTree) {
    const auto mod = getModule("bt");
    ASSERT_EQ(mod.name, "bt");

    // bt.wait 构造（此前零覆盖）
    const auto waitHandle = call(mod, "wait", {ScriptValue::fromInt(1)}).asInt();
    EXPECT_GT(waitHandle, 0);

    // cb 返回 "RUNNING" → NodeStatus::RUNNING（此前零覆盖）
    ASSERT_TRUE(call(mod, "create", {ScriptValue::fromString("bt6_running")}).asBool());
    const auto runNode = call(mod, "action",
                              {ScriptValue::fromString("run_act"),
                               callable([](const std::vector<ScriptValue>&) {
                                   return ScriptValue::fromString("RUNNING");
                               })}).asInt();
    ASSERT_GT(runNode, 0);
    ASSERT_TRUE(call(mod, "setRoot", {ScriptValue::fromString("bt6_running"),
                                      ScriptValue::fromInt(runNode)}).asBool());
    EXPECT_EQ(call(mod, "tick", {ScriptValue::fromString("bt6_running")}).asString(), "RUNNING");

    // cb 返回未知字符串 → NodeStatus::FAILURE（此前零覆盖）
    ASSERT_TRUE(call(mod, "create", {ScriptValue::fromString("bt6_garbage")}).asBool());
    const auto badNode = call(mod, "action",
                              {ScriptValue::fromString("bad_act"),
                               callable([](const std::vector<ScriptValue>&) {
                                   return ScriptValue::fromString("nonsense");
                               })}).asInt();
    ASSERT_GT(badNode, 0);
    ASSERT_TRUE(call(mod, "setRoot", {ScriptValue::fromString("bt6_garbage"),
                                      ScriptValue::fromInt(badNode)}).asBool());
    EXPECT_EQ(call(mod, "tick", {ScriptValue::fromString("bt6_garbage")}).asString(), "FAILURE");

    // bt.action 缺 callable → handle 0
    EXPECT_EQ(call(mod, "action", {ScriptValue::fromString("no_cb")}).asInt(), 0);

    // tick 未知树 → "FAILURE"（getTree 失败的 default 尾此前零覆盖）
    EXPECT_EQ(call(mod, "tick", {ScriptValue::fromString("no_such_tree_bt6")}).asString(), "FAILURE");

    // bt.remove：未知树也安全返回 null
    EXPECT_TRUE(call(mod, "remove", {ScriptValue::fromString("no_such_tree_bt6")}).isNull());

    call(mod, "remove", {ScriptValue::fromString("bt6_running")});
    call(mod, "remove", {ScriptValue::fromString("bt6_garbage")});
}

// ========== smarttrigger：setCheckInterval 对存在触发器生效（147-148 行）==========

TEST(SmartTriggerModuleGlueTest, SetCheckIntervalOnExistingTriggerSucceeds) {
    const auto mod = getModule("smarttrigger");
    ASSERT_EQ(mod.name, "smarttrigger");

    ASSERT_TRUE(call(mod, "create", {ScriptValue::fromString("sci6_trigger")}).asBool());
    // 存在路径 → true（此前仅覆盖 missing → false）
    EXPECT_EQ(call(mod, "setCheckInterval",
                   {ScriptValue::fromString("sci6_trigger"), ScriptValue::fromInt(250)}).asBool(), true);
    // 缺参防御
    EXPECT_EQ(call(mod, "setCheckInterval", {ScriptValue::fromString("sci6_trigger")}).asBool(), false);
    // 清理（remove 复用既有覆盖）
    call(mod, "remove", {ScriptValue::fromString("sci6_trigger")});
    EXPECT_EQ(call(mod, "setCheckInterval",
                   {ScriptValue::fromString("sci6_trigger"), ScriptValue::fromInt(250)}).asBool(), false);
}

// ========== node：sendHeartbeat 胶水（327 行）==========

TEST(NodeModuleGlueTest, SendHeartbeatReturnsNull) {
    const auto mod = getModule("node");
    ASSERT_EQ(mod.name, "node");

    std::unordered_map<std::string, ScriptValue> hb;
    hb["nodeId"] = ScriptValue::fromString("node-6");
    hb["version"] = ScriptValue::fromString("1.0.0");
    EXPECT_TRUE(call(mod, "sendHeartbeat", {ScriptValue::fromObject(std::move(hb))}).isNull());
    EXPECT_TRUE(call(mod, "sendHeartbeat").isNull()); // 缺参也安全
}

// ========== ocr：recognize 返回对象含 text 字段（26 行）==========

TEST(OcrModuleGlueTest, RecognizeResultIncludesTextField) {
    const auto mod = getModule("ocr");
    ASSERT_EQ(mod.name, "ocr");

    std::unordered_map<std::string, ScriptValue> region;
    region["x"] = ScriptValue::fromInt(0);
    region["y"] = ScriptValue::fromInt(0);
    region["width"] = ScriptValue::fromInt(8);
    region["height"] = ScriptValue::fromInt(8);
    const auto result = call(mod, "recognize", {ScriptValue::fromObject(std::move(region))});
    ASSERT_TRUE(result.isObject());
    ASSERT_NE(result.get("text"), nullptr); // text 字段构造行（stub 与真实实现均稳定）
    EXPECT_TRUE(result.get("success") != nullptr);
}
