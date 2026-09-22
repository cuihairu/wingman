#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// misc_modules.cpp 胶水层补测（2026-09-22 覆盖率收口）：smarttrigger / bt / node / ocr
// 四个模块此前在 Linux 上仅有零星触达（smarttrigger 主体用例被误 gate 在 WIN32，
// 见 tests/CMakeLists.txt）。本文件经 ModuleDescriptor 直接调用导出函数，
// 覆盖 parseCondition/parseAction 全部类型映射与各胶水的参数/错误分支。

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

ScriptValue makeObj(std::unordered_map<std::string, ScriptValue> fields) {
    return ScriptValue::fromObject(std::move(fields));
}

} // anonymous namespace

// ========== smarttrigger：create/remove/参数与错误分支 ==========

TEST(SmartTriggerModuleTest, CreateDuplicateAndRemoveLifecycle) {
    auto mod = getModule("smarttrigger");
    ASSERT_FALSE(mod.name.empty());

    const auto* createFn = findFunction(mod, "create");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    EXPECT_TRUE((*createFn)({ScriptValue::fromString("misc_cov_trg")}).asBool());
    // 重复创建返回已有实例（bool 语义仍为真）
    EXPECT_TRUE((*createFn)({ScriptValue::fromString("misc_cov_trg")}).asBool());

    (*removeFn)({ScriptValue::fromString("misc_cov_trg")});
    // 移除不存在的 trigger 也不崩（返回 null）
    EXPECT_TRUE((*removeFn)({ScriptValue::fromString("no_such_misc_trg")}).isNull());
}

