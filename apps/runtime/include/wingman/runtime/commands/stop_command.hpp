#pragma once

namespace wingman::runtime::commands {

/// 停止 Wingman Agent 服务
/// @return 退出码
int stopCommand();

/// 查看 Wingman Agent 服务状态
/// @return 退出码 (0=运行中, 1=已停止)
int statusCommand();

} // namespace wingman::runtime::commands
