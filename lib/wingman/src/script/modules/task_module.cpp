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
	succeeded,
	failed,
	canceled,
	timeout
};

const char* taskStatusToString(TaskStatus status) {
	switch (status) {
	case TaskStatus::pending: return "pending";
	case TaskStatus::running: return "running";
	case TaskStatus::succeeded: return "succeeded";
	case TaskStatus::failed: return "failed";
	case TaskStatus::canceled: return "canceled";
	case TaskStatus::timeout: return "timeout";
	default: return "unknown";
	}
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
			std::lock_guard<std::mutex> lock(mutex_);
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
				// Wait for timeout or task completion
				if (cond_.wait_for(lock, std::chrono::milliseconds(options_.timeoutMs),
					[this] { return status_ != TaskStatus::running; })) {
					// Task completed before timeout
					return;
				}
				// Timeout reached - set status to timeout if still running
				if (status_ == TaskStatus::running) {
					status_ = TaskStatus::timeout;
				}
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

			try {
				// Execute the work function
				ScriptValue result = work_({});

				{
					std::lock_guard<std::mutex> lock(mutex_);
					// Only set succeeded if status is still running (not timeout/canceled)
					if (status_ == TaskStatus::running) {
						result_ = result;
						status_ = TaskStatus::succeeded;
					}
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
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ == TaskStatus::pending || status_ == TaskStatus::running) {
				status_ = TaskStatus::canceled;
			}
		}
		cond_.notify_all();
		emitEvent("task.canceled");
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
			while (status_ == TaskStatus::pending || status_ == TaskStatus::running) {
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
	case ScriptValue::Null:
		return nullptr;
	case ScriptValue::Bool:
		return value.boolVal;
	case ScriptValue::Int:
		return value.intVal;
	case ScriptValue::Float:
		return value.floatVal;
	case ScriptValue::String:
		return value.strVal;
	case ScriptValue::Array: {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& item : value.arrayVal) {
			arr.push_back(toJson(item));
		}
		return arr;
	}
	case ScriptValue::Object: {
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
