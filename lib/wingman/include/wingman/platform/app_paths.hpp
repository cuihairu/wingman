#pragma once

#include <filesystem>
#include <string>

namespace wingman::platform {

/// 用户级应用数据目录（不存在则创建）。
/// Win: %LOCALAPPDATA%\wingman；Linux: $XDG_DATA_HOME（须绝对路径）未设时
/// ~/.local/share/wingman；macOS: ~/Library/Application Support/wingman。
/// 失败返回空路径，调用方自行降级。
std::filesystem::path appDataDir();

/// 漫游级应用数据根目录（不含 wingman 段、不建目录）。
/// Win: %APPDATA%（Roaming，脚本库等跨机漫游数据）；非 Windows 保持历史
/// 口径 ~/.local/share。失败返回空路径，调用方自行降级。
std::filesystem::path roamingAppDataDir();

/// 可执行文件所在目录（Win: GetModuleFileNameA 的父目录；非 Windows 保持
/// 历史口径返回当前工作目录）。Win 侧查询失败返回空路径，调用方自行降级。
std::filesystem::path executableDir();

/// 当前平台短名（windows / linux / macos），用于崩溃注解等自描述元数据。
std::string platformName();

} // namespace wingman::platform
