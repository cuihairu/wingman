# API 契约审计报告（历史资料，2026-03-08）

本文档记录的是一次历史迁移阶段的契约审计，不代表当前 `Wingman` 仓库的最新真实状态。

## 使用边界

请不要把本文档作为以下事项的依据：

- 当前后端能力判断
- 当前前端缺陷判断
- 当前 API 缺口结论
- 当前优先级安排

## 为什么不能直接沿用

原始审计范围基于旧前端服务层与旧后端 API 定义，其中包含：

- 历史 `workspace` 假设
- 早期 `messages` 契约
- 已经过时的 Profile 适配结论

这些内容和当前仓库已经发生偏移。

## 当前应以什么为准

请以真实代码为准：

- [Dashboard README](/D:/workspaces/wingman/orchestrator/dashboard/README.md:1)
- [Server README](/D:/workspaces/wingman/orchestrator/server/README.md:1)
- [main.go](/D:/workspaces/wingman/orchestrator/server/main.go:1)

## 结论

本文档仅保留作历史审计记录，不再作为当前 Wingman 研发决策输入。
