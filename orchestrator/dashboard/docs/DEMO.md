# 历史 Demo 说明

本文档来自早期 `workspace` 方案的演示材料，保留目的仅为迁移取证和历史参考。

## 当前状态

以下内容不代表当前 `wingman` 仓库已经实现或默认启用：

- `workspace` 可视化编排
- `workspace-editor/:objectKey`
- 基于配置动态生成完整业务后台
- 面向运营人员的零代码后台搭建链路

## 与当前仓库的关系

当前可联调、可验证的真实能力，请以以下内容为准：

- [Dashboard README](/D:/workspaces/wingman/orchestrator/dashboard/README.md:1)
- [Server README](/D:/workspaces/wingman/orchestrator/server/README.md:1)
- [main.go](/D:/workspaces/wingman/orchestrator/server/main.go:1)

## 当前真实能力范围

当前 `wingman` 仓库内已经落地并可验证的主线能力是：

- 登录与 JWT 鉴权
- Profile 基本资料与密码修改
- Agents 列表与关停
- Workflows 列表、详情、创建、取消
- Scripts 列表、读取、创建、保存、运行、停止
- Audit 日志查询与登录/操作审计

## 使用建议

如果你正在审核仓库或准备联调：

- 不要把本文档中的路由、页面、能力视为当前有效契约
- 不要据此判断当前后端缺失了某个 Workspace API
- 如需补齐历史 Workspace 体系，应另立需求并重新定义前后端契约

## 结论

本文档不是当前 Wingman 产品演示稿，也不是当前代码实现说明。
