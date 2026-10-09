#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createOrchestrationModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findFn(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

// 轮询等待谓词为真（时限内），替代固定 sleep 以吸收共享机负载时序
bool spinUntil(const std::function<bool()>& pred, int timeoutMs = 5000) {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (std::chrono::steady_clock::now() < deadline) {
		if (pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return pred();
}

// 事件记录器：任务可调用体运行在调度线程，日志经互斥保护
struct EventLog {
	void push(const std::string& event) {
		std::lock_guard<std::mutex> lock(mutex_);
		events_.push_back(event);
	}
	std::vector<std::string> snapshot() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return events_;
	}
	size_t count(const std::string& event) const {
		std::lock_guard<std::mutex> lock(mutex_);
		size_t n = 0;
		for (const auto& e : events_) {
			if (e == event) ++n;
		}
		return n;
	}
	size_t index(const std::string& event) const {
		std::lock_guard<std::mutex> lock(mutex_);
		for (size_t i = 0; i < events_.size(); ++i) {
			if (events_[i] == event) return i;
		}
		return events_.size();
	}

private:
	mutable std::mutex mutex_;
	std::vector<std::string> events_;
};

// 从工作流快照里取任务状态（get_workflow 返回对象，按 id 查找）
std::string taskStatus(const ScriptValue& workflow, const std::string& taskId) {
	const ScriptValue* tasks = workflow.get("tasks");
	if (!tasks || !tasks->isArray()) return "";
	for (size_t i = 0; i < tasks->size(); ++i) {
		const ScriptValue& t = tasks->at(i);
		const ScriptValue* id = t.get("id");
		const ScriptValue* status = t.get("status");
		if (id && status && id->isString() && id->asString() == taskId) {
			return status->asString();
		}
	}
	return "";
}

// 线程安全可调用体工厂（Scheduler 线程调用；测试里用 fromCallable 的
// threadSafe=true 模拟 Python 可调用体）
ScriptValue callable(std::function<ScriptValue()> body) {
	return ScriptValue::fromCallable([body](const std::vector<ScriptValue>&) -> ScriptValue {
		return body();
	}, true);
}

// 作用域结束时强制放行驻留 worker（含断言失败提前返回的路径）——
// 静态 WorkflowManager 析构会 join 全部 worker，卡死会掩盖真实失败
struct ScopedFlag {
	std::atomic<bool>& flag;
	explicit ScopedFlag(std::atomic<bool>& f) : flag(f) {}
	~ScopedFlag() { flag = true; }
};

} // namespace

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
