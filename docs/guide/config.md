> [中文](../zh/guide/config.md) | **English**

# Configuration Guide

This document is lengthy and has no English translation yet.

The section outline below mirrors the Chinese source document; see [config.md](../zh/guide/config.md) for the original.

> Scope: this page covers the script-side JSON config store (`config/config.json`, read/written via the `wingman.config` module and the C++ `Config` API). The runtime process itself reads TOML `agent.toml` (`wingman-agent start`); see the runtime-configuration section of the configuration guide.

- 配置文件位置
- 配置结构
- 配置项说明
  - Server (服务器配置)
  - Tray (托盘配置)——未实现（计划中）
  - AutoRun (自动运行配置)
  - Heartbeat (心跳配置)
  - Game (游戏配置)
- C++ API
- Lua API
- 节点模块 (node)
  - node.createHeartbeat()
  - node.sendHeartbeat(table)
  - node.getWindows()
- 运行模式
  - 1. 独立模式（默认）
  - 2. 服务器控制模式
- 环境变量
- 安全提示