TEST(SmartTriggerModuleTest, OpsOnMissingTriggerReturnFalse) {
    auto mod = getModule("smarttrigger");
    const auto* startFn = findFunction(mod, "start");
    const auto* stopFn = findFunction(mod, "stop");
    const auto* addCondFn = findFunction(mod, "addCondition");
    const auto* addActionFn = findFunction(mod, "addAction");
    const auto* intervalFn = findFunction(mod, "setCheckInterval");
    const auto* runningFn = findFunction(mod, "isRunning");
    const auto* countFn = findFunction(mod, "getTriggerCount");
    ASSERT_NE(startFn, nullptr);
    ASSERT_NE(stopFn, nullptr);
    ASSERT_NE(addCondFn, nullptr);
    ASSERT_NE(addActionFn, nullptr);
    ASSERT_NE(intervalFn, nullptr);
    ASSERT_NE(runningFn, nullptr);
    ASSERT_NE(countFn, nullptr);

    EXPECT_FALSE((*startFn)({ScriptValue::fromString("ghost_trg")}).asBool());
    EXPECT_FALSE((*stopFn)({ScriptValue::fromString("ghost_trg")}).asBool());
    EXPECT_FALSE((*addCondFn)({ScriptValue::fromString("ghost_trg"), makeObj({})}).asBool());
    EXPECT_FALSE((*addActionFn)({ScriptValue::fromString("ghost_trg"), makeObj({})}).asBool());
    EXPECT_FALSE((*intervalFn)({ScriptValue::fromString("ghost_trg"), ScriptValue::fromInt(10)}).asBool());
    EXPECT_FALSE((*runningFn)({ScriptValue::fromString("ghost_trg")}).asBool());
    EXPECT_EQ((*countFn)({ScriptValue::fromString("ghost_trg")}).asInt(), 0);

    // 参数不足的防御分支
    EXPECT_FALSE((*addCondFn)({ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*addActionFn)({ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*intervalFn)({ScriptValue::fromString("x")}).asBool());
    EXPECT_FALSE((*runningFn)({}).asBool());
    EXPECT_EQ((*countFn)({}).asInt(), 0);
}

// parseCondition 类型映射（snake_case 与大写枚举名两种风格）——经 addCondition 触达
TEST(SmartTriggerModuleTest, AddConditionAcceptsAllConditionTypeStrings) {
    auto mod = getModule("smarttrigger");
    const auto* createFn = findFunction(mod, "create");
    const auto* addCondFn = findFunction(mod, "addCondition");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(addCondFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    const std::string name = "misc_cov_types";
    ASSERT_TRUE((*createFn)({ScriptValue::fromString(name)}).asBool());

    const std::vector<std::string> types = {
        "color_found", "COLOR_FOUND",
        "color_not_found", "COLOR_NOT_FOUND",
        "image_found", "IMAGE_FOUND",
        "image_not_found", "IMAGE_NOT_FOUND",
        "text_found", "TEXT_FOUND",
        "text_not_found", "TEXT_NOT_FOUND",
        "edge_detected", "EDGE_DETECTED",
        "color_changed", "COLOR_CHANGED",
        "ocr_contains", "OCR_CONTAINS",
        "ocr_equals", "OCR_EQUALS",
    };
    for (const auto& t : types) {
        auto cond = makeObj({
            {"type", ScriptValue::fromString(t)},
            {"color", makeObj({{"r", ScriptValue::fromInt(255)}, {"g", ScriptValue::fromInt(0)}, {"b", ScriptValue::fromInt(255)}})},
            {"tolerance", ScriptValue::fromInt(10)},
            {"threshold", ScriptValue::fromFloat(0.9)},
            {"region", makeObj({{"x", ScriptValue::fromInt(0)}, {"y", ScriptValue::fromInt(0)}, {"width", ScriptValue::fromInt(100)}, {"height", ScriptValue::fromInt(100)}})},
            {"text", ScriptValue::fromString("needle")},
            {"template", ScriptValue::fromString("/tmp/tpl.png")},
            {"templatePath", ScriptValue::fromString("/tmp/tpl2.png")},
        });
        EXPECT_TRUE((*addCondFn)({ScriptValue::fromString(name), cond}).asBool()) << "type=" << t;
    }
    // 未知 type 字符串：解析走默认分支，仍成功添加（不抛错）
    EXPECT_TRUE((*addCondFn)({ScriptValue::fromString(name), makeObj({{"type", ScriptValue::fromString("bogus")}})}).asBool());

    (*removeFn)({ScriptValue::fromString(name)});
}

// parseAction 类型映射与坐标/按键/时长/脚本/消息字段
TEST(SmartTriggerModuleTest, AddActionAcceptsAllActionTypeStringsAndFields) {
    auto mod = getModule("smarttrigger");
    const auto* createFn = findFunction(mod, "create");
    const auto* addActionFn = findFunction(mod, "addAction");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(addActionFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    const std::string name = "misc_cov_actions";
    ASSERT_TRUE((*createFn)({ScriptValue::fromString(name)}).asBool());

    const std::vector<std::string> types = {
        "click", "CLICK", "key_press", "KEY_PRESS", "keypress",
        "wait", "WAIT", "delay", "lua_script", "LUA_SCRIPT", "log", "LOG", "stop", "STOP",
    };
    for (const auto& t : types) {
        auto action = makeObj({
            {"type", ScriptValue::fromString(t)},
            {"position", makeObj({{"x", ScriptValue::fromInt(12)}, {"y", ScriptValue::fromInt(34)}})},
            {"x", ScriptValue::fromInt(56)},
            {"y", ScriptValue::fromInt(78)},
            {"key", ScriptValue::fromInt(65)},
            {"keyCode", ScriptValue::fromInt(66)},
            {"waitMs", ScriptValue::fromInt(5)},
            {"delay", ScriptValue::fromInt(6)},
            {"script", ScriptValue::fromString("print('x')")},
            {"message", ScriptValue::fromString("hello")},
        });
        EXPECT_TRUE((*addActionFn)({ScriptValue::fromString(name), action}).asBool()) << "type=" << t;
    }
    EXPECT_TRUE((*addActionFn)({ScriptValue::fromString(name), makeObj({{"type", ScriptValue::fromString("bogus")}})}).asBool());

    (*removeFn)({ScriptValue::fromString(name)});
}

TEST(SmartTriggerModuleTest, StartStopIsRunningAndCountRoundTrip) {
    auto mod = getModule("smarttrigger");
    const auto* createFn = findFunction(mod, "create");
    const auto* addCondFn = findFunction(mod, "addCondition");
    const auto* startFn = findFunction(mod, "start");
    const auto* stopFn = findFunction(mod, "stop");
    const auto* runningFn = findFunction(mod, "isRunning");
    const auto* countFn = findFunction(mod, "getTriggerCount");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(addCondFn, nullptr);
    ASSERT_NE(startFn, nullptr);
    ASSERT_NE(stopFn, nullptr);
    ASSERT_NE(runningFn, nullptr);
    ASSERT_NE(countFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    const std::string name = "misc_cov_roundtrip";
    ASSERT_TRUE((*createFn)({ScriptValue::fromString(name)}).asBool());
    // 恒真条件（OCR stub 下 TEXT_NOT_FOUND 必真）驱动 watchLoop；STOP 动作自停
    ASSERT_TRUE((*addCondFn)({ScriptValue::fromString(name), makeObj({
        {"type", ScriptValue::fromString("TEXT_NOT_FOUND")},
        {"text", ScriptValue::fromString("never-on-screen")},
    })}).asBool());

    EXPECT_TRUE((*startFn)({ScriptValue::fromString(name)}).asBool());
    // TEXT_NOT_FOUND 在 stub 下恒真，watchLoop 可能已触发（竞态）——只做过程触达，
    // 确定性断言只有「stop 之后必然不在运行」
    (*runningFn)({ScriptValue::fromString(name)});
    (*countFn)({ScriptValue::fromString(name)});
    (*stopFn)({ScriptValue::fromString(name)});
    EXPECT_FALSE((*runningFn)({ScriptValue::fromString(name)}).asBool());

    (*removeFn)({ScriptValue::fromString(name)});
}

// ========== bt：行为树胶水 ==========

TEST(BehaviorTreeModuleTest, CreateTickRemoveLifecycle) {
    auto mod = getModule("bt");
    ASSERT_FALSE(mod.name.empty());
    const auto* createFn = findFunction(mod, "create");
    const auto* tickFn = findFunction(mod, "tick");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(tickFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    EXPECT_TRUE((*createFn)({ScriptValue::fromString("misc_cov_tree")}).asBool());
    // 无 root 时 tick 走 manager 默认（SUCCESS/FAILURE 都合法，不得崩）
    std::string status = (*tickFn)({ScriptValue::fromString("misc_cov_tree")}).asString();
    EXPECT_TRUE(status == "SUCCESS" || status == "FAILURE" || status == "RUNNING");

    (*removeFn)({ScriptValue::fromString("misc_cov_tree")});
    // 移除后 tick 走兜底 FAILURE 分支
    EXPECT_EQ((*tickFn)({ScriptValue::fromString("misc_cov_tree_gone")}).asString(), "FAILURE");
}

TEST(BehaviorTreeModuleTest, NodeBuildersAndInvalidHandles) {
    auto mod = getModule("bt");
    const auto* seqFn = findFunction(mod, "sequence");
    const auto* selFn = findFunction(mod, "selector");
    const auto* parFn = findFunction(mod, "parallel");
    const auto* waitFn = findFunction(mod, "wait");
    const auto* invFn = findFunction(mod, "inverter");
    const auto* repFn = findFunction(mod, "repeat");
    const auto* addChildFn = findFunction(mod, "addChild");
    const auto* condFn = findFunction(mod, "condition");
    const auto* actionFn = findFunction(mod, "action");
    ASSERT_NE(seqFn, nullptr);
    ASSERT_NE(selFn, nullptr);
    ASSERT_NE(parFn, nullptr);
    ASSERT_NE(waitFn, nullptr);
    ASSERT_NE(invFn, nullptr);
    ASSERT_NE(repFn, nullptr);
    ASSERT_NE(addChildFn, nullptr);
    ASSERT_NE(condFn, nullptr);
    ASSERT_NE(actionFn, nullptr);

    auto seq = (*seqFn)({ScriptValue::fromString("cov_seq")}).asInt();
    auto sel = (*selFn)({}).asInt(); // 无参默认名
    EXPECT_GT(seq, 0);
    EXPECT_GT(sel, 0);

    // parallel 四种 policy 字符串 + 默认
    for (const char* policy : {"SUCCEED_ON_ALL", "SUCCEED_ON_ONE", "FAIL_ON_ALL", "FAIL_ON_ONE", "UNKNOWN"}) {
        EXPECT_GT((*parFn)({ScriptValue::fromString("cov_par"), ScriptValue::fromString(policy)}).asInt(), 0) << policy;
    }

    auto waitNode = (*waitFn)({ScriptValue::fromInt(0)}).asInt();
    EXPECT_GT(waitNode, 0);

    // 无效 child 句柄 → 0
    EXPECT_EQ((*invFn)({ScriptValue::fromInt(999999)}).asInt(), 0);
    EXPECT_EQ((*repFn)({ScriptValue::fromInt(999999), ScriptValue::fromInt(3)}).asInt(), 0);
    EXPECT_EQ((*condFn)({ScriptValue::fromString("c")}).asInt(), 0); // 缺 callable
    EXPECT_EQ((*actionFn)({ScriptValue::fromString("a")}).asInt(), 0);

    // 有效装饰器/叶子
    auto inv = (*invFn)({ScriptValue::fromInt(waitNode)}).asInt();
    auto rep = (*repFn)({ScriptValue::fromInt(waitNode), ScriptValue::fromInt(2)}).asInt();
    EXPECT_GT(inv, 0);
    EXPECT_GT(rep, 0);

    // addChild：复合节点成功、叶子拒绝、无效句柄拒绝
    EXPECT_TRUE((*addChildFn)({ScriptValue::fromInt(seq), ScriptValue::fromInt(waitNode)}).asBool());
    EXPECT_TRUE((*addChildFn)({ScriptValue::fromInt(sel), ScriptValue::fromInt(waitNode)}).asBool());
    EXPECT_FALSE((*addChildFn)({ScriptValue::fromInt(inv), ScriptValue::fromInt(waitNode)}).asBool());
    EXPECT_FALSE((*addChildFn)({ScriptValue::fromInt(-1), ScriptValue::fromInt(waitNode)}).asBool());

    // condition/action 带 callable（含条件回调真值分支）
    auto condOk = (*condFn)({ScriptValue::fromString("cov_cond"), ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromBool(true); })}).asInt();
    auto actOk = (*actionFn)({ScriptValue::fromString("cov_action"), ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromString("SUCCESS"); })}).asInt();
    auto actRunning = (*actionFn)({ScriptValue::fromString("cov_action_r"), ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromString("RUNNING"); })}).asInt();
    EXPECT_GT(condOk, 0);
    EXPECT_GT(actOk, 0);
    EXPECT_GT(actRunning, 0);
}

