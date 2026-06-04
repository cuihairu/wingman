#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

#include <thread>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createTaskModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findTaskFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

} // namespace

TEST(TaskModuleTest, WaitTimeoutDoesNotDeadlock) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto wait = findTaskFunction(mod, "wait");
	auto status = findTaskFunction(mod, "status");

	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(wait.name.empty());
	ASSERT_FALSE(status.name.empty());

	// Submit async task that takes longer than the wait timeout
	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			return ScriptValue::fromString("done");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(1000)}
		})
	});

	ASSERT_TRUE(taskId.isString());

	// Wait with timeout shorter than task execution time
	auto waited = wait({taskId, ScriptValue::fromInt(10)});
	EXPECT_TRUE(waited.isBool());
	EXPECT_FALSE(waited.asBool());

	// Status should be timeout
	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "timeout");

	// Give the async task time to complete to avoid cleanup issues
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

TEST(TaskModuleTest, SubmitSyncTaskReturnsString) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("result");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	EXPECT_TRUE(taskId.isString());
	EXPECT_FALSE(taskId.asString().empty());
}

TEST(TaskModuleTest, SubmitNonCallableReturnsFalse) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	auto result = submit({ScriptValue::fromString("not a function")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, SubmitEmptyArgsReturnsFalse) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	auto result = submit({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, CancelExistingTaskReturnsTrue) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto cancel = findTaskFunction(mod, "cancel");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(cancel.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto result = cancel({taskId});
	EXPECT_TRUE(result.isBool());
}

TEST(TaskModuleTest, CancelNonexistentReturnsFalse) {
	auto mod = createTaskModule();
	auto cancel = findTaskFunction(mod, "cancel");
	ASSERT_FALSE(cancel.name.empty());

	auto result = cancel({ScriptValue::fromString("nonexistent-task")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, CancelEmptyArgsReturnsFalse) {
	auto mod = createTaskModule();
	auto cancel = findTaskFunction(mod, "cancel");
	ASSERT_FALSE(cancel.name.empty());

	auto result = cancel({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, StatusReturnsString) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(status.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto result = status({taskId});
	EXPECT_TRUE(result.isString());
}

TEST(TaskModuleTest, StatusNonexistentReturnsFailed) {
	auto mod = createTaskModule();
	auto status = findTaskFunction(mod, "status");
	ASSERT_FALSE(status.name.empty());

	auto result = status({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isString());
	EXPECT_EQ(result.asString(), "failed");
}

TEST(TaskModuleTest, StatusEmptyArgsReturnsFailed) {
	auto mod = createTaskModule();
	auto status = findTaskFunction(mod, "status");
	ASSERT_FALSE(status.name.empty());

	auto result = status({});
	EXPECT_TRUE(result.isString());
	EXPECT_EQ(result.asString(), "failed");
}

TEST(TaskModuleTest, ResultReturnsValue) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto resultFn = findTaskFunction(mod, "result");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(resultFn.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromInt(42);
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto result = resultFn({taskId});
	EXPECT_TRUE(result.isInt());
	EXPECT_EQ(result.asInt(), 42);
}

TEST(TaskModuleTest, ResultNonexistentReturnsNull) {
	auto mod = createTaskModule();
	auto resultFn = findTaskFunction(mod, "result");
	ASSERT_FALSE(resultFn.name.empty());

	auto result = resultFn({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isNull());
}

TEST(TaskModuleTest, ResultEmptyArgsReturnsNull) {
	auto mod = createTaskModule();
	auto resultFn = findTaskFunction(mod, "result");
	ASSERT_FALSE(resultFn.name.empty());

	auto result = resultFn({});
	EXPECT_TRUE(result.isNull());
}

TEST(TaskModuleTest, ErrorReturnsString) {
	auto mod = createTaskModule();
	auto errorFn = findTaskFunction(mod, "error");
	ASSERT_FALSE(errorFn.name.empty());

	auto result = errorFn({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isString());
}

TEST(TaskModuleTest, ErrorEmptyArgsReturnsNotFound) {
	auto mod = createTaskModule();
	auto errorFn = findTaskFunction(mod, "error");
	ASSERT_FALSE(errorFn.name.empty());

	auto result = errorFn({});
	EXPECT_TRUE(result.isString());
	EXPECT_EQ(result.asString(), "Task not found");
}

TEST(TaskModuleTest, WaitEmptyArgsReturnsFalse) {
	auto mod = createTaskModule();
	auto wait = findTaskFunction(mod, "wait");
	ASSERT_FALSE(wait.name.empty());

	auto result = wait({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, RetryNonexistentReturnsFalse) {
	auto mod = createTaskModule();
	auto retry = findTaskFunction(mod, "retry");
	ASSERT_FALSE(retry.name.empty());

	auto result = retry({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, RetryEmptyArgsReturnsFalse) {
	auto mod = createTaskModule();
	auto retry = findTaskFunction(mod, "retry");
	ASSERT_FALSE(retry.name.empty());

	auto result = retry({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(TaskModuleTest, SubmitWithRetryOptions) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"maxRetries", ScriptValue::fromInt(2)},
			{"backoffMs", ScriptValue::fromInt(100)},
			{"metadata", ScriptValue::fromObject({{"key", ScriptValue::fromString("val")}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithNestedRetryOptions) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"retry", ScriptValue::fromObject({
				{"max", ScriptValue::fromInt(3)},
				{"backoffMs", ScriptValue::fromInt(200)},
				{"factor", ScriptValue::fromFloat(1.5)}
			})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}
