#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "task_core.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

namespace {

using task_core::Task;
using task_core::TaskStatus;
using task_core::taskStatusToString;

// 前置任务进入这些终态即视为失败，传播（跳过）给全部后置任务——
// 状态口径沿用 task 模块失败语义（failed/canceled/timeout 均为终态失败）。
bool isTaskFailureStatus(TaskStatus status) {
	return status == TaskStatus::failed || status == TaskStatus::canceled ||
		status == TaskStatus::timeout;
}

// 分支条件真值判定（跨语言统一口径）：Bool 按值；Null 为假；Int/Float
// 非 0 为真；String 非空为真；Array/Object 非空为真；Callable 恒真。
// 不复用 ScriptValue::asBool（非 Bool 类型一律落默认值，Int 1 会被误判假）。
bool isTruthy(const ScriptValue& v) {
	switch (v.type) {
	case ScriptValue::Type::Null: return false;
	case ScriptValue::Type::Bool: return v.boolVal;
	case ScriptValue::Type::Int: return v.intVal != 0;
	case ScriptValue::Type::Float: return v.floatVal != 0.0;
	case ScriptValue::Type::String: return !v.strVal.empty();
	case ScriptValue::Type::Array: return !v.arrayVal.empty();
	case ScriptValue::Type::Object: return !v.objectVal.empty();
	case ScriptValue::Type::Callable: return true;
	}
	return false;
}

// 工作流内任务的调度视图状态：blocked（依赖未满足）/ pending（已就绪待调度）/
// running / skipped（依赖失败传播跳过）/ canceled（cancel_workflow 取消，含
// 从未开工的任务）/ done（已落账，终态经 Task 内核读出）。
enum class WfState { blocked, pending, running, skipped, canceled, done };

struct WfTask {
	std::string id;
	std::vector<std::string> dependsOn;
	ScriptValue::CallableFunc when;  // 分支条件（调度点求值一次，锁外）
	std::shared_ptr<Task> task;
	WfState state = WfState::blocked;
	bool skippedByCondition = false;  // skipped 根因：条件过滤链（不判工作流失败）
};

// 工作流运行实例：单个调度线程按拓扑序串行执行就绪任务（串行语义，
// 并发控制为下一增量）。依赖满足才调度；前置失败/取消沿依赖图传播跳过；
// 分支条件（when）不成立的任务按条件链跳过。
class WorkflowRun {
public:
	WorkflowRun(std::string id, std::string name, std::vector<WfTask> tasks)
		: id_(std::move(id)), name_(std::move(name)), tasks_(std::move(tasks)) {
		// 无依赖任务直接进入就绪态
		for (auto& t : tasks_) {
			if (t.dependsOn.empty()) t.state = WfState::pending;
			idToTask_[t.id] = &t;
		}
	}

	void start() {
		thread_ = std::thread([this]() { scheduleLoop(); });
	}

	// 取消：仅当工作流尚未终局时生效。返回是否发生了取消转换（未知/已终局
	// 的工作流恒 false——cancel_workflow 契约）。从未开工的任务直接置 canceled；
	// 执行中的任务协作取消（Task 内核语义）。
	bool cancelAll() {
		std::vector<std::shared_ptr<Task>> running;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ != "running") return false;
			cancelRequested_ = true;
			for (auto& t : tasks_) {
				if (t.state == WfState::running) {
					running.push_back(t.task);
				} else if (t.state == WfState::blocked || t.state == WfState::pending) {
					t.state = WfState::canceled;
				}
			}
		}
		// 锁外取消执行中任务（Task 方法自持互锁，且不再触发 task.* 事件）
		for (auto& task : running) task->cancel();
		return true;
	}

	// 停机收口：请求取消 + 取消执行中任务 + join 调度线程
	void stop() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			cancelRequested_ = true;
		}
		std::vector<std::shared_ptr<Task>> running;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			for (auto& t : tasks_) {
				if (t.state == WfState::running) running.push_back(t.task);
			}
		}
		for (auto& task : running) task->cancel();
		if (thread_.joinable()) thread_.join();
	}

	ScriptValue snapshot() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return snapshotLocked();
	}

	const std::string& id() const { return id_; }

	const std::string& name() const { return name_; }

