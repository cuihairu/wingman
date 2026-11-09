#include "orchestration_test_helpers.hpp"

// ========== 条件分支（when） ==========

TEST(OrchestrationModuleTest, WhenTrueRunsTaskAndEvaluatesOnce) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<int> whenCalls{0};
	std::atomic<bool> ran{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([&]() -> ScriptValue {
				ran = true;
				return ScriptValue::null();
			})},
			{"when", callable([&]() -> ScriptValue {
				++whenCalls;
				return ScriptValue::fromBool(true);
			})}
		})})}
	})});
	ASSERT_TRUE(workflowId.isString());

	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	EXPECT_TRUE(ran.load());
	// 条件在调度点求值一次，不随调度循环重复
	EXPECT_EQ(whenCalls.load(), 1);
}

TEST(OrchestrationModuleTest, WhenFalseSkipsWithoutFailingWorkflow) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	std::atomic<bool> bRan{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::null(); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([&]() -> ScriptValue {
					bRan = true;
					return ScriptValue::null();
				})},
				{"when", callable([] { return ScriptValue::fromBool(false); })}
			})
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 关键语义：条件跳过是正常分支过滤，工作流不判失败
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "a"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
	EXPECT_FALSE(bRan.load());
}

TEST(OrchestrationModuleTest, BranchConditionPicksExactlyOneSide) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 同一 flag 驱动两个工作流，验证二选一在两个方向都成立
	for (int round = 0; round < 2; ++round) {
		std::atomic<bool> flag{round == 0};
		std::atomic<bool> bRan{false};
		std::atomic<bool> cRan{false};
		auto makeTask = [&](const std::string& id, std::atomic<bool>& ran,
			std::function<bool()> cond) {
			return ScriptValue::fromObject({
				{"id", ScriptValue::fromString(id)},
				{"run", callable([&]() -> ScriptValue {
					ran = true;
					return ScriptValue::null();
				})},
				{"when", callable([cond]() -> ScriptValue {
					return ScriptValue::fromBool(cond());
				})}
			});
		};
		auto workflowId = submit({ScriptValue::fromObject({
			{"tasks", ScriptValue::fromArray({
				ScriptValue::fromObject({
					{"id", ScriptValue::fromString("a")},
					{"run", callable([] { return ScriptValue::null(); })}
				}),
				makeTask("b", bRan, [&] { return flag.load(); }),
				makeTask("c", cRan, [&] { return !flag.load(); })
			})}
		})});
		ASSERT_TRUE(workflowId.isString());

		auto workflow = get({workflowId});
		ASSERT_TRUE(spinUntil([&] {
			workflow = get({workflowId});
			const ScriptValue* status = workflow.get("status");
			return status && status->isString() && status->asString() == "succeeded";
		})) << "round " << round;
		if (round == 0) {
			ASSERT_EQ(taskStatus(workflow, "b"), "succeeded");
			ASSERT_EQ(taskStatus(workflow, "c"), "skipped");
			EXPECT_TRUE(bRan.load());
			EXPECT_FALSE(cRan.load());
		} else {
			ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
			ASSERT_EQ(taskStatus(workflow, "c"), "succeeded");
			EXPECT_FALSE(bRan.load());
			EXPECT_TRUE(cRan.load());
		}
	}
}

TEST(OrchestrationModuleTest, ConditionSkipChainDoesNotFailWorkflow) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// a 条件跳过 → b（依赖 a）→ c（依赖 b）连锁按条件链跳过；
	// 独立任务 d 照常执行；全链条件跳过不判工作流失败
	std::atomic<bool> downstreamRan{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::null(); })},
				{"when", callable([] { return ScriptValue::fromBool(false); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([&]() -> ScriptValue {
					downstreamRan = true;
					return ScriptValue::null();
				})},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", callable([&]() -> ScriptValue {
					downstreamRan = true;
					return ScriptValue::null();
				})},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("d")},
				{"run", callable([] { return ScriptValue::null(); })}
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
	ASSERT_EQ(taskStatus(workflow, "a"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "c"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "d"), "succeeded");
	EXPECT_FALSE(downstreamRan.load());
}

