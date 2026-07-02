# 未提交改动分析（2026-06-21）

> 本文档对当前工作区相对 `origin/main` (`75c54cc`) 的全部未提交改动进行系统分析，
> 用于：(1) 作为提交/PR 的参考说明；(2) 记录分析中发现的问题供后续修复；
> (3) 核对 `todo.md` 的完成度声明与代码是否一致。
>
> 分析方式：4 个并行子代理逐模块 diff + 新文件直读 + 架构约束合规核查。
> 关键结论均已抽样回查源码确认（见文末「已抽样核实」）。
>
> **🔄 更新（2026-06-21）**：第 5 节发现的问题已**全部修复**（见各条 ✅ 标记）；
> 另额外完成：远程连接状态回调通知 GUI（`connection.state_changed`）、EventBuffer
> 公平性（优先丢 log.line + dropped 计数）、全局快捷键 reload 命令、快速开始指南补充运行模式。
> 实时状态以 `todo.md` 为准；本文档为提交前的分析快照。

---

## 0. 规模总览

| 模块 | 修改文件 | 新增文件 | 净变更 |
|------|---------|---------|--------|
| Go orchestrator server (`orchestrator/server/`) | 14 | 17 | +830 / −415 |
| Dashboard React (`orchestrator/dashboard/`) | 13 | 6（5 页面 + 1 API） | +346 / −125 |
| Runtime C++ (`apps/runtime/`) | 9 | 5 | +434 / −44 |
| GUI Tauri/Svelte (`apps/gui/`) | 6 | 2 | （含上） |
| lib/wingman | 5 | 0 | 小幅 |
| 文档 / CI / 构建 | 14 | 2 | 中等 |
| **合计** | **61** | **32** | **+2027 / −930** |

整体主题：**清理架构违规 + 补齐 RBAC/事件系统/工作流编排 + 配套测试与文档**。

---

## 1. 功能特性归类

### 1.1 Go Orchestrator Server

| 功能领域 | 涉及文件 | 说明 |
|----------|---------|------|
| **RBAC 权限系统（新）** | `internal/rbac/`、`models/role.go`、`middleware/auth.go`、`handlers/profile.go` | Role/Permission（many2many）+ 幂等种子（admin/operator/viewer + 14 个 `resource:action` 权限码 + `*` 通配）+ admin bypass + `PermissionRequired` 中间件 + `/profile/permissions` 下发真实权限 |
| **用户/角色管理 API（新）** | `handlers/users.go`、`handlers/roles.go` | 用户 CRUD + 重置密码 + 分页过滤；角色 CRUD（自定义角色、内置保护、引用保护）；权限目录查询 |
| **工作流引擎增强** | `workflow/engine.go`、`models/workflow.go` | 负载均衡 `selectAgent`（inflight 最少优先）+ 指数退避重试（`MaxRetries`/`RetryBackoffSeconds`）+ `wait` 步骤类型 + 取消/超时语义 |
| **工作流模板（新）** | `workflow/templates.go`、`handlers/workflow.go` | 5 个内置模板（单步监控/并行采集/串行流水线/fan-out 汇总/带 wait 节拍）；`GET /workflow-templates` 只读 |
| **Debugger 直连模式** | `handlers/debugger.go` | 占位 501 → 结构化直连指引：`GET /info` 返回各 agent `host:9966` + launch.json；其余端点返回带 `mode:"direct_attach"` 的 501 |
| **消息/反馈（新）** | `handlers/messages.go`、`handlers/feedback.go` | 站内信（recipient `*` 广播、未读计数、单条/全部已读）；Feedback 提交 → 自动生成两条 Message + 审计 |
| **Profile 增强** | `handlers/profile.go`、`models/models.go` | User 补 `Active/Nickname/Email/Phone/Avatar/LastLoginAt`；`SafeUser` 脱敏；按需更新；`/permissions` 展开为 `{resource, actions[]}` |
| **Auth 增强** | `handlers/auth.go` | 登录拒绝 inactive（403）、记录 `last_login_at`、响应带 `active` |
| **Agent 标签（新）** | `agent/registry.go`、`handlers/agent.go` | `SetTags`（去重）+ 广播 + `PUT /agents/:id/tags`（admin）；内存态 |
| **架构合规清理** | `pkg/agent/client.go` | **删除 −262 行**：整个 server→runtime 拨号 `Client` + `Pool`（违反约束 #2）。仅保留协议常量供 listener 使用 |

