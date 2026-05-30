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

		int attempts = 0;
		while (attempts <= options_.maxRetries) {
			if (isCanceled()) {
				setStatus(TaskStatus::canceled);
				emitEvent("task.canceled");
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
						// Emit event inside the lock and after setting status
						// Note: emitEvent releases lock before calling EventHub
					}
				}
				// Only emit succeeded event if status is succeeded
				if (status() == TaskStatus::succeeded) {
					emitEvent("task.succeeded");
				}
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
	TaskManager() : nextTaskId_(1) {}

	std::string submit(ScriptValue::CallableFunc work, Task::Options opts) {
		std::string taskId = "task-" + std::to_string(nextTaskId_++);

		auto task = std::make_shared<Task>(taskId, std::move(work), std::move(opts));

		{
			std::lock_guard<std::mutex> lock(mutex_);
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
			std::thread([task]() {
				task->execute();
			}).detach();
		} else {
			// Synchronous execution (safe for Lua)
			task->execute();
		}

		return taskId;
	}

	bool cancel(const std::string& taskId) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = tasks_.find(taskId);
		if (it == tasks_.end()) return false;
		it->second->cancel();
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
	uint64_t nextTaskId_;
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
			if (auto* v = args[1].get("maxRetries")) opts.maxRetries = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("backoffMs")) opts.backoffMs = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("backoffFactor")) opts.backoffFactor = static_cast<float>(v->asFloat());
			if (auto* v = args[1].get("metadata")) opts.metadata = toJson(*v);
			if (auto* v = args[1].get("async")) opts.async = v->asBool();
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
