#include "script_function_test_helpers.hpp"

// ========== Screen module functions (stubs/platform) ==========

TEST(ScreenModuleFunctionsTest, CaptureReturnsBool) {
    auto fn = findFunction("screen", "capture");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
}

TEST(ScreenModuleFunctionsTest, CaptureRegionWithObject) {
    auto fn = findFunction("screen", "captureRegion");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({region});
    EXPECT_TRUE(result.isBool());
}

TEST(ScreenModuleFunctionsTest, GetPixelReturnsObject) {
    auto fn = findFunction("screen", "getPixel");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0), ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isObject());
}

TEST(ScreenModuleFunctionsTest, FindColorReturnsArray) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({ScriptValue::fromInt(0xFF0000), region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorsReturnsArray) {
    auto fn = findFunction("screen", "findColors");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({ScriptValue::fromInt(0xFF0000), region, ScriptValue::fromInt(10)});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorWithObjectColor) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(255)},
        {"g", ScriptValue::fromInt(0)},
        {"b", ScriptValue::fromInt(0)}
    });
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({color, region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorsWithObjectColor) {
    auto fn = findFunction("screen", "findColors");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(0)},
        {"g", ScriptValue::fromInt(255)},
        {"b", ScriptValue::fromInt(0)},
        {"a", ScriptValue::fromInt(255)}
    });
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({color, region, ScriptValue::fromInt(10)});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorWithStringColorFallback) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    // String color triggers toColor fallback (not int, not object)
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(10)},
        {"height", ScriptValue::fromInt(10)}
    });
    auto result = fn({ScriptValue::fromString("red"), region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, GetScreenWidth) {
    auto fn = findFunction("screen", "getScreenWidth");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(ScreenModuleFunctionsTest, GetScreenHeight) {
    auto fn = findFunction("screen", "getScreenHeight");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(ScreenModuleFunctionsTest, FindImageReturnsArray) {
    auto fn = findFunction("screen", "findImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isArray());
}


// ========== Window module functions (stubs/platform) ==========

TEST(WindowModuleFunctionsTest, FindReturnsResults) {
    auto fn = findFunction("window", "find");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow")});
    EXPECT_TRUE(result.isArray() || result.isBool());
}

TEST(WindowModuleFunctionsTest, GetForegroundReturnsInt) {
    auto fn = findFunction("window", "getForeground");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(WindowModuleFunctionsTest, GetTitleReturnsString) {
    auto fn = findFunction("window", "getTitle");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isString());
}

TEST(WindowModuleFunctionsTest, GetBoundsReturnsObject) {
    auto fn = findFunction("window", "getBounds");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isObject());
}

TEST(WindowModuleFunctionsTest, ActivateReturnsBool) {
    auto fn = findFunction("window", "activate");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isBool());
}

TEST(WindowModuleFunctionsTest, WaitForReturnsBool) {
    auto fn = findFunction("window", "waitFor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow"), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}


// ========== Vision module functions (stubs/platform) ==========

TEST(VisionModuleFunctionsTest, FindColorReturnsNull) {
    auto fn = findFunction("vision", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(VisionModuleFunctionsTest, FindColorWithObjectColor) {
    auto fn = findFunction("vision", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(255)},
        {"g", ScriptValue::fromInt(0)},
        {"b", ScriptValue::fromInt(0)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(VisionModuleFunctionsTest, FindAllColorsReturnsArray) {
    auto fn = findFunction("vision", "findAllColors");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isArray());
}

TEST(VisionModuleFunctionsTest, FindAllColorsWithObjectColor) {
    auto fn = findFunction("vision", "findAllColors");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(0)},
        {"g", ScriptValue::fromInt(128)},
        {"b", ScriptValue::fromInt(255)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isArray());
}

TEST(VisionModuleFunctionsTest, HasColorReturnsBool) {
    auto fn = findFunction("vision", "hasColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isBool());
}

TEST(VisionModuleFunctionsTest, HasColorWithObjectColor) {
    auto fn = findFunction("vision", "hasColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(100)},
        {"g", ScriptValue::fromInt(100)},
        {"b", ScriptValue::fromInt(100)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isBool());
}

TEST(VisionModuleFunctionsTest, GetDominantColorReturnsObject) {
    auto fn = findFunction("vision", "getDominantColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(VisionModuleFunctionsTest, FindImageReturnsObject) {
    auto fn = findFunction("vision", "findImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isObject() || result.isNull());
}


// ========== Plan 6: UiaModuleFunctionsTest ==========

TEST(UiaModuleFunctionsTest, NonEventFunctionsRegistered) {
	// 10 个非事件函数全部注册
	EXPECT_FALSE(findFunction("uia", "from_foreground").name.empty());
	EXPECT_FALSE(findFunction("uia", "from_point").name.empty());
	EXPECT_FALSE(findFunction("uia", "from_window").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_by_name").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_by_id").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_all_by_control_type").name.empty());
	EXPECT_FALSE(findFunction("uia", "wait_for_name").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_button").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_edit").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_text").name.empty());
}

TEST(UiaModuleFunctionsTest, FindByNameReturnsNullOrObject) {
	auto fn = findFunction("uia", "find_by_name");
	auto result = fn.func({ScriptValue::fromString("nonexistent_element")});
	// 无辅助功能权限：null；有权限：Object 或 null（找不到）
	EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(UiaModuleFunctionsTest, FromForegroundReturnsNullOrObject) {
	auto fn = findFunction("uia", "from_foreground");
	auto result = fn.func({});
	EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(UiaModuleFunctionsTest, FindAllByControlTypeReturnsArray) {
	auto fn = findFunction("uia", "find_all_by_control_type");
	auto result = fn.func({ScriptValue::fromInt(static_cast<int64_t>(UIARole::Button))});
	EXPECT_TRUE(result.isArray());
	// 数组元素（若有）应为 Object 或 null
	for (size_t i = 0; i < result.size(); ++i) {
		EXPECT_TRUE(result.at(i).isNull() || result.at(i).isObject());
	}
}

TEST(UiaModuleFunctionsTest, WaitForNameShortTimeoutNoCrash) {
	auto fn = findFunction("uia", "wait_for_name");
	EXPECT_NO_THROW(fn.func({ScriptValue::fromString("x"), ScriptValue::fromInt(50)}));
}

TEST(UiaModuleFunctionsTest, FindButtonAndEditReturnNullOrObject) {
	auto fnBtn = findFunction("uia", "find_button");
	auto r1 = fnBtn.func({ScriptValue::fromString("OK")});
	EXPECT_TRUE(r1.isNull() || r1.isObject());
	auto fnEdit = findFunction("uia", "find_edit");
	auto r2 = fnEdit.func({ScriptValue::fromString("username")});
	EXPECT_TRUE(r2.isNull() || r2.isObject());
}

TEST(UiaModuleFunctionsTest, UIElementObjectStructureWhenAvailable) {
	// 有辅助功能权限时验证 OO 对象结构；无权限则跳过（不失败）
	if (!uia().initialize()) {
		SUCCEED() << "无辅助功能权限，跳过 OO 结构验证";
		return;
	}
	auto fn = findFunction("uia", "from_foreground");
	auto root = fn.func({});
	if (root.isNull()) {
		SUCCEED() << "无前台元素，跳过";
		return;
	}
	ASSERT_TRUE(root.isObject());
	ASSERT_TRUE(root.get("_handle") != nullptr);
	EXPECT_TRUE(root.get("_handle")->isInt());
	EXPECT_GT(root.get("_handle")->asInt(0), 0); // handle >= 1
	// 12 方法均为 Callable
	const std::vector<std::string> methods = {
		"get_info", "click", "double_click", "focus", "get_value", "set_value",
		"get_children", "expand", "collapse", "is_expanded", "is_visible", "is_enabled"
	};
	for (const auto& m : methods) {
		const ScriptValue* method = root.get(m);
		ASSERT_TRUE(method != nullptr) << "缺少方法: " << m;
		EXPECT_TRUE(method->isCallable()) << "方法非 Callable: " << m;
	}
	// 调用 get_info 验证返回 Object（或 null，若元素失效）
	auto info = root.get("get_info")->call({});
	EXPECT_TRUE(info.isObject() || info.isNull());
}


// ========== Plan 6 Phase 2: 事件函数 ==========

TEST(UiaModuleFunctionsTest, EventFunctionsRegistered) {
	// 3 个事件函数已注册
	EXPECT_FALSE(findFunction("uia", "on_property_changed").name.empty());
	EXPECT_FALSE(findFunction("uia", "on_structure_changed").name.empty());
	EXPECT_FALSE(findFunction("uia", "remove_event_listener").name.empty());
}

TEST(UiaModuleFunctionsTest, OnPropertyChangedRejectsNonThreadSafeCallable) {
	// 默认 callable 非线程安全（callableThreadSafe=false）→ 被 callableThreadSafe 门控拒绝，返回 0
	auto fn = findFunction("uia", "on_property_changed");
	auto result = fn.func({
		ScriptValue::fromString("test"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); })  // 默认 threadSafe=false
	});
	EXPECT_EQ(result.asInt(-1), 0);
}

TEST(UiaModuleFunctionsTest, OnPropertyChangedAcceptsThreadSafeCallable) {
	// callableThreadSafe=true 通过门控（不因 callableThreadSafe 被拒）。uia() 是全局单例，
	// 可能被前序测试 initialize，故只断言返回 Int 类型（不崩溃、未拒绝），不断言具体值。
	auto result = findFunction("uia", "on_property_changed").func({
		ScriptValue::fromString("test"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, true)  // 线程安全
	});
	EXPECT_TRUE(result.isInt());
}

TEST(UiaModuleFunctionsTest, RemoveEventListenerInvalidIdReturnsFalse) {
	// 无效 listenerId 返回 false（后端找不到该监听器）
	auto fn = findFunction("uia", "remove_event_listener");
	auto result = fn.func({ScriptValue::fromInt(static_cast<int64_t>(99999))});
	EXPECT_FALSE(result.asBool());
}

