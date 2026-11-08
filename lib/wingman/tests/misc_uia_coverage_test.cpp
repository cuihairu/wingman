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

} // namespace

// ========== 查找家族：无窗口环境返回 null/空数组，模块函数体即达覆盖 ==========

TEST(UiaModuleCoverageTest, FindFamilyReturnsWithoutBackend) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    // 无参/带参查找家族：无后端 → makeUiaElementObject(null) → null；find_all → 空数组
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

    // 无参默认分支（args.size()==0 走默认空串）
    EXPECT_TRUE(call(mod, "find_by_name").isNull());
    EXPECT_TRUE(call(mod, "find_by_id").isNull());
    EXPECT_TRUE(call(mod, "find_all_by_control_type").isArray());
}

// wait 家族：短超时（50ms）绑定轮询，无后端返回 null；selector 对象逐字段解析
TEST(UiaModuleCoverageTest, WaitFamilyWithShortTimeout) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    EXPECT_TRUE(call(mod, "wait_for_name",
                     {ScriptValue::fromString("no-such"), ScriptValue::fromInt(50)}).isNull());
    EXPECT_TRUE(call(mod, "wait_for_id",
                     {ScriptValue::fromString("no-such"), ScriptValue::fromInt(50)}).isNull());
    EXPECT_TRUE(call(mod, "wait_for_role",
                     {ScriptValue::fromInt(0), ScriptValue::fromInt(50)}).isNull());
    // wait_for 选择器对象全字段 + 非对象首参 + 缺省超时
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
    EXPECT_TRUE(call(mod, "wait_for",
                     {ScriptValue::fromString("not-object"), ScriptValue::fromInt(50)}).isNull());
    EXPECT_TRUE(call(mod, "wait_for", {ScriptValue::fromObject({})}).isNull());
}

TEST(UiaModuleCoverageTest, FromAndTreeBranches) {
    ModuleDescriptor mod = getModule("uia");
    ASSERT_FALSE(mod.name.empty());

    // from_foreground / from_point / from_window：无焦点/无效 → null
    EXPECT_TRUE(call(mod, "from_foreground").isNull());
    EXPECT_TRUE(call(mod, "from_point", {ScriptValue::fromInt(10), ScriptValue::fromInt(10)}).isNull());
    EXPECT_TRUE(call(mod, "from_point").isNull()); // 缺参默认 0,0
    EXPECT_TRUE(call(mod, "from_window", {ScriptValue::fromInt(0)}).isNull());
    EXPECT_TRUE(call(mod, "from_window").isNull());

    // tree 分支矩阵：整型首参 / null 首参 / 对象含 _handle / 对象不含 / 深度钳制
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::fromInt(3)}).isNull());          // 整型 → maxDepth=3
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::fromInt(0)}).isNull());          // 钳到 1
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::fromInt(99)}).isNull());         // 钳到 16
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::null()}).isNull());              // 非对象非整型 → 默认 6
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::null(), ScriptValue::fromInt(2)}).isNull()); // args[1] 深度
    EXPECT_TRUE(call(mod, "tree", {ScriptValue::fromObject({})}).isNull());      // 对象无 _handle → 根走焦点
    EXPECT_TRUE(call(mod, "tree", {fakeElement(), ScriptValue::fromInt(4)}).isNull()); // 无效 handle → 根走焦点
    EXPECT_TRUE(call(mod, "tree").isNull());                                      // 无参
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
