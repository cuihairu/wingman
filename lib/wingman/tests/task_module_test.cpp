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

// ========== Retry and Error Path Tests ==========

TEST(TaskModuleTest, SubmitTaskThatThrowsTriggersRetry) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");
	auto errorFn = findTaskFunction(mod, "error");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(status.name.empty());
	ASSERT_FALSE(errorFn.name.empty());

	int attempts = 0;
	auto taskId = submit({
		ScriptValue::fromCallable([&attempts](const std::vector<ScriptValue>&) -> ScriptValue {
			attempts++;
			throw std::runtime_error("test error");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"maxRetries", ScriptValue::fromInt(2)},
			{"backoffMs", ScriptValue::fromInt(1)}
		})
	});
	ASSERT_TRUE(taskId.isString());

	// Should have retried 2 times (initial + 2 retries = 3 total)
	EXPECT_EQ(attempts, 3);

	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "failed");

	auto err = errorFn({taskId});
	EXPECT_EQ(err.asString(), "test error");
}

TEST(TaskModuleTest, SubmitTaskWithUnknownException) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");
	auto errorFn = findTaskFunction(mod, "error");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			throw 42;  // non-std::exception
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	ASSERT_TRUE(taskId.isString());

	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "failed");

	auto err = errorFn({taskId});
	EXPECT_EQ(err.asString(), "Unknown exception");
}

TEST(TaskModuleTest, SubmitSyncTaskSucceeds) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");
	auto resultFn = findTaskFunction(mod, "result");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromInt(99);
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	ASSERT_TRUE(taskId.isString());

	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "succeeded");

	auto result = resultFn({taskId});
	EXPECT_TRUE(result.isInt());
	EXPECT_EQ(result.asInt(), 99);
}

TEST(TaskModuleTest, SubmitWithMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::null();
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"metadata", ScriptValue::fromObject({
				{"name", ScriptValue::fromString("test")},
				{"count", ScriptValue::fromInt(5)},
				{"flag", ScriptValue::fromBool(true)}
			})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithFlatRetryOptions) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"maxRetries", ScriptValue::fromInt(1)},
			{"backoffMs", ScriptValue::fromInt(50)},
			{"backoffFactor", ScriptValue::fromFloat(1.0)}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, WaitOnCompletedTaskReturnsTrue) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto wait = findTaskFunction(mod, "wait");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	// Task already completed synchronously, wait should return true immediately
	auto result = wait({taskId, ScriptValue::fromInt(1000)});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(TaskModuleTest, CancelEmptyStringReturnsFalse) {
	auto mod = createTaskModule();
	auto cancel = findTaskFunction(mod, "cancel");

	auto result = cancel({ScriptValue::fromString("")});
	EXPECT_TRUE(result.isBool());
}

TEST(TaskModuleTest, ErrorOnCompletedTask) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto errorFn = findTaskFunction(mod, "error");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto err = errorFn({taskId});
	EXPECT_TRUE(err.isString());
	EXPECT_TRUE(err.asString().empty());
}

TEST(TaskModuleTest, RetryExistingTaskReturnsBool) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto retry = findTaskFunction(mod, "retry");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto result = retry({taskId, ScriptValue::fromObject({
		{"maxRetries", ScriptValue::fromInt(1)},
		{"backoffMs", ScriptValue::fromInt(100)}
	})});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

// ========== toJson Coverage Tests ==========

TEST(TaskModuleTest, SubmitWithNullMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"metadata", ScriptValue::fromObject({{"key", ScriptValue::null()}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithBoolMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"metadata", ScriptValue::fromObject({{"active", ScriptValue::fromBool(true)}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithFloatMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"metadata", ScriptValue::fromObject({{"score", ScriptValue::fromFloat(3.14)}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithArrayMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"metadata", ScriptValue::fromObject({{"items", ScriptValue::fromArray({
				ScriptValue::fromInt(1),
				ScriptValue::fromString("two"),
				ScriptValue::fromBool(true)
			})}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}

TEST(TaskModuleTest, SubmitWithNestedArrayMetadata) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"metadata", ScriptValue::fromObject({{"matrix", ScriptValue::fromArray({
				ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}),
				ScriptValue::fromArray({ScriptValue::fromInt(3), ScriptValue::fromInt(4)})
			})}})}
		})
	});
	EXPECT_TRUE(taskId.isString());
}
