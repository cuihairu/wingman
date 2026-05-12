# Wingman GUI

Tauri 桌面应用，用于控制 Wingman Client。

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
    ↓ WebSocket
Wingman Client (C++ + Drogon)
    ↓
Wingman Core
```

## WebSocket API

- **URL**: `ws://127.0.0.1:8080/ws`
- **协议**: 详见 `../../docs/API.md`

## Tauri 命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `connect_websocket` | `url?: string` | 连接到 WebSocket 服务器 |
| `get_scripts` | - | 获取脚本列表 |
| `start_script` | `id: string` | 启动脚本 |
| `stop_script` | `script_id: string` | 停止脚本 |
| `get_system_status` | - | 获取系统状态 |
| `get_version` | - | 获取版本信息 |
| `call_rpc` | `method: string, params?: object` | 通用 RPC 调用 |
