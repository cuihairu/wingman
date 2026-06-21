# Wingman 项目待办事项

> 最后更新: 2026-06-21
> 状态: 收尾阶段（P0 全部完成；分析发现的代码缺陷已全部修复；剩余为中低优先级功能增强与工程收尾）
>
> 📋 当前有一批未提交改动（65 改 + 34 新，+2208/−954）。改动分析见
> [docs/pending-changes.md](./docs/pending-changes.md)；其中发现的问题已全部修复
> （见下方「✅ 代码缺陷（已修复）」）。建议按 pending-changes.md 第 7 节的拆分策略提交。

> ⚠️ 本文档已于 2026-06-21 依据代码实际状态重新校准。之前的版本严重低估了 Go orchestrator
> （工作流引擎、Agent 心跳、审计均已实现）并错误描述了 dashboard 位置。

---

## 📌 位置澄清（重要）

| 路径 | 实际内容 |
|------|----------|
| `orchestrator/server/` | Go 远程中控（HTTP API + WebSocket + Agent TCP 监听） |
| `orchestrator/dashboard/` | **真正的 wingman 远程 Dashboard**（React/Umi/Ant Design Pro，含 Agents/Monitor/Scripts/Workflows 页面 + wsService + wingman.ts） |
| `apps/gui/` | 本地 Tauri GUI（Svelte 5），通过 local IPC 控制 runtime |
| `dashboard/`（仓库根） | ⚠️ 另一个产品 **Croupier** 的副本，已被 `.gitignore` 排除，与 wingman 无关，勿改 |
| ~~`dashboard_old/`~~ | 已删除（空目录） |

---

## 🎯 里程碑规划（已校准）

| 里程碑 | 目标 | 状态 | 完成度 |
|--------|------|------|--------|
| M1: 核心功能 | 基础屏幕捕获、输入模拟、Lua 脚本 | ✅ 完成 | 100% |
| M2: 触发器系统 | 条件触发、自动化配置 | ✅ 完成 | 100% |
| M3: 宏系统 | 录制回放 | ✅ 完成 | 100% |
| M4: 远程编排 | Orchestrator 中控、Agent 通信、工作流引擎 | 🚧 收尾 | ~95% |
| M5: GUI 界面 | 本地控制台 (Tauri) + 远程 Dashboard (React) | 🚧 收尾 | ~85% |
| M6: 人性化模拟 | 防检测、随机化 | ✅ 完成 | 100% |
| M7: 调试器集成 | EmmyLua 调试支持 | ✅ 完成 | 100% |

---

## 🔴 P0 - 必须完成（阻塞交付）

### RBAC 权限系统（Go orchestrator）— ✅ 已完成 (2026-06-20)

实现：`internal/rbac/`（种子+解析）、`internal/models/role.go`（Role/Permission 多对多）、
`internal/middleware/auth.go` `PermissionRequired`、`internal/handlers/users.go`/`roles.go`、
`/api/admin/{users,roles,permissions}` 路由、dashboard `services/api/admin.ts` + `Admin/Users`、`Admin/Roles` 页面。
内置角色 `admin`(*通配)/`operator`/`viewer`，权限码 `resource:action` 形式，与 dashboard `access.ts` 对齐。

- [x] **数据模型**：Role/Permission（many2many role_permissions）+ AutoMigrate + User.Active
- [x] **后端 API**：用户 CRUD + 重置密码、角色 CRUD + 权限分配、权限目录
- [x] **中间件**：`PermissionRequired`（admin 旁路 + DB 解析 + 请求级缓存）；`HandleGetPermissions` 返回真实 permissionIDs
- [x] **Dashboard 前端**：`Admin/Users`、`Admin/Roles` 页面 + 路由 + `admin.ts` 服务
- [x] **测试**：`rbac_test.go` + `handlers/rbac_test.go`（种子幂等、权限解析、inactive、CRUD、内置保护）
- [x] 可选增强：将现有 `RoleRequired("admin")` 写路由渐进迁移到 `PermissionRequired`（✅ 已接线，2026-06-21：agents/workflows/scripts/users/roles/settings 写操作均按权限码鉴权）；Swagger 文档待补

### Runtime IPC 事件推送（C++ runtime → Tauri GUI）— ✅ 机制完成 (2026-06-20)

