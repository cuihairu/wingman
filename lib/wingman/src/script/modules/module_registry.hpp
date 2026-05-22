#pragma once

#include "wingman/script/iscript_engine.hpp"
#include <vector>

namespace wingman {
namespace script {
namespace modules {

// 获取所有模块描述符（统一注册入口）
std::vector<ModuleDescriptor> getAllModules();

// 将所有模块注册到引擎
void registerAllModules(IScriptEngine& engine);

} // namespace modules
} // namespace script
} // namespace wingman
