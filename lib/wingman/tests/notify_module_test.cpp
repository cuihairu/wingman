#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createNotifyModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findNotifyFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) return fn;
	}
	return {};
}

} // namespace

TEST(NotifyModuleTest, HasExpectedFunctions) {
	auto mod = createNotifyModule();
	EXPECT_EQ(mod.name, "notify");
	ASSERT_GE(mod.functions.size(), 7u);

	bool hasDebug = false, hasInfo = false, hasWarn = false, hasError = false;
	bool hasToast = false, hasWebhook = false, hasBridge = false;
	for (const auto& fn : mod.functions) {
		if (fn.name == "debug") hasDebug = true;
		if (fn.name == "info") hasInfo = true;
		if (fn.name == "warn") hasWarn = true;
		if (fn.name == "error") hasError = true;
		if (fn.name == "toast") hasToast = true;
		if (fn.name == "webhook") hasWebhook = true;
		if (fn.name == "bridge") hasBridge = true;
	}
	EXPECT_TRUE(hasDebug);
	EXPECT_TRUE(hasInfo);
	EXPECT_TRUE(hasWarn);
	EXPECT_TRUE(hasError);
	EXPECT_TRUE(hasToast);
	EXPECT_TRUE(hasWebhook);
	EXPECT_TRUE(hasBridge);
}

TEST(NotifyModuleTest, DebugReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("test debug message")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugWithMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with meta"),
		ScriptValue::fromObject({{"key", ScriptValue::fromString("value")}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugNonStringArgReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromInt(42)});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, InfoReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "info");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("test info message")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, InfoWithMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "info");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("info with meta"),
		ScriptValue::fromObject({{"count", ScriptValue::fromInt(10)}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, InfoEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "info");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WarnReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "warn");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("test warn message")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WarnWithMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "warn");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("warn with meta"),
		ScriptValue::fromObject({{"level", ScriptValue::fromInt(2)}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WarnEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "warn");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ErrorReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "error");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("test error message")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ErrorWithMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "error");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("error with meta"),
		ScriptValue::fromObject({{"code", ScriptValue::fromInt(500)}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ErrorEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "error");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ToastReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "toast");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("Title"),
		ScriptValue::fromString("Toast message")
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ToastWithLevel) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "toast");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("Title"),
		ScriptValue::fromString("Toast message"),
		ScriptValue::fromString("warning")
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ToastEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "toast");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ToastInsufficientArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "toast");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("only one arg")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WebhookReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "webhook");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("https://example.com/hook")});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WebhookWithPayload) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "webhook");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("https://example.com/hook"),
		ScriptValue::fromObject({{"data", ScriptValue::fromString("test")}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WebhookEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "webhook");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, BridgeReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "bridge");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("source.event"),
		ScriptValue::fromString("event://target.event")
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, BridgeWithHttpTarget) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "bridge");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("source.event"),
		ScriptValue::fromString("https://example.com/webhook"),
		ScriptValue::fromObject({{"transform", ScriptValue::fromObject({{"key", ScriptValue::fromString("val")}})}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, BridgeEmptyArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "bridge");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, BridgeInsufficientArgsReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "bridge");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromString("only one")});
	EXPECT_TRUE(result.isNull());
}

// ========== toJson Coverage Tests ==========

TEST(NotifyModuleTest, DebugWithArrayMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with array"),
		ScriptValue::fromObject({{"items", ScriptValue::fromArray({
			ScriptValue::fromInt(1),
			ScriptValue::fromString("two"),
			ScriptValue::fromBool(true)
		})}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugWithFloatMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with float"),
		ScriptValue::fromObject({{"pi", ScriptValue::fromFloat(3.14)}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugWithBoolMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with bool"),
		ScriptValue::fromObject({{"active", ScriptValue::fromBool(true)}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugWithNullMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with null"),
		ScriptValue::fromObject({{"data", ScriptValue::null()}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, DebugWithNestedArrayMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("nested"),
		ScriptValue::fromObject({{"matrix", ScriptValue::fromArray({
			ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}),
			ScriptValue::fromArray({ScriptValue::fromInt(3), ScriptValue::fromInt(4)})
		})}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, InfoWithNullMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "info");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("info with null"),
		ScriptValue::fromObject({{"nothing", ScriptValue::null()}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, WarnWithArrayMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "warn");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("warn with array"),
		ScriptValue::fromObject({{"errors", ScriptValue::fromArray({
			ScriptValue::fromString("err1"),
			ScriptValue::fromString("err2")
		})}})
	});
	EXPECT_TRUE(result.isNull());
}

TEST(NotifyModuleTest, ErrorWithBoolMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "error");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("error with bool"),
		ScriptValue::fromObject({{"fatal", ScriptValue::fromBool(false)}})
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Bridge with event:// target ==========

TEST(NotifyModuleTest, BridgeWithEventTarget) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "bridge");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("source.event"),
		ScriptValue::fromString("event://target.event"),
		ScriptValue::fromObject({{"transform", ScriptValue::fromObject({{"key", ScriptValue::fromString("val")}})}})
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Webhook with options ==========

TEST(NotifyModuleTest, WebhookWithOptions) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "webhook");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("https://example.com/hook"),
		ScriptValue::fromObject({{"data", ScriptValue::fromString("test")}}),
		ScriptValue::fromObject({{"timeout", ScriptValue::fromInt(5000)}})
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Debug with Object Meta containing all types ==========

TEST(NotifyModuleTest, DebugWithObjectMetaAllTypes) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "debug");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("debug with object"),
		ScriptValue::fromObject({
			{"nested", ScriptValue::fromObject({{"a", ScriptValue::fromInt(1)}})},
			{"flag", ScriptValue::fromBool(true)},
			{"ratio", ScriptValue::fromFloat(2.5)},
			{"nothing", ScriptValue::null()},
			{"items", ScriptValue::fromArray({ScriptValue::fromString("x")})}
		})
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Toast with non-string level ==========

TEST(NotifyModuleTest, ToastWithNonStringLevelUsesDefault) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "toast");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("Title"),
		ScriptValue::fromString("Message"),
		ScriptValue::fromInt(42)  // non-string level, should use default "info"
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Warn with null meta ==========

TEST(NotifyModuleTest, WarnWithNullMeta) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "warn");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({
		ScriptValue::fromString("warn msg"),
		ScriptValue::null()
	});
	EXPECT_TRUE(result.isNull());
}

// ========== Error with non-string arg ==========

TEST(NotifyModuleTest, ErrorWithNonStringArgReturnsNull) {
	auto mod = createNotifyModule();
	auto fn = findNotifyFunction(mod, "error");
	ASSERT_FALSE(fn.name.empty());

	auto result = fn({ScriptValue::fromInt(500)});
	EXPECT_TRUE(result.isNull());
}
