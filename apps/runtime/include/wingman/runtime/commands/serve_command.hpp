#pragma once

#include <string>

namespace wingman::runtime::commands {

/// 启动 WebSocket 服务器
/// 与 Tauri UI 进行通信
/// @param host 监听地址
/// @param port 监听端口
/// @return 退出码
int serveCommand(const std::string& host = "127.0.0.1", int port = 8080);

} // namespace wingman::runtime::commands