### 1.2 Dashboard (React)

| 功能领域 | 涉及文件 | 说明 |
|----------|---------|------|
| **Account/Messages 页（新）** | `pages/Account/` | 站内信列表 + 筛选 + 已读标记 |
| **Admin/Users 页（新）** | `pages/Admin/Users/` | 用户 CRUD + 角色分配 + 启停 + 重置密码 + 分页搜索 |
| **Admin/Roles 页（新）** | `pages/Admin/Roles/` | 角色 CRUD + 权限多选分配（按 category 分组）+ 内置保护 |
| **Settings 页（新）** | `pages/Settings/` | 服务端运行配置（日志级别/并发上限/端口只读）+ 调试说明 |
| **Support/Feedback 页（新）** | `pages/Support/` | 反馈/权限申请提交（category/priority/content） |
| **API 服务层（新/改）** | `services/api/admin.ts`(新)、`messages.ts`、`permissions.ts`、`support.ts`、`wingman.ts` | stub → 真实实现；新增 `setAgentTags`、`getWorkflowTemplates` |
| **Monitor 去除 mock** | `pages/Monitor/index.tsx` | 删除 `Math.random()` 兜底；triggers 改为 WS 事件驱动（上限 20）；无 agent 时明确空态；移除无后端的 trigger 开关/macro 按钮 |
| **Agents 标签列** | `pages/Agents/index.tsx` | Popover + Select tags 内联编辑 |
| **Workflows 模板选择** | `pages/Workflows/index.tsx` | 创建弹窗加模板 Select，回填表单 |
| **MessagesBell 挂载** | `app.tsx`、`components/MessagesBell.tsx` | 顶栏挂载；跳转路径修正 |

### 1.3 Runtime (C++)

| 功能领域 | 涉及文件 | 说明 |
|----------|---------|------|
| **事件缓冲（新）** | `event_buffer.hpp/.cpp` | 线程安全有界队列（`kMaxEvents=1000`，超出 `pop_front` + `dropped_` 计数） |
| **日志事件 sink（新）** | `event_log_sink.hpp` | 继承 `base_sink<std::mutex>`，过滤 level，转发为 `log.line` |
| **events.drain RPC（新）** | `rpc/handlers/event_handler.cpp` | `drain(max=500)` → `{events[], remaining}`，pull 模型 |
| **触发器事件** | `local_ipc_server.cpp` | `TriggerManager::setOnFired` → push `trigger.fired` |
| **脚本状态事件** | `standalone_mode.cpp` | start/pause/resume/stop 转换 → push `script.state_changed` |
| **Agent outbound 增强** | `remote_client.cpp/.hpp`、`config.hpp`、`agent_config.cpp` | 单一持久 `reconnectLoop` + `computeBackoff`（base×2^attempt 封顶 + 抖动）+ `max_reconnect_interval` 配置；有界 outbox（`kMaxOutbox=100`）断线缓冲、重连 flush |
| **触发器回调机制** | `lib/wingman/trigger.hpp` + win32/posix | 新增 `setOnFired` 钩子（锁外回调避免死锁） |

### 1.4 GUI (Tauri/Svelte)

| 功能领域 | 涉及文件 | 说明 |
|----------|---------|------|
| **事件轮询（新）** | `stores/events.ts`、`commands/events.rs` | 500ms `setInterval` → `drain_events` → 分发 logs/triggers；App.svelte 按连接启停 |
| **日志接收** | `stores/logs.ts` | `addRuntime` + 1000 条上限截断 |
| **触发器接收** | `stores/triggers.ts` | `markFired(id,name)` |
| **Scripts 页增强** | `routes/scripts/+page.svelte` | **启动器 + 状态可视化**（非编辑器）：连接状态胶囊、按路径启动、quick 脚本卡、运行概览统计 |