现状：runtime 通过 RPC `events.drain` 暴露缓冲事件，GUI 轮询拉取并分发。采用 **pull 模型**
（非 type=2 push），避免 Rust IPC 客户端在 Windows 阻塞 IO 下引入异步读取循环导致帧错位
（设计决策见 `docs/architecture-decisions.md` "Runtime-to-UI Event Delivery"）。

- [x] **事件缓冲**：`apps/runtime/.../event_buffer.hpp`（线程安全有界队列，上限 1000）
- [x] **日志事件**：spdlog `EventLogSink` → `log.line`（main.cpp 已挂载）
- [x] **触发器事件**：`TriggerManager::setOnFired` 回调（win32+posix）→ `trigger.fired`（local_ipc_server 已接 EventBuffer）
- [x] **drain RPC**：`events.drain` handler（local_ipc_server 已注册）
- [x] **Rust 命令**：`commands::events::drain_events`（main.rs 已注册）
- [x] **GUI 分发**：`stores/events.ts` 轮询器（500ms）→ `logs.addRuntime` + `triggers.markFired`；App.svelte 按连接启停
- [x] **logs 页面**：`logs.ts` 增加 `addRuntime` + 容量上限（1000 条）
- [x] **脚本状态**：`script.state_changed`（StandaloneMode 各状态转换已接 EventBuffer）
- [x] **截图事件**：`screenshot.frame` —— 经评估**不接入 drain 缓冲**（全屏 base64 大负载会淹没有界缓冲；截图保持按需 `screenshot.capture`，见 architecture-decisions.md）
- [x] IPC 调用超时处理（Rust `IpcClient` 已有 30s 超时；GUI `connection.refresh` 捕获错误并置 disconnected）

### Debugger 端点实现（Go orchestrator）— ✅ 完成（直连模式，2026-06-20）

现状：EmmyLua 调试由 VSCode 直连 runtime:9966，Go server 不中转调试协议（双向流不适合
dashboard → server → agent 请求/响应模型）。原裸 501 stub 已替换为结构化「直连模式」契约。

- [x] `GET /api/debugger/info`：返回调试模式说明 + 各 agent 调试端点（host:9966）+ VSCode launch.json 片段
- [x] `connect/command/breakpoints`：返回结构化 501，指向直连模式（不再是裸 stub）
- [x] 测试：`debugger_test.go`（info 返回 direct_attach + agent 端点；connect 返回指引）
- [x] README 文档更新

---

## ✅ 代码缺陷（2026-06-21 分析发现，已全部修复）

> 来源：[docs/pending-changes.md](./docs/pending-changes.md) 第 5 节。
> 全部 10 项已修复（含回归测试 `TestBroadcastMessagePerUserReadState`）。

### 高优先级

- [x] **Dashboard `listPermissions` 导出重名冲突**：`admin.ts` 版本重命名为 `listPermissionCatalog` / `ListPermissionCatalogResponse`，Roles 页已改用新名；`permissions.ts` 保留为 Profile 页使用的规范化版本
- [x] **广播消息已读状态污染（`handlers/messages.go`）**：新增 `MessageRead` per-user 关联表（`message_reads`，AutoMigrate 已接入）；列表/未读数/标记已读全部改用 `EXISTS` 子查询判定 per-user 状态，不再更新共享行 `status`。回归测试 `TestBroadcastMessagePerUserReadState` 已加

### 中优先级

- [x] **路由重复注册（`main.go`）**：移除 `/api/v1` 组中重复的 messages/feedback，仅保留 `/api`（dashboard 兼容）组
- [x] **`PermissionRequired` 为死代码**：已接线——`agents:manage` / `workflows:run` / `scripts:edit` / `scripts:run` / `users:manage` / `roles:manage` / `settings:view` / `settings:edit` 分组鉴权（admin 自动放行），取代原粗粒度 `RoleRequired("admin")`
- [x] **Runtime jitter 粒度粗（`remote_client.cpp`）**：`computeBackoff` 返回毫秒、`sleepInterruptible` 接受毫秒，抖动改为 0~999ms 精确粒度
- [x] **Dashboard Settings 端点不一致**：Go server 在 `/api` 组补充 `/settings`（GET `settings:view` / PUT `settings:edit`），Settings 页改用 `/api/settings`，与其余 dashboard 调用前缀统一
- [x] **Dashboard `canUserManage`/`canRoleManage` 路由未用**：新增 `canAccessAdmin` 控制菜单可见性；`/admin/users` 用 `canUserManage`、`/admin/roles` 用 `canRoleManage` 细粒度守卫

