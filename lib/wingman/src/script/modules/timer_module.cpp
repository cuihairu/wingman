#include "wingman/script/iscript_engine.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace wingman {
namespace script {
namespace modules {

namespace {

using Clock = std::chrono::steady_clock;

// 全局定时器服务：单个工作线程负责计时，到点后在 timer 线程调用脚本回调。
// Python 侧回调经 GIL 包装天然跨线程安全（见 python_marshal.cpp）；Lua 回调
// 非线程安全，与 event.on + 后台线程 emit 的既有敞口一致，由脚本作者知悉使用。
// 对齐 filewatcher 的"锁内摘条目、锁外调用"模式：回调执行不持有锁。
class TimerService {
public:
	static TimerService& instance() {
		static TimerService service;
		return service;
	}

	// intervalMs > 0 表示周期定时器，否则为一次性
	uint64_t start(int64_t delayMs, int64_t intervalMs, ScriptValue::CallableFunc callback) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (stopping_) return 0;
		uint64_t id = nextId_++;
		timers_[id] = Entry{Clock::now() + std::chrono::milliseconds(delayMs), intervalMs,
		                    std::move(callback)};
		cond_.notify_all(); // 新 timer 可能比当前等待的 deadline 更早
		return id;
	}

	bool cancel(uint64_t id) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (timers_.erase(id) == 0) return false;
		}
		cond_.notify_all(); // 等待线程醒来后重新计算最早的 deadline
		return true;
	}

	size_t clearAll() {
		std::lock_guard<std::mutex> lock(mutex_);
		size_t n = timers_.size();
		timers_.clear();
		cond_.notify_all();
		return n;
	}

	size_t count() {
		std::lock_guard<std::mutex> lock(mutex_);
		return timers_.size();
	}

	bool exists(uint64_t id) {
		std::lock_guard<std::mutex> lock(mutex_);
		return timers_.count(id) > 0;
	}

private:
	struct Entry {
		Clock::time_point deadline;
		int64_t intervalMs; // 0 = one-shot
		ScriptValue::CallableFunc callback;
	};

	TimerService() : worker_([this] { run(); }) {}

	~TimerService() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stopping_ = true;
			timers_.clear(); // 丢弃待触发回调，避免进程退出时触碰已析构的脚本引擎
		}
		cond_.notify_all();
		if (worker_.joinable()) worker_.join();
	}

	void run() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (!stopping_) {
			if (timers_.empty()) {
				cond_.wait(lock, [this] { return stopping_ || !timers_.empty(); });
				continue;
			}

			// 找最早到期的条目（timer 数量小，线性扫描足够）
			auto earliest = timers_.begin();
			for (auto it = timers_.begin(); it != timers_.end(); ++it) {
				if (it->second.deadline < earliest->second.deadline) earliest = it;
			}
			uint64_t id = earliest->first;
			Clock::time_point deadline = earliest->second.deadline;

			cond_.wait_until(lock, deadline, [this, id, deadline] {
				return stopping_ || !timers_.count(id) || timers_[id].deadline != deadline;
			});
			if (stopping_) break;

			auto it = timers_.find(id);
			if (it == timers_.end() || it->second.deadline > Clock::now()) {
				continue; // 已取消，或因更早的新 timer 提前醒来
			}

			// 到期：一次性摘除；周期性重排（落后过多时重置基点，避免追赶风暴）
			ScriptValue::CallableFunc callback = it->second.callback;
			int64_t intervalMs = it->second.intervalMs;
			if (intervalMs > 0) {
				Clock::time_point next = it->second.deadline + std::chrono::milliseconds(intervalMs);
				if (next <= Clock::now()) next = Clock::now() + std::chrono::milliseconds(intervalMs);
				it->second.deadline = next;
			} else {
				timers_.erase(it);
			}

			lock.unlock();
			try {
				callback({});
			} catch (const std::exception& e) {
				spdlog::warn("[timer] callback exception for timer {}: {}", id, e.what());
			} catch (...) {
				spdlog::warn("[timer] callback unknown exception for timer {}", id);
			}
			lock.lock();
		}
	}

	std::mutex mutex_;
	std::condition_variable cond_;
	std::unordered_map<uint64_t, Entry> timers_;
	uint64_t nextId_ = 1;
	bool stopping_ = false;
	std::thread worker_;
};