### 1.5 lib/wingman（健壮性修复）

| 文件 | 改动 |
|------|------|
| `trigger.hpp` + win32/posix trigger | `setOnFired` 钩子；锁外回调 |
| `team_module.cpp` | `getTeamStatus` 在 `client==nullptr` 时返回结构化空 JSON（防 Lua nil 解引用） |
| `vision.cpp` | `getDominantColor` ROI 越界/空 Mat 防御；非连续 Mat `clone()` 再 `reshape` |

### 1.6 文档 / CI / 构建

| 文件 | 改动 |
|------|------|
| `docs/protocols.md`（新，167 行） | 权威协议规范：Local IPC（4B 头+JSON）/ Agent TCP（16B 头+JSON，纠正旧称 Protobuf）/ Dashboard WS，三条链路总览 |
| `docs/development-environment.md`（新，105 行） | VS Code 扩展/配置、Lua 补全、EmmyLua attach 调试、Python stub、tasks |
| `docs/architecture-decisions.md` | 新增 ADR「Runtime-to-UI Event Delivery (Pull, not Push)」 |
| `docs/api/debugger.md`、`api/debugging.md`、`guide/debugging.md` | 统一直连模式：`emmylua_new`、端口 9966、插件 `tangzx.emmylua`；删除虚构 API |
| `docs/remote_protocol.md` | 加历史警告横幅，纠正 Protobuf 误述 |
| `todo.md` | 2026-06-20 全面重校准（位置澄清表、里程碑、完成度、Sprint A-E） |
| `.github/workflows/ci.yml` | go-build 单平台 → 三平台矩阵（ubuntu/windows/macos）+ `go vet` + `-race` |
| `CMakeLists.txt` | runtime tests 改由 `WINGMAN_BUILD_CLIENT_TESTS` 内部控制 |
| `.gitignore` | `.vscode/` 白名单例外提交仓库级配置 |

---

## 2. 架构合规性核查

依据 `CLAUDE.md` 四条硬约束 + `docs/architecture-decisions.md`：

| 约束 | 结论 | 证据 |
|------|------|------|
| #1 Go server 是远程中控编排器 | ✅ | 所有新接口（RBAC/消息/模板/标签）集中在 server |
| #2 Runtime 主动 outbound 连接 | ✅✅ | `client.go` 删除 Client/Pool（server→runtime 拨号）正是清理违规代码；全仓无残留引用 |
| #3 Dashboard 只连 Go server | ✅ | 所有新 API 都打 Go server，无 runtime 直连 |
| #4 Runtime 禁止 HTTP/WS server | ✅ | Debugger 明确拒绝中转调试流，改为 VSCode 直连 runtime:9966；events.drain 复用既有 RpcDispatcher |
| ADR: 事件走 pull 模型 | ✅ | EventBuffer + drain，未引入 `type=2` push 帧 |
| ADR: 截图不走 drain | ✅ | 无 `screenshot.frame` 挂载点 |

**结论：本次改动显著增强了架构合规性**，client.go 清理与 debugger 直连化是针对约束 #2/#3 的正面修正。

---

## 3. 测试覆盖

| 测试集 | 新增文件 | 测试函数数 | 覆盖点 |
|--------|---------|-----------|--------|
| Go rbac | `rbac/rbac_test.go` | 7 | 种子幂等、admin bypass、权限解析、inactive |
| Go workflow | `engine_test.go`、`templates_test.go` | 19 | DAG 环检测、selectAgent 负载均衡、重试/取消/wait、模板合法性 |
| Go handlers | `rbac_test.go`、`debugger_test.go`、`auth_test.go`、`profile_test.go`、`handlers_test.go` | 26 | 用户/角色 CRUD、脚本 CRUD/运行、agent shutdown/setTags、截图、审计、feedback→message、debugger 直连 |
| Go agent/ws | `registry_test.go`、`hub_test.go` | 10 | 注册/心跳超时/标签、hub 广播/rooms |
| Go listener | `listener_test.go`（改） | 3 | 修复 goroutine 内 `t.Fatalf` 竞态 |
| **Go 合计** | **16+ 文件** | **75 函数**（`go test ./...` 全绿；含回归测试 `TestBroadcastMessagePerUserReadState`） | |
| C++ runtime | （CMake 已接入新源，未新增测试文件） | — | 维持 1584 测试 / 90.04% 基线 |

