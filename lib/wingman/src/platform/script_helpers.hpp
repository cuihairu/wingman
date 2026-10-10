// 脚本管理器平台查询薄层：环境变量读取与文件修改时间。
// 平台分支由 CMake 按目录选源收敛，调用点（script_manager.cpp）保持零
// 平台宏（薄层纪律，docs/platform-abstraction-design.md §8）。
#pragma once

#include <cstdint>
#include <string>

namespace wingman::platform {

// 读进程环境变量；未设置返回空串
std::string readEnvironmentVariable(const std::string& key);

// 文件最后修改时间戳。单位沿用历史口径：Windows 毫秒 epoch、POSIX 纳秒
// epoch——两平台单位不一致是既有行为（同平台内 reload 对比只比同源数值）。
uint64_t readFileModifiedTime(const std::string& path);

} // namespace wingman::platform
