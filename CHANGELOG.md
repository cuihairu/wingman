# 变更日志 (Changelog)

本项目所有显著变更记录于此文件。

- 格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)；
- 提交信息遵循 [Conventional Commits](https://www.conventionalcommits.org/zh-hans/)（feat / fix / test / docs / chore / refactor / perf / ci）；
- 条目按时间倒序排列，commit 哈希链接至 GitHub；
- `M1`–`M8` 前缀标注对应的 [ROADMAP](ROADMAP.md) 里程碑：M1 MVP / M2 触发器 / M3 宏系统 / M4 远程编排 / M5 GUI / M6 人性化模拟 / M7 调试器 / M8 发布准备。

## [Unreleased]

自 v0.1.1 以来共 210 个提交（feat 48 / fix 88 / docs 28 / test 8 / ci 8 / refactor 2 / chore 12）。

### 里程碑完成度速览（M1–M7）

| 里程碑 | 状态 | 本区间关键进展 |
|--------|------|----------------|
| M1 MVP | ✅ 完成 | 稳定维护；Linux 文件监控/截屏与跨平台修复（[efb326d](https://github.com/cuihairu/wingman/commit/efb326d)） |
| M2 触发器 | ✅ 完成 | Dashboard Monitor 触发器接入真实 API 全链路（[310f42c](https://github.com/cuihairu/wingman/commit/310f42c)）；GUI 触发器可视化配置 |
| M3 宏系统 | ✅ 完成 | 宏录制、主题切换、托盘与远程事件转发（[62304b0](https://github.com/cuihairu/wingman/commit/62304b0)） |
| M4 远程编排 | 🚧 收尾中 | RBAC / 审计日志 / 工作流引擎 / Team 协同 / 端到端测试 / Swagger 全量落地 |
| M5 GUI | 🚧 收尾中 | IPC 连接管理、触发器可视化、屏幕预览、日志实时显示、脚本全生命周期 |
| M6 人性化 | ✅ 完成 | Human 高层 API 全量桥接至脚本层（getConfig/setConfig 至 naturalClick 等 11 项） |
| M7 调试器 | ✅ 直连模式 | EmmyLua VSCode 直连 runtime:9966，server 提供直连指引端点 |

### feat 新增功能

**GUI（M5）**

- 脚本管理增强——完整生命周期控制与实时状态联动：script.pause/resume/restart/unload、五态操作矩阵、批量操作、state_changed 事件联动（[3510da9](https://github.com/cuihairu/wingman/commit/3510da9)）
- 完成 GUI 四项待办：IPC 连接管理 / 触发器可视化 / 屏幕预览 / 日志实时显示（[ad5ae6c](https://github.com/cuihairu/wingman/commit/ad5ae6c)）
- 本地 IPC 连接状态可观测性与断线检测改进（[0feeb5e](https://github.com/cuihairu/wingman/commit/0feeb5e)）
- Profile 管理 UI 与 Tauri 命令、开机自动连接（[e9fd0d2](https://github.com/cuihairu/wingman/commit/e9fd0d2)、[8de31d6](https://github.com/cuihairu/wingman/commit/8de31d6)）
- 本地控制台 UI 打磨（[a89282d](https://github.com/cuihairu/wingman/commit/a89282d)）、截图面板内联重试（[1c383a5](https://github.com/cuihairu/wingman/commit/1c383a5)）

**Runtime 脚本能力（M1/M2/M6）**

- M6 Human 高层 API 桥接：getConfig/setConfig（[e0680e6](https://github.com/cuihairu/wingman/commit/e0680e6)）、setDelayRange/setMoveSpeed/setTypingVariance（[dae20e5](https://github.com/cuihairu/wingman/commit/dae20e5)）、middleClick（[9494236](https://github.com/cuihairu/wingman/commit/9494236)）、moveTo 贝塞尔重载（[36a9db4](https://github.com/cuihairu/wingman/commit/36a9db4)）、naturalClick（[01bf7c5](https://github.com/cuihairu/wingman/commit/01bf7c5)）、moveMouse（[ebdf53c](https://github.com/cuihairu/wingman/commit/ebdf53c)）、randomDelay/naturalType（[409f1e2](https://github.com/cuihairu/wingman/commit/409f1e2)）
- 行为树（BT）节点构造器全套：NodeRegistry 与 sequence/selector/parallel（[fd0a616](https://github.com/cuihairu/wingman/commit/fd0a616)）、wait/inverter/repeat（[01f702e](https://github.com/cuihairu/wingman/commit/01f702e)）、addChild 组合（[9af4240](https://github.com/cuihairu/wingman/commit/9af4240)）、condition 脚本回调（[8e5b409](https://github.com/cuihairu/wingman/commit/8e5b409)）、action 节点 + setRoot + 端到端 tick（[0d40268](https://github.com/cuihairu/wingman/commit/0d40268)）
- UIA Phase 2 事件监听：接口扩展 + macOS AXObserver（[035ff01](https://github.com/cuihairu/wingman/commit/035ff01)）、Windows COM 事件监听（[5383584](https://github.com/cuihairu/wingman/commit/5383584)）、脚本层事件函数 + callableThreadSafe 门控（[808558c](https://github.com/cuihairu/wingman/commit/808558c)）
- UIA OO API：UIElement 绑定与注册表 + 10 个非事件函数（[f20ad83](https://github.com/cuihairu/wingman/commit/f20ad83)）、getChildren/expand/doubleClick/fromWindow/findAll/wait 扩展（[60cee78](https://github.com/cuihairu/wingman/commit/60cee78)）
- 多显示器支持：`screen.listMonitors` RPC 与 IScreen 注入（[06949d0](https://github.com/cuihairu/wingman/commit/06949d0)）、displayId 贯穿 Tauri 与多显示器选择（[f37fe79](https://github.com/cuihairu/wingman/commit/f37fe79)）
- kv 模块暴露 save/load/enableAutoSave/hexists/hkeys（[39c39d7](https://github.com/cuihairu/wingman/commit/39c39d7)）；db/ini 模块（[48f524e](https://github.com/cuihairu/wingman/commit/48f524e)）
- 独立 crypto 模块（AES-256-GCM，[0e18477](https://github.com/cuihairu/wingman/commit/0e18477)）
- Python 引擎 camelCase→snake_case 别名（[16364a2](https://github.com/cuihairu/wingman/commit/16364a2)）
- Linux 支持改进：文件监控 / 截屏 / 跨平台修复（[efb326d](https://github.com/cuihairu/wingman/commit/efb326d)）；Linux/macOS 平台实现补全（[7ee2b04](https://github.com/cuihairu/wingman/commit/7ee2b04)、[0b02e9b](https://github.com/cuihairu/wingman/commit/0b02e9b)）

**远程编排（M4）**

- Dashboard Monitor 触发器接入真实 API 全链路（[310f42c](https://github.com/cuihairu/wingman/commit/310f42c)）
- RBAC 权限系统、runtime 事件系统、工作流引擎、dashboard admin（[ad755f9](https://github.com/cuihairu/wingman/commit/ad755f9)、[84bc03b](https://github.com/cuihairu/wingman/commit/84bc03b)）
- 审计日志贯穿 server 与 dashboard（[9e2d394](https://github.com/cuihairu/wingman/commit/9e2d394)）；私有 IP 标记 LAN（[8eeea66](https://github.com/cuihairu/wingman/commit/8eeea66)）
- transport 模块与远程事件通道（[a0b0cc2](https://github.com/cuihairu/wingman/commit/a0b0cc2)）；Inbox/Team 模块替换 RemoteChannel，实现 TCP 分布式协同（[7b6d553](https://github.com/cuihairu/wingman/commit/7b6d553)）
- IPC 架构落地：runtime 本地控制改用 IPC 替代 WebSocket（[75ea7ea](https://github.com/cuihairu/wingman/commit/75ea7ea)、[cade7f3](https://github.com/cuihairu/wingman/commit/cade7f3)）；macOS/Linux Unix Domain Socket IPC（[ffdc215](https://github.com/cuihairu/wingman/commit/ffdc215)）
- wingman 命名空间统一并补齐缺失的 Lua/Python 模块（[0b8ec24](https://github.com/cuihairu/wingman/commit/0b8ec24)）
- 宏录制、主题切换、系统托盘、远程事件转发（[62304b0](https://github.com/cuihairu/wingman/commit/62304b0)）；工作流自动化与测试工具收尾（[d0a9993](https://github.com/cuihairu/wingman/commit/d0a9993)）

### fix 缺陷修复

**安全加固（P0/P1/P2 审计系列）**

- 修复全部 P0 关键安全漏洞（[963bc8c](https://github.com/cuihairu/wingman/commit/963bc8c)）及 P0 沙箱与脚本生命周期问题（[a79a792](https://github.com/cuihairu/wingman/commit/a79a792)）
- IPC / clipboard / kvstore 加固（[11f7e7c](https://github.com/cuihairu/wingman/commit/11f7e7c)）；超时 / 沙箱 / 校验加固（[6f8288d](https://github.com/cuihairu/wingman/commit/6f8288d)）
- hub.go RLock 下写 map、WS 鉴权（[1794bde](https://github.com/cuihairu/wingman/commit/1794bde)）；listener.go TCP 读竞争、session use-after-free（[df4d352](https://github.com/cuihairu/wingman/commit/df4d352)）
- 命令执行 / 哈希校验 / agent 选择（[27fcb8f](https://github.com/cuihairu/wingman/commit/27fcb8f)）；transport/IPC/线程/SSRF（[77cf8b9](https://github.com/cuihairu/wingman/commit/77cf8b9)）；registry 与请求响应协议（[20fc486](https://github.com/cuihairu/wingman/commit/20fc486)）
- 拒绝空白 JWT 密钥（[45bf4f4](https://github.com/cuihairu/wingman/commit/45bf4f4)）

**依赖与构建**

- 修复 Dependabot 安全告警 3 high / 4 moderate / 2 low（[4e59fb2](https://github.com/cuihairu/wingman/commit/4e59fb2)、[add3aa4](https://github.com/cuihairu/wingman/commit/add3aa4)）及 @babel/core 传递依赖（[798a330](https://github.com/cuihairu/wingman/commit/798a330)）
- CMake 先 find_package(Python3) 再找 pybind11，消除 LNK1104 根因（[e008549](https://github.com/cuihairu/wingman/commit/e008549)）；Python 引擎 MSVC 构建错误（[01c6804](https://github.com/cuihairu/wingman/commit/01c6804)、[6216bd5](https://github.com/cuihairu/wingman/commit/6216bd5)）
- Python 引擎改用 vcpkg python3 统一管理（[3d3e968](https://github.com/cuihairu/wingman/commit/3d3e968)、[b0ac170](https://github.com/cuihairu/wingman/commit/b0ac170)、[bec9a8b](https://github.com/cuihairu/wingman/commit/bec9a8b)）

**运行时与测试稳定性**

- 消除 middleware cleanupInterval 数据竞争（[f2c98d0](https://github.com/cuihairu/wingman/commit/f2c98d0)）
- 修复 Windows 平台差异导致的 3 个测试失败（[5691fbd](https://github.com/cuihairu/wingman/commit/5691fbd)）与跨平台用例（[9af9f72](https://github.com/cuihairu/wingman/commit/9af9f72)）
- macOS Bitmap 加载、事件 once-id、Unix 时间戳、跨平台 getWindows（[2305ab0](https://github.com/cuihairu/wingman/commit/2305ab0)）
- :memory: DB 路径、事务死锁与 INI 测试（[13d1cfb](https://github.com/cuihairu/wingman/commit/13d1cfb)）；共享缓存内存 SQLite 防表丢失（[76597f0](https://github.com/cuihairu/wingman/commit/76597f0)）
- dashboard 与 server 契约对齐（[e08661b](https://github.com/cuihairu/wingman/commit/e08661b)）；agent 管理操作按权限门控（[4af7999](https://github.com/cuihairu/wingman/commit/4af7999)）

### refactor / perf 重构与性能

- 移除 RunMode 互斥，支持能力组合（[5a8edc7](https://github.com/cuihairu/wingman/commit/5a8edc7)）
- dashboard 死前端代码清理（[109feec](https://github.com/cuihairu/wingman/commit/109feec)）

### test 测试

- Go server 覆盖率提升至 ≥98% 并修复并发缺陷（[2b00e79](https://github.com/cuihairu/wingman/commit/2b00e79)）
- Agent→Orchestrator 端到端集成测试（[153c15a](https://github.com/cuihairu/wingman/commit/153c15a)）
- dashboard 覆盖率提升至 95% 并加入门禁（[90682da](https://github.com/cuihairu/wingman/commit/90682da)）
- GUI 引入 vitest 与脚本管理单元测试（[f08adc8](https://github.com/cuihairu/wingman/commit/f08adc8)）
- agent 心跳辅助方法覆盖，包覆盖率回到 99%（[f600040](https://github.com/cuihairu/wingman/commit/f600040)）
- 跨平台平台守卫与 ModuleFunctionsAreCallable 白名单（[7f85c13](https://github.com/cuihairu/wingman/commit/7f85c13)）

### docs 文档

- 补全所有 handler 端点 Swagger 注解（[5c6db74](https://github.com/cuihairu/wingman/commit/5c6db74)）
- API 文档对齐实际 33 个模块（[59e8831](https://github.com/cuihairu/wingman/commit/59e8831)）及 screen/input/vision/ocr/fsm/event/kv/perf/util/config/http 全量对齐（[a32e7cc](https://github.com/cuihairu/wingman/commit/a32e7cc)、[3e112d3](https://github.com/cuihairu/wingman/commit/3e112d3)、[47fb06b](https://github.com/cuihairu/wingman/commit/47fb06b)）
- Dashboard 与 Runtime GUI 使用教程（[8074bfa](https://github.com/cuihairu/wingman/commit/8074bfa)）；架构文档记录 display selection 设计（[201375a](https://github.com/cuihairu/wingman/commit/201375a)）
- 行为树已实现节点与组装 API 文档（[145b556](https://github.com/cuihairu/wingman/commit/145b556)）；ONNX API 对齐实现（[ccc17d0](https://github.com/cuihairu/wingman/commit/ccc17d0)）

### ci / chore 工程与维护

- 新增 deploy-server workflow（自建 runner docker，[a41a150](https://github.com/cuihairu/wingman/commit/a41a150)）与 dashboard 部署（[cd55ff4](https://github.com/cuihairu/wingman/commit/cd55ff4)），镜像构建超时 15m→45m（[dc3a5e0](https://github.com/cuihairu/wingman/commit/dc3a5e0)）
- release workflow 与可复用 build-package（[4c14086](https://github.com/cuihairu/wingman/commit/4c14086)）；nightly 构建补齐 Go server / dashboard / 多平台包（[bef1cf4](https://github.com/cuihairu/wingman/commit/bef1cf4)）
- GitHub Actions 升级至最新版本（[9f8bd1c](https://github.com/cuihairu/wingman/commit/9f8bd1c)）；docs-only 变更跳过 CI（[0aa55b1](https://github.com/cuihairu/wingman/commit/0aa55b1)）
- 可选 Python 引擎构建开关（[77c6287](https://github.com/cuihairu/wingman/commit/77c6287)）；vcpkg 下载重试（[e23970c](https://github.com/cuihairu/wingman/commit/e23970c)）
- 移除 dashboard 遗留 Croupier 代码与孤儿文档（[fc7f7cd](https://github.com/cuihairu/wingman/commit/fc7f7cd)、[183556e](https://github.com/cuihairu/wingman/commit/183556e)）

## [v0.1.1] - 2026-06-06

共 54 个提交，主题：测试覆盖率冲刺至 90% 与遗留栈清理（feat 1 / fix 24 / test 23 / refactor 4 / chore 2）。

### feat 新增功能

- M1 实现代码库遗留 stub 与 TODO（[22be8af](https://github.com/cuihairu/wingman/commit/22be8af)）

### fix 缺陷修复

- trigger KeyPress 捕获 stoi 异常、配置零初始化、强制要求 actions（[8069b52](https://github.com/cuihairu/wingman/commit/8069b52)）
- CI：lua 端口镜像与缓存治理（GitHub 镜像预取 [1b91a1e](https://github.com/cuihairu/wingman/commit/1b91a1e)、overlay 端口 [73762cd](https://github.com/cuihairu/wingman/commit/73762cd)、vcpkg 资产缓存 [2a5bc8d](https://github.com/cuihairu/wingman/commit/2a5bc8d)、失败也保存缓存 [31661e7](https://github.com/cuihairu/wingman/commit/31661e7)）
- OpenCppCoverage 与 IPC/FileWatcher 崩溃用例排除（[c671c0f](https://github.com/cuihairu/wingman/commit/c671c0f)、[db1bb75](https://github.com/cuihairu/wingman/commit/db1bb75)、[348cdbc](https://github.com/cuihairu/wingman/commit/348cdbc)）
- 测试韧性：script_manager 引擎不可用时兜底（[8536f85](https://github.com/cuihairu/wingman/commit/8536f85)、[c905402](https://github.com/cuihairu/wingman/commit/c905402)）

### refactor 重构

- M4 移除遗留远程栈：Drogon HTTP server 与 RemoteControlServer/Client（[3ab6a0f](https://github.com/cuihairu/wingman/commit/3ab6a0f)）、18 个死代码文件（[13f15b4](https://github.com/cuihairu/wingman/commit/13f15b4)）、legacy remote runtime stack（[8c4f142](https://github.com/cuihairu/wingman/commit/8c4f142)）
- 移除 account/qrcode/auth 模块，TOTP 纯函数化（[535fbfe](https://github.com/cuihairu/wingman/commit/535fbfe)）
- 统一 input API 与可选脚本依赖（[c9a8e85](https://github.com/cuihairu/wingman/commit/c9a8e85)）；大规模代码重构与构建系统优化（[81de6bf](https://github.com/cuihairu/wingman/commit/81de6bf)）

### test 测试

- 覆盖率冲刺 90%：新增 49 个测试（[9b03119](https://github.com/cuihairu/wingman/commit/9b03119)）、修复 5 个失败用例（[12aaff7](https://github.com/cuihairu/wingman/commit/12aaff7)）、补齐最后 5 行达到阈值（[089c136](https://github.com/cuihairu/wingman/commit/089c136)）
- 行为树节点 getName 覆盖（[690aadb](https://github.com/cuihairu/wingman/commit/690aadb)）；smart_trigger 全条件/动作类型（[8fa8899](https://github.com/cuihairu/wingman/commit/8fa8899)）；TCP channel 集成（[01b7f88](https://github.com/cuihairu/wingman/commit/01b7f88)）；game_profile INI 解析与导入导出（[56434d2](https://github.com/cuihairu/wingman/commit/56434d2)）
- ImageAnalyzer / PatternMatcher（[29fa9e2](https://github.com/cuihairu/wingman/commit/29fa9e2)）；Bitmap 越界与移动语义（[2da9f07](https://github.com/cuihairu/wingman/commit/2da9f07)）；script_manager / tcp_channel / trigger_engine（[4e67739](https://github.com/cuihairu/wingman/commit/4e67739)）；FSM / notify / task（[9eb9172](https://github.com/cuihairu/wingman/commit/9eb9172)、[265d786](https://github.com/cuihairu/wingman/commit/265d786)）；TOTP 边界与 EventHub（[4291d95](https://github.com/cuihairu/wingman/commit/4291d95)）

## [v0.1.0] - 2026-06-01

自 nightly 以来共 261 个提交，主题：三流分离架构、平台抽象层与 Linux/macOS 跨平台化（feat 22 / fix 140 / docs 23 / ci 22 / test 18 / security 8 / refactor 5 / chore 11 / build 1）。

### feat 新增功能

**架构与平台（M1/M4）**

- 实现三流分离架构和事件驱动重构（[da14c6f](https://github.com/cuihairu/wingman/commit/da14c6f)）
- 平台抽象层：Windows/macOS 实现（[a81d3b8](https://github.com/cuihairu/wingman/commit/a81d3b8)）、Screen 支持与 Mock（[ad19282](https://github.com/cuihairu/wingman/commit/ad19282)）、UI Automation 抽象（[c2a60b7](https://github.com/cuihairu/wingman/commit/c2a60b7)）
- Linux 完整支持（X11/XTest/XRandR，[2decda7](https://github.com/cuihairu/wingman/commit/2decda7)）；macOS 窗口管理补全（[7e73121](https://github.com/cuihairu/wingman/commit/7e73121)）
- 跨平台化：Trigger 类（[74bc066](https://github.com/cuihairu/wingman/commit/74bc066)）、Performance（[fa97c32](https://github.com/cuihairu/wingman/commit/fa97c32)）、Lua（[40e8179](https://github.com/cuihairu/wingman/commit/40e8179)）

**脚本与功能模块**

- 脚本层多语言抽象：Lua (sol2) + Python (pybind11)（[38691e6](https://github.com/cuihairu/wingman/commit/38691e6)）
- P0 模块：event callbacks / FSM / Task / Notify（[4efa390](https://github.com/cuihairu/wingman/commit/4efa390)）
- M5 实现 GUI 界面（[bd6b163](https://github.com/cuihairu/wingman/commit/bd6b163)）
- IPC 通信抽象层（[100b957](https://github.com/cuihairu/wingman/commit/100b957)）；Clipboard 与 FileWatcher（[6de1374](https://github.com/cuihairu/wingman/commit/6de1374)）
- M4 server 安全增强与配置管理（[4054070](https://github.com/cuihairu/wingman/commit/4054070)）；脚本管理器集成与窗口枚举（[fc01864](https://github.com/cuihairu/wingman/commit/fc01864)）
- WebSocket 控制器与远程截图（[86c9ccb](https://github.com/cuihairu/wingman/commit/86c9ccb)）
- Lua callback/callable 支持（[5ce5ca3](https://github.com/cuihairu/wingman/commit/5ce5ca3)）

### fix 缺陷修复

- 本区间以稳定性修复为主（140 个），覆盖平台抽象层、跨平台编译与 CI 链路（详见 [compare 视图](https://github.com/cuihairu/wingman/compare/nightly...v0.1.0)）

### ci 持续集成

- Linux/macOS 加入 nightly 构建（[9e71d3d](https://github.com/cuihairu/wingman/commit/9e71d3d)）；启用 Linux C++ 覆盖率并上传 codecov（[fe2ba04](https://github.com/cuihairu/wingman/commit/fe2ba04)）

### docs 文档

- 文档代码块统一行号（[6bad707](https://github.com/cuihairu/wingman/commit/6bad707)）

## [nightly] - 2026-05-13

初始开发冲刺（2026-05-04 → 2026-05-13，392 个提交）：M1–M7 框架全量落地（feat 64 / fix 159 / ci 53 / docs 41 / refactor 23 / test 16 / chore 12 / build 7 / perf 3 / revert 1）。

### feat 新增功能（按里程碑）

**M1 MVP**

- 实现核心 C++ 模块 Phase 1 MVP：screen/input/window/pixel（[452b4a0](https://github.com/cuihairu/wingman/commit/452b4a0)）
- 完成 Phase 7 和 Milestone 1（[b3eb619](https://github.com/cuihairu/wingman/commit/b3eb619)）
- HTTP 客户端、JSON 封装、KV 存储和组队编排引擎（[222ab53](https://github.com/cuihairu/wingman/commit/222ab53)）
- Vision / OCR / ML / SmartTrigger / BehaviorTree 竞品对标功能（[5d51f2c](https://github.com/cuihairu/wingman/commit/5d51f2c)）

**M2 触发器 / M3 宏系统**

- M3 完成 Milestone 3 宏系统（[9ae0a0f](https://github.com/cuihairu/wingman/commit/9ae0a0f)）
- 定时截图上报（[3691dda](https://github.com/cuihairu/wingman/commit/3691dda)）

**M4 远程编排**

- 远程控制 TCP 服务器框架（[b85d9ae](https://github.com/cuihairu/wingman/commit/b85d9ae)）与 Milestone 4 完善（[246940e](https://github.com/cuihairu/wingman/commit/246940e)）
- Go HTTP Server + C++ Agent TCP 通信架构（[cf04948](https://github.com/cuihairu/wingman/commit/cf04948)）
- protobuf 替换 TCP 通信中的 JSON（[f2c6724](https://github.com/cuihairu/wingman/commit/f2c6724)）；TCP Server/Client 增强与工作流编排引擎（[ca22dd0](https://github.com/cuihairu/wingman/commit/ca22dd0)）
- 节点状态汇报与心跳机制（[85ba8b7](https://github.com/cuihairu/wingman/commit/85ba8b7)）

**M5 GUI**

- M5 实现原生 GUI 界面（[3ed78ff](https://github.com/cuihairu/wingman/commit/3ed78ff)）
- 集成并简化 Ant Design Pro Dashboard（[612d692](https://github.com/cuihairu/wingman/commit/612d692)）、HTTP Server 与 Dashboard 完整功能（[96b2159](https://github.com/cuihairu/wingman/commit/96b2159)）
- 系统托盘：图标模块（[38a1f9b](https://github.com/cuihairu/wingman/commit/38a1f9b)）、可配置菜单（[8ded2d0](https://github.com/cuihairu/wingman/commit/8ded2d0)）、状态指示（[c51f633](https://github.com/cuihairu/wingman/commit/c51f633)）

**M6 人性化模拟**

- M6 实现人性化模拟系统（贝塞尔鼠标轨迹等，[ede6d85](https://github.com/cuihairu/wingman/commit/ede6d85)）

**M7 调试器**

- M7 实现 VS Code 调试器基础功能（[7677735](https://github.com/cuihairu/wingman/commit/7677735)）；WebSocket 调试器事件推送（[c12c1d2](https://github.com/cuihairu/wingman/commit/c12c1d2)）
- VS Code 扩展：完整 DAP、补全、悬停、诊断（[516d01f](https://github.com/cuihairu/wingman/commit/516d01f)）

**M8 发布准备**

- 发布准备文件：InnoSetup / 便携版 / 自签名脚本（[bc2c256](https://github.com/cuihairu/wingman/commit/bc2c256)、[eea4c57](https://github.com/cuihairu/wingman/commit/eea4c57)）
- 版本系统与 Nightly 构建工作流（[38b1158](https://github.com/cuihairu/wingman/commit/38b1158)）

**其他**

- 四层存储系统 Session/Local/Team/Server（[26153fe](https://github.com/cuihairu/wingman/commit/26153fe)）；Lua HTTP 路由系统（[326670c](https://github.com/cuihairu/wingman/commit/326670c)）
- Node.js/TypeScript 客户端（[ab2c57a](https://github.com/cuihairu/wingman/commit/ab2c57a)）、Python 客户端（[bb64e3b](https://github.com/cuihairu/wingman/commit/bb64e3b)）
- UIA 事件监听器与控件类型扩展（[2daed3e](https://github.com/cuihairu/wingman/commit/2daed3e)、[1603ba8](https://github.com/cuihairu/wingman/commit/1603ba8)）
- 游戏配置管理（[a121fc3](https://github.com/cuihairu/wingman/commit/a121fc3)）、验证码 TOTP/Email 模块（[12f3528](https://github.com/cuihairu/wingman/commit/12f3528)）、安全模块（[b9c4d26](https://github.com/cuihairu/wingman/commit/b9c4d26)）
- MMORPG 自动化等示例脚本库（[1db8afb](https://github.com/cuihairu/wingman/commit/1db8afb)）

### perf 性能

- 性能优化模块（[649c9ac](https://github.com/cuihairu/wingman/commit/649c9ac)）

---

## 链接

[Unreleased]: https://github.com/cuihairu/wingman/compare/v0.1.1...HEAD
[v0.1.1]: https://github.com/cuihairu/wingman/compare/v0.1.0...v0.1.1
[v0.1.0]: https://github.com/cuihairu/wingman/compare/nightly...v0.1.0
[nightly]: https://github.com/cuihairu/wingman/releases/tag/nightly
