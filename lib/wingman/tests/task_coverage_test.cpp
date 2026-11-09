#include "task_module_test_common.hpp"

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

// ========== Async Rejection Tests ==========

TEST(TaskModuleTest, SubmitAsyncNonThreadSafeCallableReturnsFalse) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	ASSERT_FALSE(submit.name.empty());

	// async=true with callableThreadSafe=false should be rejected
	auto result = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}, false),  // callableThreadSafe = false
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)}
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

// ========== Task Canceled During Execution ==========

TEST(TaskModuleTest, CancelRunningTask) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto cancel = findTaskFunction(mod, "cancel");
	auto status = findTaskFunction(mod, "status");
	auto result = findTaskFunction(mod, "result");

	// Submit async task that sleeps long enough for us to cancel
	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			return ScriptValue::fromString("done");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	ASSERT_TRUE(taskId.isString());

	// Give task time to start running
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto taskStatus1 = status({taskId});
	EXPECT_EQ(taskStatus1.asString(), "running");

	// Cancel the task while it's running
	auto cancelResult = cancel({taskId});
	EXPECT_TRUE(cancelResult.isBool());
	EXPECT_TRUE(cancelResult.asBool());

	// Wait for task to finish processing the cancel
	std::this_thread::sleep_for(std::chrono::milliseconds(600));

	// Task should have been cleaned up after cancel (not found = was canceled)
	// If the task completed successfully, it would have "done" as result
	auto taskResult = result({taskId});
	EXPECT_TRUE(taskResult.isNull() || taskResult.asString() != "done");
}

// ========== Task Result After Succeed ==========

TEST(TaskModuleTest, TaskResultWithObjectValue) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto resultFn = findTaskFunction(mod, "result");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromObject({
				{"code", ScriptValue::fromInt(200)},
				{"message", ScriptValue::fromString("ok")}
			});
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto result = resultFn({taskId});
	EXPECT_TRUE(result.isObject());
}

// ========== Task With Zero Timeout ==========

TEST(TaskModuleTest, SubmitWithZeroTimeout) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("ok");
		}),
		ScriptValue::fromObject({
			{"timeoutMs", ScriptValue::fromInt(0)}
		})
	});
	ASSERT_TRUE(taskId.isString());
	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "succeeded");
}

// ========== Task error() on Failed Task ==========

TEST(TaskModuleTest, ErrorOnFailedTaskWithMessage) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto errorFn = findTaskFunction(mod, "error");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			throw std::runtime_error("task failed reason");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	auto err = errorFn({taskId});
	EXPECT_TRUE(err.isString());
	EXPECT_EQ(err.asString(), "task failed reason");
}

// ========== Task wait() with Default Timeout ==========

TEST(TaskModuleTest, WaitWithDefaultTimeout) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto wait = findTaskFunction(mod, "wait");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("fast");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());

	// wait() with only taskId, no timeout arg (uses default)
	auto result = wait({taskId});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

// ========== Task Submit With No Options ==========

TEST(TaskModuleTest, SubmitWithNoOptions) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto status = findTaskFunction(mod, "status");

	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("default_opts");
		})
	});
	ASSERT_TRUE(taskId.isString());
	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "succeeded");
}

// ========== Task Retry With Options ==========

TEST(TaskModuleTest, RetryWithOptions) {
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
		{"maxRetries", ScriptValue::fromInt(2)},
		{"backoffMs", ScriptValue::fromInt(50)}
	})});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

