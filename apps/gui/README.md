# Wingman GUI

Tauri 桌面应用，用于本地控制 Wingman runtime。

> 架构约束: GUI 不连接 runtime WebSocket/HTTP server。GUI 前端通过 Tauri `invoke()` 调用 Rust backend，Rust backend 通过本地 IPC 连接 runtime。详见 `../../docs/architecture-decisions.md`。

## 前置要求

- Windows 10/11
- Rust 1.70+
- Node.js 18+
- WebView2 Runtime

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

- Start runtime for local UI with `wingman-runtime start --standalone`.
- Windows 默认使用 Named Pipe。
- macOS/Linux 默认使用 Unix Domain Socket。
- Windows Unix Domain Socket 可做运行时探测支持，但不是默认主路径。
- Local TCP 仅允许显式 debug fallback，默认关闭。

## Tauri 命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `connect_ipc` | `endpoint?: string` | 连接到本地 runtime IPC |
| `get_scripts` | - | 获取脚本列表 |
| `start_script` | `id: string` | 启动脚本 |
| `stop_script` | `script_id: string` | 停止脚本 |
| `get_system_status` | - | 获取系统状态 |
| `get_version` | - | 获取版本信息 |
| `call_command` | `method: string, params?: object` | 通用本地命令调用 |
