# Wingman 项目待办

> 最后更新: 2026-11-02
> 本文件是需求清单；路线图见 ROADMAP.md，过程记录见 CHANGELOG.md 与 git log。

## 未完成

- [ ] 视频教程 EP01 机器侧素材预采（BLOCKED：待用户提供已解锁 Windows 真机或授权人工预采）
- [ ] 视频教程 EP01-EP06 人工成片（BLOCKED：待用户档期）
- [x] 配置开关效果动态图：全仓开关枚举 + off/on 截图与切换 GIF 流水线，docs「配置效果」双语页嵌入，可重跑兼作回归（c301206：apps/gui/scripts/capture-config-gifs.mjs、docs/guide/config-effects.md、docs/zh/guide/config-effects.md；vitest 529/529、docs build 绿、采集重跑成功）

## 已完成

### 架构决策与 Execution

- [x] 四层模型 / 原语边界 / Capability / Execution 四条 ADR 落文档（0c2bbe6）
- [x] Execution v1 + Capability v1：executions 表 + 查询 API + run_script 生命周期 + capabilities 上报（4d4b6c3）
- [x] Execution v2：batch 接口 + workflow 引擎步骤接线（8a9997b）
- [x] Dashboard executions 页 + agents 页 capabilities 展示（aa2202f）
- [x] runtime 侧 capabilities 实际上报（AgentLoopbackTest 钉定）

### C++ Runtime 与脚本层

- [x] Lua (sol2) + Python (pybind11) 双脚本引擎，42 个模块统一 `wingman.*` 命名空间
- [x] file 模块 13 函数 std::filesystem 实现（16 例）
- [x] hotkey 模块轮询式全局热键（9 例）
- [x] notify tray 事件链（9 例）
- [x] task pause/resume 协作式暂停（47 例）
- [x] 事件系统 listener/listeners/clear(type)（50 例）
- [x] ScriptManager 状态机死锁与数据竞争修复（4 例回归钉）
- [x] 加密资源打包闭环：PBKDF2 + AES-256-GCM + PACK_HEADER v2（53 例）
- [x] X11 显示瞬态拒绝根治（10 例）
- [x] 真机验证自动化：XRecord e2e + verify-xrecord-desktop.sh / verify-macos-runtime.sh
- [x] 死代码清理：protobuf 链 / libs/debug / clasp / 双 system_handler / XOR 遗留

### Go 中控

- [x] 工作流引擎：DAG + 持久化 + 取消/超时/重试 + 负载均衡 + 6 模板 + wait/condition/screenshot 步骤
- [x] 工作流取消终态竞争根治（-race -count=50 绿）
- [x] RBAC：模型 + 中间件 + API + Dashboard 页面，8 权限码
- [x] Team/Inbox 三断链修复（11 例，覆盖率 100%）
- [x] Agent 分组标签 + 批量操作 API 与 UI（10 例）
- [x] FrameListener.teamMgr 数据竞争修复（4c7ddd9）
- [x] VaultProvider 抽象 + LocalProvider + 注册表切换 + CI 扫描（ce4b4ee / f501962 / 2c0923a）
- [x] per-agent token 签发/吊销（/api/agent-tokens）
- [x] challenge-response 注册三端（HMAC-SHA256，RFC 4231 向量）
- [x] 自动更新：manifest API + 客户端 semver/流式 sha256
- [x] Swagger 47 端点全注解

### Android Agent

- [x] A1 链路打通：下发/执行/回传，零监听端口（ce4a648）
- [x] A2 能力闭环：手势注入/采集/找色找图/远程截图（c1df705 / c0a67fe）
- [x] A3-P1 token 认证（4c9a8f4）
- [x] A3 可靠性：开机自启 + 崩溃自重启 + 30s 看门狗 + 保活指引（6cecfad，15 例）
- [x] A3 受限设置引导（21 例 + Go 契约 11 例）
- [x] A3-P2 token Keystore 加密迁移 + challenge 全链（5 例）
- [x] API 34 模拟器全链路验证，三处阻断缺陷修复（95570d7，22/22）
- [x] 共用核心下沉 libs/agentcore + libs/androidagent（3ab3b7a）

### 桌面 GUI（Tauri）与 Dashboard

