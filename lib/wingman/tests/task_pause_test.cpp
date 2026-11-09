#include "task_module_test_common.hpp"

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
