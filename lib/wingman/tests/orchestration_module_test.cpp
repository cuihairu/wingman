#include "orchestration_test_helpers.hpp"

// ========== 提交校验 ==========

TEST(OrchestrationModuleTest, SubmitRejectsMalformedDefinitions) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	// 缺参 / 非对象 / tasks 缺失 / tasks 空数组
	ASSERT_TRUE(submit({}).isNull());
	ASSERT_TRUE(submit({ScriptValue::fromString("nope")}).isNull());
	ASSERT_TRUE(submit({ScriptValue::fromObject({})}).isNull());
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({})}
	})}).isNull());
}

TEST(OrchestrationModuleTest, SubmitRejectsMalformedTaskEntries) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	// 任务非对象
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromInt(1)})}
	})}).isNull());

	// 缺 id / id 非字符串
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"run", callable([] { return ScriptValue::null(); })}
		})})}
	})}).isNull());

	// 缺 run / run 非可调用体
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")}
		})})}
	})}).isNull());
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", ScriptValue::fromInt(7)}
		})})}
	})}).isNull());

	// dependsOn 非数组 / 含非字符串元素
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([] { return ScriptValue::null(); })},
			{"dependsOn", ScriptValue::fromString("x")}
		})})}
	})}).isNull());
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([] { return ScriptValue::null(); })},
			{"dependsOn", ScriptValue::fromArray({ScriptValue::fromInt(1)})}
		})})}
	})}).isNull());

	// 重复 id
	ASSERT_TRUE(submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::null(); })}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", callable([] { return ScriptValue::null(); })}
			})
		})}
	})}).isNull());
}

TEST(OrchestrationModuleTest, SubmitRejectsUnknownDependency) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	auto id = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", callable([] { return ScriptValue::null(); })},
			{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("ghost")})}
		})})}
	})});
	ASSERT_TRUE(id.isNull());
}

TEST(OrchestrationModuleTest, SubmitRejectsDependencyCycles) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	auto run = callable([] { return ScriptValue::null(); });

	// 自依赖
	auto selfDep = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", run},
			{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
		})})}
	})});
	ASSERT_TRUE(selfDep.isNull());

	// 两节点环 a→b→a
	auto twoNode = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(twoNode.isNull());

	// 三节点环 a→b→c→a（a 的 dependsOn 在 c，DFS 从 a 出发应回边命中）
	auto threeNode = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("b")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("c")})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("c")},
				{"run", run},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	})});
	ASSERT_TRUE(threeNode.isNull());
}

TEST(OrchestrationModuleTest, SubmitRejectsNonThreadSafeCallable) {
	auto mod = createOrchestrationModule();
	auto submit = findFn(mod, "submit_workflow");
	ASSERT_FALSE(submit.name.empty());

	// 订阅 orchestration.error 拒绝事件（事件负载 {"error": reason}）
	std::atomic<int> errorEvents{0};
	std::string lastError;
	std::mutex errorMutex;
	auto sub = wingman::EventHub::instance().subscribe("orchestration.error",
		[&](const wingman::EventMessage& msg) {
			errorEvents++;
			std::lock_guard<std::mutex> lock(errorMutex);
			lastError = msg.payload.value("error", "");
		},
		"orchestration-test");

	// threadSafe=false 模拟 Lua 可调用体（fromCallable 第二参默认 false）
	auto luaLike = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	});

	auto id = submit({ScriptValue::fromObject({
		{"tasks", ScriptValue::fromArray({ScriptValue::fromObject({
			{"id", ScriptValue::fromString("a")},
			{"run", luaLike}
		})})}
	})});
	ASSERT_TRUE(id.isNull());
	ASSERT_EQ(errorEvents.load(), 1);
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		ASSERT_FALSE(lastError.empty());
	}

	wingman::EventHub::instance().unsubscribe(sub);
}


