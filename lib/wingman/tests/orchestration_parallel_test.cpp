#include "orchestration_test_helpers.hpp"

TEST(OrchestrationModuleTest, IndependentWorkflowsRunConcurrently) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());

	std::atomic<bool> firstStarted{false};
	std::atomic<bool> firstRelease{false};
	auto first = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		firstStarted = true;
		while (!firstRelease) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return ScriptValue::null();
	}, true);
	auto second = callable([] { return ScriptValue::null(); });

	// 第二个工作流不得被第一个的驻留阻塞：各自独立调度线程
	auto wf1 = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", first}
		})})}
	})});
	auto wf2 = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("x")},
			{"run", second}
		})})}
	})});
	ASSERT_TRUE(wf1.isString() && wf2.isString());

	auto workflow2 = get({wf2});
	ASSERT_TRUE(spinUntil([&] {
		workflow2 = get({wf2});
		const ScriptValue* status = workflow2.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow2, "x"), "succeeded");

	firstRelease = true;
	auto workflow1 = get({wf1});
	ASSERT_TRUE(spinUntil([&] {
		workflow1 = get({wf1});
		const ScriptValue* status = workflow1.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
}


// ========== 并发控制（maxParallel） ==========

TEST(OrchestrationModuleTest, MaxParallelTwoRunsIndependentTasksConcurrently) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<bool> aStarted{false};
	std::atomic<bool> bStarted{false};
	std::atomic<bool> aRelease{false};
	ScopedFlag releaseA{aRelease};
	auto a = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		aStarted = true;
		while (!aRelease) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return ScriptValue::null();
	}, true);
	auto b = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		bStarted = true;
		return ScriptValue::null();
	}, true);

	// maxParallel=2：a 驻留时独立任务 b 必须已开工（无依赖即可并行）
	auto workflowId = submit({ScriptValue::fromObject({
		{"maxParallel", ScriptValue::fromInt(2)},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", b}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ASSERT_TRUE(spinUntil([&] { return aStarted.load() && bStarted.load(); }));

	aRelease = true;
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
}

TEST(OrchestrationModuleTest, MaxParallelCapsConcurrentTasks) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// a、b 驻留占满额度；c 必须等空位——c 未开工是不依赖时序的不变量
	// （调度线程只在 runningCount < maxParallel 时选任务）
	std::atomic<bool> aStarted{false}, bStarted{false}, cStarted{false};
	std::atomic<bool> aRelease{false}, bRelease{false};
	ScopedFlag releaseA{aRelease}, releaseB{bRelease};
	auto blocked = [](std::atomic<bool>& started, std::atomic<bool>& release) {
		return ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
			started = true;
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			return ScriptValue::null();
		}, true);
	};
	auto c = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		cStarted = true;
		return ScriptValue::null();
	}, true);

	auto workflowId = submit({ScriptValue::fromObject({
		{"maxParallel", ScriptValue::fromInt(2)},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({{"id", ScriptValue::fromString("a")}, {"run", blocked(aStarted, aRelease)}}),
			ScriptValue::fromObject({{"id", ScriptValue::fromString("b")}, {"run", blocked(bStarted, bRelease)}}),
			ScriptValue::fromObject({{"id", ScriptValue::fromString("c")}, {"run", c}})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ASSERT_TRUE(spinUntil([&] { return aStarted.load() && bStarted.load(); }));
	// 额度已满：给足调度机会后 c 仍不得开工
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	EXPECT_FALSE(cStarted.load());

	// 释放一个任务腾出空位 → c 开工
	aRelease = true;
	ASSERT_TRUE(spinUntil([&] { return cStarted.load(); }));
	bRelease = true;

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
}

TEST(OrchestrationModuleTest, MaxParallelDefaultKeepsSerialExecution) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 不给 maxParallel（默认 1）：独立任务 b 在 a 驻留期间不得开工——
	// v1 串行语义保持不变
	std::atomic<bool> aStarted{false}, bStarted{false};
	std::atomic<bool> aRelease{false};
	ScopedFlag releaseA{aRelease};
	auto a = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		aStarted = true;
		while (!aRelease) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return ScriptValue::null();
	}, true);
	auto b = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		bStarted = true;
		return ScriptValue::null();
	}, true);

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({{"id", ScriptValue::fromString("a")}, {"run", a}}),
			ScriptValue::fromObject({{"id", ScriptValue::fromString("b")}, {"run", b}})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ASSERT_TRUE(spinUntil([&] { return aStarted.load(); }));
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	EXPECT_FALSE(bStarted.load());

	aRelease = true;
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
}

