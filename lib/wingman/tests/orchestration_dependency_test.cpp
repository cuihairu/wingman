#include "orchestration_test_helpers.hpp"

// ========== 依赖图调度 ==========

TEST(OrchestrationModuleTest, LinearChainRunsInDependencyOrder) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	EventLog log;
	auto work = [&](const std::string& name) {
		return callable([&log, name]() -> ScriptValue {
			log.push(name + "-start");
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			log.push(name + "-end");
			return ScriptValue::null();
		});
	};

	auto workflowId = submit({ScriptValue::fromObject({
		{"name", ScriptValue::fromString("chain")},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", work("a")},
				{"dependsOn", ScriptValue::fromArray({})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", work("b")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", work("c")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 轮询直到工作流终局
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() &&
			(status->asString() == "succeeded" || status->asString() == "failed");
	}));
	ASSERT_EQ(workflow.get("status")->asString(), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "c"), "succeeded");

	// 事件序列钉死依赖顺序：a 完整结束才开工 b，b 结束才开工 c
	const auto events = log.snapshot();
	ASSERT_EQ(events.size(), 6u);
	ASSERT_EQ(log.index("a-end"), 1u);
	ASSERT_LT(log.index("a-end"), log.index("b-start"));
	ASSERT_LT(log.index("b-end"), log.index("c-start"));
}

TEST(OrchestrationModuleTest, DiamondFanOutFanInRespectsDependencies) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	EventLog log;
	auto work = [&](const std::string& name) {
		return callable([&log, name]() -> ScriptValue {
			log.push(name + "-start");
			log.push(name + "-end");
			return ScriptValue::null();
		});
	};

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", work("a")}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", work("b")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", work("c")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("d")},
				{"run", work("d")},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b"), ScriptValue::fromString("c")})}
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
	ASSERT_EQ(workflow.get("status")->asString(), "succeeded");

	// a 先于 b/c；b、c 双双结束后才轮到 d
	const auto events = log.snapshot();
	ASSERT_EQ(log.index("a-end"), 1u);
	ASSERT_LT(log.index("a-end"), log.index("b-start"));
	ASSERT_LT(log.index("a-end"), log.index("c-start"));
	ASSERT_LT(log.index("b-end"), log.index("d-start"));
	ASSERT_LT(log.index("c-end"), log.index("d-start"));
}

TEST(OrchestrationModuleTest, BlockedStatusObservableWhileDependencyRunning) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<bool> aStarted{false};
	std::atomic<bool> aRelease{false};
	auto a = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		aStarted = true;
		while (!aRelease) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return ScriptValue::null();
	}, true);
	auto b = callable([] { return ScriptValue::null(); });

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", b},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 依赖阻塞态可观测：a running、b blocked、工作流 running
	ASSERT_TRUE(spinUntil([&] { return aStarted.load(); }));
	auto workflow = get({workflowId});
	ASSERT_EQ(workflow.get("status")->asString(), "running");
	ASSERT_EQ(taskStatus(workflow, "a"), "running");
	ASSERT_EQ(taskStatus(workflow, "b"), "blocked");

	// 放行 a 后全链成功
	aRelease = true;
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
}

// ========== 传播矩阵（前置失败/取消/超时 → 后置跳过）==========

TEST(OrchestrationModuleTest, FailedPredecessorSkipsDependents) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<int> bAttempts{0};
	auto a = callable([]() -> ScriptValue {
		throw std::runtime_error("boom");
	});
	auto b = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		bAttempts++;
		return ScriptValue::null();
	}, true);

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", b},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() &&
			(status->asString() == "failed" || status->asString() == "succeeded");
	}));
	ASSERT_EQ(workflow.get("status")->asString(), "failed");
	ASSERT_EQ(taskStatus(workflow, "a"), "failed");
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
	// 被跳过的任务不得开工
	ASSERT_EQ(bAttempts.load(), 0);
	// 失败任务携带错误信息（task 模块语义）
	const ScriptValue* aErr = nullptr;
	const ScriptValue* tasks = workflow.get("tasks");
	ASSERT_TRUE(tasks && tasks->isArray());
	for (size_t i = 0; i < tasks->size(); ++i) {
		const ScriptValue& t = tasks->at(i);
		const ScriptValue* id = t.get("id");
		if (id && id->isString() && id->asString() == "a") {
			aErr = t.get("error");
		}
	}
	ASSERT_TRUE(aErr != nullptr && aErr->isString());
	ASSERT_EQ(aErr->asString(), "boom");
}

