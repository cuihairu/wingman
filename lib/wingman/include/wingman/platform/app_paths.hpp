#pragma once

#include <filesystem>
#include <string>

namespace wingman::platform {

/// 用户级应用数据目录（不存在则创建）。
/// Win: %LOCALAPPDATA%\wingman；Linux: $XDG_DATA_HOME（须绝对路径）未设时
/// ~/.local/share/wingman；macOS: ~/Library/Application Support/wingman。
/// 失败返回空路径，调用方自行降级。
std::filesystem::path appDataDir();

/// 当前平台短名（windows / linux / macos），用于崩溃注解等自描述元数据。
std::string platformName();

} // namespace wingman::platform
