#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"

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

// Task status enum
enum class TaskStatus {
	pending,
	running,
	paused,
	succeeded,
	failed,
	canceled,
	timeout
};

const char* taskStatusToString(TaskStatus status) {
	switch (status) {
	case TaskStatus::pending: return "pending";
	case TaskStatus::running: return "running";
	case TaskStatus::paused: return "paused";
	case TaskStatus::succeeded: return "succeeded";
	case TaskStatus::failed: return "failed";
	case TaskStatus::canceled: return "canceled";
	case TaskStatus::timeout: return "timeout";
	default: return "unknown";
	}
}

// 终态判定（调用方已持 Task::mutex_）
bool isTerminalStatus(TaskStatus status) {
	return status == TaskStatus::succeeded || status == TaskStatus::failed ||
		status == TaskStatus::canceled || status == TaskStatus::timeout;
}

// Task implementation
class Task {
public:
	struct Options {
		int timeoutMs = 30000;
		int maxRetries = 0;
		int backoffMs = 500;
		float backoffFactor = 2.0f;
		nlohmann::json metadata;
		bool async = false;  // Execute in background thread (default: false for Lua safety)
	};

	Task(std::string id, ScriptValue::CallableFunc work, Options opts)
		: id_(std::move(id)), work_(std::move(work)), options_(std::move(opts)) {}

	void execute() {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			// 提交后、开工前被暂停：驻留到 resume（或被 cancel 打断）。
			// resume 按 pausedFrom_ 恢复原状态，开工前的暂停恢复后仍是 pending
			while (status_ == TaskStatus::paused) {
				cond_.wait(lock);
			}
			// pending 才开工；canceled/终态退出；running 为重入（首次调用已开工）
			if (status_ != TaskStatus::pending) return;
			status_ = TaskStatus::running;
			startTime_ = std::chrono::steady_clock::now();
		}

		emitEvent("task.started");

		// Start timeout monitor thread if timeoutMs > 0
		std::thread timeoutThread;
		if (options_.timeoutMs > 0) {
			timeoutThread = std::thread([this]() {
				std::unique_lock<std::mutex> lock(mutex_);
				auto deadline = std::chrono::steady_clock::now() +
					std::chrono::milliseconds(options_.timeoutMs);
				while (!isTerminalStatus(status_)) {
					// 暂停期间超时时钟停走：驻留到解除暂停，deadline 顺延暂停时长。
					// 谓词在进入等待前先求值，pause() 的 notify 不会丢失
					if (status_ == TaskStatus::paused) {
						auto parkedAt = std::chrono::steady_clock::now();
						cond_.wait(lock, [this] { return status_ != TaskStatus::paused; });
						deadline += std::chrono::steady_clock::now() - parkedAt;
						continue;
					}
					if (cond_.wait_until(lock, deadline, [this] {
							return isTerminalStatus(status_) || status_ == TaskStatus::paused;
						})) {
						// 终态（loop top 退出）或转入暂停（park 分支）
						continue;
					}
					// Timeout reached - set status to timeout if still running
					if (status_ == TaskStatus::running) {
						status_ = TaskStatus::timeout;
						// Task::wait 睡在本 cond_ 上，任何终态置位都必须唤醒
						cond_.notify_all();
					}
					return;
				}
				// Task completed (terminal) before timeout
			});
		}

		int attempts = 0;
		while (attempts <= options_.maxRetries) {
			if (isCanceled()) {
				setStatus(TaskStatus::canceled);
				emitEvent("task.canceled");
				if (timeoutThread.joinable()) timeoutThread.join();
				return;
			}

			// Check if already timed out
			if (status() == TaskStatus::timeout) {
				emitEvent("task.timeout");
				if (timeoutThread.joinable()) timeoutThread.join();
				return;
			}

			// 暂停检查点（重试间隙）：驻留到 resume
			{
				std::unique_lock<std::mutex> lock(mutex_);
				while (status_ == TaskStatus::paused) {
					cond_.wait(lock);
				}
			}
			// 驻留期间可能被 cancel/timeout 打断：回到 loop top 走既有出口，
			// 不得落入 try 执行 work
			if (isCanceled() || status() == TaskStatus::timeout) continue;

			try {
				// Execute the work function
				ScriptValue result = work_({});

				{
					std::unique_lock<std::mutex> lock(mutex_);
					// Only set succeeded if status is still running (not timeout/canceled)
					if (status_ == TaskStatus::running) {
						result_ = result;
						status_ = TaskStatus::succeeded;
					} else if (status_ == TaskStatus::paused) {
						// 暂停落在 work 执行中：扣住完成不落账，resume 后再提交；
						// 被 cancel/timeout 打断则丢弃结果
						while (status_ == TaskStatus::paused) {
							cond_.wait(lock);
						}
						if (status_ == TaskStatus::running) {
							result_ = result;
							status_ = TaskStatus::succeeded;
						}
					}
					// Task::wait 与 timeoutThread 都睡在本 cond_ 上：成功置位
					// 同样必须唤醒，否则 wait 只能空等到 deadline（实测 500ms 的
					// work 令 wait(5000) 阻塞满 5s 才返回 true）
					cond_.notify_all();
				}
				// Only emit succeeded event if status is succeeded
				if (status() == TaskStatus::succeeded) {
					emitEvent("task.succeeded");
				} else if (status() == TaskStatus::timeout) {
					emitEvent("task.timeout");
				}
				if (timeoutThread.joinable()) timeoutThread.join();
				return;
			} catch (const std::exception& e) {
				{
					std::lock_guard<std::mutex> lock(mutex_);
					error_ = e.what();
				}
				attempts++;

				if (attempts <= options_.maxRetries) {
					// Backoff before retry
					int backoff = options_.backoffMs *
						static_cast<int>(std::pow(options_.backoffFactor, attempts - 1));
					std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
				}
			} catch (...) {
				{
					std::lock_guard<std::mutex> lock(mutex_);
					error_ = "Unknown exception";
				}
				break;
			}
		}