private:
	std::string effectiveStatus(const WfTask& t) const {
		switch (t.state) {
		case WfState::blocked: return "blocked";
		case WfState::pending: return "pending";
		case WfState::running: return "running";
		case WfState::skipped: return "skipped";
		case WfState::canceled: return "canceled";
		case WfState::done: return taskStatusToString(t.task->status());
		}
		return "unknown";
	}

	// 前置失败链根因分类：0=无失败；1=条件跳过链（分支过滤，不判工作流
	// 失败）；2=失败链（failed/canceled/timeout 及其传播的 skipped）。
	// 混合前置时失败链优先——任一前置真失败即按失败传播。
	int depFailureKind(const WfTask& d) const {
		if (d.state == WfState::canceled) return 2;
		if (d.state == WfState::done && isTaskFailureStatus(d.task->status())) return 2;
		if (d.state == WfState::skipped) return d.skippedByCondition ? 1 : 2;
		return 0;
	}

	bool depSucceeded(const WfTask& d) const {
		return d.state == WfState::done && d.task->status() == TaskStatus::succeeded;
	}

	// 调用方已持 mutex_
	ScriptValue snapshotLocked() const {
		std::vector<ScriptValue> taskVals;
		taskVals.reserve(tasks_.size());
		for (const auto& t : tasks_) {
			std::vector<ScriptValue> deps;
			deps.reserve(t.dependsOn.size());
			for (const auto& dep : t.dependsOn) {
				deps.push_back(ScriptValue::fromString(dep));
			}
			std::unordered_map<std::string, ScriptValue> obj;
			obj["id"] = ScriptValue::fromString(t.id);
			obj["status"] = ScriptValue::fromString(effectiveStatus(t));
			obj["dependsOn"] = ScriptValue::fromArray(std::move(deps));
			// 失败任务携带错误信息（task 模块语义：异常消息或 "Unknown exception"）
			if (t.state == WfState::done && t.task->status() == TaskStatus::failed) {
				std::string err = t.task->error();
				if (!err.empty()) obj["error"] = ScriptValue::fromString(err);
			}
			taskVals.push_back(ScriptValue::fromObject(std::move(obj)));
		}

		std::unordered_map<std::string, ScriptValue> result;
		result["id"] = ScriptValue::fromString(id_);
		result["name"] = ScriptValue::fromString(name_);
		result["status"] = ScriptValue::fromString(status_);
		result["tasks"] = ScriptValue::fromArray(std::move(taskVals));
		return ScriptValue::fromObject(std::move(result));
	}

	void scheduleLoop() {
		try {
			for (;;) {
				std::unique_lock<std::mutex> lock(mutex_);
				if (cancelRequested_) {
					finalizeLocked();
					return;
				}
				// 传播不动点：依赖全成功 → blocked 升 pending；前置失败链/条件链
				// → blocked/pending 落 skipped（根因随链继承，失败链优先）。
				// 状态单向推进，至多 N 轮收敛。
				bool changed = true;
				while (changed) {
					changed = false;
					for (auto& t : tasks_) {
						if (t.state != WfState::blocked && t.state != WfState::pending) continue;
						int worst = 0;
						bool allSucceeded = true;
						for (const auto& dep : t.dependsOn) {
							const WfTask& d = *idToTask_.at(dep);
							worst = std::max(worst, depFailureKind(d));
							if (!depSucceeded(d)) allSucceeded = false;
						}
						if (worst != 0) {
							t.state = WfState::skipped;
							t.skippedByCondition = worst == 1;
							changed = true;
						} else if (allSucceeded && t.state == WfState::blocked) {
							t.state = WfState::pending;
							changed = true;
						}
					}
				}
				WfTask* cand = nullptr;
				for (auto& t : tasks_) {
					if (t.state == WfState::pending) {
						cand = &t;
						break;
					}
				}
				if (!cand) {
					// 无待调度任务且全部终态：定稿工作流状态
					finalizeLocked();
					return;
				}
				// 分支条件在调度点求值一次：锁外执行（谓词可能重入查询/取消
				// 本工作流，勿持锁调脚本）
				if (cand->when) {
					ScriptValue::CallableFunc when = std::move(cand->when);
					cand->when = nullptr;  // 只求值一次，取消竞态下也不重复
					lock.unlock();
					bool go = true;
					std::string whenErr;
					try {
						go = isTruthy(when({}));
					} catch (const std::exception& e) {
						whenErr = e.what();
					} catch (...) {
						whenErr = "Unknown exception";
					}
					lock.lock();
					// 求值窗口内的取消竞态：pending 已被改写则交还取消路径
					if (cand->state != WfState::pending) continue;
					if (!whenErr.empty()) {
						// 条件求值异常沿用 task 模块失败语义：任务落 failed 并
						// 携带错误信息，沿依赖图按失败链传播
						cand->task->fail("when predicate threw: " + whenErr);
						cand->state = WfState::done;
						continue;
					}
					if (!go) {
						// 条件不成立 = 分支过滤：落条件链 skipped，后置任务由
						// 下一轮不动点按条件链传播（不判工作流失败）
						cand->state = WfState::skipped;
						cand->skippedByCondition = true;
						continue;
					}
				}
				cand->state = WfState::running;
				std::shared_ptr<Task> toRun = cand->task;
				lock.unlock();
				// 执行在锁外：Task 内核自带超时/重试/取消语义，串行调度保证
				// 任一时刻最多一个 running（并发控制为下一增量）
				toRun->execute();
				lock.lock();
				cand->state = WfState::done;
			}
		} catch (...) {
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ == "running") status_ = "failed";
		}
	}

	// 定稿：调用方已持 mutex_，且确认无待运行任务。条件链 skipped 是正常
	// 分支过滤，不判失败；失败链（failed/timeout/失败传播的 skipped）判失败。
	void finalizeLocked() {
		if (status_ != "running") return;
		if (cancelRequested_) {
			status_ = "canceled";
			return;
		}
		bool allGood = true;
		for (const auto& t : tasks_) {
			if (t.state == WfState::done && t.task->status() == TaskStatus::succeeded) continue;
			if (t.state == WfState::skipped && t.skippedByCondition) continue;
			allGood = false;
			break;
		}
		status_ = allGood ? "succeeded" : "failed";
	}

	mutable std::mutex mutex_;
	std::string id_;
	std::string name_;
	std::vector<WfTask> tasks_;
	std::unordered_map<std::string, const WfTask*> idToTask_;
	std::thread thread_;
	bool cancelRequested_ = false;
	std::string status_ = "running";
};

