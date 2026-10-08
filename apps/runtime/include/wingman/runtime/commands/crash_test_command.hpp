#pragma once

namespace wingman::runtime::commands {

/// 故意触发空指针崩溃，验收崩溃采集链路（dump 落盘 + 符号化还原）。
/// 启用采集的构建下进程必然异常退出，本函数不返回。
int crashTestCommand();

} // namespace wingman::runtime::commands
