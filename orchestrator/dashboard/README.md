# Wingman Dashboard

`orchestrator/dashboard` 是 Wingman 的前端管理台，对接当前仓库内的 `orchestrator/server`。

当前文档只描述本仓库已经存在且可联调的能力，不再沿用历史迁移内容中的旧产品和旧接口说明。

## 概览

- 前端开发地址：`http://localhost:8000`
- 后端默认地址：`http://127.0.0.1:9527`
- WebSocket：`GET /ws`
- Agent TCP Listener：`127.0.0.1:8888`
- 数据库：SQLite
- 默认数据库文件：`orchestrator/server/data/wingman.db`

## 当前后端实现

当前 dashboard 对接的是本仓库 Go 服务：

- HTTP 框架：Gin
- 数据访问：GORM + SQLite
- 认证：JWT Bearer Token
- JWT 密钥：`WINGMAN_JWT_SECRET`，长度至少 32 个字符

实现参考：

- `../server/main.go`
- `../server/internal/config/config.go`
- `../server/internal/middleware/auth.go`

## 当前已对接页面

- `Agents`
- `Workflows`
- `Scripts`
- `Profile`
- `Admin`

这些页面主要通过 `src/services/wingman.ts` 以及 `src/services/api/*` 发起请求。

## 当前已实现接口

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
- `GET /api/scripts`
- `POST /api/scripts/content`
- `GET /api/audit`

### 仅 `admin`

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
- `POST /api/agents/:agentId/shutdown`
- `POST /api/workflows`
- `POST /api/workflows/:id/cancel`
- `POST /api/scripts`
- `POST /api/scripts/delete`
- `DELETE /api/scripts`
- `POST /api/scripts/save`
- `POST /api/scripts/run`
- `POST /api/scripts/stop`
- `POST /api/scripts/logs`
- `POST /api/debugger/connect`
- `POST /api/debugger/command`
- `GET /api/debugger/breakpoints`
- `POST /api/debugger/breakpoints`

## 启动

```bash
pnpm install
pnpm dev
```

默认访问 `http://localhost:8000`。

## 常用命令

```bash
pnpm dev
pnpm build
pnpm test
pnpm lint
pnpm tsc
```

## License

Apache-2.0