---

## 4. 完成度评估（按功能）

| 功能 | 完成度 | 备注 |
|------|--------|------|
| RBAC 模型 + 种子 + 解析 | ✅ 完整 | 幂等、有测试 |
| **RBAC 路由级细粒度鉴权** | ⚠️ **半成品** | `PermissionRequired` 已实现但 main.go 中 **0 处调用**（死代码）；所有管理路由仍用粗粒度 `RoleRequired("admin")` |
| 用户/角色管理 API | ✅ 完整 | 含校验、引用保护、审计 |
| 工作流负载均衡 + 重试 + wait | ✅ 完整 | 19 个引擎测试 |
| 工作流模板 | ✅ 完整 | 5 模板 + 只读接口 |
| Debugger 直连模式 | ✅ 完整 | 设计上故意「不实现」代理，文档/测试齐全 |
| 消息/反馈 | ⚠️ 功能完整 | 广播消息已读状态污染（见问题 #1） |
| Agent tags | ✅ 完整 | 内存态，重启丢失（设计如此） |
| Runtime EventBuffer + drain | ✅ 完整 | 三类事件已接 |
| Runtime 指数退避重连 | ✅ 功能完整 | jitter 粒度粗（见问题 #5） |
| Runtime outbox 缓冲 | ✅ 功能完整 | flush 中途断线静默丢（幂等可接受） |
| GUI 事件轮询 | ✅ 完整 | pull 模型自洽 |
| GUI Scripts 页 | 🚧 启动器完成 | 非编辑器；todo 自评「较薄」 |
| Dashboard 新页面 ×5 | ✅ 高完成 | listPermissions 重名（见问题 #2） |
| Dashboard Monitor 去 mock | ✅ 完整 | 诚实降级 |

---

## 5. 分析中发现的问题

> ✅ 全部问题已修复。#13 Dashboard Agents 标签前端 access 控制已于 2026-07-02 补齐（前端按 `agents:manage` 禁用编辑入口；后端 403 仍兜底）。

### 🔴 高

1. ✅ **Dashboard `listPermissions` 导出重名冲突**（已修复）
   - `admin.ts` 版本重命名为 `listPermissionCatalog` / `ListPermissionCatalogResponse`，Roles 页已改用新名
   - `permissions.ts` 保留为 Profile 页使用的规范化版本（resource/action 拆分）

2. ✅ **广播消息已读状态污染（Go messages.go）**（已修复）
   - 新增 `MessageRead` per-user 关联表（`message_reads`，AutoMigrate 接入）
   - 列表/未读数/标记已读改用 `EXISTS` 子查询判定 per-user 状态，不再更新共享行 `status`
   - 回归测试 `TestBroadcastMessagePerUserReadState` 已加

### 🟡 中

3. ✅ **路由重复注册（Go main.go）**（已修复）
   - 移除 `/api/v1` 组中重复的 messages/feedback，仅保留 `/api`（dashboard 兼容）组

4. ✅ **`PermissionRequired` 为死代码**（已修复）
   - 已接线：`agents:manage` / `workflows:run` / `scripts:edit` / `scripts:run` / `users:manage` / `roles:manage` / `settings:view` / `settings:edit` 分组鉴权（admin 自动放行），取代原粗粒度 `RoleRequired("admin")`

