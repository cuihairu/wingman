#include "orchestration_test_helpers.hpp"

// ========== 查询面 ==========

TEST(OrchestrationModuleTest, SnapshotShapeAndQueries) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	auto getAll = findFn(mod, "get_all_workflows");
	ASSERT_FALSE(submit.name.empty());

	auto workflowId = submit({ScriptValue::fromObject({
		{"name", ScriptValue::fromString("demo")},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::null(); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([] { return ScriptValue::null(); })},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// get_workflow 未知 ID → null
	ASSERT_TRUE(get({ScriptValue::fromString("no-such")}).isNull());

	// 轮询到终局再断言任务态（提交返回时调度线程可能尚未落账）
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_TRUE(workflow.isObject());
	ASSERT_EQ(workflow.get("id")->asString(), workflowId.asString());
	ASSERT_EQ(workflow.get("name")->asString(), "demo");
	ASSERT_EQ(workflow.get("status")->asString(), "succeeded");

	const ScriptValue* tasks = workflow.get("tasks");
	ASSERT_TRUE(tasks && tasks->isArray());
	ASSERT_EQ(tasks->size(), 2u);
	const ScriptValue& first = tasks->at(0);
	ASSERT_EQ(first.get("id")->asString(), "a");
	ASSERT_EQ(first.get("status")->asString(), "succeeded");
	// 无依赖任务的 dependsOn 为空数组
	const ScriptValue* firstDeps = first.get("dependsOn");
	ASSERT_TRUE(firstDeps && firstDeps->isArray() && firstDeps->size() == 0u);
	const ScriptValue& second = tasks->at(1);
	ASSERT_EQ(second.get("id")->asString(), "b");
	ASSERT_EQ(second.get("status")->asString(), "succeeded");
	const ScriptValue* secondDeps = second.get("dependsOn");
	ASSERT_TRUE(secondDeps && secondDeps->isArray());
	ASSERT_EQ(secondDeps->size(), 1u);
	ASSERT_EQ(secondDeps->at(0).asString(), "a");

	// get_all_workflows 含本工作流快照
	auto all = getAll({});
	ASSERT_TRUE(all.isArray());
	bool found = false;
	for (size_t i = 0; i < all.size(); ++i) {
		const ScriptValue* id = all.at(i).get("id");
		if (id && id->isString() && id->asString() == workflowId.asString()) {
			found = true;
		}
	}
	ASSERT_TRUE(found);
}


// ========== 流程级状态事件 ==========

// 成功链：started → task_finished×N（调度序）→ finished(succeeded)，全序可断言
TEST(OrchestrationModuleTest, FlowEventsOrderedForSucceededChain) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	EventLog log;
	std::mutex captureMutex;
	nlohmann::json startedPayload, finishedPayload;
	std::vector<std::pair<std::string, std::string>> finishedTasks;  // (task, status)
	auto sub = wingman::EventHub::instance().subscribe("orchestration.started",
		[&](const wingman::EventMessage& msg) {
			log.push("started");
			std::lock_guard<std::mutex> lock(captureMutex);
			startedPayload = msg.payload;
		}, "orchestration-test");
	auto subTask = wingman::EventHub::instance().subscribe("orchestration.task_finished",
		[&](const wingman::EventMessage& msg) {
			std::lock_guard<std::mutex> lock(captureMutex);
			log.push("task:" + msg.payload.value("task", ""));
			finishedTasks.emplace_back(msg.payload.value("task", ""), msg.payload.value("status", ""));
		}, "orchestration-test");
	auto subFin = wingman::EventHub::instance().subscribe("orchestration.finished",
		[&](const wingman::EventMessage& msg) {
			log.push("finished");
			std::lock_guard<std::mutex> lock(captureMutex);
			finishedPayload = msg.payload;
		}, "orchestration-test");

	auto work = callable([]() -> ScriptValue {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return ScriptValue::null();
	});
	auto workflowId = submit({ScriptValue::fromObject({
		{"name", ScriptValue::fromString("flow-ok")},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", work},
				{"dependsOn", ScriptValue::fromArray({})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", work},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());
	const std::string wfId = workflowId.asString();

	ASSERT_TRUE(spinUntil([&] {
		auto workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	// finished 事件经调度线程发出，快照终态后稍等事件落账
	ASSERT_TRUE(spinUntil([&] {
		std::lock_guard<std::mutex> lock(captureMutex);
		return !finishedPayload.empty();
	}));

	{
		std::lock_guard<std::mutex> lock(captureMutex);
		// 全序：started → task:a → task:b → finished（maxParallel 默认串行）
		EXPECT_EQ(log.count("started"), 1u);
		EXPECT_EQ(log.count("finished"), 1u);
		EXPECT_EQ(log.index("started"), 0u);
		EXPECT_EQ(log.index("task:a"), 1u);
		EXPECT_EQ(log.index("task:b"), 2u);
		EXPECT_EQ(log.index("finished"), 3u);
		// started 负载：workflow id + name
		EXPECT_EQ(startedPayload.value("workflow", ""), wfId);
		EXPECT_EQ(startedPayload.value("name", ""), "flow-ok");
		// task_finished 负载：调度序 succeeded，且带 workflow 归属
		ASSERT_EQ(finishedTasks.size(), 2u);
		EXPECT_EQ(finishedTasks[0].first, "a");
		EXPECT_EQ(finishedTasks[0].second, "succeeded");
		EXPECT_EQ(finishedTasks[1].first, "b");
		EXPECT_EQ(finishedTasks[1].second, "succeeded");
		// finished 负载：终态 + 计数
		EXPECT_EQ(finishedPayload.value("workflow", ""), wfId);
		EXPECT_EQ(finishedPayload.value("status", ""), "succeeded");
		const nlohmann::json& counts = finishedPayload.at("tasks");
		EXPECT_EQ(counts.value("succeeded", 0), 2);
		EXPECT_EQ(counts.value("failed", 0), 0);
		EXPECT_EQ(counts.value("skipped", 0), 0);
		EXPECT_EQ(counts.value("total", 0), 2);
	}

	wingman::EventHub::instance().unsubscribe(sub);
	wingman::EventHub::instance().unsubscribe(subTask);
	wingman::EventHub::instance().unsubscribe(subFin);
}

// 失败链：失败任务 task_finished(failed) 携带 error；finished 落 failed，
// 失败传播的 skipped 不单发任务事件、只在 finished 计数可见
TEST(OrchestrationModuleTest, FlowEventsFailureChainCountsSkipped) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	EventLog log;
	std::mutex captureMutex;
	nlohmann::json finishedPayload;
	std::string failingTaskError;
	auto subTask = wingman::EventHub::instance().subscribe("orchestration.task_finished",
		[&](const wingman::EventMessage& msg) {
			log.push("task:" + msg.payload.value("task", ""));
			std::lock_guard<std::mutex> lock(captureMutex);
			if (msg.payload.value("task", "") == "a") {
				failingTaskError = msg.payload.value("error", "");
			}
		}, "orchestration-test");
	auto subFin = wingman::EventHub::instance().subscribe("orchestration.finished",
		[&](const wingman::EventMessage& msg) {
			log.push("finished");
			std::lock_guard<std::mutex> lock(captureMutex);
			finishedPayload = msg.payload;
		}, "orchestration-test");

	auto boom = callable([]() -> ScriptValue { throw std::runtime_error("boom"); });
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", boom},
				{"dependsOn", ScriptValue::fromArray({})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([]() -> ScriptValue { return ScriptValue::null(); })},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ASSERT_TRUE(spinUntil([&] {
		auto workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "failed";
	}));
	ASSERT_TRUE(spinUntil([&] {
		std::lock_guard<std::mutex> lock(captureMutex);
		return !finishedPayload.empty();
	}));

	{
		std::lock_guard<std::mutex> lock(captureMutex);
		// 仅真实执行的任务单发事件；传播 skipped 的 b 不发
		EXPECT_EQ(log.count("task:a"), 1u);
		EXPECT_EQ(log.count("task:b"), 0u);
		EXPECT_EQ(failingTaskError, "boom");
		EXPECT_EQ(finishedPayload.value("status", ""), "failed");
		const nlohmann::json& counts = finishedPayload.at("tasks");
		EXPECT_EQ(counts.value("succeeded", 0), 0);
		EXPECT_EQ(counts.value("failed", 0), 1);
		EXPECT_EQ(counts.value("skipped", 0), 1);
		EXPECT_EQ(counts.value("total", 0), 2);
	}

	wingman::EventHub::instance().unsubscribe(subTask);
	wingman::EventHub::instance().unsubscribe(subFin);
}

// 条件过滤链：when=false 的分支落 skipped，工作流仍 succeeded（正常控制流）
TEST(OrchestrationModuleTest, FlowEventsConditionSkipKeepsSucceeded) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::mutex captureMutex;
	nlohmann::json finishedPayload;
	auto subFin = wingman::EventHub::instance().subscribe("orchestration.finished",
		[&](const wingman::EventMessage& msg) {
			std::lock_guard<std::mutex> lock(captureMutex);
			finishedPayload = msg.payload;
		}, "orchestration-test");

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([]() -> ScriptValue { return ScriptValue::null(); })},
				{"when", callable([]() -> ScriptValue { return ScriptValue::fromBool(false); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([]() -> ScriptValue { return ScriptValue::fromBool(true); })}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ASSERT_TRUE(spinUntil([&] {
		auto workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_TRUE(spinUntil([&] {
		std::lock_guard<std::mutex> lock(captureMutex);
		return !finishedPayload.empty();
	}));

	{
		std::lock_guard<std::mutex> lock(captureMutex);
		EXPECT_EQ(finishedPayload.value("status", ""), "succeeded");
		const nlohmann::json& counts = finishedPayload.at("tasks");
		EXPECT_EQ(counts.value("succeeded", 0), 1);
		EXPECT_EQ(counts.value("skipped", 0), 1);
	}

	wingman::EventHub::instance().unsubscribe(subFin);
}

// 取消：finished(canceled) 在取消决策点发出；执行中未收账任务不发
// task_finished（内核终态以快照为准），计数落 other
TEST(OrchestrationModuleTest, FlowEventsCanceledReportsRunningAsOther) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto cancel = findFn(mod, "cancel_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(cancel.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::mutex captureMutex;
	nlohmann::json finishedPayload;
	size_t taskEvents = 0;
	auto subTask = wingman::EventHub::instance().subscribe("orchestration.task_finished",
		[&](const wingman::EventMessage&) {
			std::lock_guard<std::mutex> lock(captureMutex);
			++taskEvents;
		}, "orchestration-test");
	auto subFin = wingman::EventHub::instance().subscribe("orchestration.finished",
		[&](const wingman::EventMessage& msg) {
			std::lock_guard<std::mutex> lock(captureMutex);
			finishedPayload = msg.payload;
		}, "orchestration-test");

	// 旗标经 shared_ptr 由 worker 与测试两侧共同持有：worker 的生存期不归
	// 测试帧管（工作流登记在进程级 manager，退出时才统一 join）——栈上旗标
	// 在测试帧结束后即悬垂，worker 5ms 轮询一旦错过帧内存的 true，后续读
	// 到的是复用栈帧的垃圾值，会永远循环到进程退出 join 卡死
	auto release = std::make_shared<std::atomic<bool>>(false);
	auto blocked = ScriptValue::fromCallable([release](const std::vector<ScriptValue>&) -> ScriptValue {
		while (!release->load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return ScriptValue::null();
	}, true);
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({{"id", ScriptValue::fromString("a")}, {"run", blocked}})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 等任务确实开工（running）再取消，钉住 other=1 的判定窗口
	ASSERT_TRUE(spinUntil([&] {
		auto workflow = get({workflowId});
		return taskStatus(workflow, "a") == "running";
	}));
	ASSERT_TRUE(cancel({workflowId}).asBool());

	ASSERT_TRUE(spinUntil([&] {
		auto workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "canceled";
	}));
	ASSERT_TRUE(spinUntil([&] {
		std::lock_guard<std::mutex> lock(captureMutex);
		return !finishedPayload.empty();
	}));

	{
		std::lock_guard<std::mutex> lock(captureMutex);
		EXPECT_EQ(finishedPayload.value("status", ""), "canceled");
		const nlohmann::json& counts = finishedPayload.at("tasks");
		EXPECT_EQ(counts.value("canceled", 0), 0);
		EXPECT_EQ(counts.value("other", 0), 1);
		EXPECT_EQ(counts.value("total", 0), 1);
		EXPECT_EQ(taskEvents, 0u);  // 执行中未收账：无 task_finished
	}

	// 断言后再放行 worker（协作取消不打断执行体，工作体须有限）
	*release = true;
	wingman::EventHub::instance().unsubscribe(subTask);
	wingman::EventHub::instance().unsubscribe(subFin);
}

// ========== 子任务产出聚合 ==========

// 快照聚合 run() 返回值：Int/Object 落 result 字段；返回 null 与失败任务
// 不携带 result（失败走 error 字段）
TEST(OrchestrationModuleTest, TaskResultsAggregateIntoSnapshot) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::fromInt(42); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([] {
					return ScriptValue::fromObject({{"x", ScriptValue::fromString("v")}});
				})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", callable([] { return ScriptValue::null(); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("d")},
				{"run", callable([]() -> ScriptValue { throw std::runtime_error("no result"); })}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ScriptValue workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "failed";
	}));

	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "c"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "d"), "failed");

	const ScriptValue* tasks = workflow.get("tasks");
	ASSERT_TRUE(tasks && tasks->isArray());
	auto findById = [&](const std::string& id) -> const ScriptValue* {
		for (size_t i = 0; i < tasks->size(); ++i) {
			const ScriptValue& t = tasks->at(i);
			const ScriptValue* tid = t.get("id");
			if (tid && tid->isString() && tid->asString() == id) return &t;
		}
		return nullptr;
	};
	const ScriptValue* ta = findById("a");
	const ScriptValue* tb = findById("b");
	const ScriptValue* tc = findById("c");
	const ScriptValue* td = findById("d");
	ASSERT_NE(ta, nullptr);
	ASSERT_NE(tb, nullptr);
	ASSERT_NE(tc, nullptr);
	ASSERT_NE(td, nullptr);

	// Int 产出原样聚合
	const ScriptValue* resA = ta->get("result");
	ASSERT_TRUE(resA && resA->isInt());
	EXPECT_EQ(resA->asInt(), 42);
	// Object 产出原样聚合
	const ScriptValue* resB = tb->get("result");
	ASSERT_TRUE(resB && resB->isObject());
	const ScriptValue* x = resB->get("x");
	ASSERT_TRUE(x && x->isString());
	EXPECT_EQ(x->asString(), "v");
	// 返回 null：字段省略（与未产出同形）
	EXPECT_EQ(tc->get("result"), nullptr);
	// 失败任务：无 result，有 error
	EXPECT_EQ(td->get("result"), nullptr);
	const ScriptValue* errD = td->get("error");
	ASSERT_TRUE(errD && errD->isString());
	EXPECT_EQ(errD->asString(), "no result");
}

// 下游任务经 get_workflow 读上游产出（组合即数据流：b = a + 1）
TEST(OrchestrationModuleTest, DownstreamTaskReadsUpstreamResult) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	ScriptValue workflowId;  // run 执行前 submit 已返回，捕获引用安全
	auto bRun = callable([&]() -> ScriptValue {
		ScriptValue wf = get({workflowId});
		const ScriptValue* tasks = wf.get("tasks");
		if (!tasks || !tasks->isArray()) return ScriptValue::fromInt(-1);
		for (size_t i = 0; i < tasks->size(); ++i) {
			const ScriptValue& t = tasks->at(i);
			const ScriptValue* id = t.get("id");
			if (id && id->isString() && id->asString() == "a") {
				const ScriptValue* res = t.get("result");
				if (res && res->isInt()) return ScriptValue::fromInt(res->asInt() + 1);
			}
		}
		return ScriptValue::fromInt(-1);
	});
	workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::fromInt(41); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", bRun},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	ScriptValue workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");

	const ScriptValue* tasks = workflow.get("tasks");
	ASSERT_TRUE(tasks && tasks->isArray());
	for (size_t i = 0; i < tasks->size(); ++i) {
		const ScriptValue& t = tasks->at(i);
		const ScriptValue* id = t.get("id");
		if (id && id->isString() && id->asString() == "b") {
			const ScriptValue* res = t.get("result");
			ASSERT_TRUE(res && res->isInt());
			EXPECT_EQ(res->asInt(), 42);
			return;
		}
	}
	FAIL() << "task b missing from snapshot";
}

