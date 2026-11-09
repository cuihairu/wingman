#include "event_module_test_common.hpp"

// ========== listener / listeners 查询与按事件名清理 ==========

TEST(EventModuleTest, ListenerByIdReturnsDescriptor) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenerFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listener") { listenerFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenerFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	auto subId = (*onFunc)({
		ScriptValue::fromString("test.listener.byid"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("my-handler")
	});
	ASSERT_TRUE(subId.isInt());

	auto result = (*listenerFunc)({subId});
	ASSERT_TRUE(result.isObject());
	auto* id = result.get("id");
	ASSERT_NE(id, nullptr);
	EXPECT_EQ(id->asInt(), subId.asInt());
	auto* type = result.get("type");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->asString(), "test.listener.byid");
	auto* name = result.get("name");
	ASSERT_NE(name, nullptr);
	EXPECT_EQ(name->asString(), "my-handler");
	auto* once = result.get("once");
	ASSERT_NE(once, nullptr);
	EXPECT_FALSE(once->asBool());

	// 未知订阅 ID 返回 nil
	auto missing = (*listenerFunc)({ScriptValue::fromInt(999999)});
	EXPECT_TRUE(missing.isNull());

	hub.clear();
}

TEST(EventModuleTest, ListenerByIdReportsOnceAndAnonymousName) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onceFunc = nullptr;
	const ScriptFunction* listenerFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") { onceFunc = &fn.func; }
		if (fn.name == "listener") { listenerFunc = &fn.func; }
	}
	ASSERT_NE(onceFunc, nullptr);
	ASSERT_NE(listenerFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	auto subId = (*onceFunc)({
		ScriptValue::fromString("test.listener.once"),
		ScriptValue::fromCallable(callback)
	});
	ASSERT_TRUE(subId.isInt());

	auto result = (*listenerFunc)({subId});
	ASSERT_TRUE(result.isObject());
	auto* name = result.get("name");
	ASSERT_NE(name, nullptr);
	EXPECT_EQ(name->asString(), "");
	auto* once = result.get("once");
	ASSERT_NE(once, nullptr);
	EXPECT_TRUE(once->asBool());

	hub.clear();
}

TEST(EventModuleTest, ListenerByNameReturnsDescriptor) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenerFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listener") { listenerFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenerFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.listener.byname"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("find-me")
	});

	auto result = (*listenerFunc)({ScriptValue::fromString("find-me")});
	ASSERT_TRUE(result.isObject());
	auto* type = result.get("type");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->asString(), "test.listener.byname");
	auto* name = result.get("name");
	ASSERT_NE(name, nullptr);
	EXPECT_EQ(name->asString(), "find-me");

	// 未知名称返回 nil
	auto missing = (*listenerFunc)({ScriptValue::fromString("no-such-handler")});
	EXPECT_TRUE(missing.isNull());

	hub.clear();
}

TEST(EventModuleTest, ListenerByNameDuplicateReturnsEarliest) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenerFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listener") { listenerFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenerFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	auto firstId = (*onFunc)({
		ScriptValue::fromString("test.dup.first"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("dup-handler")
	});
	(*onFunc)({
		ScriptValue::fromString("test.dup.second"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("dup-handler")
	});

	// 同名订阅取最早注册者，结果确定
	auto result = (*listenerFunc)({ScriptValue::fromString("dup-handler")});
	ASSERT_TRUE(result.isObject());
	auto* id = result.get("id");
	ASSERT_NE(id, nullptr);
	EXPECT_EQ(id->asInt(), firstId.asInt());
	auto* type = result.get("type");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->asString(), "test.dup.first");

	hub.clear();
}