5. ✅ **Runtime jitter 粒度粗（remote_client.cpp computeBackoff）**（已修复）
   - `computeBackoff` 返回毫秒、`sleepInterruptible` 接受毫秒，抖动改为 0~999ms 精确粒度

6. ✅ **Dashboard Settings 端点不一致**（已修复）
   - Go server 在 `/api` 组补充 `/settings`（GET `settings:view` / PUT `settings:edit`），Settings 页改用 `/api/settings`

7. ✅ **Dashboard `canUserManage`/`canRoleManage` 路由未用**（已修复）
   - 新增 `canAccessAdmin` 控制菜单可见性；`/admin/users` 用 `canUserManage`、`/admin/roles` 用 `canRoleManage` 细粒度守卫

### 🟢 低

8. ✅ **ROADMAP.md 与 todo.md 状态不同步**（已修复）
   - ROADMAP RBAC/事件/Debugger/测试从「未开始」改为「已完成」

9. ✅ **triggers.ts `markFired` 注释与实现不符**（已修复）
   - 新增 `last_triggered_at` 字段，`markFired(id,name,timestamp)` 记录命中时间戳；events.ts 传入 `event.timestamp`

10. ✅ **EventBuffer 跨类型 FIFO 公平性**（已修复）
    - 容量超限时优先丢弃高频 `log.line`，保护低频重要事件；`events.drain` 响应新增 `dropped` 累计计数

11. ✅ **`script.state_changed` 未覆盖 error**（已修复）
    - `StandaloneMode::start()` 订阅 `ScriptManager` error 事件，异常退出推送 `state:"error"` + 错误信息

12. ✅ **EventLogSink 无 payload 大小截断**（已修复）
    - 消息超 4096 字节截断并追加 `...[truncated]`

13. ✅ **Dashboard Agents 标签无前端 access 控制**（已修复）
    - `access.ts` 新增 `canAgentManage`，Agents 页按 `agents:manage` 禁用标签编辑和关闭 Agent 写操作入口；后端 403 继续兜底

---

## 6. 已抽样核实

以下结论已直接回查源码确认（非仅采信子代理）：

- ✅ 路由重复注册：main.go:117-121（`/api/v1`）与 156-160（`/api`）均注册 messages/feedback
- ✅ listPermissions 重名：permissions.ts:24/64 与 admin.ts:150/155
- ✅ PermissionRequired 死代码：全仓仅 3 处匹配，均在定义/注释，0 处调用
- ✅ client.go 删除：`git diff` 显示 −262 行，仅保留协议常量
- ✅ 架构合规：runtime 侧无 accept/listen，事件走 drain

---

## 7. 建议的提交策略

本次改动体量大（+2027/−930，93 个文件），建议拆分为多个语义提交以便 review：

1. `refactor(server): remove architecture-violating server→runtime dialing`（client.go 删除）
2. `feat(server): add RBAC with roles/permissions + admin middleware`（rbac/ + users/roles + 模型 + 测试）
3. `feat(server): workflow load balancing, retry, wait step, templates`（engine + templates + 测试）
4. `feat(server): debugger direct-attach mode + messages/feedback`（debugger + messages + feedback + 测试）
5. `feat(server): agent tags + profile/auth enhancements`（registry + agent + profile + auth）
6. `feat(runtime): event buffer + drain RPC + pull-model event delivery`（event_buffer + sink + handler + CMake）
7. `feat(runtime): exponential backoff reconnect + bounded outbox`（remote_client + config）
8. `feat(gui): event polling + scripts launcher page`（events.ts/rs + scripts page + stores）
9. `feat(dashboard): RBAC admin pages + settings + support + monitor mock removal`（新页面 + services）
10. `feat(lib): trigger onFired callback + team/vision robustness`（trigger + team_module + vision）
11. `docs: protocols spec + dev environment + debug direct-attach + todo recalibration`
12. `ci: expand go matrix to 3 platforms + vet + race`

> 若不拆分，至少在单个提交的 body 中引用本分析的「功能特性归类」与「问题」章节。

---

*本分析文档为未提交改动的快照，合并后可删除。*
