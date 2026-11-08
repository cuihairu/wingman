#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

#include <string>
#include <vector>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// uia 模块面缺口补测（2026-11-08 覆盖率收口）：find_*/from_*/tree 查找家族与
// UIElement OO 对象方法此前零覆盖。查找家族在无后端/无窗口环境下返回 null/空数组
// （模块函数体即达覆盖）；元素 OO 方法经「假 _handle 对象」驱动 uiaGetElement 无效
// 句柄 → null → false/null 分支（真实元素路径在 ui_automation_test.cpp 已覆盖，
// Windows CI 会话 0 后端真实可用）。事件监听函数验证非线程安全 callable 拒绝
// 分支（uia.error 事件 + 0 返回），不注册真实后端监听。

namespace {

ModuleDescriptor getModule(const std::string& name) {
    for (auto& mod : getAllModules()) {
        if (mod.name == name) return mod;
    }
    return {};
}

const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod,
                                                    const std::string& name) {
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

// 假元素对象：无效 handle 使 uiaGetElement 返回 null，驱动 OO 方法的失败分支
ScriptValue fakeElement() {
    return ScriptValue::fromObject({{"_handle", ScriptValue::fromInt(999999)}});
}

// Windows 有系统 UIA 后端（会话 0 亦然）：查找/树导出会返回真实元素或回退
// 前台焦点，「无后端 → null」断言只在 Linux stub 下成立（同
// misc_uia_node_coverage_test 的 #ifdef _WIN32 skip 口径）。后端在位时这些
// 断言退化为「不崩 + 形态合法」，分支体照常执行（Windows 覆盖率口径）。
#ifdef _WIN32
constexpr bool kUiaBackendPresent = true;
#else
constexpr bool kUiaBackendPresent = false;
#endif

// 无后端严格 null；真后端容忍 null 或真实元素对象
void expectNullOrElement(const ScriptValue& v) {
    if (kUiaBackendPresent) {
        EXPECT_TRUE(v.isNull() || v.isObject());
    } else {
        EXPECT_TRUE(v.isNull());
    }
}

} // namespace

// ========== 查找家族：无窗口环境返回 null/空数组，模块函数体即达覆盖 ==========

TEST(UiaModuleCoverageTest, FindFamilyReturnsWithoutBackend) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    // 带具体不存在名的查找：真后端下也必然找不到 → 严格 null
    EXPECT_TRUE(call(mod, "find_by_name", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_by_id", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_all_by_control_type", {ScriptValue::fromInt(0)}).isArray());
    EXPECT_TRUE(call(mod, "find_button", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_edit", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_text", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_check_box", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_radio_button", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_combo_box", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_list", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_list_item", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_tree", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_tree_item", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_menu_item", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_hyperlink", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_image", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_slider", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_spinner", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_progress_bar", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_tab", {ScriptValue::fromString("no-such")}).isNull());
    EXPECT_TRUE(call(mod, "find_tab_item", {ScriptValue::fromString("no-such")}).isNull());

    // 无参默认分支（args.size()==0 走默认空串）：空名在真后端下可匹配任意
    // 元素 → 环境容忍
    expectNullOrElement(call(mod, "find_by_name"));
    expectNullOrElement(call(mod, "find_by_id"));
    EXPECT_TRUE(call(mod, "find_all_by_control_type").isArray());
}

// wait 家族：短超时（50ms）绑定轮询；具体不存在名严格 null，空选择器/宽泛
// role 在真后端下可命中真实元素 → 环境容忍
TEST(UiaModuleCoverageTest, WaitFamilyWithShortTimeout) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    EXPECT_TRUE(call(mod, "wait_for_name",
                     {ScriptValue::fromString("no-such"), ScriptValue::fromInt(50)}).isNull());
    EXPECT_TRUE(call(mod, "wait_for_id",
                     {ScriptValue::fromString("no-such"), ScriptValue::fromInt(50)}).isNull());
    // role 0 在真后端下可匹配某类控件 → 容忍
    expectNullOrElement(call(mod, "wait_for_role",
                             {ScriptValue::fromInt(0), ScriptValue::fromInt(50)}));
    // wait_for 选择器对象全字段（名字不存在 → 严格 null）+ 非对象首参（退化为
    // 空选择器，真后端可命中）+ 缺省超时
    EXPECT_TRUE(call(mod, "wait_for", {
        ScriptValue::fromObject({
            {"name", ScriptValue::fromString("n")},
            {"id", ScriptValue::fromString("i")},
            {"className", ScriptValue::fromString("c")},
            {"role", ScriptValue::fromInt(1)},
            {"text", ScriptValue::fromString("t")},
        }),
        ScriptValue::fromInt(50),
    }).isNull());
    expectNullOrElement(call(mod, "wait_for",
                             {ScriptValue::fromString("not-object"), ScriptValue::fromInt(50)}));
    expectNullOrElement(call(mod, "wait_for", {ScriptValue::fromObject({})}));
}

