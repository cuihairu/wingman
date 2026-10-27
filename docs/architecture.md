> [中文](zh/architecture.md) | **English**

# Wingman Architecture

> Hard architecture constraints are documented in `docs/architecture-decisions.md`. Read that document before modifying runtime, GUI, orchestrator, or transport.

## Directory Structure

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── agent/                    ← 主运行时（主动 Agent + 本地 IPC 服务端）
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── agent.cpp         ← Agent 主逻辑（主动连接编排器）
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
│   └── android/                  ← Android 端侧 Agent（Kotlin，实验性）
│
├── lib/wingman/                  ← 核心库
│   ├── include/wingman/
│   │   ├── screen.hpp            ← 屏幕捕获
│   │   ├── platform/iinput.hpp   ← 输入模拟接口
│   │   ├── trigger.hpp           ← 触发器
│   │   ├── vision.hpp            ← 视觉识别
│   │   ├── behavior_tree.hpp     ← 行为树
│   │   ├── ocr.hpp               ← OCR
│   │   └── ...
│   ├── src/
│   │   ├── vision.cpp / kvstore.cpp / ...
│   │   └── platform/<os>/        ← screen / input 等平台实现（win/linux/mac/android/mock）
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
│   │   │   ├── stream_manager.hpp ← 流管理器
│   │   │   └── stream_type.hpp    ← 流类型定义
│   │   └── src/
│   │       ├── transport_client.cpp
│   │       ├── transport_server.cpp
│   │       ├── simple_protocol.cpp
│   │       ├── stream_channel.cpp
│   │       ├── stream_manager.cpp
│   │       ├── session/
│   │       └── channel/
│   │
│   ├── lua/                      ← Lua 绑定（桥接 Lua → 核心库）
│   │   ├── include/wingman/lua/
│   │   └── src/
│   │
│   ├── python/                   ← Python 引擎绑定（默认关闭，WINGMAN_ENABLE_PYTHON=ON）
│   ├── agentcore/                ← 远程 agent 公共件（注册/心跳/事件缓冲）
│   ├── androidagent/             ← Android 链路共享件（C++，供 android app 桥接）
│   └── crash/                    ← 崩溃采集封装（Crashpad client 接入）
│
├── orchestrator/                 ← 编排层（唯一 Control Plane）
│   ├── server/                   ← Go 服务端
│   └── dashboard/                ← Web 控制面板（React/Umi）
│
├── examples/                     ← 示例代码
├── scripts/                      ← 脚本
├── docs/                         ← 文档
└── CMakeLists.txt                ← 根 CMake（聚合构建）
```

> This tree is an architecture-view excerpt; the full directory tree (including build and deployment surfaces such as `cmake/`, `deployments/`, `third_party/`) is in
> [project-structure.md](project-structure.md).

## Architecture Layering (Four-Layer Model)

Wingman is a four-layer model with a hard **Control Plane / Execution Plane
boundary** running across the middle (for the rationale, see *Four-Layer Model* in `docs/architecture-decisions.md`):

```
 CONTROL PLANE                      Go server（orchestrator/）
   Dashboard (React) · RBAC · Audit · Agent Registry · Workflow 引擎
        │
        │ Agent TCP（outbound，注册令牌鉴权）
        ▼
 AGENT LAYER（执行节点身份）         runtime Agent + Android Agent
    identity · register({agentId, platform, capabilities}) · heartbeat
    · command · event
        ↓
 RUNTIME LAYER（Execution Plane）    apps/agent
    ScriptManager · Lua/Python 引擎 · CommandDispatcher · TriggerManager
    · 本地 IPC（Tauri → runtime）
        ↓
 AUTOMATION CORE（lib/wingman）      screen · input · window · vision · OCR
    · ML · process · kv · trigger · behavior（平台实现在 platform/<os>/）