### 低优先级

- [x] **`triggers.ts` `markFired` 注释与实现不符**：新增 `last_triggered_at` 字段，`markFired(id,name,timestamp)` 记录命中时间戳；events.ts 传入 `event.timestamp`；注释与实现一致
- [x] **`script.state_changed` 未覆盖 error**：`StandaloneMode::start()` 订阅 `ScriptManager` error 事件，异常退出推送 `state:"error"` + 错误信息
- [x] **EventLogSink 无 payload 大小截断**：消息超 4096 字节截断并追加 `...[truncated]`
- [x] **EventBuffer 跨类型 FIFO 公平性**：容量超限时优先丢弃高频 `log.line`，保护低频重要事件（trigger/connection/script）；`events.drain` 响应新增 `dropped` 累计丢弃计数，GUI 可感知

### Windows 平台修复（2026-06-21）

- [x] **CRLF 警告刷屏**：仓库无 `.gitattributes` + `core.autocrlf=true` → 每次 git 操作 48 文件警告。新增 `.gitattributes`（源码统一 LF、`.bat/.cmd/.ps1/.sln/.vcxproj` 保留 CRLF、二进制标记 binary）并 renormalize，警告归零
- [x] **`ClipboardTest.Clear` 偶发失败**（Win32 剪贴板）：①`clear()` 绕过 `openClipboard()` 重试逻辑直接调 `OpenClipboard`，锁竞争时静默失败 → 改用重试版本；②`openClipboard` 重试预算 5×10ms=50ms 太紧 → 提升到 20×25ms=500ms；③剪贴板测试改为 `TEST_F` + `SetUp` 探测可用性，OS 拒绝访问时 `GTEST_SKIP`（环境问题）而非误报失败。全套件 1705/1705 稳定

---

## 🟡 P1 - 高优先级（功能完善）

### Orchestrator Dashboard（`orchestrator/dashboard/`）收尾

**已完成页面**（已对接真实 API + WebSocket，勿重做）：
- ✅ Welcome、Agents（getAgents + shutdownAgent + WS 事件）
- ✅ Scripts（Monaco 编辑器 + CRUD + run/stop/logs）
- ✅ Workflows（submit/cancel + WS submitted/status_changed/progress + Steps 可视化）
- ✅ Admin/LoginLogs、Admin/OperationLogs（listAudit + CSV 导出）
- ✅ User/Login、Profile（7 tab，最完整）

**待完善**：
- [x] **Monitor 页面**（`Monitor/index.tsx`）：mock 已移除（trigger 列表改为 WS 事件驱动上限 20，无 agent 时空态 + Alert）；远程 `trigger.list` API 未暴露（设计性增强，非阻塞）
- [x] **Settings 页面**（独立 `pages/Settings/index.tsx`，读写 server 键值，admin 可编辑）
- [x] Dashboard 截图实时推送（Monitor 监听 `screenshot` 事件，runtime 按需 capture——架构决策：不进 drain）

### Tauri GUI（`apps/gui/`）收尾

**已完成**：dashboard/scripts/screen/triggers/settings/logs 六页面 + IPC 连接重试 + 全部 Tauri 命令 + events 轮询（logs/trigger/script 事件已接）。
（注：`editor/` 空占位目录已删除）

- [x] `scripts/+page.svelte`：**启动器**定位（按路径加载/运行/停止 + 状态可视化），**不做内置编辑器**——脚本编辑统一用 VS Code（EmmyLua 补全 + wingman.d.lua + launch.json 调试，见 `docs/development-environment.md`）；`editor/` 空占位目录已删除
- [x] logs 页面接收 IPC 推送的 runtime 日志（events.ts → logs.addRuntime）
- [x] dashboard 截图实时推送（按需 screenshot.capture + events 轮询）

### Go orchestrator 增强

**已完成**：工作流引擎（DAG + 环检测 + 持久化 + WS 事件）、Agent 心跳（30s/90s 超时）、
JWT auth（bcrypt + 限流）、审计日志、Team/投票/Inbox。

- [x] **工作流引擎增强**（引擎已可用，重试+负载均衡+模板已加，2026-06-20）
  - [x] 指数退避重试策略（`WorkflowStep.MaxRetries`/`RetryBackoffSeconds`，取消不重试）
  - [x] Agent 负载均衡（`selectAgent` 选在执行步骤最少的 agent；显式 worker 优先）
  - [x] 工作流模板库（`GET /api/workflow-templates`：5 内置模板 + dashboard 模板选择器）
  - [~] 独立步骤类型（`wait` 已实现；condition/screenshot 待续）
