#include <gtest/gtest.h>

#include "wingman/script/module_registry.hpp"
#include "wingman/system.hpp"
#include "wingman/human.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// 第九批覆盖率收口（2026-09-23）：timer_module_test.cpp 覆盖了 after/every/
// cancel 老入口，本文件补齐 setTimeout/setInterval/clearTimeout/clearInterval
// 等价入口与 cancel 正路径；system 胶水补 getCpuUsage/getDiskInfo 带参/
// getDisplayInfo 的 xrandr 解析链（PATH 注入 fake xrandr，真实 popen 链路）；
// human 胶水补 setConfig 通用入口的 move_speed/typing_variance 分支
// （现有测试只走 setMoveSpeed/setTypingVariance 专用函数）。

namespace {

ModuleDescriptor getModule(const std::string& name) {
	for (auto& mod : getAllModules()) {
		if (mod.name == name) return mod;
	}
	return {};
}

ScriptValue call(const ModuleDescriptor& mod, const std::string& fn, std::vector<ScriptValue> args = {}) {
	for (const auto& f : mod.functions) {
		if (f.name == fn) return f.func(args);
	}
	return ScriptValue::null();
}

ScriptValue nullCallback() {
	return ScriptValue::fromCallable([](const std::vector<ScriptValue>&) {
		return ScriptValue::null();
	});
}

bool waitFor(const std::function<bool()>& cond, int timeoutMs) {
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (std::chrono::steady_clock::now() < deadline) {
		if (cond()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return cond();
}

} // anonymous namespace

// ========== timer 胶水补齐 ==========

TEST(TimerGlueRoundtripTest, SetTimeoutFiresCallbackOnce) {
	auto mod = getModule("timer");
	ASSERT_FALSE(mod.name.empty());

	std::atomic<bool> fired{false};
	auto id = call(mod, "setTimeout", {ScriptValue::fromInt(20),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired = true;
			return ScriptValue::null();
		})});
	EXPECT_GT(id.asInt(), 0);
	EXPECT_TRUE(waitFor([&] { return fired.load(); }, 2000));
	EXPECT_TRUE(waitFor([&] { return call(mod, "count", {}).asInt() == 0; }, 2000));
}

TEST(TimerGlueRoundtripTest, SetTimeoutRejectsBadArgs) {
	auto mod = getModule("timer");
	ASSERT_FALSE(mod.name.empty());

	EXPECT_EQ(call(mod, "setTimeout", {}).asInt(), 0);                          // 缺参
	EXPECT_EQ(call(mod, "setTimeout", {ScriptValue::fromInt(10)}).asInt(), 0);  // 缺回调
	EXPECT_EQ(call(mod, "setTimeout", {ScriptValue::fromInt(10),
		ScriptValue::fromString("not-callable")}).asInt(), 0);
	EXPECT_EQ(call(mod, "setInterval", {ScriptValue::fromString("x"),
		ScriptValue::fromString("y")}).asInt(), 0);
}

TEST(TimerGlueRoundtripTest, SetIntervalFiresRepeatedlyUntilCancel) {
	auto mod = getModule("timer");
	ASSERT_FALSE(mod.name.empty());

	std::atomic<int> fired{0};
	auto id = call(mod, "setInterval", {ScriptValue::fromInt(30),
		ScriptValue::fromCallable([&fired](const std::vector<ScriptValue>&) {
			fired.fetch_add(1);
			return ScriptValue::null();
		})});
	ASSERT_GT(id.asInt(), 0);

	EXPECT_TRUE(waitFor([&] { return fired.load() >= 2; }, 2000));

	// cancel 正路径：周期定时器取消返回 true，此后不再增长
	EXPECT_TRUE(call(mod, "cancel", {id}).asBool());
	std::this_thread::sleep_for(std::chrono::milliseconds(120));
	int stopped = fired.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(120));
	EXPECT_EQ(fired.load(), stopped);
	EXPECT_FALSE(call(mod, "exists", {id}).asBool());
}

TEST(TimerGlueRoundtripTest, ClearAliasesStopTimers) {
	auto mod = getModule("timer");
	ASSERT_FALSE(mod.name.empty());

	auto timeoutId = call(mod, "setTimeout", {ScriptValue::fromInt(5000), nullCallback()});
	auto intervalId = call(mod, "setInterval", {ScriptValue::fromInt(5000), nullCallback()});
	ASSERT_GT(timeoutId.asInt(), 0);
	ASSERT_GT(intervalId.asInt(), 0);

	EXPECT_TRUE(call(mod, "clearTimeout", {timeoutId}).asBool());
	EXPECT_TRUE(call(mod, "clearInterval", {intervalId}).asBool());
	// 别名函数缺参 false / 未知 id false
	EXPECT_FALSE(call(mod, "clearTimeout", {}).asBool());
	EXPECT_FALSE(call(mod, "clearInterval", {}).asBool());
	EXPECT_FALSE(call(mod, "cancel", {}).asBool());
	EXPECT_FALSE(call(mod, "cancel", {ScriptValue::fromInt(98765432)}).asBool());
}

// ========== system 胶水补齐 ==========