```

Boundary rules:

- **Automation Core is unaware of the layers above**: `lib/wingman` knows nothing about Agent / Dashboard /
  Workflow / User / Team concepts; it is the most reusable asset of the entire platform and also the
  portable core of the Android agent.
- **Runtime = Execution Plane**: the scripting engine, the transport-agnostic CommandDispatcher,
  the execution lifecycle, and the local UI path all live in this layer; it is not a server.
- **Agent = remote identity layer**: registers with the Go server, is addressed by `agent_id`, and holds
  an outbound TCP session with a capability set (see the Capability System decision).
- **Go server = the only Control Plane**: the registry, RBAC, audit, workflow scheduling, and
  all Dashboard APIs are owned by it; it dials no runtime.

The two local and remote control paths (equivalent to the two sides of the RUNTIME LAYER above):

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

## Platform Core Concepts and Current State

The current implementation locations of the seven core concepts (convergence review snapshot 2026-10-04; ✅ landed, 🔶 partial/evolving):

| Concept | Current Implementation Location | Status |
|------|--------------|------|
| **Agent** | Agent TCP registration/heartbeat/command lifecycle ([protocols.md](protocols.md)) + A3-P1 registration token ([agent-token-auth-design.md](agent-token-auth-design.md)) | ✅ |
| **Capability** | v1 landed: vocabulary frozen at 13 terms (ADR); desktop runtime registers and reports `platform` + aligned vocabulary (ml.onnx declared per the ML build, no overclaiming), Android configured via capabilitiesJson; server validates, stores, and exposes `capabilities`/`unknownCapabilities` to the Dashboard (shown on the Agents page). Module-level vocabulary (file.*/tray etc.) is decided separately and is not part of the v1 vocabulary | ✅ |
| **Execution** | v1 landed: unified Execution records — `run_script`, batch fan-out (offline skip semantics aligned with single-shot), workflow steps (one record per attempt, WorkflowID/StepID attached, timeout/cancelled terminal-state distinction); Dashboard Executions page is read-only. Follow-ups: screenshot steps attach to Execution, wait/condition not issued by an agent does not attach | ✅ |
| **Workflow** | Go server `internal/workflow` DAG engine (cycle detection/timeout/retry/wait) + Dashboard Workflows page | ✅ |
| **Artifact** | Deployment screenshot artifact (`deploy-screenshot`, deployment pipeline); the platform-level Execution artifact model is documented under *Execution* in architecture-decisions.md | 🔶 |
| **Control Plane** | Go server: registry / RBAC / audit / workflow / batch operations / Guacamole gateway | ✅ |
| **Execution Plane** | apps/agent + Android agent (outbound execution, local IPC) | ✅ |

## Call Chains

```
Lua 脚本
    ↓
