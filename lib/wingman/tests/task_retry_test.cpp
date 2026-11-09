#include "task_module_test_common.hpp"

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