TEST(OrchestrationModuleTest, SubmitRejectsInvalidMaxParallel) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	std::atomic<int> errorEvents{0};
	auto sub = wingman::EventHub::instance().subscribe("orchestration.error",
		[&](const wingman::EventMessage&) { errorEvents++; },
		"orchestration-test");

	auto goodRun = callable([] { return ScriptValue::null(); });
	auto makeWf = [&](const ScriptValue& mp) {
		return ScriptValue::fromObject({
			{"maxParallel", mp},
			{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", goodRun}
			})})}
		});
	};
	// 0 与负数拒绝（非整数类型经 asInt 落 0，同路拒绝）
	ASSERT_TRUE(submit({makeWf(ScriptValue::fromInt(0))}).isNull());
	ASSERT_TRUE(submit({makeWf(ScriptValue::fromInt(-1))}).isNull());
	ASSERT_EQ(errorEvents.load(), 2);

	// 显式 maxParallel=1 合法且照常调度
	auto id = submit({makeWf(ScriptValue::fromInt(1))});
	ASSERT_TRUE(id.isString());

	wingman::EventHub::instance().unsubscribe(sub);
}

TEST(OrchestrationModuleTest, ParallelExecutionRespectsDependencies) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// maxParallel=2 的菱形依赖图：并发只作用于无依赖关系的任务，
	// 依赖边约束不变（b/c 都必须晚于 a、早于 d）
	EventLog log;
	auto recorded = [&](const std::string& event) {
		return ScriptValue::fromCallable([&log, event](const std::vector<ScriptValue>&) -> ScriptValue {
			log.push(event);
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			return ScriptValue::null();
		}, true);
	};

	auto workflowId = submit({ScriptValue::fromObject({
		{"maxParallel", ScriptValue::fromInt(2)},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({{"id", ScriptValue::fromString("a")}, {"run", recorded("a")}}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", recorded("b")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", recorded("c")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("d")},
				{"run", recorded("d")},
				{"dependsOn", ScriptValue::fromArray({
					ScriptValue::fromString("b"),
					ScriptValue::fromString("c")
				})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));

	// 依赖序不变量：a 收尾先于 b/c 开工，b、c 收尾都先于 d 开工
	EXPECT_LT(log.index("a"), log.index("b"));
	EXPECT_LT(log.index("a"), log.index("c"));
	EXPECT_LT(log.index("b"), log.index("d"));
	EXPECT_LT(log.index("c"), log.index("d"));
}

TEST(OrchestrationModuleTest, CancelWorkflowWithParallelTasksRunning) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto cancel = findFn(mod, "cancel_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(cancel.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 两个任务同时执行中，取消把两个都协作取消，调度线程定稿 canceled
	std::atomic<bool> aRelease{false}, bRelease{false};
	ScopedFlag releaseA{aRelease}, releaseB{bRelease};
	auto blocked = [](std::atomic<bool>& release) {
		return ScriptValue::fromCallable([&release](const std::vector<ScriptValue>&) -> ScriptValue {
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			return ScriptValue::null();
		}, true);
	};
	auto workflowId = submit({ScriptValue::fromObject({
		{"maxParallel", ScriptValue::fromInt(2)},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({{"id", ScriptValue::fromString("a")}, {"run", blocked(aRelease)}}),
			ScriptValue::fromObject({{"id", ScriptValue::fromString("b")}, {"run", blocked(bRelease)}})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto cancelRes = cancel({workflowId});
	ASSERT_TRUE(cancelRes.isBool());
	ASSERT_TRUE(cancelRes.asBool());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "canceled";
	}));
	ASSERT_EQ(taskStatus(workflow, "a"), "canceled");
	ASSERT_EQ(taskStatus(workflow, "b"), "canceled");

	// 放行驻留 worker（协作式取消不打断执行体，测试须保证工作体有限）
	aRelease = true;
	bRelease = true;
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "canceled";
	}));
}