// 工作流管理器（进程内全局）：提交校验、实例登记、停机收口
class WorkflowManager {
public:
	~WorkflowManager() { shutdown(); }

	void shutdown() {
		std::vector<std::shared_ptr<WorkflowRun>> runs;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			for (const auto& [id, run] : workflows_) runs.push_back(run);
			workflows_.clear();
		}
		for (auto& run : runs) run->stop();
	}

	std::string submit(const std::string& name, std::vector<WfTask> tasks) {
		std::string workflowId;
		std::shared_ptr<WorkflowRun> run;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			workflowId = "wf-" + std::to_string(nextWorkflowId_++);
			run = std::make_shared<WorkflowRun>(workflowId, name, std::move(tasks));
			workflows_[workflowId] = run;
		}
		run->start();
		return workflowId;
	}

	bool cancel(const std::string& workflowId) {
		std::shared_ptr<WorkflowRun> run;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = workflows_.find(workflowId);
			if (it == workflows_.end()) return false;
			run = it->second;
		}
		return run->cancelAll();
	}

	ScriptValue get(const std::string& workflowId) const {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = workflows_.find(workflowId);
		if (it == workflows_.end()) return ScriptValue::null();
		return it->second->snapshot();
	}

	std::vector<ScriptValue> getAll() const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<ScriptValue> result;
		result.reserve(workflows_.size());
		for (const auto& [id, run] : workflows_) {
			result.push_back(run->snapshot());
		}
		return result;
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<std::string, std::shared_ptr<WorkflowRun>> workflows_;
	uint64_t nextWorkflowId_ = 1;
};

