// Agent 平台身份薄层：可执行名/进程扫描名/stub 候选路径随平台不同
// （Windows 带 .exe 后缀，MSVC 构建树 stub 多 Release/Debug 配置目录）。
// 平台分支由 CMake 按目录选源收敛（platform/win/ vs platform/posix/），
// 本头文件与调用点保持零平台宏（薄层纪律，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace wingman::runtime::platform {

// 进程扫描名列表（stop/status 命令的查找口径）
std::vector<std::string> agentProcessNames();

// build 命令的 stub 可执行文件候选路径（按优先级排列）
std::vector<std::filesystem::path> stubCandidatePaths();

} // namespace wingman::runtime::platform