TEST(EventModuleTest, ListenerBadArgsReturnFalse) {
	auto mod = createEventModule();
	const ScriptFunction* listenerFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "listener") { listenerFunc = &fn.func; break; }
	}
	ASSERT_NE(listenerFunc, nullptr);

	// 缺参：与 on/off 的错误约定一致返回 false
	auto noArgs = (*listenerFunc)({});
	EXPECT_TRUE(noArgs.isBool());
	EXPECT_FALSE(noArgs.asBool());

	// 不支持的参数类型（既非 ID 也非名称）
	auto badType = (*listenerFunc)({ScriptValue::fromBool(true)});
	EXPECT_TRUE(badType.isBool());
	EXPECT_FALSE(badType.asBool());
}

TEST(EventModuleTest, ListenersReturnsRegisteredListeners) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* onceFunc = nullptr;
	const ScriptFunction* listenersFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "once") { onceFunc = &fn.func; }
		if (fn.name == "listeners") { listenersFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(onceFunc, nullptr);
	ASSERT_NE(listenersFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.listeners.many"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("first")
	});
	(*onFunc)({
		ScriptValue::fromString("test.listeners.many"),
		ScriptValue::fromCallable(callback)
	});
	(*onceFunc)({
		ScriptValue::fromString("test.listeners.many"),
		ScriptValue::fromCallable(callback)
	});
	(*onFunc)({
		ScriptValue::fromString("test.listeners.other"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("elsewhere")
	});

	auto result = (*listenersFunc)({ScriptValue::fromString("test.listeners.many")});
	ASSERT_TRUE(result.isArray());
	ASSERT_EQ(result.arrayVal.size(), 3u);
	// 按订阅 ID 升序（注册顺序）
	EXPECT_EQ(result.arrayVal[0].get("name")->asString(), "first");
	EXPECT_FALSE(result.arrayVal[0].get("once")->asBool());
	EXPECT_EQ(result.arrayVal[1].get("name")->asString(), "");
	EXPECT_TRUE(result.arrayVal[2].get("once")->asBool());
	for (const auto& item : result.arrayVal) {
		EXPECT_EQ(item.get("type")->asString(), "test.listeners.many");
	}

	// 其他事件不受影响
	auto others = (*listenersFunc)({ScriptValue::fromString("test.listeners.other")});
	ASSERT_TRUE(others.isArray());
	ASSERT_EQ(others.arrayVal.size(), 1u);
	EXPECT_EQ(others.arrayVal[0].get("name")->asString(), "elsewhere");

	hub.clear();
}

TEST(EventModuleTest, ListenersUnknownTypeReturnsEmptyArray) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* listenersFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "listeners") { listenersFunc = &fn.func; break; }
	}
	ASSERT_NE(listenersFunc, nullptr);

	auto result = (*listenersFunc)({ScriptValue::fromString("never.subscribed")});
	EXPECT_TRUE(result.isArray());
	EXPECT_TRUE(result.arrayVal.empty());

	hub.clear();
}

TEST(EventModuleTest, ListenersBadArgsReturnFalse) {
	auto mod = createEventModule();
	const ScriptFunction* listenersFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "listeners") { listenersFunc = &fn.func; break; }
	}
	ASSERT_NE(listenersFunc, nullptr);

	auto noArgs = (*listenersFunc)({});
	EXPECT_TRUE(noArgs.isBool());
	EXPECT_FALSE(noArgs.asBool());

	auto badType = (*listenersFunc)({ScriptValue::fromInt(42)});
	EXPECT_TRUE(badType.isBool());
	EXPECT_FALSE(badType.asBool());
}

