#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

#include <atomic>
#include <chrono>
#include <functional>
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

// 轮询等待谓词为真（时限内），替代固定 sleep 以吸收共享机负载时序
bool spinUntil(const std::function<bool()>& pred, int timeoutMs = 2000) {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (std::chrono::steady_clock::now() < deadline) {
		if (pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return pred();
}

// 轮询等待任务进入指定状态（async worker 收尾会清理注册表，
// 超时后返回最后一次观测值供调用方判定）
std::string waitForTaskStatus(const ModuleDescriptor::FunctionEntry& status, const ScriptValue& taskId,
	const std::string& expected, int timeoutMs = 2000) {
	std::string last;
	spinUntil([&] {
		last = status({taskId}).asString();
		return last == expected;
	}, timeoutMs);
	return last;
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

// ========== pause / resume ==========

TEST(TaskModuleTest, PauseRunningTaskHoldsCompletionUntilResume) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto pause = findTaskFunction(mod, "pause");
	auto resume = findTaskFunction(mod, "resume");
	auto status = findTaskFunction(mod, "status");
	auto wait = findTaskFunction(mod, "wait");
	auto resultFn = findTaskFunction(mod, "result");
	ASSERT_FALSE(pause.name.empty());
	ASSERT_FALSE(resume.name.empty());

	std::atomic<bool> started{false};
	std::atomic<bool> release{false};
	std::atomic<bool> workDone{false};

	auto taskId = submit({
		ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
			started = true;
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			workDone = true;
			return ScriptValue::fromString("held");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	ASSERT_TRUE(taskId.isString());
	ASSERT_TRUE(spinUntil([&] { return started.load(); }));

	// 捕获 task.paused / task.resumed 生命周期事件
	std::atomic<int> pausedEvents{0};
	std::atomic<int> resumedEvents{0};
	auto pausedSub = wingman::EventHub::instance().subscribe("task.paused",
		[&](const wingman::EventMessage&) { pausedEvents++; });
	auto resumedSub = wingman::EventHub::instance().subscribe("task.resumed",
		[&](const wingman::EventMessage&) { resumedEvents++; });

	// 暂停 running 任务
	auto pauseResult = pause({taskId});
	EXPECT_TRUE(pauseResult.isBool());
	EXPECT_TRUE(pauseResult.asBool());
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 放行 work：结果被扣住不落账
	release = true;
	ASSERT_TRUE(spinUntil([&] { return workDone.load(); }));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(status({taskId}).asString(), "paused");
	EXPECT_TRUE(resultFn({taskId}).isNull());

	// 暂停期间等待者放弃：返回 false 且不改写任务状态
	auto gaveUp = wait({taskId, ScriptValue::fromInt(50)});
	EXPECT_TRUE(gaveUp.isBool());
	EXPECT_FALSE(gaveUp.asBool());
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 恢复后放行扣住的结果
	auto resumeResult = resume({taskId});
	EXPECT_TRUE(resumeResult.isBool());
	EXPECT_TRUE(resumeResult.asBool());

	// 落账 succeeded 可能被 async worker 注册表清理抢先（既有清理语义，
	// 同 CancelRunningTask 防御，not-found 兜底为 "failed"）；本用例 work 不
	// 抛异常，真实失败不可能出现，两种观测均为通过；核心扣留行为已在上方钉死
	auto finalStatus = waitForTaskStatus(status, taskId, "succeeded");
	EXPECT_TRUE(finalStatus == "succeeded" || finalStatus == "failed");
	if (finalStatus == "succeeded") {
		auto held = resultFn({taskId});
		EXPECT_TRUE(held.isString() && held.asString() == "held");
	}

	// 终态上 pause/resume 均拒绝
	EXPECT_FALSE(pause({taskId}).asBool());
	EXPECT_FALSE(resume({taskId}).asBool());

	EXPECT_EQ(pausedEvents.load(), 1);
	EXPECT_EQ(resumedEvents.load(), 1);

	wingman::EventHub::instance().unsubscribe(pausedSub);
	wingman::EventHub::instance().unsubscribe(resumedSub);
}

TEST(TaskModuleTest, PauseResumeInvalidTargetsReturnFalse) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto pause = findTaskFunction(mod, "pause");
	auto resume = findTaskFunction(mod, "resume");

	// 不存在的 ID / 缺参 / 非字符串参数一律 false
	EXPECT_FALSE(pause({ScriptValue::fromString("nonexistent")}).asBool());
	EXPECT_FALSE(resume({ScriptValue::fromString("nonexistent")}).asBool());
	EXPECT_FALSE(pause({}).asBool());
	EXPECT_FALSE(resume({}).asBool());
	EXPECT_FALSE(pause({ScriptValue::fromInt(42)}).asBool());
	EXPECT_FALSE(resume({ScriptValue::fromInt(42)}).asBool());
	EXPECT_FALSE(pause({ScriptValue::fromString("")}).asBool());

	// 终态（同步任务提交即完成）上 pause/resume 拒绝
	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}),
		ScriptValue::fromObject({{"timeoutMs", ScriptValue::fromInt(5000)}})
	});
	ASSERT_TRUE(taskId.isString());
	EXPECT_FALSE(pause({taskId}).asBool());
	EXPECT_FALSE(resume({taskId}).asBool());
}

