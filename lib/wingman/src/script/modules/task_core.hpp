#pragma once

// Task 内核：task / orchestration 两模块共用的单任务执行语义（状态机、
// 超时监控、重试退避、协作取消）。自 task_module.cpp 原样抽出，行为逐字保持，
// 仅加 Options::emitEvents 门控（工作流任务关闭 task.* 事件发射——流程级状态
// 事件是后续批次项）。
//
// 线程契约：Task 方法自带互锁，可从任意线程调用；但 work_ 的可调用体
// 线程安全性由调用方保证（task 模块 async 路径与 orchestration 提交时均以
// callableThreadSafe 门控）。

#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace wingman {
namespace script {
namespace modules {
namespace task_core {

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

inline const char* taskStatusToString(TaskStatus status) {
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
inline bool isTerminalStatus(TaskStatus status) {
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
		bool emitEvents = true;  // task.* 事件发射门控（编排任务置 false）
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

	// 调度侧失败落账（工作流分支条件求值异常等任务未开工即失败的路径）：
	// 仅 pending 态生效，置 failed 并记录错误信息。不经过 work/重试路径，
	// 语义与 execute 内的失败落账一致（终态唤醒等待者）。
	bool fail(const std::string& reason) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (status_ != TaskStatus::pending) return false;
			status_ = TaskStatus::failed;
			error_ = reason;
		}
		cond_.notify_all();
		emitEvent("task.failed");
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
			if (options_.emitEvents) {
				EventHub::instance().emit("task.timeout", {
					{"taskId", id_},
					{"status", taskStatusToString(cachedStatus)},
					{"metadata", cachedMetadata}
				}, "task");
			}
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
		if (!options_.emitEvents) return;
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

} // namespace task_core
} // namespace modules
} // namespace script
} // namespace wingman