- [x] GUI 六页面 + IPC 重试 + events 轮询 + 托盘 + 主题 + 全局快捷键
- [x] GUI scripts 页文件树/预览/新建/删除（Rust 9 例 + vitest 18 例）
- [x] Dashboard 9 页面 + 路由权限守卫 + WS 事件对接 + 8 语言 i18n
- [x] Dashboard API 前缀契约锁定（26 例）
- [x] Guacamole 远控：票据/网关 + 剪贴板/文件传输/录制 + 审计 + e2e 栈（20fdb30，jest 261→351）
- [x] 浏览器内录像回放（jest 401→420）
- [x] SSH/SFTP 文件浏览器 v2（v1 为 cc7d094）
- [x] flaky 根治：RTL 窗口自适应 + 固定 sleep 清除（420/420）

### 测试与覆盖率

- [x] Go server 覆盖率 stmt/branch/func 100%（597 测试函数）
- [x] C++ Linux 覆盖率 TOTAL 90%（2509 注册：2478 passed + 31 skip）
- [x] 覆盖率收口：agentcore/IPC-RPC/AgentConfig/LuaScriptEngine/lua_marshal/Agent 主类/StandaloneMode/module_helpers/Clipboard/ResourceLoader 等系列补测
- [x] 测试副产品真缺陷修复 11 处
- [x] flake 根治：X11 静默期 / 共享 deadline / 取消写序 / guacd 契约跨平台
- [x] staticcheck P3 装饰项清理：remote_session filters 返回传播 + 测试重复 import/类型断言收口（5cedf8e，见 docs/review/2026-11-01-code-review.md）

### 构建与发布

- [x] CI 矩阵：Windows 全量+覆盖率 / Linux full(+Python) / compat / Go 三平台 / Dashboard / Android / 平台边界守卫
- [x] 一键安装三件套接入 nightly（shellcheck + ps1 AST 实测）
- [x] Release 上传健壮性：重试 + 远端大小校验
- [x] libuuid vendor 预缓存（禁网实证）
- [x] 文档站 VitePress：构建绿 + 死链 0
- [x] 打包分发：Windows zip/NSIS、Linux tar.gz/AppImage/deb、macOS dmg/app、Docker

### 文档

