# Wingman GUI

Tauri 桌面应用，用于本地控制 Wingman runtime。

> 架构约束: GUI 不连接 runtime WebSocket/HTTP server。GUI 前端通过 Tauri `invoke()` 调用 Rust backend，Rust backend 通过本地 IPC 连接 runtime。详见 `../../docs/architecture-decisions.md`。

## 前置要求

- Windows 10/11 / macOS 12+ / Linux（CI 三平台 `pnpm tauri build` 均通过）
- Rust 1.70+
- Node.js 18+（pnpm，仓库含 pnpm-lock.yaml）
- WebView2 Runtime（Windows）

## 开发

### 1. 安装依赖

```bash
npm install
```

### 2. 启动开发服务器

```bash
npm run dev
```

### 3. 启动 Tauri (在另一个终端)

```bash
npm run tauri dev
```

## 构建

### 生产构建

```bash
npm run tauri build
```

构建产物位于 `src-tauri/target/release/bundle/`

## 架构

```
Tauri GUI (Rust + HTML/JS)
    ↓ Tauri invoke()
Tauri Rust backend
    ↓ Local IPC
Wingman Runtime
    ↓
Wingman Core
```

## Local IPC

- Start runtime for local UI with `wingman-agent start --standalone`.
- Windows 默认使用 Named Pipe。
- macOS/Linux 默认使用 Unix Domain Socket。
- Windows Unix Domain Socket 可做运行时探测支持，但不是默认主路径。
- Local TCP 仅允许显式 debug fallback，默认关闭。

## Tauri 命令

共 54 个（`src-tauri/src/commands/` 与 `tray.rs`），按域分组：

| 域 | 命令 |
|----|------|
| 连接 | `connect_ipc` `disconnect_ipc` `is_connected` `get_ipc_state` |
| 脚本 | `get_scripts` `start_script` `stop_script` `pause_script` `resume_script` `restart_script` `unload_script` `start_active_profile_scripts` `stop_active_profile_scripts` |
| 脚本文件 | `get_scripts_root` `set_scripts_root` `list_script_files` `read_script_file` `write_script_file` `delete_script_file` |
| 系统 | `get_system_status` `get_version` `get_runtime_info` `toggle_pause` `pause_all` `resume_all` `stop_all` `is_paused` `reload_hotkeys` |
| 触发器 | `get_triggers` `add_trigger` `remove_trigger` `update_trigger` `toggle_trigger` |
| 宏 | `macro_record` `macro_stop` `macro_play` `macro_status` `macro_save` `macro_load` `macro_clear` |
| 截屏 | `capture_screenshot` `list_monitors` |
| 远程配置 | `get_remote_config` `set_remote_config` |
| 档案 | `get_profiles` `get_active_profile` `set_active_profile` `create_profile` `delete_profile` `update_profile` `export_profile_json` `import_profile_json` |
| 事件/托盘 | `drain_events` `tray_control` |
