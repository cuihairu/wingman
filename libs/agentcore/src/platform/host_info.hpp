// Agentcore 平台身份薄层：主机名查询随平台不同（Windows GetComputerNameA /
// POSIX uname）。平台分支由 CMake 按目录选源收敛（platform/win/ vs
// platform/posix/），本头文件与调用点保持零平台宏（薄层纪律，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include <string>

namespace wingman::runtime::platform {

// 主机名（失败时返回平台占位名）
std::string hostname();

} // namespace wingman::runtime::platform