- [x] **Agent 管理**
  - [x] 负载均衡（`selectAgent` 选在执行步骤最少的 agent；显式 worker 优先）
  - [x] Agent 分组/标签（`PUT /api/agents/:id/tags` + 注册表 SetTags + Dashboard 标签列/Popover 编辑）
- [ ] **通知/历史**
  - [x] WebSocket 事件广播完善（runtime `EventBuffer` 远程 sink → `RemoteClient::sendAgentEvent` → server `handleEvent` 广播 `trigger_fired`/`script_state` 到 Dashboard WS；listener.go 新增 `script_state` case）
  - [x] 通知历史持久化（Message 模型已入库；per-user 已读见代码缺陷修复）
  - [ ] script_output 转发（需接 ScriptManager `setOutputCallback`，当前未捕获脚本 stdout）
- [ ] **API 文档**：Swagger/OpenAPI（当前无）

### Runtime Agent Outbound（C++ — 基本完成）

**已完成**：`remote_client.cpp` 心跳线程（30s `agent.heartbeat`）、重连线程、注册流程、**指数退避重连**（2026-06-20）。

- [x] 断线重连指数退避（`reconnectLoop` + `computeBackoff`：base×2^attempt 封顶 + 抖动；`max_reconnect_interval` 配置；连接成功重置计数）
- [x] 持久重连循环（同时处理初始失败与断线，可被 stop 打断）
- [x] 命令队列 / 离线缓存（有界 outbox：断线缓冲 Notify 类，重连后 flush；Response 类丢弃避免陈旧）
- [x] 连接状态回调通知 GUI（`RemoteClient::onEvent` → EventBuffer `connection.state_changed` → events.ts → connection store `remote` 字段 + 日志；GUI 可显示远程链路状态）

---

## 🟢 P2 - 中优先级（功能增强）

### 跨平台验证

> 编译跨平台已由 CI 矩阵保证（C++ Ubuntu/macOS、Go 三平台、Dashboard 三平台打包）。
> 以下为各平台**运行时功能**的人工/集成验证（需在实际 OS 上执行）：

- [ ] **macOS**（基础实现已存在）：UDS / Clipboard / CGWindowList 截图 / FileWatcher / CGEvent 输入 — ⚠️ 需 macOS 真机验证，无法在当前环境完成
- [ ] **Linux**（基础实现已存在）：UDS / X11+XTest / inotify / xclip / XGetImage — ⚠️ 需 Linux 真机验证，无法在当前环境完成

### 配置和协议统一

- [x] 统一 IPC 协议格式（JSON envelope over Named Pipe/UDS，已就位，见 `docs/protocols.md`）
- [x] 统一 Agent-Orchestrator 通信协议文档（见 `docs/protocols.md`，TCP 二进制帧 16B header + JSON body）
- [x] 统一配置文件格式和路径（TOML：`config.hpp` 已支持 `loadFromString(TOML)`/`loadFromFile`/`saveToFile` + `apps/runtime/config/agent.toml` 在用）
- [x] 清理 `pkg/agent/client.go` 已废弃的 dialing Client/Pool（架构违规代码已移除，保留共享协议类型）

### 高级功能

- [x] OCR 支持（引擎已实现：`lib/wingman/src/ocr.cpp` + `ocr_stub.cpp` + test；build flag `WINGMAN_ENABLE_OCR`；可选增强：runtime RPC 暴露 + 多语言）
- [x] ML/AI 支持（引擎已实现：`lib/wingman/src/ml.cpp` + `ml_stub.cpp` + test；build flag `WINGMAN_ENABLE_ML`；`docs/guides/yolo-guide.md`）
- [x] 宏系统 UI（引擎 `recorder.hpp` + win32/cocoa/x11；**引擎修复**：低层钩子加消息泵线程 + 线程安全；runtime `macro_handler` RPC：start/stop/play/status/save/load/clear；Tauri 命令；GUI 宏录制页：录制/停止/回放/速度/重复/保存载入 + 侧栏入口）
- [ ] 游戏配置模板库（导入/导出/版本管理）

### 用户体验