// ========== 历史账本（提交/终局落盘 + 启动恢复） ==========
// 账本路径重定向：本进程第一次访问账本前把 WINGMAN_WORKFLOW_LEDGER 指到
// 临时文件，避免测试写入真实 app 数据目录。静态初始化在主程序开始前完成，
// 与本 TU 内任意 TEST 的执行顺序无关。
namespace {

std::string ledgerTestFile() {
	std::string file;
	try {
		const std::filesystem::path p = std::filesystem::temp_directory_path() /
			("wf-ledger-test-" + std::to_string(wingman::Process::getCurrentId()) + ".jsonl");
		std::error_code ec;
		std::filesystem::remove(p, ec);
		file = p.string();
	} catch (...) {
		// 临时目录不可得时不改环境变量，单例退回 app 数据目录
	}
	return file;
}

struct LedgerEnvGuard {
	LedgerEnvGuard() {
		const std::string file = ledgerTestFile();
		if (!file.empty()) {
#ifdef _WIN32
			_putenv_s("WINGMAN_WORKFLOW_LEDGER", file.c_str());
#else
			setenv("WINGMAN_WORKFLOW_LEDGER", file.c_str(), 0);
#endif
		}
	}
};

const LedgerEnvGuard g_ledgerEnvGuard;

} // namespace

TEST(OrchestrationModuleTest, HistoryRegisteredWithSignature) {
	auto mod = createOrchestrationModule();
	const auto history = findFn(mod, "history");
	ASSERT_FALSE(history.name.empty());
	EXPECT_NE(history.signature.find("limit"), std::string::npos);
}

TEST(OrchestrationModuleTest, SubmittedWorkflowPersistsToHistory) {
	auto mod = createOrchestrationModule();
	const auto submit = findFn(mod, "submit_workflow");
	const auto get = findFn(mod, "get_workflow");
	const auto history = findFn(mod, "history");
	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(history.name.empty());

	const ScriptValue aRun = callable([] { return ScriptValue::fromInt(3); });
	const ScriptValue bRun = callable([] { return ScriptValue::fromInt(7); });
	const ScriptValue definition = ScriptValue::fromObject({
		{"name", ScriptValue::fromString("ledger-check")},
		{"tasks", ScriptValue::fromArray({
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("a")},
				{"run", aRun},
				{"dependsOn", ScriptValue::fromArray({})}
			}),
			ScriptValue::fromObject({
				{"id", ScriptValue::fromString("b")},
				{"run", bRun},
				{"dependsOn", ScriptValue::fromArray({ScriptValue::fromString("a")})}
			})
		})}
	});

	const ScriptValue workflowId = submit({definition});
	ASSERT_TRUE(workflowId.isString());

	EXPECT_TRUE(spinUntil([&] {
		const ScriptValue snap = get({workflowId});
		const ScriptValue* status = snap.get("status");
		return status && status->isString() && status->asString() == "succeeded";
	}));

	// 终局账本落盘与调度线程异步：轮询至本条工作流的终局记录出现
	const std::string want = workflowId.asString();
	EXPECT_TRUE(spinUntil([&] {
		const ScriptValue records = history({ScriptValue::fromInt(500)});
		if (!records.isArray()) return false;
		for (size_t i = 0; i < records.size(); ++i) {
			const ScriptValue rec = records.at(i);
			const ScriptValue* id = rec.get("id");
			if (!id || !id->isString() || id->asString() != want) continue;
			const ScriptValue* status = rec.get("status");
			if (!status || !status->isString() || status->asString() != "succeeded") return false;
			const ScriptValue* tasks = rec.get("tasks");
			if (!tasks) return false;
			const ScriptValue* taskB = tasks->get("b");
			return taskB && taskB->isString() && taskB->asString() == "succeeded";
		}
		return false;
	}));

	// 默认 limit 生效：视图非空，每项必带 status
	const ScriptValue small = history({});
	ASSERT_TRUE(small.isArray());
	EXPECT_GT(small.size(), 0u);
	for (size_t i = 0; i < small.size(); ++i) {
		EXPECT_TRUE(small.at(i).get("status")) << "view " << i << " missing status";
	}
}
