# Wingman Server

`orchestrator/server` 是当前仓库内实际使用的 Go 后端，负责：

- HTTP API
- JWT 认证
- WebSocket Hub
- Agent TCP Listener
- SQLite 数据持久化

## 技术栈

- HTTP 框架：Gin
- ORM：GORM
- 数据库：SQLite
- 认证：JWT Bearer Token

## 默认网络配置

- HTTP 监听：`127.0.0.1:9527`
- Swagger UI（API 文档）：`http://127.0.0.1:9527/swagger/index.html`
- WebSocket：`GET /ws`
- Agent TCP Listener：`127.0.0.1:8888`

这些默认值来自：

- `internal/config/config.go`
- `main.go`

## 关键环境变量

- `WINGMAN_HOST`
- `WINGMAN_PORT`
- `WINGMAN_DB_PATH`
- `WINGMAN_AGENT_ADDR`
- `WINGMAN_SCRIPTS_DIR`
- `WINGMAN_STATIC_DIR`
- `WINGMAN_CORS_ORIGINS`
- `WINGMAN_JWT_SECRET`
- `WINGMAN_ADMIN_PASSWORD`
- `WINGMAN_AGENT_TOKENS`
- `WINGMAN_GUACD_ADDR`（guacd 网关地址，默认 `127.0.0.1:4822`）
- `WINGMAN_GUACD_DRIVE_PATH`（guacd 驱动器挂载路径，默认 `/wingman-drive`）
- `WINGMAN_GUACD_RECORDING_PATH`（会话录像路径，默认空）
- `WINGMAN_RECORDING_DIR`（录像目录，默认空）

其中：

- `WINGMAN_JWT_SECRET` 必填，且长度至少 32 个字符
- `WINGMAN_ADMIN_PASSWORD` 仅用于首次无用户时引导初始化管理员账号
- `WINGMAN_AGENT_TOKENS` agent 注册 token 白名单（逗号分隔，支持多 token
  并存以平滑轮换）；**空/未设置 = 关闭注册鉴权（默认，向后兼容）**。
  详见 `docs/agent-token-auth-design.md`

## 启动

```bash
cd orchestrator/server
go build -o ../../build/go-server.exe .
../../build/go-server.exe
```

如需联动 Agent：

```bash
../../build/wingman-agent.exe
```

### 静态资源（Dashboard）路径

服务通过 `WINGMAN_STATIC_DIR` 定位 dashboard 构建产物（`/assets/*`、`/index.html`）。

- 默认值：`../build/dist`（相对 server CWD，匹配打包布局）
- 本地从源码运行时，dashboard 构建产物在 `orchestrator/dashboard/dist`，需显式指定：

```bash
$env:WINGMAN_STATIC_DIR = "../../orchestrator/dashboard/dist"
../../build/go-server.exe
```

构建 dashboard：

```bash
cd orchestrator/dashboard
pnpm install
pnpm build     # 产物输出到 orchestrator/dashboard/dist
```

## 常用接口

> 本节只列常用端点，并非穷举；消息/反馈/执行记录/触发器/团队/批量操作/远程桌面（含 guacamole WebSocket）/保险箱/录像/会话审计等域见 `internal/handlers/routes.go`。
> 完整交互式 API 文档见 Swagger UI：`http://127.0.0.1:9527/swagger/index.html`
> 改动 handler 注解后用 `swag init -g main.go -o docs --parseDependency --parseInternal` 重新生成。

### 认证

- `POST /api/v1/auth/login`
- `POST /api/v1/auth/logout`

### 登录后可访问

- `GET /api/v1/status`
- `GET /api/v1/health`
- `GET /api/v1/profile`
- `GET /api/v1/profile/games`
- `GET /api/v1/profile/permissions`
- `PUT /api/v1/profile`
- `PUT /api/v1/profile/password`
- `GET /api/v1/windows`
- `GET /api/v1/settings`
- `GET /api/agents`
- `GET /api/agents/:agentId`
- `GET /api/workflows`
- `GET /api/workflows/:id`
- `GET /api/workflows/:id/workers`
- `GET /api/workflows/:id/steps/:stepId/status`
- `GET /api/workflow-templates`
- `GET /api/scripts`
- `POST /api/scripts/content`
- `GET /api/audit`

### 仅 `admin`（`/api/v1/*` 组）

`/api/v1/*` 组仍为粗粒度 `RoleRequired("admin")`：

- `POST /api/v1/screenshot`
- `GET /api/v1/scripts`
- `POST /api/v1/scripts`
- `DELETE /api/v1/scripts`
- `POST /api/v1/scripts/content`
- `POST /api/v1/scripts/save`
- `POST /api/v1/scripts/run`
- `POST /api/v1/scripts/stop`
- `POST /api/v1/scripts/logs`
- `PUT /api/v1/settings`

### `/api` 组（按权限码）