- [x] 快速开始 / Dashboard / Runtime GUI / 脚本开发 / 协议 / 贡献指南
- [x] 文档-代码一致性审计落地：docs/审计-文档一致性.md（2026-10-08 批 273 项；2026-11-01 巡检轮见同文件）
- [x] Android 设计文档 / 保活 / 受限设置 / 手动验证 / 教程分镜
- [x] Dependabot 清零（2026-10-21，8ab069b，docs/dashboard build PASS）
- [x] 2026-10-28~31 文档/UI/测试覆盖审核批次闭环：todo 元数据、decisions 引用、UI 审计报告（对比度/路由计数/P3 系列）、ScreenshotView 测试缺口、todo 审核记录纠偏（代表提交 6857299 / 8782ec4 / 24441c9 / e780354 / f5a63ef / 56f6b05；逐条明细见 docs/review/* 与 git log）

## 注意事项

1. 架构优先：改 runtime/orchestrator 前先读 docs/architecture-decisions.md
2. IPC 边界：GUI 只经 local IPC 控制 runtime；runtime 禁止 HTTP/WS server
3. 远程链路：Dashboard → Go server → runtime（outbound）；Dashboard 不直连 runtime
4. Dashboard 在 orchestrator/dashboard/；仓库根 dashboard/ 是无关的 Croupier 副本，勿改
5. C++ 依赖一律走 vcpkg，版本以 vcpkg.json 与 baseline 为准
6. 测试基线：C++ 2509 注册（2478 passed + 31 skip，TOTAL 90%）；Go 597 测试函数
7. 密钥红线：密钥不进仓库；解锁口令与明文凭据不落日志、不出现在错误消息

## 相关文档

- [ROADMAP.md](./ROADMAP.md) — 项目开发路线图
- [docs/architecture.md](./docs/architecture.md) — 架构设计文档
- [docs/architecture-decisions.md](./docs/architecture-decisions.md) — 架构决策记录（硬约束）
- [docs/API.md](./docs/API.md) — API 文档
- [docs/development-environment.md](./docs/development-environment.md) — VS Code 开发环境

## 审核发现（68572995..HEAD，只审不改，2026-11-03）

覆盖范围：git diff 68572995d4d4bc18522c3d051060a6a500bc88ed..HEAD 全部 118 个文件。代码类已逐行审：apps/gui/src-tauri/src/commands/profiles.rs、apps/gui/src/routes/{dashboard,screen,updates}/+page.svelte、apps/gui/src/app.css、.vscode/tasks.json、orchestrator/server/internal/agent/{client,listener}.go、orchestrator/server/internal/handlers/remote_session.go、orchestrator/server/integration/{harness,workflow}_test.go、orchestrator/server/internal/handlers/capstore_test.go、orchestrator/server/{main_coverage,main_wait}_test.go、orchestrator/dashboard/tests/components.test.tsx；其余为 docs/README/BUILD 文档与截图（抽查命令/路径一致性，未见问题）。

- [x] 审核发现：writeNotifyFrameLocked 只给 body 写设 5s 截止，header 写（writeMsgHeaderLocked）在设截止之前执行且无界；对端窗口为零时 16 字节头写仍可无限阻塞持锁，修复不完整（orchestrator/server/internal/agent/listener.go:958）— 属实并已修复：522f34e 抽出 writeFrameLocked 对 header+body 统一设截止（listener.go:959-976），并补 TestWriteFrameLockedBoundsHeaderWrite（listener_gaps_test.go:195）
- [x] 审核发现：0c7e208 仅绑定 notify 写；sendPong(859-860)/handleRequest(626-627)/SendCommandWithTimeout(349) 仍持 ac.mu 无界写，阻塞期间可让 TeamManager.mu 下 sendNotify 的 ac.mu 获取一并卡到 TCP 重传超时，同类锁持问题未消除（orchestrator/server/internal/agent/listener.go:859）— 属实并已修复：522f34e 将 SendCommandWithTimeout/handleRequest/sendPong 全部改走有界 writeFrameLocked，写失败即 Close（listener.go:344/622/856）
- [x] 审核发现：notify 有界写与写失败主动拆链（writeNotifyFrameLocked / sendNotify 关闭连接）无单元测试覆盖，属行为变更缺回归测试（orchestrator/server/internal/agent/listener.go:953）— 属实并已修复：522f34e 补 TestSendNotifyWriteFailureClosesConn（listener_gaps_test.go:210）与 TestSendCommandWriteFailureBoundsAndCleans（listener_gaps_test.go:223）
- [x] 审核发现：profile_path 由 panic 改返回 Err（1eda7cc）无测试断言不安全 id 现返回 Err 而非 panic；且 get_active_profile 静默吞 Err 无日志（apps/gui/src-tauri/src/commands/profiles.rs:24）— 属实并已修复：522f34e 将 get_active_profile 改为 fail-closed 显式返回 Err（profiles.rs:117-132），并补 profile_path_rejects_unsafe_id / get_active_profile_returns_error_on_unsafe_active_id（profiles.rs 测试模块）
- [x] 审核发现：1eda7cc 修复未覆盖 scripts.rs:264 的同名未校验 profile_path（无 is_safe_filename 检查），unsafe id 处理在库内不一致，残余未校验路径（apps/gui/src-tauri/src/commands/scripts.rs:264）— 属实并已修复：522f34e 删除 scripts.rs 未校验 profile_path，active_profile_script_paths 改经 crate::commands::profiles::profile_path（scripts.rs:235），并同步修复 hotkeys.rs 同型路径（hotkeys.rs:154）

其余已审文件结论：remote_session.go 的 applyRemoteSessionFilters 返回赋值（静态检查 p3 整理）在 GORM clone=0 语义下为行为等价的防御性改动，无功能变化、无注入（参数化占位符 + groupBy/bucket 白名单）；workflow_test.go 断言放宽为允许同时刻（Windows 时钟粒度）合理；components.test.tsx 新用例遵循文件既有 FakeWS 约定；profiles.rs 其余 `?` 传播与错误处理正确；Svelte/app.css 为暗色 hover 对比度样式，无安全影响；.vscode/tasks.json 与 BUILD.md 新命令均指向存在的 build-scripts/ci/configure-windows.ps1（VS 2022 多配置生成器 + --config Release 一致）。
