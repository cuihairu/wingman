#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "task_core.hpp"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace wingman {
namespace script {
namespace modules {

namespace {

using task_core::Task;
using task_core::TaskStatus;
using task_core::isTerminalStatus;
using task_core::taskStatusToString;

// Task manager
class TaskManager {
public:
	TaskManager() : nextTaskId_(1), shutdown_(false) {}

	~TaskManager() {
		shutdown();
	}

	void shutdown() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			shutdown_ = true;
		}

		// 先取消所有未终态任务再 join：暂停中的任务可能无限驻留（超时时钟
		// 已停走），cancel 唤醒其驻留点，停机才能收敛
		std::vector<std::shared_ptr<Task>> active;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			for (const auto& entry : tasks_) active.push_back(entry.second);
		}
		for (const auto& task : active) task->cancel();

		// Wait for all worker threads to finish
		for (auto& worker : workers_) {
			if (worker.joinable()) {
				worker.join();
			}
		}
		workers_.clear();
	}

	std::string submit(ScriptValue::CallableFunc work, Task::Options opts) {
		std::shared_ptr<Task> task;
		std::string taskId;

		{
			std::lock_guard<std::mutex> lock(mutex_);
			taskId = "task-" + std::to_string(nextTaskId_++);
			task = std::make_shared<Task>(taskId, std::move(work), std::move(opts));
			tasks_[taskId] = task;
		}

		// Emit submitted event
		EventHub::instance().emit("task.submitted", {
			{"taskId", taskId},
			{"metadata", opts.metadata}
		}, "task");

		// Execute in background thread only if async is true
		// Note: Lua is NOT thread-safe, so async must be false for Lua callbacks
		if (opts.async) {
			// Use managed thread instead of detached
			std::lock_guard<std::mutex> lock(mutex_);
			if (shutdown_) {
				return ""; // Reject if shutdown started
			}
			workers_.emplace_back([this, task]() {
				task->execute();
				cleanupFinishedTasks();
			});
		} else {
			// Synchronous execution (safe for Lua)
			task->execute();
		}

		return taskId;
	}

	bool cancel(const std::string& taskId) {
		std::shared_ptr<Task> task;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = tasks_.find(taskId);
			if (it == tasks_.end()) return false;
			task = it->second;
		}
		// Call cancel() after releasing lock to avoid deadlock on task.canceled event
		task->cancel();
		return true;
	}

	// 暂停/恢复与 cancel 同款纪律：锁外调用 task 方法，避免 task.paused/
	// task.resumed 事件回调内再进管理器造成死锁
	bool pause(const std::string& taskId) {
		std::shared_ptr<Task> task;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = tasks_.find(taskId);
			if (it == tasks_.end()) return false;
			task = it->second;
		}
		return task->pause();
	}

	bool resume(const std::string& taskId) {
		std::shared_ptr<Task> task;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = tasks_.find(taskId);
			if (it == tasks_.end()) return false;
			task = it->second;
		}
		return task->resume();
	}

	TaskStatus status(const std::string& taskId) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tasks_.find(taskId);
		if (it == tasks_.end()) return TaskStatus::failed;
		return it->second->status();
	}

	ScriptValue result(const std::string& taskId) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tasks_.find(taskId);
		if (it == tasks_.end()) return ScriptValue::null();
		return it->second->result();
	}

	std::string error(const std::string& taskId) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tasks_.find(taskId);
		if (it == tasks_.end()) return "Task not found";
		return it->second->error();
	}

	bool wait(const std::string& taskId, int timeoutMs) {
		std::shared_ptr<Task> task;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = tasks_.find(taskId);
			if (it == tasks_.end()) return false;
			task = it->second;
		}
		return task->wait(timeoutMs);
	}

	bool retry(const std::string& taskId, Task::Options opts) {
		std::shared_ptr<Task> oldTask;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = tasks_.find(taskId);
			if (it == tasks_.end()) return false;
			oldTask = it->second;
		}

		// Use the original work function with new options
		std::string newTaskId = submit(oldTask->getWork(), std::move(opts));

		// Optionally, we could update the original task ID to point to the new task
		// For now, just return success
		return !newTaskId.empty();
	}

private:
	std::unordered_map<std::string, std::shared_ptr<Task>> tasks_;
	std::mutex mutex_;
	std::vector<std::thread> workers_;
	std::atomic<bool> shutdown_;
	uint64_t nextTaskId_;

	void cleanupFinishedTasks() {
		std::lock_guard<std::mutex> lock(mutex_);
		// Remove completed/failed/canceled tasks to prevent memory leak
		auto it = tasks_.begin();
		while (it != tasks_.end()) {
			auto status = it->second->status();
			if (status == TaskStatus::succeeded ||
				status == TaskStatus::failed ||
				status == TaskStatus::canceled ||
				status == TaskStatus::timeout) {
				it = tasks_.erase(it);
			} else {
				++it;
			}
		}
	}
};

// Global task manager
TaskManager g_taskManager;

