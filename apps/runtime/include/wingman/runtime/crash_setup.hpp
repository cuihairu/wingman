#pragma once

namespace wingman::runtime {

/// 初始化崩溃采集。在事件日志 sink 挂上之后、命令分发之前调用，覆盖全部
/// 命令路径与嵌入式脚本路径；编译未启 Crashpad 时为空实现。
/// 失败内部降级为无采集（仅记警告），不影响进程继续运行。
void setupCrashReporting(const char* argv0);

} // namespace wingman::runtime
