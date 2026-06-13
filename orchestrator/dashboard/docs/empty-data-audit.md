# Empty Data Audit（历史资料）

本文档记录的是一次历史空数据审计，不代表当前 `Wingman` 仓库的真实页面、真实后端目录或当前缺口清单。

## 重要说明

文中提到的大量页面、后端路径和接口来源于历史迁移阶段，例如：

- `internal/api/storage/service.go`
- `internal/api/ops/*`
- `internal/api/function/*`
- `Telemetry` / `Ops` / `Functions/Instances` 等页面链路

这些内容与当前 `orchestrator/server` 的真实实现不一致，不能直接作为当前问题判断依据。

## 当前应以什么为准

请以以下真实入口为准：

- [Dashboard README](/D:/workspaces/wingman/orchestrator/dashboard/README.md:1)
- [Server README](/D:/workspaces/wingman/orchestrator/server/README.md:1)
- [main.go](/D:/workspaces/wingman/orchestrator/server/main.go:1)

## 结论

本文档仅保留作历史审计记录，不再作为当前 Wingman 页面空数据治理输入。