- [x] 全局快捷键（`hotkeys.rs`：profile 热键 + F5/F6/F7/F12 默认；`reload_hotkeys` Tauri 命令已暴露供 profile 变更后刷新）
- [x] 系统托盘（`tray.rs`：左键切换窗口显隐 + 右键菜单「显示/隐藏」「退出」；Cargo `tray-icon` feature）
- [x] 主题系统（亮/暗双主题：`app.css` CSS 变量 + `stores/theme.ts` 持久化 + TopBar 日/月切换按钮）

---

## 🔵 P3 - 低优先级（工程优化）

### 测试（Go server 覆盖率大幅提升 — 75 个测试函数）

- [x] **Go orchestrator 单元测试**
  - [x] rbac（种子/解析/admin 旁路/inactive）+ handlers 用户/角色 CRUD
  - [x] workflow engine（DAG 环检测/步骤校验/selectAgent 负载均衡/executeStep 成功-重试-取消-无 agent/Submit 端到端）
  - [x] handlers：script（创建/校验/保存/读取/运行/日志）、agent（列表/查询/shutdown）、screenshot（广播/校验/超限）、audit（过滤/空态）、debugger（info 直连模式）
  - [x] websocket hub（注册/广播/rooms/agent 事件形状）
  - [x] registry 心跳（注册/列表/状态更新/超时判定离线/Stop 幂等）
  - [x] middleware auth（JWT 校验）
  - [x] 修复 listener_test.go 既有 vet 警告（goroutine 内 Fatalf → channel 回传测试 goroutine）
  - [x] handlers（script/agent/audit/screenshot）补全
- [ ] **C++ runtime**：当前 1584 测试 / 90.04% 覆盖（保持水准）
- [ ] **集成测试**：GUI → IPC → Runtime；Agent → Orchestrator；Dashboard → API
- [ ] **性能测试**：截图基准、WS 并发、工作流调度

### 文档

- [x] 快速开始指南（`docs/guide/getting-started.md`：环境/vcpkg/编译/第一个脚本 + 三种运行模式）
- [ ] Dashboard 使用教程、Runtime GUI 使用教程
- [ ] 脚本开发指南（Lua/Python API）
- [x] IPC 协议规范、Agent-Orchestrator 协议规范（`docs/protocols.md`）
- [ ] 贡献指南

### 构建和部署

- [x] CI/CD：GitHub Actions — C++（Windows 全量+覆盖率 / Ubuntu+macOS proto/transport）、Go（Ubuntu/Windows/macOS vet+build+test-race）、Dashboard（3 OS 打包）
- [ ] 自动发布（tag→release）
- [ ] 打包分发：Windows MSIX/InnoSetup（InnoSetup 已有）、macOS dmg、Linux AppImage、Docker（Orchestrator）

---

## 📊 各模块实际完成度（已校准 2026-06-20）

| 模块 | 子功能 | 完成度 | 说明 |
|------|--------|--------|------|
| **Runtime (C++)** | 核心引擎 | 95% | screen/input/trigger/vision/btree/macro/human 全部完成 |
| | IPC Server | 92% | Named Pipe + UDS + events.drain（pull 模型，log/trigger/script/connection 事件已接 + dropped 计数） |
| | RPC Handlers | 85% | system/trigger/script/screen/window/events 已接通 |
| | Agent Outbound | 100% | 心跳 + 重连 + 指数退避（毫秒抖动）+ 离线 outbox + 连接状态回调通知 GUI |
| | 脚本引擎 | 100% | Lua (sol2) + Python (pybind11) 双语言 |
| **GUI (Tauri)** | 框架 | 100% | Tauri 2.0 + Svelte 5 |
| | 页面实现 | 88% | 六页面可用 + events 轮询；scripts 页较薄 |
| | IPC 通信 | 92% | 连接重试/命令/事件轮询/30s 超时已通 |
| **Orchestrator (Go)** | HTTP API | 92% | auth/agent/script/workflow/audit/profile/settings/status/window/workflow-templates/admin(RBAC) |
| | WebSocket | 90% | Hub + rooms + agent/workflow/debugger 事件广播 |
| | Agent 监听 | 90% | FrameListener TCP 协议 + 心跳 + Team/Inbox |
| | 工作流引擎 | 97% | DAG/环检测/持久化/取消/超时/重试/负载均衡/模板（缺独立步骤类型） |
| | 权限系统 | 95% | ✅ RBAC（模型+中间件+API+Dashboard 页面 + PermissionRequired 已接线 8 权限码）；可选：Swagger |
| | Debugger | 100% | `/api/debugger/info` 直连模式契约 |
| | 测试 | 70% | 75 个测试函数覆盖 rbac/workflow/handlers/hub/registry/middleware/debugger；vet 全清 |
| **Dashboard (React)** | 页面框架 | 97% | 9 页面（+Settings）+ 路由 + dist 已构建 |
| | 组件实现 | 90% | Agents/Scripts/Workflows/Admin/Login/Profile/Settings/Support 完成；Monitor 去 mock（WS 事件驱动 + 空态） |
| | WebSocket | 90% | wsService + 自动重连 + agent/workflow 事件 |
| | API 对接 | 92% | wingman.ts + admin.ts（用户/角色/权限/模板）全对接 |

