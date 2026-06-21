#pragma once

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

// 脚本管理模块：包装 runtime 的 ScriptManager（经 setScriptManager 注入）。
// 懒查询全局指针，注册阶段不访问 ScriptManager，规避 ScriptManager <-> ScriptEngine 的循环依赖。
ModuleDescriptor createScriptModule();

} // namespace modules
} // namespace script
} // namespace wingman
