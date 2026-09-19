#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createTimerModule();
void cleanupTimerModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findTimerFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

// 轮询等待条件成立（上限 timeoutMs），避免固定 sleep 导致的 flaky
bool waitFor(const std::function<bool()>& cond, int timeoutMs) {
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (std::chrono::steady_clock::now() < deadline) {
		if (cond()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return cond();
}

class TimerModuleTest : public ::testing::Test {
protected:
	void TearDown() override {
		// 每个用例结束清理全局定时器，避免用例间泄漏
		cleanupTimerModule();
	}
};

TEST_F(TimerModuleTest, AfterFiresOnce) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	ASSERT_FALSE(after.name.empty());

	std::atomic<int> fired{0};
	auto id = after({
		ScriptValue::fromInt(20),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})
	});
	EXPECT_GT(id.asInt(), 0);

	EXPECT_TRUE(waitFor([&fired] { return fired.load() > 0; }, 2000));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(fired.load(), 1); // 一次性：不再重复触发
}

TEST_F(TimerModuleTest, EveryFiresRepeatedly) {
	auto mod = createTimerModule();
	auto every = findTimerFunction(mod, "every");
	ASSERT_FALSE(every.name.empty());

	std::atomic<int> fired{0};
	auto id = every({
		ScriptValue::fromInt(20),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})
	});
	EXPECT_GT(id.asInt(), 0);

	EXPECT_TRUE(waitFor([&fired] { return fired.load() >= 3; }, 2000));
}

TEST_F(TimerModuleTest, CancelPreventsFire) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	auto clearTimer = findTimerFunction(mod, "clearTimer");
	ASSERT_FALSE(after.name.empty());
	ASSERT_FALSE(clearTimer.name.empty());

	std::atomic<int> fired{0};
	auto id = after({
		ScriptValue::fromInt(50),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})
	});
	EXPECT_TRUE(clearTimer({id}).asBool());

	std::this_thread::sleep_for(std::chrono::milliseconds(120));
	EXPECT_EQ(fired.load(), 0);

	// 重复取消返回 false
	EXPECT_FALSE(clearTimer({id}).asBool());
}

TEST_F(TimerModuleTest, CancelIntervalStopsRepeats) {
	auto mod = createTimerModule();
	auto every = findTimerFunction(mod, "every");
	auto clearInterval = findTimerFunction(mod, "clearInterval");
	ASSERT_FALSE(every.name.empty());
	ASSERT_FALSE(clearInterval.name.empty());

	std::atomic<int> fired{0};
	auto id = every({
		ScriptValue::fromInt(20),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})
	});
	EXPECT_TRUE(waitFor([&fired] { return fired.load() >= 2; }, 2000));
	EXPECT_TRUE(clearInterval({id}).asBool());

	int atCancel = fired.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	EXPECT_LE(fired.load(), atCancel + 1); // 取消后最多再触发一次在途回调
}

TEST_F(TimerModuleTest, CallbackExceptionDoesNotKillWorker) {
	auto mod = createTimerModule();
	auto every = findTimerFunction(mod, "every");
	ASSERT_FALSE(every.name.empty());

	std::atomic<int> fired{0};
	auto id = every({
		ScriptValue::fromInt(15),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) -> ScriptValue {
			fired.fetch_add(1);
			throw std::runtime_error("callback boom");
		})
	});
	(void)id;

	// 抛异常的回调不应终止 timer 线程：后续周期仍会触发
	EXPECT_TRUE(waitFor([&fired] { return fired.load() >= 3; }, 2000));
}

TEST_F(TimerModuleTest, ExistsAndCount) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	auto exists = findTimerFunction(mod, "exists");
	auto count = findTimerFunction(mod, "count");
	ASSERT_FALSE(after.name.empty());
	ASSERT_FALSE(exists.name.empty());
	ASSERT_FALSE(count.name.empty());

	auto idA = after({ScriptValue::fromInt(1000), ScriptValue::fromCallable(
		[](const std::vector<ScriptValue>&) { return ScriptValue::null(); })});
	auto idB = after({ScriptValue::fromInt(1000), ScriptValue::fromCallable(
		[](const std::vector<ScriptValue>&) { return ScriptValue::null(); })});
	EXPECT_TRUE(exists({idA}).asBool());
	EXPECT_TRUE(exists({idB}).asBool());
	EXPECT_GE(count({}).asInt(), 2);

	// 不存在的 id
	EXPECT_FALSE(exists({ScriptValue::fromInt(99999999)}).asBool());

	// 触发后不存在
	auto idC = after({ScriptValue::fromInt(10), ScriptValue::fromCallable(
		[](const std::vector<ScriptValue>&) { return ScriptValue::null(); })});
	EXPECT_TRUE(waitFor([&] { return !exists({idC}).asBool(); }, 2000));
}