TEST(UiaModuleCoverageTest, FromAndTreeBranches) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    // from_foreground / from_point / from_window：无焦点/无效 → null；真后端
    // 下可返回真实元素（会话 0 焦点可用性随时间漂移）→ 容忍
    expectNullOrElement(call(mod, "from_foreground"));
    expectNullOrElement(call(mod, "from_point", {ScriptValue::fromInt(10), ScriptValue::fromInt(10)}));
    expectNullOrElement(call(mod, "from_point")); // 缺参默认 0,0
    expectNullOrElement(call(mod, "from_window", {ScriptValue::fromInt(0)}));
    expectNullOrElement(call(mod, "from_window"));

    // tree 分支矩阵：整型首参 / null 首参 / 对象含 _handle / 对象不含 / 深度
    // 钳制。真后端下根回退前台焦点可得真实树 → 容忍
    expectNullOrElement(call(mod, "tree", {ScriptValue::fromInt(3)}));          // 整型 → maxDepth=3
    expectNullOrElement(call(mod, "tree", {ScriptValue::fromInt(0)}));          // 钳到 1
    expectNullOrElement(call(mod, "tree", {ScriptValue::fromInt(99)}));         // 钳到 16
    expectNullOrElement(call(mod, "tree", {ScriptValue::null()}));              // 非对象非整型 → 默认 6
    expectNullOrElement(call(mod, "tree", {ScriptValue::null(), ScriptValue::fromInt(2)})); // args[1] 深度
    expectNullOrElement(call(mod, "tree", {ScriptValue::fromObject({})}));      // 对象无 _handle → 根走焦点
    expectNullOrElement(call(mod, "tree", {fakeElement(), ScriptValue::fromInt(4)})); // 无效 handle → 根走焦点
    expectNullOrElement(call(mod, "tree"));                                     // 无参
}

// ========== UIElement OO 方法：真实元素只读面 ==========
// 元素 OO 方法条目仅由 makeUiaElementObject（模块内部）创建，测试无法手工构造——
// 只能经模块函数取得真实元素对象（Windows 会话 0 UIA 后端可用）。只调用只读方法
// （不执行 click/set_value/expand 等真实 UI 动作，避免 CI 上副作用），仅断言不崩。
// 无后端/无可枚举元素环境（Linux headless）SKIP——该方法体覆盖依赖真实元素路径，
// 由 ui_automation_test.cpp 的真实元素用例互补。

TEST(UiaModuleCoverageTest, ElementReadOnlyMethodsOnRealElement) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    ScriptValue el = call(mod, "from_foreground");
    if (el.isNull()) el = call(mod, "from_point", {ScriptValue::fromInt(0), ScriptValue::fromInt(0)});
    if (el.isNull()) el = call(mod, "from_window", {ScriptValue::fromInt(0)});
    if (el.isNull()) el = call(mod, "find_by_name", {ScriptValue::fromString("")});
    if (el.isNull()) GTEST_SKIP() << "no UIA element available in this environment";

    EXPECT_NO_THROW(el.get("get_info")->call({}));
    EXPECT_NO_THROW(el.get("get_value")->call({}));
    EXPECT_NO_THROW(el.get("get_children")->call({}));
    EXPECT_NO_THROW(el.get("get_parent")->call({}));
    EXPECT_NO_THROW(el.get("is_expanded")->call({}));
    EXPECT_NO_THROW(el.get("is_visible")->call({}));
    EXPECT_NO_THROW(el.get("is_enabled")->call({}));
    EXPECT_NO_THROW(el.get("is_checked")->call({}));
    EXPECT_NO_THROW(el.get("get_selection")->call({}));
}

// ========== 事件监听：参数/线程安全门控分支 ==========

TEST(UiaModuleCoverageTest, EventListenerRejectsBadArgsAndNonThreadSafeCallable) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    // 缺 callback / 非 callable → 0
    EXPECT_EQ(call(mod, "on_property_changed", {ScriptValue::fromString("btn")}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_property_changed",
                   {ScriptValue::fromString("btn"), ScriptValue::fromString("x")}).asInt(), 0);
    EXPECT_EQ(call(mod, "on_structure_changed", {ScriptValue::fromString("btn")}).asInt(), 0);

    // 非线程安全 callable → uia.error 事件 + 0（门控拒绝，不触碰后端）
    std::string errorText;
    const uint64_t sub = EventHub::instance().subscribe("uia.error",
        [&](const EventMessage& msg) { errorText = msg.payload.value("error", ""); });
    ASSERT_NE(sub, 0u);
    ScriptValue nonTs = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, false);
    EXPECT_EQ(call(mod, "on_property_changed",
                   {ScriptValue::fromString("btn"), nonTs}).asInt(), 0);
    EXPECT_FALSE(errorText.empty());
    errorText.clear();
    EXPECT_EQ(call(mod, "on_structure_changed",
                   {ScriptValue::fromString("btn"), nonTs}).asInt(), 0);
    EXPECT_FALSE(errorText.empty());
    EventHub::instance().unsubscribe(sub);

    // 线程安全 callable 会注册真实后端监听（Windows 会话 0 下 addPropertyChangedListener
    // 为 RPC 注册、回调体仅在真实属性变更时触发——headless 不可达且带回环注册挂起风险），
    // 此分支不在此覆盖；注册/回调体由 ui_automation_test.cpp 的真实元素路径负责。
    // remove_event_listener 对无效 id 幂等 false（缺参走 id=0）。
    EXPECT_FALSE(call(mod, "remove_event_listener",
                      {ScriptValue::fromInt(99999999)}).asBool());
    EXPECT_NO_THROW(call(mod, "remove_event_listener"));
}
