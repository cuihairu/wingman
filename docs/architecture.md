# Wingman 项目架构

> 架构硬约束见 `docs/architecture-decisions.md`。修改 runtime、GUI、orchestrator 或 transport 前必须先阅读该文档。

## 目录结构

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── runtime/                  ← 主运行时（主动 Agent + 本地 IPC 服务端）
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── agent.cpp         ← Agent 主逻辑（主动连接编排器）
│   │   │   ├── remote_client.cpp ← 远程客户端（主动 outbound 连接编排器）
│   │   │   ├── standalone_mode.cpp ← 单机模式
│   │   │   └── commands/         ← CLI 子命令
│   │   ├── include/wingman/runtime/
│   │   ├── tests/                ← 应用测试
│   │   └── CMakeLists.txt
│   │
│   ├── gui/                      ← Tauri/Svelte GUI
│   │   ├── src-tauri/
│   │   ├── src/
│   │   └── package.json
│   │
│   └── client/                   ← 客户端库
│
├── lib/wingman/                  ← 核心库
│   ├── include/wingman/
│   │   ├── screen.hpp            ← 屏幕捕获
│   │   ├── input.hpp             ← 输入模拟
│   │   ├── trigger.hpp           ← 触发器
│   │   ├── vision.hpp            ← 视觉识别
│   │   ├── behavior_tree.hpp     ← 行为树
│   │   ├── ocr.hpp               ← OCR
│   │   └── ...
│   ├── src/
│   │   ├── screen.cpp
│   │   ├── input.cpp
│   │   └── ...
│   ├── tests/                    ← 核心库测试
│   └── CMakeLists.txt
│
├── libs/                         ← 内部辅助库
│   ├── transport/                ← 网络传输库（TCP + 自定义协议）
│   │   ├── include/wingman/transport/
│   │   │   ├── transport.hpp     ← 传输抽象
│   │   │   ├── transport_client.hpp ← TCP 客户端
│   │   │   ├── transport_server.hpp ← TCP 服务器
│   │   │   ├── simple_protocol.hpp  ← 消息协议（Header + Body）
│   │   │   ├── session/          ← 会话层（含 tcp_session）
│   │   │   ├── channel/          ← 消息通道
│   │   │   ├── stream_channel.hpp ← 流通道
│   │   │   └── stream_manager.hpp ← 流管理器
│   │   └── src/
│   │       ├── transport_client.cpp
│   │       ├── transport_server.cpp
│   │       ├── simple_protocol.cpp
│   │       ├── session/
│   │       └── channel/
│   │
│   ├── lua/                      ← Lua 绑定（桥接 Lua → 核心库）
│   │   ├── include/wingman/lua/
│   │   └── src/
│   │
│
│
├── examples/                     ← 示例代码
├── scripts/                      ← 脚本
├── docs/                         ← 文档
└── CMakeLists.txt                ← 根 CMake（聚合构建）
```

## 架构分层（四层模型）

Wingman 是四层模型，中间横着一条硬性的 **Control Plane / Execution Plane
边界**（决策详见 `docs/architecture-decisions.md` 的 *Four-Layer Model*）：

```
 CONTROL PLANE                      Go server（apps/orchestrator）
   Dashboard (React) · RBAC · Audit · Agent Registry · Workflow 引擎
        │
        │ Agent TCP（outbound，注册令牌鉴权）
        ▼
 AGENT LAYER（执行节点身份）         runtime Agent + Android Agent
    identity · register({agentId, platform, capabilities}) · heartbeat
    · command · event
        ↓
 RUNTIME LAYER（Execution Plane）    apps/runtime
    ScriptManager · Lua/Python 引擎 · CommandDispatcher · TriggerManager
    · 本地 IPC（Tauri → runtime）
        ↓
 AUTOMATION CORE（lib/wingman）      screen · input · window · vision · OCR
    · ML · process · kv · trigger · behavior（平台实现在 platform/<os>/）
