#pragma once

// Runtime -> 脚本模块的依赖反转注入点。
// 这些声明放在公开头，供 apps/runtime 在启动时把自身持有的实例注入
// 到脚本模块，使 wingman.<module>.* 与 RPC.<module>.* 共享同一实例。
// 声明与内部 macro_module.cpp / script_module.cpp 的实现对应。

namespace wingman {
class MacroRecorder;
class ScriptManager;
} // namespace wingman

namespace wingman {
namespace script {
namespace modules {

// 注入 MacroRecorder 实例供 wingman.macro.* 使用（与 RPC macro.* 共享）。
// 未注入时模块内部 lazy 创建默认实例。
void setGlobalRecorder(MacroRecorder* recorder);

} // namespace modules

// 注入 ScriptManager 实例供 wingman.script.* 使用。
// 懒查询：注册阶段不访问，规避 ScriptManager <-> ScriptEngine 循环依赖。
void setScriptManager(ScriptManager* manager);

} // namespace script
} // namespace wingman
