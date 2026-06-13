# Workspace API 文档（历史草案）

本文档来自早期 `workspace` 方案设计，记录的是一组历史 API 草案，不代表当前 `Wingman` 仓库中的真实后端实现。

## 重要声明

当前 `orchestrator/server` 并未实现以下接口族：

- `/api/v1/workspaces/**`
- `/api/v1/functions/descriptors`
- `/api/v1/functions/:functionId/invoke`

因此，本文档不能作为当前仓库的：

- 联调契约
- 测试基线
- 产品能力说明
- 缺陷判断依据

## 当前真实实现应以什么为准

请优先参考以下真实入口：

- [Dashboard README](/D:/workspaces/wingman/orchestrator/dashboard/README.md:1)
- [Server README](/D:/workspaces/wingman/orchestrator/server/README.md:1)
- [main.go](/D:/workspaces/wingman/orchestrator/server/main.go:1)

## 当前已落地的主要接口方向

当前仓库真实存在并可验证的接口主线包括：

- `POST /api/v1/auth/login`
- `POST /api/v1/auth/logout`
- `GET /api/v1/profile`
- `GET /api/v1/profile/games`
- `GET /api/v1/profile/permissions`
- `PUT /api/v1/profile`
- `PUT /api/v1/profile/password`
- `GET /api/agents`
- `GET /api/agents/:agentId`
- `POST /api/agents/:agentId/shutdown`
- `GET /api/workflows`
- `GET /api/workflows/:id`
- `POST /api/workflows`
- `POST /api/workflows/:id/cancel`
- `GET /api/scripts`
- `POST /api/scripts/content`
- `POST /api/scripts`
- `POST /api/scripts/save`
- `POST /api/scripts/run`
- `POST /api/scripts/stop`
- `GET /api/audit`

## 如何使用本文档

本文档仅适用于：

- 历史迁移取证
- 评估旧 `workspace` 方案是否值得重建
- 为未来重设计提供命名、路由和对象结构参考

不适用于：

- 当前功能开发
- 当前后端验收
- 当前前端联调

## 如果未来要恢复 Workspace 体系

建议基于当前 Wingman 后端重新设计，而不是直接按本文档恢复。至少需要重新确认：

- 认证模型
- 角色与权限边界
- 审计写入策略
- 配置存储模型
- 发布/回滚流程
- 与 Agents / Workflows / Scripts 的关系

## 结论

本文档是历史 API 草案，不是当前 Wingman 有效接口文档。
