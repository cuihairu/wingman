#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

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

namespace {

ModuleDescriptor::FunctionEntry findTaskFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

} // namespace

TEST(TaskModuleTest, WaitTimeoutDoesNotDeadlock) {
	auto mod = createTaskModule();
	auto submit = findTaskFunction(mod, "submit");
	auto wait = findTaskFunction(mod, "wait");
	auto status = findTaskFunction(mod, "status");

	ASSERT_FALSE(submit.name.empty());
	ASSERT_FALSE(wait.name.empty());
	ASSERT_FALSE(status.name.empty());

	// Submit async task that takes longer than the wait timeout
	auto taskId = submit({
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			return ScriptValue::fromString("done");
		}, true),
		ScriptValue::fromObject({
			{"async", ScriptValue::fromBool(true)},
			{"timeoutMs", ScriptValue::fromInt(1000)}
		})
	});

	ASSERT_TRUE(taskId.isString());

	// Wait with timeout shorter than task execution time
	auto waited = wait({taskId, ScriptValue::fromInt(10)});
	EXPECT_TRUE(waited.isBool());
	EXPECT_FALSE(waited.asBool());

	// Status should be timeout
	auto taskStatus = status({taskId});
	EXPECT_EQ(taskStatus.asString(), "timeout");

	// Give the async task time to complete to avoid cleanup issues
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}