---

## 🗓️ 迭代计划（已校准）

### Sprint A (2周) — RBAC 权限系统 ✅ 已完成 (2026-06-20)
- [x] Role/Permission 模型 + AutoMigrate
- [x] 用户/角色管理 API + PermissionRequired 中间件
- [x] Dashboard 权限页面 (Admin/Users、Admin/Roles) + 路由守卫

### Sprint B (2周) — Runtime IPC 事件推送 ✅ 完成 (2026-06-20)
- [x] runtime → local IPC 事件缓冲 + `events.drain` RPC（EventBuffer + spdlog sink）
- [x] GUI 事件轮询分发（events.ts → logs.addRuntime + triggers.markFired）
- [x] trigger.fired 事件源（TriggerManager::setOnFired → EventBuffer）
- [x] script.state_changed 事件源（StandaloneMode → EventBuffer）
- [x] screenshot.frame 评估：不接入 drain（按需 capture）
- [x] IPC 调用超时（Rust 30s + GUI 错误处理）

### Sprint C (1周) — Dashboard 收尾 ✅ 完成
- [x] Monitor triggers/macros 去 mock（WS 事件驱动 + 空态）、指标空态
- [x] 独立 Settings 页面
- [x] 截图链路（按需 capture + events）

### Sprint D (1周) — Debugger + 增强（完成）
- [x] Debugger 端点明确降级为直连模式（`/api/debugger/info` + 结构化 501）
- [x] 工作流重试策略（指数退避）+ Agent 负载均衡（selectAgent 最少在执行）
- [x] Agent 断线指数退避（reconnectLoop + computeBackoff）
- [x] Agent 离线命令队列（有界 outbox + flush）
- [x] 工作流模板库（4 内置模板 + dashboard 选择器）
- [x] Agent 分组/标签（PUT tags API + Dashboard 标签编辑）

### Sprint E (2周) — 测试 + 跨平台 + 收尾
- [x] Go orchestrator 测试覆盖（handlers/engine/hub/registry/rbac — 75 个测试函数，vet 全清）
- [x] 跨平台 CI 矩阵：C++（Windows 全量 + Ubuntu/macOS proto/transport）、Go（Ubuntu/Windows/macOS vet+build+test-race）、Dashboard（3 OS 打包）
- [ ] 文档（使用教程）+ 自动发布（tag→release）— 见 P3 文档/构建章节

---

## 📝 注意事项

1. **架构优先**：修改 runtime/orchestrator 前，先看 `docs/architecture-decisions.md`
2. **IPC 边界**：GUI 只能通过 local IPC 控制 runtime，禁止 runtime 开 HTTP/WS server
3. **远程链路**：Dashboard → Go server → runtime (outbound)，Dashboard 不直连 runtime
4. **Dashboard 位置**：真正的 dashboard 在 `orchestrator/dashboard/`，根目录 `dashboard/` 是无关的 Croupier 副本
5. **vcpkg 约束**：所有 C++ 依赖必须走 vcpkg x64-windows-static
6. **测试基线**：C++ runtime 1584 测试 / 90.04% 覆盖；Go server 75 个测试函数（rbac/workflow/handlers/hub/registry/middleware/debugger），vet 全清

---

## 🔗 相关文档

- [ROADMAP.md](./ROADMAP.md) — 项目开发路线图
- [docs/architecture.md](./docs/architecture.md) — 架构设计文档
- [docs/architecture-decisions.md](./docs/architecture-decisions.md) — 架构决策记录（硬约束）
- [docs/API.md](./docs/API.md) — API 文档
- [docs/development-environment.md](./docs/development-environment.md) — VS Code 开发环境