TEST(BehaviorTreeModuleTest, SetRootAndTickExecuteTree) {
    auto mod = getModule("bt");
    const auto* createFn = findFunction(mod, "create");
    const auto* setRootFn = findFunction(mod, "setRoot");
    const auto* tickFn = findFunction(mod, "tick");
    const auto* seqFn = findFunction(mod, "sequence");
    const auto* actionFn = findFunction(mod, "action");
    const auto* addChildFn = findFunction(mod, "addChild");
    const auto* removeFn = findFunction(mod, "remove");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(setRootFn, nullptr);
    ASSERT_NE(tickFn, nullptr);
    ASSERT_NE(seqFn, nullptr);
    ASSERT_NE(actionFn, nullptr);
    ASSERT_NE(addChildFn, nullptr);
    ASSERT_NE(removeFn, nullptr);

    ASSERT_TRUE((*createFn)({ScriptValue::fromString("misc_cov_exec")}).asBool());
    auto seq = (*seqFn)({ScriptValue::fromString("cov_exec_seq")}).asInt();
    auto act = (*actionFn)({ScriptValue::fromString("cov_exec_act"), ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromString("SUCCESS"); })}).asInt();
    ASSERT_TRUE((*addChildFn)({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)}).asBool());

    // 无效句柄/未知树 → false
    EXPECT_FALSE((*setRootFn)({ScriptValue::fromString("misc_cov_exec"), ScriptValue::fromInt(-5)}).asBool());
    EXPECT_FALSE((*setRootFn)({ScriptValue::fromString("no_such_tree"), ScriptValue::fromInt(seq)}).asBool());

    EXPECT_TRUE((*setRootFn)({ScriptValue::fromString("misc_cov_exec"), ScriptValue::fromInt(seq)}).asBool());
    EXPECT_EQ((*tickFn)({ScriptValue::fromString("misc_cov_exec")}).asString(), "SUCCESS");

    (*removeFn)({ScriptValue::fromString("misc_cov_exec")});
}