```

边界规则：

- **Automation Core 不感知上层**：`lib/wingman` 不知道 Agent / Dashboard /
  Workflow / User / Team 概念，是整个平台最可复用的资产，也是 Android
  agent 的可移植核心。
- **Runtime = Execution Plane**：脚本引擎、传输无关的 CommandDispatcher、
  执行生命周期、本地 UI 路径都在这一层；它不是 server。
- **Agent = 远程身份层**：向 Go server 注册、以 `agent_id` 寻址、持有
  outbound TCP 会话与能力集（见 Capability System 决策）。
- **Go server = 唯一 Control Plane**：注册表、RBAC、审计、工作流调度与
  全部 Dashboard API 都由它拥有；它不 dial 任何 runtime。

本地与远程两条控制路径（等价于上面的 RUNTIME LAYER 两侧）：

```
┌─────────────────────────────────────────────────────────┐
│                 Remote orchestration                     │
│ dashboard/browser ──HTTP/WS──► Go orchestrator            │
│                                      ▲                    │
│                                      │ outbound transport │
│                                      │                    │
│                              runtime agent                │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                  Local standalone UI                     │
│ Tauri UI ──invoke──► Tauri Rust backend ──IPC──► runtime  │
│                                                   │       │
│                                                   ▼       │
│                                        lib/wingman core   │
└─────────────────────────────────────────────────────────┘
```

## 平台核心概念与现状

七个个核心概念的当前实现位置（2026-10-04 收敛审查快照；✅ 已落地，🔶 部分/演进中）：

| 概念 | 当前实现位置 | 状态 |
|------|--------------|------|
| **Agent** | Agent TCP 注册/心跳/命令生命周期（[protocols.md](protocols.md)）+ A3-P1 注册令牌（[agent-token-auth-design.md](agent-token-auth-design.md)） | ✅ |
| **Capability** | v1 已落地：词汇表 13 词冻结（ADR）；desktop runtime 注册上报 `platform` + 对齐词汇（ml.onnx 按 ML 构建声明，不虚报）、Android 经 capabilitiesJson 配置；server 校验入库并向 Dashboard 暴露 `capabilities`/`unknownCapabilities`（Agents 页展示）。模块级词汇（file.*/tray 等）另行决策，不入 v1 词汇表 | ✅ |
| **Execution** | v1 已落地：统一 Execution 记录——`run_script`、batch fan-out（offline 跳过语义对齐单发）、workflow 步骤（每 attempt 一条，WorkflowID/StepID 挂载，timeout/cancelled 终态区分）；Dashboard Executions 页只读视图。后续：screenshot 步骤接 Execution、wait/condition 非 agent 下发不接 | ✅ |
| **Workflow** | Go server `internal/workflow` DAG 引擎（环检测/超时/重试/等待）+ Dashboard Workflows 页 | ✅ |
| **Artifact** | 部署截图 artifact（`deploy-screenshot`，部署链路）；平台级 Execution artifact 模型见 architecture-decisions.md *Execution* | 🔶 |
| **Control Plane** | Go server：registry / RBAC / audit / workflow / 批量操作 / Guacamole 网关 | ✅ |
| **Execution Plane** | apps/runtime + Android agent（outbound 执行、本地 IPC） | ✅ |

## 调用链

```
Lua 脚本
    ↓
