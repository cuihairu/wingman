#pragma once

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
