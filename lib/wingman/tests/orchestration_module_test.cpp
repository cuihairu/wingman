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
