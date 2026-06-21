#pragma once

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
class MacroRecorder;
} // namespace wingman

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createMacroModule();

// 依赖反转注入点：runtime 启动时把持有的 MacroRecorder 实例注入进来，
// 让 wingman.macro.* 与 RPC macro.* 操作同一实例，避免双实例冲突。
// 未注入时（如单脚本 CLI 模式）模块内部 lazy 创建默认实例。
void setGlobalRecorder(MacroRecorder* recorder);

// 引擎 shutdown 时调用，释放模块内部默认实例。
void cleanupMacroModule();

} // namespace modules
} // namespace script
} // namespace wingman