`/api` 组写接口已由粗粒度 `RoleRequired("admin")` 改为细粒度 `PermissionRequired`（按权限码放行，`admin` 通配自动通过），划分见 `internal/handlers/routes.go`：

- `agents:manage`：`POST /api/agents/:agentId/shutdown`、`PUT /api/agents/:agentId/tags`、`POST /api/teams`、触发器增删改查/toggle、`POST /api/agents/batch/trigger`
- `workflows:run`：`POST /api/workflows`、`POST /api/workflows/:id/cancel`
- `scripts:edit`：`POST /api/scripts`、`POST /api/scripts/delete`、`DELETE /api/scripts`、`POST /api/scripts/save`
- `scripts:run`：`POST /api/scripts/run`、`POST /api/scripts/stop`、`POST /api/scripts/logs`、`POST /api/agents/batch/run-script`、`POST /api/agents/batch/stop-script`

### 用户/角色/权限管理（RBAC，`admin`）

权限码采用 `resource:action` 形式（如 `users:manage`、`scripts:run`），内置 `admin`/`operator`/`viewer` 三种角色。启动时由 `rbac.Seed` 幂等写入；`admin` 角色拥有通配权限 `*`。

- `GET /api/admin/users`（分页，支持 `role`/`keyword` 过滤）
- `POST /api/admin/users`（创建：username/password/role/active）
- `GET /api/admin/users/:id`
- `PUT /api/admin/users/:id`（更新 role/active）
- `DELETE /api/admin/users/:id`（禁止删除自身与内置 admin）
- `POST /api/admin/users/:id/reset-password`
- `GET /api/admin/roles`（含权限列表）
- `GET /api/admin/roles/:code`
- `POST /api/admin/roles`（自定义角色）
- `PUT /api/admin/roles/:code`（更新名称/权限）
- `DELETE /api/admin/roles/:code`（禁止删除内置角色与仍被引用的角色）
- `GET /api/admin/permissions`（权限目录，支持 `category` 过滤）

前端通过 `GET /api/v1/profile/permissions` 获取当前用户 `permissionIDs`（含角色码），由 dashboard `access.ts` 转为访问控制标记（`canAdmin`/`canUserManage`/`canRoleManage`）。

### 工作流步骤类型

`POST /api/workflows` 的 `steps[]` 当前支持：

- `script`：默认类型，执行指定 `.lua` 脚本
- `wait`：无需脚本，按 `timeoutSeconds` 或 `parameters.seconds` 等待
- `condition`：无需脚本，按 `parameters.value/operator/expected` 做声明式判断
- `screenshot`：无需脚本，向选定 agent 下发 `screenshot.capture` 并广播截图事件

`condition` 失败会让该步骤标记为 `failed`，其下游步骤按依赖失败语义跳过。`screenshot` 需要至少一个在线 agent；若无可用 agent 或截图失败，工作流会按普通失败步骤处理。

### Debugger（直连模式）

Wingman 的 Lua 调试基于 EmmyLua，由 **VSCode 直连 runtime 的调试端口（默认 9966）**，Go server **不**中转调试协议（调试是双向流，不适合 dashboard → server → agent 的请求/响应模型）。

- `GET /api/debugger/info` — 返回调试模式说明 + 各 agent 调试端点（host:9966）+ VSCode launch.json 片段
- `POST /api/debugger/connect`、`POST /api/debugger/command`、`GET/POST /api/debugger/breakpoints` — 返回结构化 501，指向直连模式与 info 端点

接口都要求：已登录 + 角色 `admin`。前端应据 `mode: "direct_attach"` 展示 VSCode 附加指引而非当作错误。

## 认证说明

- 认证方式：`Authorization: Bearer <token>`
- Token 过期时间：15 分钟
- 未带 token、格式错误或 token 无效时返回 `401`
- 角色不满足时返回 `403`

## 数据库

默认数据库文件：

- `./data/wingman.db`

当前自动迁移的表包括：

- `users`
- `scripts`
- `settings`
- `execution_logs`
- `audit_logs`
- `agents`
- `workflows`
- `step_statuses`
- `roles`
- `permissions`
- `role_permissions`
- `RemoteSessionAudit`
- `Message`
- `MessageRead`
- `Feedback`
- `Execution`
- `VaultMaster`
- `RemoteCredential`

## 管理员初始化

当 `users` 表为空时：

- 如果设置了 `WINGMAN_ADMIN_PASSWORD`，服务会创建默认管理员账号 `admin`
- 如果没有设置，服务仍然会启动，但不会自动创建管理员账号

这点很重要：未设置 `WINGMAN_ADMIN_PASSWORD` 不会阻止服务启动。

## 当前边界说明

当前服务端并未实现以下历史能力：

- `workspace` 配置发布/回滚接口
- PostgreSQL / Redis 后端
- 独立的外部 `croupier` 服务契约

如需确认实际行为，请以代码为准：

- [main.go](main.go)
- [config.go](internal/config/config.go)
- [auth.go](internal/middleware/auth.go)