// Global workflow manager
WorkflowManager g_workflowManager;

// 提交校验失败：发 orchestration.error 事件并返回 null（与 task 模块
// task.error 同一形状，脚本可订阅获知拒绝原因）
ScriptValue rejectWithError(const std::string& reason) {
	EventHub::instance().emit("orchestration.error", {
		{"error", reason}
	}, "orchestration");
	return ScriptValue::null();
}

// 依赖环检测（DFS 三色）：自环与多节点环均返回 false
bool hasNoDependencyCycle(const std::vector<WfTask>& tasks) {
	std::unordered_map<std::string, int> color;  // 0 白 1 灰 2 黑
	std::function<bool(const WfTask&)> visit = [&](const WfTask& t) -> bool {
		color[t.id] = 1;
		for (const auto& dep : t.dependsOn) {
			const auto it = color.find(dep);
			int c = it == color.end() ? 0 : it->second;
			if (c == 1) return false;  // 回边成环
			if (c == 0) {
				const WfTask* depTask = nullptr;
				for (const auto& cand : tasks) {
					if (cand.id == dep) { depTask = &cand; break; }
				}
				if (!depTask || !visit(*depTask)) return false;
			}
		}
		color[t.id] = 2;
		return true;
	};
	for (const auto& t : tasks) {
		if (color[t.id] == 0 && !visit(t)) return false;
	}
	return true;
}

} // namespace