libs/lua/ (Lua 绑定)
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/agent/ (应用：CLI + 运行模式)
```

## Control Plane

Runtime remote mode and the local standalone UI are two different control paths.

| Scenario | Control Path | Constraint |
|------|----------|------|
| Remote orchestration | runtime agent proactively connects outbound to the Go orchestrator | The Go server is the only remote control entry point |
| Local standalone UI | Tauri UI -> Rust backend -> local IPC -> runtime | Does not use a runtime HTTP/WebSocket server |

> **Forbidden**: The Runtime must not introduce an HTTP/WebSocket server as a local UI or remote control surface. WebSocket is only allowed for dashboard/browser communication with the Go server.

Trigger commands (`trigger.*`) reuse the same set of RPC handlers: the runtime's `Agent` holds a shared
`TriggerManager`, and both the local IPC dispatcher and the remote dispatcher register `trigger.*`;
the Go server passes them through to the Dashboard via
`GET /api/agents/:agentId/triggers` (read, login only),
`POST /api/agents/:agentId/triggers` (create),
`POST /api/agents/:agentId/triggers/toggle` (enable/disable),
`PUT /api/agents/:agentId/triggers/:triggerId` (update), and
`DELETE /api/agents/:agentId/triggers/:triggerId` (delete) (all require agents:manage)
(see Dispatcher Reuse in architecture-decisions.md).

Remote registration configuration (orchestrator address + A3-P1 registration token) is read/written via local IPC's
`config.getRemote` / `config.setRemote`, with a corresponding entry on the GUI settings page;
`config.setRemote` validates and hot-rebuilds the remote client, then writes back to `agent.toml`. This command is
**registered only on the local IPC dispatcher** and deliberately provides no remote agent command — remote configuration must not be rewritten from the Go server
side (see Remote Config Commands in architecture-decisions.md).

**Agent grouping and batch operations**: agent tags are held in memory by the Registry and are
write-through persisted to `models.Agent.tags` via the `TagStore`
callback (restored on server restart, memory values retained on reconnect).
Batch endpoints `POST /api/agents/batch/run-script`, `/stop-script` (scripts:run), and
`/api/agents/batch/trigger` (agents:manage) fan out over existing agent commands in the Go server
side by selector (union of agentIds/tags), introducing no new commands or transport channels;
partial failures are not treated as overall failures, always returning 200 plus per-device results (see
Agent Groups & Batch Operations in architecture-decisions.md for details).

## Transport Layer

`libs/transport` provides two different network capabilities for different purposes:

| Purpose | User | Protocol | Description |
|------|--------|------|------|
| Orchestration communication | `remote_client` (runtime → Go server) | Custom Message protocol | runtime proactively connects outbound to the Go orchestrator |
| Script networking | `wingman.transport` module | TCP + UDP (asio) | TCP client/server and UDP sockets used in scripts |

Both share the TCP infrastructure of `libs/transport` (`TransportClient`, `TransportServer`, `Session`, `Channel`), but their lifecycles and management are independent. Script-layer UDP is implemented directly on asio and does not go through `libs/transport`.

The script-layer transport API is documented in `docs/api/transport.md`. The orchestration communication protocol is documented in `docs/architecture-decisions.md`.

## Local IPC Strategy

| Platform | Default IPC | Notes |
|------|----------|------|
| Windows | Named Pipe | Windows Unix Domain Socket support is detectable but is not the default primary path |
| macOS/Linux | Unix Domain Socket | Uses a socket in the user runtime directory |
| All platforms | Local TCP | Only an explicit debug fallback, disabled by default |

The local IPC wire format is fixed as `uint32 little-endian length + JSON envelope`. The envelope uses transport-level fields:

```json
{
  "type": 0,
  "method": "system.getStatus",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`payload` carries the dispatcher request/response data. The GUI should not construct WebSocket JSON-RPC; the runtime should also not add an HTTP/WebSocket listener for the local UI.

## Design Principles

1. **Core library independence** - `lib/wingman/` does not depend on `apps/` and can be reused on its own
2. **Nearby testing** - every module has its own `tests/` directory
3. **Clear separation of concerns** - apps (applications), lib (core library), libs (support libraries) are separated
4. **Namespace correspondence** - subdirectory headers nest by directory: `include/wingman/<dir>/xxx.hpp` → `namespace wingman::<dir>` (same pattern for capture / core / ipc / platform / rpc / vision); top-level headers are flat in `namespace wingman` (31 of 33, with `crypt.hpp` / `core.hpp` as exceptions using `wingman::crypt` / `wingman::core`)
5. **Control plane separation** - remote orchestration goes through the Go server, the local UI goes through IPC, and the runtime exposes no WebSocket/HTTP control surface

## Multi-Display Screenshots

Screenshots support selecting a target display via `displayId`. This capability is built on the `IScreen` platform abstraction (`lib/wingman/include/wingman/platform/iscreen.hpp`), with implementations ready on all three platforms (Windows `EnumDisplayMonitors`, macOS `CGGetActiveDisplayList`, Linux X11/XRandR).

- Display enumeration: queried via the RPC method `screen.listMonitors`, returning a list of `{ id, name, isPrimary, bounds }`
- Screenshot per display: the `screenshot.capture` payload adds an optional `displayId` field, defaulting to the primary display when omitted (backward compatible)
- Abstraction constraint: `IScreen` is the sole specification of the screen abstraction; the old static `wingman::Screen` class is frozen (used only by historical callers, no longer extended), and new code must go through `IScreen`

See the *Display Selection* section of `docs/architecture-decisions.md` and `docs/platform-abstraction-design.md` for details.

## Reference Projects

- [moderncpp-project-template](https://github.com/madduci/moderncpp-project-template) - app/library separation structure
- [Botcraft](https://github.com/adepierre/Botcraft) - game bot library design