TEST(OrchestrationModuleTest, IndependentBranchRunsWhenSiblingFails) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<int> cAttempts{0};
	auto a = callable([]() -> ScriptValue {
		throw std::runtime_error("boom");
	});
	auto c = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		cAttempts++;
		return ScriptValue::null();
	}, true);

	// a 失败、与 a 无依赖的 c 必须照常执行——失败只沿依赖边传播
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", c}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "failed";
	}));
	ASSERT_EQ(workflow.get("status")->asString(), "failed");
	ASSERT_EQ(taskStatus(workflow, "a"), "failed");
	ASSERT_EQ(taskStatus(workflow, "c"), "succeeded");
	ASSERT_EQ(cAttempts.load(), 1);
}

TEST(OrchestrationModuleTest, SkipPropagatesTransitively) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	auto a = callable([]() -> ScriptValue {
		throw std::runtime_error("boom");
	});
	auto run = callable([] { return ScriptValue::null(); });

	// a 失败 → b 跳过 → c（依赖 b）连锁跳过
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "failed";
	}));
	ASSERT_EQ(taskStatus(workflow, "a"), "failed");
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "c"), "skipped");
}

TEST(OrchestrationModuleTest, TimeoutPredecessorSkipsDependents) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	auto a = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		return ScriptValue::null();
	}, true);
	auto run = callable([] { return ScriptValue::null(); });

	// a 超时（80ms < 400ms）→ 后置 b 跳过；状态沿用 task 模块 timeout 语义
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a},
				{"timeoutMs", ScriptValue::fromInt(80)}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() &&
			(status->asString() == "failed" || status->asString() == "succeeded");
	}));
	ASSERT_EQ(workflow.get("status")->asString(), "failed");
	ASSERT_EQ(taskStatus(workflow, "a"), "timeout");
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
}

TEST(OrchestrationModuleTest, RetryWithinBudgetRecoversBeforePropagation) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<int> attempts{0};
	auto a = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		if (attempts.fetch_add(1) == 0) {
			throw std::runtime_error("transient");
		}
		return ScriptValue::null();
	}, true);
	auto b = callable([] { return ScriptValue::null(); });

	// 失败语义沿用 task 模块：重试额度内的失败不传播，恢复后照常调度后置
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a},
				{"maxRetries", ScriptValue::fromInt(1)},
				{"backoffMs", ScriptValue::fromInt(1)}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", b},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
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
	ASSERT_EQ(attempts.load(), 2);
	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
}

// ========== 取消 ==========

TEST(OrchestrationModuleTest, CancelWorkflowCancelsDependents) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto cancel = findFn(mod, "cancel_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(cancel.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<bool> aStarted{false};
	// 协作式取消无法打断执行中的可调用体：work 固定驻留 300ms 后自然返回，
	// cancel 置 Task canceled 后工作流定稿（不依赖 work 观察取消）
	auto a = ScriptValue::fromCallable([&](const std::vector<ScriptValue>&) -> ScriptValue {
		aStarted = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		return ScriptValue::null();
	}, true);
	auto b = callable([] { return ScriptValue::null(); });

	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", a}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", b},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 执行中取消：返回 true，随后全链 canceled
	ASSERT_TRUE(spinUntil([&] { return aStarted.load(); }));
	ASSERT_TRUE(cancel({workflowId}).asBool());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "canceled";
	}));
	ASSERT_EQ(workflow.get("status")->asString(), "canceled");
	ASSERT_EQ(taskStatus(workflow, "a"), "canceled");
	// 未开工的后置任务是 canceled（用户取消语义），而非依赖失败的 skipped
	ASSERT_EQ(taskStatus(workflow, "b"), "canceled");
}

TEST(OrchestrationModuleTest, CancelUnknownOrTerminalWorkflowReturnsFalse) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto cancel = findFn(mod, "cancel_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(cancel.name.empty());

	// 未知 ID
	ASSERT_FALSE(cancel({ScriptValue::fromString("no-such-workflow")}).asBool());
	ASSERT_FALSE(cancel({ScriptValue::fromInt(42)}).asBool());
	ASSERT_FALSE(cancel({}).asBool());

	// 已终局的工作流：恒 false
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([] { return ScriptValue::null(); })}
		})})}
	})});
	ASSERT_TRUE(workflowId.isString());
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_FALSE(cancel({workflowId}).asBool());
	// 取消拒绝不得改写已完成的工作流
	ASSERT_EQ(get({workflowId}).get("status")->asString(), "succeeded");
}