ModuleDescriptor createOrchestrationModule() {
	ModuleDescriptor mod;
	mod.name = "orchestration";

	// submit_workflow(workflow) -> workflowId | null
	// workflow: {name?: string, tasks: [{id, run, dependsOn?, when?,
	//            timeoutMs?, maxRetries?, backoffMs?, backoffFactor?}]}
	// 任务在独立调度线程上执行：run/when 必须是线程安全可调用体
	// （callableThreadSafe——Lua 可调用体一律拒绝，见 task 模块同形门控）。
	// when 在前置满足后的调度点求值一次，不成立落条件链 skipped（不判
	// 工作流失败）；求值异常按 task 模块失败语义落 failed。
	// 拒绝路径（坏定义/环/非线程安全可调用体）发 orchestration.error 事件。
	mod.functions.push_back({"submit_workflow", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isObject()) {
			return rejectWithError("workflow must be an object");
		}
		const ScriptValue& workflow = args[0];

		const ScriptValue* tasksVal = workflow.get("tasks");
		if (!tasksVal || !tasksVal->isArray() || tasksVal->size() == 0) {
			return rejectWithError("workflow.tasks must be a non-empty array");
		}

		std::string name;
		if (const ScriptValue* nameVal = workflow.get("name")) {
			if (!nameVal->isString()) {
				return rejectWithError("workflow.name must be a string");
			}
			name = nameVal->asString();
		}

		std::vector<WfTask> tasks;
		std::unordered_set<std::string> seenIds;
		for (size_t i = 0; i < tasksVal->size(); ++i) {
			const ScriptValue& entry = tasksVal->at(i);
			if (!entry.isObject()) {
				return rejectWithError("task[" + std::to_string(i) + "] must be an object");
			}
			const ScriptValue* idVal = entry.get("id");
			if (!idVal || !idVal->isString() || idVal->asString().empty()) {
				return rejectWithError("task[" + std::to_string(i) + "].id must be a non-empty string");
			}
			const ScriptValue* runVal = entry.get("run");
			if (!runVal || !runVal->isCallable()) {
				return rejectWithError("task[" + std::to_string(i) + "].run must be a callable");
			}

			WfTask task;
			task.id = idVal->asString();
			if (!seenIds.insert(task.id).second) {
				return rejectWithError("duplicate task id '" + task.id + "'");
			}

			if (const ScriptValue* depsVal = entry.get("dependsOn")) {
				if (!depsVal->isArray()) {
					return rejectWithError("task[" + std::to_string(i) + "].dependsOn must be an array of task ids");
				}
				for (size_t j = 0; j < depsVal->size(); ++j) {
					const ScriptValue& dep = depsVal->at(j);
					if (!dep.isString() || dep.asString().empty()) {
						return rejectWithError("task[" + std::to_string(i) + "].dependsOn must be an array of task ids");
					}
					task.dependsOn.push_back(dep.asString());
				}
			}

			// 分支条件：与 run 同门控——条件在调度线程求值，非线程安全
			// 可调用体（Lua）一律拒绝
			if (const ScriptValue* whenVal = entry.get("when")) {
				if (!whenVal->isCallable()) {
					return rejectWithError("task[" + std::to_string(i) + "].when must be a callable");
				}
				if (!whenVal->callableThreadSafe) {
					return rejectWithError(
						"task '" + idVal->asString() + "': when callable is not thread-safe "
						"(e.g. Lua callables). Conditions evaluate on the scheduler "
						"thread; use a Python callable instead.");
				}
				task.when = whenVal->callableVal;
			}

			// 任务执行选项与 task 模块同词汇表、同默认值（失败语义沿用）
			Task::Options opts;
			opts.emitEvents = false;  // 流程级状态事件为后续批次项
			if (const ScriptValue* o = entry.get("timeoutMs")) opts.timeoutMs = static_cast<int>(o->asInt());
			if (const ScriptValue* o = entry.get("maxRetries")) opts.maxRetries = static_cast<int>(o->asInt());
			if (const ScriptValue* o = entry.get("backoffMs")) opts.backoffMs = static_cast<int>(o->asInt());
			if (const ScriptValue* o = entry.get("backoffFactor")) opts.backoffFactor = static_cast<float>(o->asFloat());

			// 线程安全门控：调度线程调用可调用体，非线程安全可调用体（Lua）
			// 一律拒绝——与 task 模块 async 路径同形语义
			if (!runVal->callableThreadSafe) {
				return rejectWithError(
					"task '" + task.id + "': run callable is not thread-safe "
					"(e.g. Lua callables). Workflow tasks execute on a scheduler "
					"thread; use a Python callable instead.");
			}

			task.task = std::make_shared<Task>(task.id, runVal->callableVal, std::move(opts));
			tasks.push_back(std::move(task));
		}

		// 依赖引用与环检测
		std::unordered_set<std::string> knownIds(seenIds.begin(), seenIds.end());
		for (const auto& task : tasks) {
			for (const auto& dep : task.dependsOn) {
				if (!knownIds.count(dep)) {
					return rejectWithError("task '" + task.id + "': unknown dependency '" + dep + "'");
				}
			}
		}
		if (!hasNoDependencyCycle(tasks)) {
			return rejectWithError("dependency cycle detected in workflow.tasks");
		}

		return ScriptValue::fromString(g_workflowManager.submit(name, std::move(tasks)));
	}, "workflow:{name?:string, tasks:[{id:string, run:function, dependsOn?:[string], when?:function, timeoutMs?, maxRetries?, backoffMs?, backoffFactor?}]} -> workflowId:string?"});

	// cancel_workflow(workflowId) -> bool
	// 取消执行中的工作流：执行中任务协作取消，未开工任务直接 canceled；
	// 未知 ID 或已终局的工作流返回 false。
	mod.functions.push_back({"cancel_workflow", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_workflowManager.cancel(args[0].asString()));
	}, "workflowId:string -> bool"});

	// get_workflow(workflowId) -> workflow | null
	// 快照：{id, name, status: running|succeeded|failed|canceled,
	//        tasks: [{id, status, dependsOn, error?}]}
	// 任务状态：blocked/pending/running/skipped/canceled（调度视图）+
	// succeeded/failed/timeout（终态，task 模块语义）。
	mod.functions.push_back({"get_workflow", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();
		return g_workflowManager.get(args[0].asString());
	}, "workflowId:string -> workflow?"});

	// get_all_workflows() -> [workflow]
	mod.functions.push_back({"get_all_workflows", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromArray(g_workflowManager.getAll());
	}, "() -> {workflow}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
