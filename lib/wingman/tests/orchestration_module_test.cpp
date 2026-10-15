#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"
#include "wingman/process.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
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
