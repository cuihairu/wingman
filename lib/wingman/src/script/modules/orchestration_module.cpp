#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "task_core.hpp"

#include <nlohmann/json.hpp>

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

// 工作流内任务的调度视图状态：blocked（依赖未满足）/ pending（已就绪待调度）/
// running / skipped（依赖失败传播跳过）/ canceled（cancel_workflow 取消，含
// 从未开工的任务）/ done（已落账，终态经 Task 内核读出）。
enum class WfState { blocked, pending, running, skipped, canceled, done };

struct WfTask {
	std::string id;
	std::vector<std::string> dependsOn;
	std::shared_ptr<Task> task;
	WfState state = WfState::blocked;
};

// 工作流运行实例：单个调度线程按拓扑序串行执行就绪任务（v1 串行语义，
// 并发控制见后续批次）。依赖满足才调度；前置失败/取消沿依赖图传播跳过。
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

	bool depFailed(const WfTask& d) const {
		if (d.state == WfState::skipped || d.state == WfState::canceled) return true;
		return d.state == WfState::done && isTaskFailureStatus(d.task->status());
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
				std::shared_ptr<Task> toRun;
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (cancelRequested_) {
						finalizeLocked();
						return;
					}
					// 传播不动点：依赖全成功 → blocked 升 pending；任一前置失败 →
					// blocked/pending 落 skipped。状态单向推进，至多 N 轮收敛。
					bool changed = true;
					while (changed) {
						changed = false;
						for (auto& t : tasks_) {
							if (t.state != WfState::blocked && t.state != WfState::pending) continue;
							bool anyFailed = false;
							bool allSucceeded = true;
							for (const auto& dep : t.dependsOn) {
								const WfTask& d = *idToTask_.at(dep);
								if (depFailed(d)) anyFailed = true;
								if (!depSucceeded(d)) allSucceeded = false;
							}
							if (anyFailed) {
								t.state = WfState::skipped;
								changed = true;
							} else if (allSucceeded && t.state == WfState::blocked) {
								t.state = WfState::pending;
								changed = true;
							}
						}
					}
					for (auto& t : tasks_) {
						if (t.state == WfState::pending) {
							t.state = WfState::running;
							toRun = t.task;
							break;
						}
					}
					if (!toRun) {
						// 无待调度任务且全部终态：定稿工作流状态
						finalizeLocked();
						return;
					}
				}
				// 执行在锁外：Task 内核自带超时/重试/取消语义，串行调度保证
				// 任一时刻最多一个 running（v1 并发控制留后续批次）
				toRun->execute();
				{
					std::lock_guard<std::mutex> lock(mutex_);
					for (auto& t : tasks_) {
						if (t.task == toRun) {
							t.state = WfState::done;
							break;
						}
					}
				}
			}
		} catch (...) {
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ == "running") status_ = "failed";
		}
	}

	// 定稿：调用方已持 mutex_，且确认无待运行任务
	void finalizeLocked() {
		if (status_ != "running") return;
		if (cancelRequested_) {
			status_ = "canceled";
			return;
		}
		bool allSucceeded = true;
		for (const auto& t : tasks_) {
			if (effectiveStatus(t) != "succeeded") {
				allSucceeded = false;
				break;
			}
		}
		status_ = allSucceeded ? "succeeded" : "failed";
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
	// workflow: {name?: string, tasks: [{id, run, dependsOn?, timeoutMs?,
	//            maxRetries?, backoffMs?, backoffFactor?}]}
	// 任务在独立调度线程上执行：run 必须是线程安全可调用体
	// （callableThreadSafe——Lua 可调用体一律拒绝，见 task 模块同形门控）。
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
	}, "workflow:{name?:string, tasks:[{id:string, run:function, dependsOn?:[string], timeoutMs?, maxRetries?, backoffMs?, backoffFactor?}]} -> workflowId:string?"});

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