// ========== node：NodeStatus 心跳/窗口枚举胶水 ==========

TEST(NodeModuleTest, CreateHeartbeatReturnsShape) {
    auto mod = getModule("node");
    ASSERT_FALSE(mod.name.empty());
    const auto* hbFn = findFunction(mod, "createHeartbeat");
    const auto* sendFn = findFunction(mod, "sendHeartbeat");
    const auto* winFn = findFunction(mod, "getWindows");
    ASSERT_NE(hbFn, nullptr);
    ASSERT_NE(sendFn, nullptr);
    ASSERT_NE(winFn, nullptr);

    auto hb = (*hbFn)({});
    ASSERT_TRUE(hb.isObject());
    EXPECT_FALSE(hb.get("json")->asString().empty()); // toJson 恒产出
    // nodeId/version 默认构造可为空（运行时注册后才有值）——只断言字段存在
    ASSERT_NE(hb.get("nodeId"), nullptr);
    ASSERT_NE(hb.get("version"), nullptr);

    // sendHeartbeat 简化实现恒返回 null
    EXPECT_TRUE((*sendFn)({makeObj({})}).isNull());

    // getWindows 返回数组（无头 0 窗口合法，Xvfb 下通常有 root 窗口）
    auto wins = (*winFn)({});
    ASSERT_TRUE(wins.isArray());
    for (size_t i = 0; i < wins.size(); ++i) {
        auto w = wins.at(static_cast<int>(i));
        EXPECT_TRUE(w.isObject());
        ASSERT_NE(w.get("title"), nullptr);
        ASSERT_NE(w.get("handle"), nullptr);
        ASSERT_NE(w.get("isForeground"), nullptr);
        ASSERT_NE(w.get("bounds"), nullptr);
    }
}

// ========== ocr：识别胶水（stub 下恒失败，真实现下形状不变）==========

TEST(OcrModuleTest, RecognizeShapeStableAcrossStubAndReal) {
    auto mod = getModule("ocr");
    ASSERT_FALSE(mod.name.empty());
    const auto* recFn = findFunction(mod, "recognize");
    const auto* recTextFn = findFunction(mod, "recognizeText");
    ASSERT_NE(recFn, nullptr);
    ASSERT_NE(recTextFn, nullptr);

    auto region = makeObj({
        {"x", ScriptValue::fromInt(0)}, {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(50)}, {"height", ScriptValue::fromInt(50)},
    });
    auto result = (*recFn)({region});
    ASSERT_TRUE(result.isObject());
    // stub：success=false；真实现：success 可真可假——只断言字段形状
    ASSERT_NE(result.get("success"), nullptr);
    ASSERT_NE(result.get("text"), nullptr);
    ASSERT_NE(result.get("confidence"), nullptr);

    // recognizeText：stub 下 success=false → null；真实现成功时返回 string
    auto text = (*recTextFn)({region});
    EXPECT_TRUE(text.isNull() || text.isString());
}