libs/lua/ (Lua 绑定)
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/runtime/ (应用：CLI + 运行模式)
```

## 控制面

Runtime 远程模式和本地单机 UI 是两条不同控制路径。

| 场景 | 控制路径 | 约束 |
|------|----------|------|
| 远程编排 | runtime agent 主动 outbound 连接 Go orchestrator | Go server 是唯一远程中控入口 |
| 本地单机 UI | Tauri UI -> Rust backend -> local IPC -> runtime | 不使用 runtime HTTP/WebSocket server |

> **禁止**: Runtime 不得引入 HTTP/WebSocket server 作为本地 UI 或远程控制面。WebSocket 只允许用于 dashboard/browser 与 Go server 通信。

触发器命令（`trigger.*`）复用同一套 RPC handler：runtime 的 `Agent` 持有共享
`TriggerManager`，本地 IPC dispatcher 与远程 dispatcher 都注册 `trigger.*`；
Go server 经 `/api/agents/:agentId/triggers`（读，登录即可）与 `/triggers/toggle`、
`POST /triggers`（新增）、`PUT /triggers/:triggerId`（更新）、
`DELETE /triggers/:triggerId`（删除）（均需 agents:manage）透传给 Dashboard
（见 architecture-decisions.md 的 Dispatcher Reuse）。

远程注册配置（orchestrator 地址 + A3-P1 注册令牌）经本地 IPC 的
`config.getRemote` / `config.setRemote` 读写，GUI 设置页提供对应入口；
`config.setRemote` 校验后热重建远程客户端并写回 `agent.toml`。该命令**仅注册在
本地 IPC dispatcher**，刻意不提供远程 agent 命令——远程配置不允许从 Go server
侧改写（见 architecture-decisions.md 的 Remote Config Commands）。

**Agent 分组与批量操作**：agent 标签由 Registry 内存持有，并经 `TagStore`
回调写穿持久化到 `models.Agent.tags`（server 重启恢复、断线重连保留内存值）。
批量端点 `POST /api/agents/batch/run-script`、`/stop-script`（scripts:run）与
`/api/agents/batch/trigger`（agents:manage）在 Go server 侧按选择器
（agentIds/tags 并集）对既有 agent 命令做 fan-out，不引入新命令与传输通道；
部分失败不算整体失败，一律 200 + 逐台结果（详见 architecture-decisions.md 的
Agent Groups & Batch Operations）。

## Transport 层

`libs/transport` 提供两种不同用途的网络能力：

| 用途 | 使用者 | 协议 | 说明 |
|------|--------|------|------|
| 编排通信 | `remote_client` (runtime → Go server) | 自定义 Message 协议 | runtime 主动 outbound 连接 Go orchestrator |
| 脚本网络 | `wingman.transport` 模块 | TCP + UDP (asio) | 脚本中使用的 TCP 客户端/服务器、UDP socket |

两者共用 `libs/transport` 的 TCP 基础设施（`TransportClient`、`TransportServer`、`Session`、`Channel`），但生命周期和管理方式独立。脚本层 UDP 直接基于 asio 实现，不经过 `libs/transport`。

脚本层 transport API 详见 `docs/api/transport.md`。编排通信协议详见 `docs/architecture-decisions.md`。

## 本地 IPC 策略

| 平台 | 默认 IPC | 备注 |
|------|----------|------|
| Windows | Named Pipe | Windows Unix Domain Socket 可探测支持，但不作为默认主路径 |
| macOS/Linux | Unix Domain Socket | 使用用户运行时目录下的 socket |
| 全平台 | Local TCP | 仅显式 debug fallback，默认关闭 |

本地 IPC wire 格式固定为 `uint32 little-endian length + JSON envelope`。Envelope 使用 transport-level 字段：

```json
{
  "type": 0,
  "method": "system.getStatus",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`payload` 承载 dispatcher 请求/响应数据。GUI 不应构造 WebSocket JSON-RPC；runtime 也不应为了本地 UI 增加 HTTP/WebSocket listener。

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`
5. **控制面分离** - 远程编排走 Go server，本地 UI 走 IPC，runtime 不暴露 WebSocket/HTTP 控制面

## 多显示器截图

截图支持按 `displayId` 选择目标显示器。该能力基于 `IScreen` 平台抽象（`lib/wingman/include/wingman/platform/iscreen.hpp`），三平台实现已就绪（Windows `EnumDisplayMonitors`、macOS `CGGetActiveDisplayList`、Linux X11/XRandR）。

- 显示器枚举：通过 RPC method `screen.listMonitors` 查询，返回 `{ id, name, isPrimary, bounds }` 列表
- 按显示器截图：`screenshot.capture` payload 增加可选 `displayId` 字段，省略时默认主屏（向后兼容）
- 抽象约束：`IScreen` 是屏幕抽象的唯一规范，旧的静态 `wingman::Screen` 类已冻结（仅历史调用方使用，不再扩展），新代码必须走 `IScreen`

详见 `docs/architecture-decisions.md` 的 *Display Selection* 小节与 `docs/platform-abstraction-design.md`。

## 参考项目

- [moderncpp-project-template](https://github.com/madduci/moderncpp-project-template) - 应用与库分离结构
- [Botcraft](https://github.com/adepierre/Botcraft) - 游戏机器人库设计