TEST(EventModuleTest, ClearByTypeRemovesOnlyThatEvent) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	const ScriptFunction* listenersFunc = nullptr;
	const ScriptFunction* clearFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
		if (fn.name == "listeners") { listenersFunc = &fn.func; }
		if (fn.name == "clear") { clearFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);
	ASSERT_NE(listenersFunc, nullptr);
	ASSERT_NE(clearFunc, nullptr);

	int countA = 0, countB = 0;
	auto callbackA = [&countA](const std::vector<ScriptValue>&) -> ScriptValue {
		countA++;
		return ScriptValue::null();
	};
	auto callbackB = [&countB](const std::vector<ScriptValue>&) -> ScriptValue {
		countB++;
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.clear.a"),
		ScriptValue::fromCallable(callbackA),
		ScriptValue::fromString("a1")
	});
	(*onFunc)({
		ScriptValue::fromString("test.clear.a"),
		ScriptValue::fromCallable(callbackA)
	});
	(*onFunc)({
		ScriptValue::fromString("test.clear.b"),
		ScriptValue::fromCallable(callbackB),
		ScriptValue::fromString("b1")
	});

	// 按事件名清理：返回 nil，只影响该事件
	auto result = (*clearFunc)({ScriptValue::fromString("test.clear.a")});
	EXPECT_TRUE(result.isNull());

	auto remainingA = (*listenersFunc)({ScriptValue::fromString("test.clear.a")});
	EXPECT_TRUE(remainingA.isArray());
	EXPECT_TRUE(remainingA.arrayVal.empty());
	auto remainingB = (*listenersFunc)({ScriptValue::fromString("test.clear.b")});
	ASSERT_EQ(remainingB.arrayVal.size(), 1u);
	EXPECT_EQ(remainingB.arrayVal[0].get("name")->asString(), "b1");

	(*emitFunc)({ScriptValue::fromString("test.clear.a")});
	EXPECT_EQ(countA, 0);
	(*emitFunc)({ScriptValue::fromString("test.clear.b")});
	EXPECT_EQ(countB, 1);

	// 清理未注册的事件：无副作用
	auto missing = (*clearFunc)({ScriptValue::fromString("never.subscribed")});
	EXPECT_TRUE(missing.isNull());

	hub.clear();
}

TEST(EventModuleTest, ClearNoArgsStillClearsAll) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenersFunc = nullptr;
	const ScriptFunction* clearFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listeners") { listenersFunc = &fn.func; }
		if (fn.name == "clear") { clearFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenersFunc, nullptr);
	ASSERT_NE(clearFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.clearall.x"),
		ScriptValue::fromCallable(callback)
	});
	(*onFunc)({
		ScriptValue::fromString("test.clearall.y"),
		ScriptValue::fromCallable(callback)
	});

	auto result = (*clearFunc)({});
	EXPECT_TRUE(result.isNull());

	EXPECT_TRUE((*listenersFunc)({ScriptValue::fromString("test.clearall.x")}).arrayVal.empty());
	EXPECT_TRUE((*listenersFunc)({ScriptValue::fromString("test.clearall.y")}).arrayVal.empty());

	hub.clear();
}

TEST(EventModuleTest, ClearWithNullArgClearsAll) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenersFunc = nullptr;
	const ScriptFunction* clearFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listeners") { listenersFunc = &fn.func; }
		if (fn.name == "clear") { clearFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenersFunc, nullptr);
	ASSERT_NE(clearFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.clearnull.x"),
		ScriptValue::fromCallable(callback)
	});

	// Lua 的 clear(nil) 与 clear() 等价：全量清理
	auto result = (*clearFunc)({ScriptValue::null()});
	EXPECT_TRUE(result.isNull());
	EXPECT_TRUE((*listenersFunc)({ScriptValue::fromString("test.clearnull.x")}).arrayVal.empty());

	hub.clear();
}

TEST(EventModuleTest, ClearWithBadArgTypeReturnsFalse) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* listenersFunc = nullptr;
	const ScriptFunction* clearFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "listeners") { listenersFunc = &fn.func; }
		if (fn.name == "clear") { clearFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(listenersFunc, nullptr);
	ASSERT_NE(clearFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	(*onFunc)({
		ScriptValue::fromString("test.clearbad.x"),
		ScriptValue::fromCallable(callback)
	});

	// 非字符串参数是用法错误：返回 false，不做静默全量清理
	auto result = (*clearFunc)({ScriptValue::fromInt(123)});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());

	// 原有监听不受影响
	EXPECT_EQ((*listenersFunc)({ScriptValue::fromString("test.clearbad.x")}).arrayVal.size(), 1u);

	hub.clear();
}
