#pragma once

#include "wingman/script/iscript_engine.hpp"
#include <vector>

namespace wingman {
namespace script {
namespace modules {

// Get all module descriptors (unified registration entry point)
std::vector<ModuleDescriptor> getAllModules();

// Register all modules to the engine
void registerAllModules(IScriptEngine& engine);

} // namespace modules
} // namespace script
} // namespace wingman