TEST(OrchestrationModuleTest, FailureChainDominatesConditionSkip) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// a 失败：b（依赖 a，when=true）按失败链跳过且判工作流失败；
	// x 条件跳过；y 混合前置 [a, x] 失败链优先于条件链
	std::atomic<bool> bRan{false};
	std::atomic<bool> yRan{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([]() -> ScriptValue {
					throw std::runtime_error("boom");
				})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([&]() -> ScriptValue {
					bRan = true;
					return ScriptValue::null();
				})},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})},
				{"when", callable([] { return ScriptValue::fromBool(true); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("x")},
				{"run", callable([] { return ScriptValue::null(); })},
				{"when", callable([] { return ScriptValue::fromBool(false); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("y")},
				{"run", callable([&]() -> ScriptValue {
					yRan = true;
					return ScriptValue::null();
				})},
				{"dependsOn", ScriptValue::fromArray({
					ScriptValue::fromString("a"),
					ScriptValue::fromString("x")
				})}
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
	ASSERT_EQ(taskStatus(workflow, "b"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "x"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "y"), "skipped");
	EXPECT_FALSE(bRan.load());
	EXPECT_FALSE(yRan.load());
}

TEST(OrchestrationModuleTest, WhenPredicateThrowsFailsTask) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 条件求值异常沿用 task 模块失败语义：任务落 failed 携带错误信息，
	// 后置任务按失败链跳过；run 不得被调用
	std::atomic<bool> runCalled{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([&]() -> ScriptValue {
					runCalled = true;
					return ScriptValue::null();
				})},
				{"when", callable([]() -> ScriptValue {
					throw std::runtime_error("boom");
				})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", callable([] { return ScriptValue::null(); })},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
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
	EXPECT_FALSE(runCalled.load());
	// 失败任务携带条件异常信息
	const ScriptValue* tasks = workflow.get("tasks");
	ASSERT_TRUE(tasks && tasks->isArray());
	for (size_t i = 0; i < tasks->size(); ++i) {
		const ScriptValue& t = tasks->at(i);
		const ScriptValue* id = t.get("id");
		if (id && id->isString() && id->asString() == "a") {
			const ScriptValue* err = t.get("error");
			ASSERT_TRUE(err != nullptr && err->isString());
			EXPECT_EQ(err->asString(), "when predicate threw: boom");
		}
	}
}

TEST(OrchestrationModuleTest, WhenTruthinessFollowsValueSemantics) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 真值口径：Int/Float 非 0 为真、String 非空为真（Bool 在其余用例覆盖；
	// Null 走不到此处——提交层要求 when 必须是可调用体）
	auto makeTask = [](const std::string& id, const ScriptValue& cond,
		std::atomic<bool>& ran) {
		return ScriptValue::fromObject({
			{"id", ScriptValue::fromString(id)},
			{"run", callable([&]() -> ScriptValue {
				ran = true;
				return ScriptValue::null();
			})},
			{"when", callable([cond]() -> ScriptValue { return cond; })}
		});
	};
	std::atomic<bool> i0Ran{false}, i1Ran{false}, esRan{false},
		nsRan{false}, fzRan{false}, foRan{false};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			makeTask("i0", ScriptValue::fromInt(0), i0Ran),
			makeTask("i1", ScriptValue::fromInt(1), i1Ran),
			makeTask("es", ScriptValue::fromString(""), esRan),
			makeTask("ns", ScriptValue::fromString("x"), nsRan),
			makeTask("fz", ScriptValue::fromFloat(0.0), fzRan),
			makeTask("fo", ScriptValue::fromFloat(0.5), foRan)
		})}
	})});
	ASSERT_TRUE(workflowId.isString());

	// 条件跳过不判失败：6 个任务 3 skipped 3 succeeded，工作流 succeeded
	auto workflow = get({workflowId});
	ASSERT_TRUE(spinUntil([&] {
		workflow = get({workflowId});
		const ScriptValue* status = workflow.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));
	ASSERT_EQ(taskStatus(workflow, "i0"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "i1"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "es"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "ns"), "succeeded");
	ASSERT_EQ(taskStatus(workflow, "fz"), "skipped");
	ASSERT_EQ(taskStatus(workflow, "fo"), "succeeded");
	EXPECT_FALSE(i0Ran.load());
	EXPECT_TRUE(i1Ran.load());
	EXPECT_FALSE(esRan.load());
	EXPECT_TRUE(nsRan.load());
	EXPECT_FALSE(fzRan.load());
	EXPECT_TRUE(foRan.load());
}

TEST(OrchestrationModuleTest, SubmitRejectsBadWhenCallables) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	// 订阅 orchestration.error 拒绝事件（事件负载 {"error": reason}）
	std::atomic<int> errorEvents{0};
	std::mutex errorMutex;
	std::vector<std::string> errors;
	auto sub = wingman::EventHub::instance().subscribe("orchestration.error",
		[&](const wingman::EventMessage& msg) {
			errorEvents++;
			std::lock_guard<std::mutex> lock(errorMutex);
			errors.push_back(msg.payload.value("error", ""));
		},
		"orchestration-test");

	auto goodRun = callable([] { return ScriptValue::null(); });
	// when 非可调用体
	auto id1 = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", goodRun},
			{"when", ScriptValue::fromInt(42)}
		})})}
	})});
	// when 非线程安全可调用体（threadSafe=false 模拟 Lua）
	auto id2 = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", goodRun},
			{"when", ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
				return ScriptValue::null();
			})}
		})})}
	})});
	ASSERT_TRUE(id1.isNull());
	ASSERT_TRUE(id2.isNull());
	ASSERT_EQ(errorEvents.load(), 2);
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		ASSERT_EQ(errors.size(), 2u);
		EXPECT_NE(errors[0].find("when"), std::string::npos);
		EXPECT_NE(errors[1].find("when"), std::string::npos);
	}

	wingman::EventHub::instance().unsubscribe(sub);
}

TEST(OrchestrationModuleTest, WhenEvaluatedOnceAcrossRetries) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	auto get = findFn(mod, "get_workflow");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(get.name.empty());

	// 条件求值一次，与重试无关；重试语义属于 run（沿用 task 模块）
	std::atomic<int> whenCalls{0};
	std::atomic<int> attempts{0};
	auto workflowId = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([&]() -> ScriptValue {
				++attempts;
				throw std::runtime_error("always fails");
			})},
			{"when", callable([&]() -> ScriptValue {
				++whenCalls;
				return ScriptValue::fromBool(true);
			})},
			{"maxRetries", ScriptValue::fromInt(1)},
			{"backoffMs", ScriptValue::fromInt(1)}
		})})}
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
	ASSERT_EQ(attempts.load(), 2);  // 首次 + 1 次重试
	EXPECT_EQ(whenCalls.load(), 1);
}