// Helper to convert ScriptValue to JSON
nlohmann::json toJson(const ScriptValue& value) {
	switch (value.type) {
	case ScriptValue::Type::Null:
		return nullptr;
	case ScriptValue::Type::Bool:
		return value.boolVal;
	case ScriptValue::Type::Int:
		return value.intVal;
	case ScriptValue::Type::Float:
		return value.floatVal;
	case ScriptValue::Type::String:
		return value.strVal;
	case ScriptValue::Type::Array: {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& item : value.arrayVal) {
			arr.push_back(toJson(item));
		}
		return arr;
	}
	case ScriptValue::Type::Object: {
		nlohmann::json obj = nlohmann::json::object();
		for (const auto& [key, item] : value.objectVal) {
			obj[key] = toJson(item);
		}
		return obj;
	}
	default:
		return nullptr;
	}
}

} // namespace

ModuleDescriptor createTaskModule() {
	ModuleDescriptor mod;
	mod.name = "task";

	// submit(work, options?) -> taskId
	mod.functions.push_back({"submit", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isCallable()) {
			return ScriptValue::fromBool(false);
		}

		Task::Options opts;
		opts.timeoutMs = 30000;
		opts.maxRetries = 0;
		opts.backoffMs = 500;
		opts.backoffFactor = 2.0f;

		if (args.size() > 1 && args[1].isObject()) {
			if (auto* v = args[1].get("timeoutMs")) opts.timeoutMs = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("metadata")) opts.metadata = toJson(*v);
			if (auto* v = args[1].get("async")) opts.async = v->asBool();

			// Support both flat (maxRetries, backoffMs, backoffFactor) and nested (retry.max, retry.backoffMs, retry.factor) formats
			// Nested format takes precedence if both are provided
			if (auto* retry = args[1].get("retry")) {
				if (retry->isObject()) {
					if (auto* v = retry->get("max")) opts.maxRetries = static_cast<int>(v->asInt());
					if (auto* v = retry->get("backoffMs")) opts.backoffMs = static_cast<int>(v->asInt());
					if (auto* v = retry->get("factor")) opts.backoffFactor = static_cast<float>(v->asFloat());
				}
			} else {
				// Fall back to flat format
				if (auto* v = args[1].get("maxRetries")) opts.maxRetries = static_cast<int>(v->asInt());
				if (auto* v = args[1].get("backoffMs")) opts.backoffMs = static_cast<int>(v->asInt());
				if (auto* v = args[1].get("backoffFactor")) opts.backoffFactor = static_cast<float>(v->asFloat());
			}
		}

		// Runtime check: reject async=true for non-thread-safe callables (e.g., Lua functions)
		if (opts.async && !args[0].callableThreadSafe) {
			EventHub::instance().emit("task.error", {
				{"error", "async=true is not supported for non-thread-safe callables (e.g., Lua functions). Use async=false or switch to Python."}
			}, "task");
			return ScriptValue::fromBool(false);
		}

		std::string taskId = g_taskManager.submit(args[0].callableVal, std::move(opts));
		return ScriptValue::fromString(taskId);
	}, "work:function, options?:{timeoutMs?,maxRetries?,backoffMs?,backoffFactor?,metadata?,async?} -> taskId:string"});

	// cancel(taskId) -> bool
	mod.functions.push_back({"cancel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_taskManager.cancel(args[0].asString()));
	}, "taskId:string -> bool"});

	// pause(taskId) -> bool
	// 协作式暂停：pending/running -> paused。暂停在生命周期检查点生效
	// （开工前驻留、work 完成后扣住结果、重试间隙停试），超时时钟停走；
	// 非 pending/running 态返回 false。
	mod.functions.push_back({"pause", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_taskManager.pause(args[0].asString()));
	}, "taskId:string -> bool"});

	// resume(taskId) -> bool
	// paused -> running，放行所有驻留检查点；非 paused 态返回 false。
	mod.functions.push_back({"resume", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_taskManager.resume(args[0].asString()));
	}, "taskId:string -> bool"});

	// status(taskId) -> status
	mod.functions.push_back({"status", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromString("failed");
		return ScriptValue::fromString(taskStatusToString(g_taskManager.status(args[0].asString())));
	}, "taskId:string -> status:string"});

	// wait(taskId, timeoutMs?) -> bool
	mod.functions.push_back({"wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);
		int timeoutMs = args.size() > 1 && args[1].isInt() ? static_cast<int>(args[1].asInt()) : 30000;
		return ScriptValue::fromBool(g_taskManager.wait(args[0].asString(), timeoutMs));
	}, "taskId:string, timeoutMs?:int -> bool"});

	// result(taskId) -> result
	mod.functions.push_back({"result", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();
		return g_taskManager.result(args[0].asString());
	}, "taskId:string -> result:any"});

	// error(taskId) -> error
	mod.functions.push_back({"error", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromString("Task not found");
		return ScriptValue::fromString(g_taskManager.error(args[0].asString()));
	}, "taskId:string -> error:string"});

	// retry(taskId, options?) -> bool
	mod.functions.push_back({"retry", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::fromBool(false);

		Task::Options opts;
		opts.timeoutMs = 30000;
		opts.maxRetries = 0;

		if (args.size() > 1 && args[1].isObject()) {
			if (auto* v = args[1].get("maxRetries")) opts.maxRetries = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("backoffMs")) opts.backoffMs = static_cast<int>(v->asInt());
		}

		return ScriptValue::fromBool(g_taskManager.retry(args[0].asString(), std::move(opts)));
	}, "taskId:string, options?:{maxRetries?,backoffMs?} -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