TEST(SystemGlueRoundtripTest, GetCpuUsageSaneAfterWarmup) {
	auto mod = getModule("system");
	ASSERT_FALSE(mod.name.empty());

	auto first = call(mod, "getCpuUsage");
	EXPECT_TRUE(first.isInt()); // 首调返回 0（首次采样基线）
	std::this_thread::sleep_for(std::chrono::milliseconds(60));
	auto second = call(mod, "getCpuUsage");
	EXPECT_GE(second.asInt(), 0);
	EXPECT_LE(second.asInt(), 100);
}

TEST(SystemGlueRoundtripTest, GetDiskInfoWithPathReturnsSingleObject) {
	auto mod = getModule("system");
	ASSERT_FALSE(mod.name.empty());

	auto result = call(mod, "getDiskInfo", {ScriptValue::fromString("/")});
	ASSERT_TRUE(result.isObject());
	const auto* total = result.get("total");
	ASSERT_NE(total, nullptr);
	EXPECT_GT(total->asInt(), 0);
}

// xrandr 解析链：getCommandOutput 经 popen("xrandr ...") 继承 PATH，注入 fake
// 脚本即可驱动 platform 层解析循环（connected/disconnected/primary/分辨率）
#if defined(__linux__)
TEST(SystemGlueRoundtripTest, GetDisplayInfoParsesFakeXrandrOutput) {
	namespace fs = std::filesystem;
	auto fakeDir = fs::temp_directory_path() / "wg_fake_xrandr_bin";
	fs::create_directories(fakeDir);
	auto scriptPath = fakeDir / "xrandr";
	{
		std::ofstream out(scriptPath, std::ios::trunc);
		out << "#!/bin/sh\ncat <<'XRANDEOF'\n"
		    << "Screen 0: minimum 8 x 8, current 1920 x 1080, maximum 1920 x 1080\n"
		    << "XVFB-1 connected primary 1920x1080+0+0 (normal left inverted right x axis y axis) 0mm x 0mm\n"
		    << "HDMI-0 disconnected (normal left inverted right x axis y axis)\n"
		    << "XRANDEOF\n";
	}
	::chmod(scriptPath.c_str(), 0755);

	const char* oldPath = ::getenv("PATH");
	std::string saved = oldPath ? oldPath : "";
	::setenv("PATH", (fakeDir.string() + ":" + saved).c_str(), 1);

	auto mod = getModule("system");
	auto result = call(mod, "getDisplayInfo");

	::setenv("PATH", saved.c_str(), 1); // 无论断言结果如何先还原 PATH

	ASSERT_TRUE(result.isArray());
	ASSERT_EQ(result.size(), 1u); // disconnected 行被跳过
	const auto& d = result.at(0);
	EXPECT_EQ(d.get("name")->asString(), "XVFB-1");
	EXPECT_EQ(d.get("index")->asInt(), 0);
	EXPECT_EQ(d.get("width")->asInt(), 1920);
	EXPECT_EQ(d.get("height")->asInt(), 1080);
	EXPECT_TRUE(d.get("isPrimary")->asBool());

	fs::remove_all(fakeDir);
}

// getNetworkAdapters 全遍历（ifa_addr == nullptr 的 continue 分支在本机
// 环回/链路层条目上自然发生）
TEST(SystemGlueRoundtripTest, GetNetworkAdaptersEnumerates) {
	auto mod = getModule("system");
	auto result = call(mod, "getNetworkAdapters");
	EXPECT_TRUE(result.isArray());
}
#endif // __linux__

// ========== human setConfig 通用入口补齐 ==========

namespace {
struct HumanConfigGuard {
	HumanMouseConfig mc = Human::mouse().getConfig();
	~HumanConfigGuard() { Human::setMouseConfig(mc); }
};
} // anonymous namespace

TEST(HumanSetConfigKeysTest, SetConfigMoveSpeedAndTypingVariance) {
	HumanConfigGuard guard;
	auto mod = getModule("human");
	ASSERT_FALSE(mod.name.empty());

	EXPECT_TRUE(call(mod, "setConfig", {ScriptValue::fromString("move_speed"),
		ScriptValue::fromFloat(2.5)}).isNull());
	auto cfg = call(mod, "getConfig");
	ASSERT_NE(cfg.get("move_speed"), nullptr);
	EXPECT_NEAR(cfg.get("move_speed")->asFloat(), 2.5, 1e-9);
	EXPECT_NEAR(Human::mouse().getConfig().maxMoveDuration, 150, 1e-9); // 300/2.5

	EXPECT_TRUE(call(mod, "setConfig", {ScriptValue::fromString("typing_variance"),
		ScriptValue::fromFloat(0.4)}).isNull());
	cfg = call(mod, "getConfig");
	ASSERT_NE(cfg.get("typing_variance"), nullptr);
	EXPECT_NEAR(cfg.get("typing_variance")->asFloat(), 0.4, 1e-9);

	// 未知 key 静默忽略、缺参不崩溃
	EXPECT_TRUE(call(mod, "setConfig", {ScriptValue::fromString("unknown_key"),
		ScriptValue::fromInt(1)}).isNull());
	EXPECT_TRUE(call(mod, "setConfig", {}).isNull());
}