		// All retries exhausted or error
		setStatus(TaskStatus::failed);
		emitEvent("task.failed");
		if (timeoutThread.joinable()) timeoutThread.join();
	}

	void cancel() {
		bool transitioned = false;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			// 暂停中的任务可直接取消（唤醒驻留点、丢弃扣住的结果）
			if (status_ == TaskStatus::pending || status_ == TaskStatus::running ||
				status_ == TaskStatus::paused) {
				status_ = TaskStatus::canceled;
				transitioned = true;
			}
		}
		cond_.notify_all();
		// 只在真正发生转换时发事件（对终态任务的 cancel 不发误导性 canceled 事件）
		if (transitioned) emitEvent("task.canceled");
	}

	// 暂停：pending/running -> paused。协作式语义——无法挂起正在执行的
	// work，暂停在生命周期检查点生效：开工前驻留、work 完成后扣住结果
	// 不落账、重试间隙不下一次尝试；超时时钟在暂停期间停走。
	bool pause() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ != TaskStatus::pending && status_ != TaskStatus::running) return false;
			pausedFrom_ = status_;
			status_ = TaskStatus::paused;
		}
		cond_.notify_all();
		emitEvent("task.paused");
		return true;
	}

	// 恢复：paused -> pausedFrom_（开工前的暂停恢复为 pending，执行中的
	// 恢复为 running），放行所有驻留检查点（开工、结果落账、重试）
	bool resume() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ != TaskStatus::paused) return false;
			status_ = pausedFrom_;
		}
		cond_.notify_all();
		emitEvent("task.resumed");
		return true;
	}

	TaskStatus status() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return status_;
	}

	ScriptValue result() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return result_;
	}

	std::string error() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return error_;
	}

	nlohmann::json metadata() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return options_.metadata;
	}

	bool wait(int timeoutMs = 30000) {
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
		bool timedOut = false;
		TaskStatus cachedStatus;
		nlohmann::json cachedMetadata;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			// paused 同样视为未完成继续等待
			while (status_ == TaskStatus::pending || status_ == TaskStatus::running ||
				status_ == TaskStatus::paused) {
				if (cond_.wait_until(lock, deadline) == std::cv_status::timeout) {
					if (status_ == TaskStatus::pending || status_ == TaskStatus::running) {
						status_ = TaskStatus::timeout;
						timedOut = true;
						cond_.notify_all();
						// Cache status and metadata BEFORE releasing lock
						cachedStatus = status_;
						cachedMetadata = options_.metadata;
						break;
					}
					if (status_ == TaskStatus::paused) {
						// 暂停是显式行为：等待者放弃只返回 false，不改写任务状态
						// 也不发 task.timeout 事件（任务自身的超时时钟仍挂着）
						return false;
					}
				}
			}
			cachedStatus = status_;
			cachedMetadata = options_.metadata;
		}
		if (timedOut) {
			// Emit event AFTER releasing lock, using cached values
			EventHub::instance().emit("task.timeout", {
				{"taskId", id_},
				{"status", taskStatusToString(cachedStatus)},
				{"metadata", cachedMetadata}
			}, "task");
			return false;
		}
		return true;
	}

	const std::string& id() const { return id_; }
	
	const ScriptValue::CallableFunc& getWork() const { return work_; }
	const Task::Options& getOptions() const { return options_; }

private:
	void setStatus(TaskStatus s) {
		std::lock_guard<std::mutex> lock(mutex_);
		status_ = s;
		cond_.notify_all();
	}

	bool isCanceled() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return status_ == TaskStatus::canceled;
	}

	void emitEvent(const std::string& eventType) {
		EventHub::instance().emit(eventType, {
			{"taskId", id_},
			{"status", taskStatusToString(status())},
			{"metadata", metadata()}
		}, "task");
	}

	std::string id_;
	ScriptValue::CallableFunc work_;
	Options options_;

	mutable std::mutex mutex_;
	std::condition_variable cond_;

	TaskStatus status_ = TaskStatus::pending;
	// 暂停前的状态：resume 恢复它（开工前暂停 -> pending；执行中暂停 -> running）。
	// 若一律恢复 running，「worker 启动前完成 pause+resume」会使 execute 入口
	// 误判重入而直接返回，work 永不执行且无超时监控。
	TaskStatus pausedFrom_ = TaskStatus::pending;
	ScriptValue result_;
	std::string error_;
	std::chrono::steady_clock::time_point startTime_;
};

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
