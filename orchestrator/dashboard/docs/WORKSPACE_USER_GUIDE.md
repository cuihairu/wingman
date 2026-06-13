# Workspace 用户指南（历史资料）

本文档描述的是早期 `workspace` 编辑/渲染体系，仅保留用于历史参考。

## 重要说明

以下内容不是当前 `wingman` 仓库默认可用功能：

- `Workspace` 页面
- `workspace-editor`
- 基于 `workspace` 配置渲染业务后台
- Workspace/Tab/按钮三级治理链路

## 当前仓库应以什么为准

当前有效说明请优先参考：

- [Dashboard README](/D:/workspaces/wingman/orchestrator/dashboard/README.md:1)
- [Server README](/D:/workspaces/wingman/orchestrator/server/README.md:1)

当前真实后端是：

- Go + Gin
- GORM + SQLite
- HTTP 默认 `127.0.0.1:9527`
- Agent TCP Listener 默认 `127.0.0.1:8888`

## 当前真实可用页面方向

当前仓库主要围绕以下能力演进：

- Agents
- Workflows
- Scripts
- Profile
- Admin / Audit

## 如果你正在联调

请不要根据本文档去要求当前仓库提供以下接口：

- `/api/v1/workspaces/**`
- `/workspace-editor/:objectKey`
- Workspace 配置发布/回滚/导入导出

这些能力在当前仓库中没有形成真实后端闭环。

## 结论

本文档仅作历史资料保留，不构成当前 Wingman 的产品说明、使用手册或接口承诺。
