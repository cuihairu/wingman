#pragma once
#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createFsmModule();
void cleanupFsmModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;


inline ModuleDescriptor::FunctionEntry findFsmFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) return fn;
	}
	return {};
}

class FsmModuleTest : public ::testing::Test {
protected:
	void TearDown() override {
		cleanupFsmModule();
	}
};