// 从 ScriptValue 参数解析 (ms, callback) 的公共逻辑；失败时返回 false
bool parseTimerArgs(const std::vector<ScriptValue>& args, int64_t& ms,
                    ScriptValue::CallableFunc& callback) {
	if (args.size() < 2) return false;
	if (!args[1].isCallable()) return false;
	ms = std::max<int64_t>(0, args[0].asInt());
	callback = args[1].callableVal;
	return true;
}

} // namespace

// Cleanup function for global state (called during engine shutdown)
void cleanupTimerModule() {
	TimerService::instance().clearAll();
}

ModuleDescriptor createTimerModule() {
	ModuleDescriptor mod;
	mod.name = "timer";

	// after(ms: int, callback: function) -> timerId:int（一次性，到点触发一次）
	mod.functions.push_back({"after", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int64_t ms;
		ScriptValue::CallableFunc callback;
		if (!parseTimerArgs(args, ms, callback)) {
			spdlog::warn("[timer] after requires (ms:int, callback:function)");
			return ScriptValue::fromInt(0);
		}
		return ScriptValue::fromInt(static_cast<int64_t>(
			TimerService::instance().start(ms, 0, std::move(callback))));
	}, "ms:int, callback:function -> timerId:int"});

	// every(ms: int, callback: function) -> timerId:int（周期性，直到 clearTimer）
	mod.functions.push_back({"every", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int64_t ms;
		ScriptValue::CallableFunc callback;
		if (!parseTimerArgs(args, ms, callback)) {
			spdlog::warn("[timer] every requires (ms:int, callback:function)");
			return ScriptValue::fromInt(0);
		}
		return ScriptValue::fromInt(static_cast<int64_t>(
			TimerService::instance().start(ms, ms > 0 ? ms : 1, std::move(callback))));
	}, "ms:int, callback:function -> timerId:int"});

	mod.functions.push_back({"setTimeout", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int64_t ms;
		ScriptValue::CallableFunc callback;
		if (!parseTimerArgs(args, ms, callback)) return ScriptValue::fromInt(0);
		return ScriptValue::fromInt(static_cast<int64_t>(
			TimerService::instance().start(ms, 0, std::move(callback))));
	}, "ms:int, callback:function -> timerId:int"});

	mod.functions.push_back({"setInterval", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int64_t ms;
		ScriptValue::CallableFunc callback;
		if (!parseTimerArgs(args, ms, callback)) return ScriptValue::fromInt(0);
		return ScriptValue::fromInt(static_cast<int64_t>(
			TimerService::instance().start(ms, ms > 0 ? ms : 1, std::move(callback))));
	}, "ms:int, callback:function -> timerId:int"});

	// clearTimer(timerId: int) -> bool（取消一次性或周期定时器）
	mod.functions.push_back({"clearTimer", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(TimerService::instance().cancel(
			static_cast<uint64_t>(args[0].asInt())));
	}, "timerId:int -> bool"});

	mod.functions.push_back({"cancel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(TimerService::instance().cancel(
			static_cast<uint64_t>(args[0].asInt())));
	}, "timerId:int -> bool"});

	mod.functions.push_back({"clearTimeout", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(TimerService::instance().cancel(
			static_cast<uint64_t>(args[0].asInt())));
	}, "timerId:int -> bool"});

	mod.functions.push_back({"clearInterval", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(TimerService::instance().cancel(
			static_cast<uint64_t>(args[0].asInt())));
	}, "timerId:int -> bool"});

	mod.functions.push_back({"exists", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(TimerService::instance().exists(
			static_cast<uint64_t>(args[0].asInt())));
	}, "timerId:int -> bool"});

	mod.functions.push_back({"count", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(static_cast<int64_t>(TimerService::instance().count()));
	}, "() -> int"});

	// clearAll() -> int（清理全部待触发定时器，返回清理数量）
	mod.functions.push_back({"clearAll", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(static_cast<int64_t>(TimerService::instance().clearAll()));
	}, "() -> int"});

	// sleep(ms: int) -> nil（同步阻塞当前脚本线程，Lua/Python 均线程安全）
	mod.functions.push_back({"sleep", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int64_t ms = args.empty() ? 0 : std::max<int64_t>(0, args[0].asInt());
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		return ScriptValue::null();
	}, "ms:int -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