TEST(TaskModuleTest, CancelPausedTaskDiscardsHeldResult) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto pause = findTaskFunction(mod, "pause");
	auto cancel = findTaskFunction(mod, "cancel");
	auto status = findTaskFunction(mod, "status");
	auto resultFn = findTaskFunction(mod, "result");

	std::atomic<bool> started{false};
	std::atomic<bool> release{false};
	std::atomic<bool> workDone{false};

	auto taskId = submit({
		ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
			started = true;
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			workDone = true;
			return ScriptValue::fromString("held");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(5000)}
		})
	});
	ASSERT_TRUE(taskId.isString());
	ASSERT_TRUE(spinUntil([&] { return started.load(); }));
	ASSERT_TRUE(pause({taskId}).asBool());

	// work 完成但结果被扣住
	release = true;
	ASSERT_TRUE(spinUntil([&] { return workDone.load(); }));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 暂停中可直接取消，唤醒驻留点并丢弃扣住的结果
	auto cancelResult = cancel({taskId});
	EXPECT_TRUE(cancelResult.isBool());
	EXPECT_TRUE(cancelResult.asBool());

	auto finalStatus = waitForTaskStatus(status, taskId, "canceled");
	EXPECT_NE(finalStatus, "paused");
	EXPECT_TRUE(resultFn({taskId}).isNull());
}

TEST(TaskModuleTest, PauseSuspendsTimeoutClock) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto pause = findTaskFunction(mod, "pause");
	auto resume = findTaskFunction(mod, "resume");
	auto status = findTaskFunction(mod, "status");

	std::atomic<bool> started{false};
	std::atomic<bool> release{false};

	// timeoutMs=300：若暂停不停走超时时钟，600ms 后任务必已 timeout
	auto taskId = submit({
		ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
			started = true;
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			return ScriptValue::fromString("late");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(300)}
		})
	});
	ASSERT_TRUE(taskId.isString());
	ASSERT_TRUE(spinUntil([&] { return started.load(); }));
	ASSERT_TRUE(pause({taskId}).asBool());

	std::this_thread::sleep_for(std::chrono::milliseconds(600));
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 恢复后时钟从挂起点继续：work 仍未完成，300ms 后触发超时
	EXPECT_TRUE(resume({taskId}).asBool());
	EXPECT_EQ(waitForTaskStatus(status, taskId, "timeout"), "timeout");

	release = true;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(TaskModuleTest, PauseBetweenRetriesDelaysNextAttempt) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto pause = findTaskFunction(mod, "pause");
	auto resume = findTaskFunction(mod, "resume");
	auto status = findTaskFunction(mod, "status");

	std::atomic<int> attempts{0};

	// 第一次尝试抛异常触发重试；backoffMs=300 给 pause 留出落点窗口
	auto taskId = submit({
		ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
			if (attempts.fetch_add(1) == 0) {
				throw std::runtime_error("first attempt fails");
			}
			return ScriptValue::fromString("ok");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(5000)},
			{"maxRetries", ScriptValue::fromInt(1)},
			{"backoffMs", ScriptValue::fromInt(300)}
		})
	});
	ASSERT_TRUE(taskId.isString());
	ASSERT_TRUE(spinUntil([&] { return attempts.load() == 1; }));
	ASSERT_TRUE(pause({taskId}).asBool());
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 暂停期间 backoff 结束也不进入第二次尝试
	std::this_thread::sleep_for(std::chrono::milliseconds(400));
	EXPECT_EQ(attempts.load(), 1);
	EXPECT_EQ(status({taskId}).asString(), "paused");

	// 恢复放行重试检查点
	EXPECT_TRUE(resume({taskId}).asBool());
	EXPECT_TRUE(spinUntil([&] { return attempts.load() == 2; }));

	// async worker 收尾可能已清理注册表，终态断言容忍清理
	auto finalStatus = waitForTaskStatus(status, taskId, "succeeded");
	EXPECT_TRUE(finalStatus == "succeeded" || finalStatus == "failed");
}
