# Workspace 开发指南（历史资料）

本文档面向早期 `workspace` 技术方案，保留目的仅为迁移分析和后续重构参考。

## 当前仓库边界

当前 `wingman` 仓库的运行后端并不提供本文档所述完整 `workspace` API，也不存在完整可用的 Workspace 编辑发布链路。

不要把本文档中的以下内容当成当前真实实现：

- `WorkspaceConfig`
- `WorkspaceRenderer`
- `workspaceConfig.ts`
- `workspace-editor`
- `/api/v1/workspaces/**`
- Workspace 发布、回滚、导入、导出

## 当前真实技术基线

当前后端真实实现以 `orchestrator/server` 为准：

- 服务框架：Gin
- 数据访问：GORM
- 数据库：SQLite
- HTTP 默认地址：`127.0.0.1:9527`
- Agent TCP Listener：`127.0.0.1:8888`

关键入口：

- [main.go](/D:/workspaces/wingman/orchestrator/server/main.go:1)
- [config.go](/D:/workspaces/wingman/orchestrator/server/internal/config/config.go:1)

## 当前已落地的真实接口方向

当前仓库内可验证的主接口主要包括：

- `/api/v1/auth/*`
- `/api/v1/profile*`
- `/api/agents*`
- `/api/workflows*`
- `/api/scripts*`
- `/api/audit`

## 如何看待本文档

建议把本文档仅用于以下场景：

- 理解历史前端结构来源
- 审核历史 `workspace` 设计是否值得重建
- 为未来重构提供术语和模块拆分参考

不建议用于：

- 当前联调
- 当前测试用例设计
- 当前后端缺陷判断
- 当前产品能力承诺

## 结论

若要在 Wingman 中重建 Workspace 体系，应基于当前后端重新设计契约，而不是直接按本文档恢复。
