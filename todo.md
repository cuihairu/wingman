# Wingman 项目待办

## P0
- 收敛架构边界，明确只保留一条主链路：`lib/wingman` -> `apps/runtime` -> `apps/inspector` / `orchestrator`
- 清理 legacy 入口与重复实现，删除或隔离 `WINGMAN_BUILD_SERVER`、`WINGMAN_BUILD_DEBUGGER` 相关旧路径
- 补齐核心运行链路的真实实现，优先完成 `script`、`start/stop/status`、`serve`、`websocket`、`screenshot` 等主功能
- 统一配置、协议、模块导出与命名，避免 `apps/`、`libs/`、`orchestrator/` 之间出现职责重叠

## P1
- 处理大量 `TODO` / stub，优先清理影响主流程的 `ocr`、`ml`、`security`、`storage`、`game_profile`、`http_server`
- 修正文档与代码不一致的问题，重点同步 `README.md`、`docs/architecture.md`、`orchestrator/server/README.md`
- 清理仓库中的历史构建产物、实验目录和重复文档，降低维护噪音
- 为核心模块补齐测试覆盖，建立最小可回归集
- 继续收敛 CI / nightly：Windows 负责测试和 Codecov，Linux / macOS 只做兼容编译；补齐 IPC 测试和依赖边界

## P2
- 统一构建选项语义，减少过多开关导致的组合复杂度
- 梳理并简化跨平台实现，抽象平台差异层
- 规范命令行、服务端、GUI 三者的启动模式和职责
- 补充项目级路线图，明确哪些功能是当前版本必须完成，哪些是后续规划

## 结论
- 这个项目的方向是合理的，但当前更像“重构中的工程”，不是“已经收敛的工程”
- 后续工作的重点不是继续加功能，而是先收敛边界、补齐主链路、清掉 legacy
