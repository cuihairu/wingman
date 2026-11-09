#include "script_function_test_helpers.hpp"

// ========== System module functions ==========

TEST(SystemModuleFunctionsTest, GetCpuInfo) {
    auto fn = findFunction("system", "getCpuInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    // Should have 'cores' key
    auto* cores = result.get("cores");
    ASSERT_NE(cores, nullptr);
    EXPECT_GT(cores->asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetMemoryInfo) {
    auto fn = findFunction("system", "getMemoryInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    auto* total = result.get("total");
    ASSERT_NE(total, nullptr);
    EXPECT_GT(total->asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetOsInfo) {
    auto fn = findFunction("system", "getOsInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    auto* platform = result.get("platform");
    ASSERT_NE(platform, nullptr);
    EXPECT_FALSE(platform->asString().empty());
}

TEST(SystemModuleFunctionsTest, GetUptime) {
    auto fn = findFunction("system", "getUptime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetDateTime) {
    auto fn = findFunction("system", "getDateTime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
    EXPECT_FALSE(result.asString().empty());
}

TEST(SystemModuleFunctionsTest, GetTimeZone) {
    auto fn = findFunction("system", "getTimeZone");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
}

TEST(SystemModuleFunctionsTest, GetProcessCount) {
    auto fn = findFunction("system", "getProcessCount");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetThreadCount) {
    auto fn = findFunction("system", "getThreadCount");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetGpuInfo) {
    auto fn = findFunction("system", "getGpuInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetDiskInfoDefault) {
    auto fn = findFunction("system", "getDiskInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetDisplayInfo) {
    auto fn = findFunction("system", "getDisplayInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetNetworkAdapters) {
    auto fn = findFunction("system", "getNetworkAdapters");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}


// ========== Human module functions (stubs/platform) ==========

TEST(HumanModuleFunctionsTest, MouseMoveDoesNotCrash) {
    auto fn = findFunction("human", "mouse_move");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_click");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseRightClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_rightClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseDoubleClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_doubleClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseDragDoesNotCrash) {
    auto fn = findFunction("human", "mouse_drag");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100)
    }));
}

TEST(HumanModuleFunctionsTest, MouseScrollDoesNotCrash) {
    auto fn = findFunction("human", "mouse_scroll");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}

TEST(HumanModuleFunctionsTest, KeyboardPressDoesNotCrash) {
    auto fn = findFunction("human", "keyboard_press");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(65)}));
}

TEST(HumanModuleFunctionsTest, KeyboardDownUpDoesNotCrash) {
    auto downFn = findFunction("human", "keyboard_down");
    auto upFn = findFunction("human", "keyboard_up");
    ASSERT_FALSE(downFn.name.empty());
    EXPECT_NO_THROW(downFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(upFn({ScriptValue::fromInt(65)}));
}

TEST(HumanModuleFunctionsTest, KeyboardTypeDoesNotCrash) {
    auto fn = findFunction("human", "keyboard_type");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hello")}));
}


// ========== Plan 5: human 高层 API（文档声明） ==========

TEST(HumanModuleFunctionsTest, GetConfigReturnsHighLevelFields) {
    auto fn = findFunction("human", "getConfig");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    // 文档承诺的 4 个扁平字段（human.md:359）
    EXPECT_NE(result.get("delay_min"), nullptr);
    EXPECT_NE(result.get("delay_max"), nullptr);
    EXPECT_NE(result.get("move_speed"), nullptr);
    EXPECT_NE(result.get("typing_variance"), nullptr);
}

TEST(HumanModuleFunctionsTest, SetConfigUpdatesDelayRangeRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setConfig");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    ASSERT_FALSE(getFn.name.empty());

    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_min"), ScriptValue::fromInt(40)}).isNull());
    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_max"), ScriptValue::fromInt(120)}).isNull());

    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 40);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 120);
}

TEST(HumanModuleFunctionsTest, SetConfigUnknownKeyDoesNotCrash) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto fn = findFunction("human", "setConfig");
    ASSERT_FALSE(fn.name.empty());
    // 未知 key 应静默忽略（脚本层惯例），不抛异常
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_key"), ScriptValue::fromInt(1)}));
}


// ========== Input module functions (stubs/platform) ==========

TEST(InputModuleFunctionsTest, ClickDoesNotCrash) {
    auto fn = findFunction("input", "click");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(InputModuleFunctionsTest, MoveDoesNotCrash) {
    auto fn = findFunction("input", "move");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(InputModuleFunctionsTest, ScrollDoesNotCrash) {
    auto fn = findFunction("input", "scroll");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}

TEST(InputModuleFunctionsTest, KeyDownUpDoesNotCrash) {
    auto downFn = findFunction("input", "keyDown");
    auto upFn = findFunction("input", "keyUp");
    auto keyFn = findFunction("input", "key");
    ASSERT_FALSE(downFn.name.empty());
    EXPECT_NO_THROW(downFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(upFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(keyFn({ScriptValue::fromInt(65)}));
}

TEST(InputModuleFunctionsTest, TypeDoesNotCrash) {
    auto fn = findFunction("input", "type");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hello")}));
}

TEST(InputModuleFunctionsTest, DelayDoesNotCrash) {
    auto fn = findFunction("input", "delay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1)}));
}

TEST(InputModuleFunctionsTest, RandomDelayDoesNotCrash) {
    auto fn = findFunction("input", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}));
}

TEST(InputModuleFunctionsTest, RandomDelaySwapsWhenMaxLessThanMin) {
    auto fn = findFunction("input", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    // max < min triggers std::swap branch
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}


// ========== Process module functions (stubs/platform) ==========

TEST(ProcessModuleFunctionsTest, FindReturnsResults) {
    auto fn = findFunction("process", "find");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_process")});
    EXPECT_TRUE(result.isArray() || result.isBool());
}

TEST(ProcessModuleFunctionsTest, StartReturnsInt) {
    auto fn = findFunction("process", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.exe")});
    EXPECT_TRUE(result.isInt());
}

TEST(ProcessModuleFunctionsTest, ExistsReturnsBool) {
    auto fn = findFunction("process", "exists");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, WaitReturnsBool) {
    auto fn = findFunction("process", "wait");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, TerminateReturnsBool) {
    auto fn = findFunction("process", "terminate");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, WaitForReturnsBool) {
    auto fn = findFunction("process", "waitFor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_process"), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}


// ========== GameProfile module functions (stubs) ==========

TEST(GameProfileModuleFunctionsTest, ListReturnsArray) {
    auto fn = findFunction("gameprofile", "list");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(GameProfileModuleFunctionsTest, LoadReturnsBool) {
    auto fn = findFunction("gameprofile", "load");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, SaveReturnsBool) {
    auto fn = findFunction("gameprofile", "save");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, GetReturnsStrings) {
    auto fn = findFunction("gameprofile", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}

TEST(GameProfileModuleFunctionsTest, GetActiveReturnsStrings) {
    auto fn = findFunction("gameprofile", "getActive");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}

TEST(GameProfileModuleFunctionsTest, SetActiveReturnsBool) {
    auto fn = findFunction("gameprofile", "setActive");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, FindByWindowReturnsStrings) {
    auto fn = findFunction("gameprofile", "findByWindow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow")});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}