TEST_F(TimerModuleTest, ClearAllDropsPending) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	auto clearAll = findTimerFunction(mod, "clearAll");
	ASSERT_FALSE(after.name.empty());
	ASSERT_FALSE(clearAll.name.empty());

	std::atomic<int> fired{0};
	for (int i = 0; i < 5; ++i) {
		after({ScriptValue::fromInt(200), ScriptValue::fromCallable([&fired](
			const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})});
	}

	auto cleared = clearAll({}).asInt();
	EXPECT_GE(cleared, 5);
	EXPECT_EQ(findTimerFunction(mod, "count")({}).asInt(), 0);

	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	EXPECT_EQ(fired.load(), 0);
}

TEST_F(TimerModuleTest, InvalidArgsReturnZeroId) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	auto every = findTimerFunction(mod, "every");
	ASSERT_FALSE(after.name.empty());
	ASSERT_FALSE(every.name.empty());

	// 缺参数 / 回调不可调用
	EXPECT_EQ(after({}).asInt(), 0);
	EXPECT_EQ(after({ScriptValue::fromInt(10)}).asInt(), 0);
	EXPECT_EQ(after({ScriptValue::fromInt(10), ScriptValue::fromString("not-a-fn")}).asInt(), 0);
	EXPECT_EQ(every({}).asInt(), 0);
	EXPECT_EQ(every({ScriptValue::fromString("x"), ScriptValue::fromString("y")}).asInt(), 0);
}

TEST_F(TimerModuleTest, CancelInvalidArgsSafe) {
	auto mod = createTimerModule();
	auto clearTimer = findTimerFunction(mod, "clearTimer");
	ASSERT_FALSE(clearTimer.name.empty());

	EXPECT_FALSE(clearTimer({}).asBool());
	EXPECT_FALSE(clearTimer({ScriptValue::fromInt(0)}).asBool());
	EXPECT_FALSE(clearTimer({ScriptValue::fromInt(-1)}).asBool());
}

TEST_F(TimerModuleTest, CallbackReceivesNoArgs) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	ASSERT_FALSE(after.name.empty());

	std::atomic<bool> argCountOk{false};
	after({ScriptValue::fromInt(10), ScriptValue::fromCallable([&argCountOk](
		const std::vector<ScriptValue>& cbArgs) {
		argCountOk = cbArgs.empty();
		return ScriptValue::null();
	})});
	EXPECT_TRUE(waitFor([&] { return argCountOk.load(); }, 2000));
}

TEST_F(TimerModuleTest, SleepBlocksAtLeastRequested) {
	auto mod = createTimerModule();
	auto sleepFn = findTimerFunction(mod, "sleep");
	ASSERT_FALSE(sleepFn.name.empty());

	auto start = std::chrono::steady_clock::now();
	sleepFn({ScriptValue::fromInt(60)});
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start).count();
	EXPECT_GE(elapsed, 55); // 留少量调度余量

	// 负值安全（视为 0，立即返回）
	auto t2 = std::chrono::steady_clock::now();
	sleepFn({ScriptValue::fromInt(-5)});
	EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t2).count(), 100);
}

TEST_F(TimerModuleTest, CleanupDropsAllTimers) {
	auto mod = createTimerModule();
	auto after = findTimerFunction(mod, "after");
	auto count = findTimerFunction(mod, "count");
	ASSERT_FALSE(after.name.empty());
	ASSERT_FALSE(count.name.empty());

	std::atomic<int> fired{0};
	after({ScriptValue::fromInt(30), ScriptValue::fromCallable([&fired](
		const std::vector<ScriptValue>&) {
		fired.fetch_add(1);
		return ScriptValue::null();
	})});
	EXPECT_GT(count({}).asInt(), 0);

	cleanupTimerModule();
	EXPECT_EQ(count({}).asInt(), 0);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(fired.load(), 0);
}

} // namespace
