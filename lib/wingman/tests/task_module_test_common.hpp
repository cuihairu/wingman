#pragma once
#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createTaskModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;


inline ModuleDescriptor::FunctionEntry findTaskFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

// 轮询等待谓词为真（时限内），替代固定 sleep 以吸收共享机负载时序
inline bool spinUntil(const std::function<bool()>& pred, int timeoutMs = 2000) {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (std::chrono::steady_clock::now() < deadline) {
		if (pred()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return pred();
}

// 轮询等待任务进入指定状态（async worker 收尾会清理注册表，
// 超时后返回最后一次观测值供调用方判定）
inline std::string waitForTaskStatus(const ModuleDescriptor::FunctionEntry& status, const ScriptValue& taskId,
	const std::string& expected, int timeoutMs = 2000) {
	std::string last;
	spinUntil([&] {
		last = status({taskId}).asString();
		return last == expected;
	}, timeoutMs);
	return last;
}


