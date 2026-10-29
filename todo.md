# Wingman 项目待办

> 最后更新: 2026-10-29
> 本文件是需求清单；路线图见 ROADMAP.md，过程记录见 CHANGELOG.md 与 git log。

## 未完成

- [ ] 视频教程 EP01 机器侧素材预采（wingman-agent.exe 已就位 win-worker `D:\wingman-tutorial\agent-bin\`（2026.10.18-nightly，sha256 已验），零注入录屏管线已验证，详见 ROADMAP.md M8.2）**环境级 BLOCKED**：win-worker 已按用户 2026-10-27 令永久停用，EP01 依赖其桌面解锁；手动动作=用户提供已解锁 Windows 真机，或授权人工执行机器侧预采（照 docs/tutorials/video-storyboard.md「机器侧预采」配方，约 2 分钟出片）；此前不虚构进展、不生成代餐素材
- [ ] 视频教程人工成片（EP01-EP06 实拍旁白录制/SRT/发布；分镜与逐字旁白 docs/tutorials/video-storyboard.md 已完备，详见 ROADMAP.md M8.2）**BLOCKED**：需人工出镜/配音录制、字幕与发布

## 已完成

### 架构决策与 Execution

- [x] 四层模型 / 原语边界 / Capability / Execution 四条 ADR 落文档（0c2bbe6）
- [x] Execution v1 + Capability v1 代码落地：executions 表 + 查询 API + run_script 生命周期 + capabilities 上报（4d4b6c3，go test 全绿）
- [x] Execution v2：batch 接口 + workflow 引擎步骤接线（8a9997b）
- [x] Dashboard executions 页 + agents 页 capabilities 展示（aa2202f）
- [x] runtime 侧 capabilities 实际上报（desktop 词汇表清单，AgentLoopbackTest 钉定）

### C++ Runtime 与脚本层

- [x] Lua (sol2) + Python (pybind11) 双脚本引擎，42 个模块统一 `wingman.*` 命名空间
- [x] file 模块 13 函数 std::filesystem 真实现（file_module_test 16 例）
- [x] hotkey 模块轮询式全局热键（9 用例，全量 2085 passed）
- [x] notify tray 四函数事件链（notify_module_test 4 + notify_bridge_test 2 + GUI vitest 3）
- [x] task pause/resume 协作式暂停（TaskModuleTest 47 例，两树 2518 注册全绿）
- [x] 事件系统 listener/listeners/clear(type) 查询与按名清理（EventModuleTest 50 例）
- [x] ScriptManager 状态机死锁环 + stop 数据竞争修复（回归钉 4 例）
- [x] 加密资源打包到加载闭环：PBKDF2 + AES-256-GCM，PACK_HEADER v2（resource_pack_test 43 例 + crypt_test +10，全量 2234 例 0 红）
- [x] X11 显示瞬态拒绝根治：openX11Display 重试 + 回归钉（10/10 绿）
- [x] 真机验证自动化：XRecord 正向 e2e 用例 + verify-xrecord-desktop.sh / verify-macos-runtime.sh
- [x] 死代码清理：protobuf 链 / libs/debug / clasp / 双 system_handler / XOR 加密遗留

### Go 中控

- [x] 工作流引擎：DAG + 环检测 + 持久化 + 取消/超时/重试 + 负载均衡 + 6 内置模板 + wait/condition/screenshot 步骤
- [x] 工作流取消终态竞争根治（-race -count=50 绿，回归 4 例）
- [x] RBAC 权限系统：模型 + 中间件 + API + Dashboard 页面，8 权限码接线
- [x] Team/Inbox 投递/清理/建团三断链修复（team_delivery_test 11 例，Go 覆盖率 100%）
- [x] Agent 分组标签持久化 + 批量操作 API 与 UI（batch_test 10 例 + 集成测试）
- [x] FrameListener.teamMgr 数据竞争修复（4c7ddd9，CI 全绿）
- [x] VaultProvider 抽象 + LocalProvider 默认实现 + 注册表切换 + 接入点扫描入 CI（ce4b4ee / f501962 / 2c0923a，TestVaultProviderSwitchByConfig 留证）
- [x] per-agent token 签发/吊销管理面（models.AgentToken + /api/agent-tokens）
- [x] challenge-response 注册：server + 桌面 + Android 三端（HMAC-SHA256，RFC 4231 向量单测）
- [x] 自动更新：发布/manifest/下载 API + 客户端 manifest 解析/semver/流式 sha256
- [x] Swagger 47 端点全注解（swag init 再生成）

### Android Agent

- [x] A1 链路打通：下发/执行/回传，零监听端口（ce4a648）
- [x] A2 能力闭环：手势注入/MediaProjection 采集/找色找图/远程截图（c1df705 / c0a67fe）
- [x] A3-P1 token 认证（4c9a8f4）
- [x] A3 可靠性：开机自启（默认关）+ 崩溃自重启 + 30s 看门狗 + 机型保活指引（6cecfad，JVM 15 例）
- [x] A3 受限设置引导：脚本预授权 + App 内引导（JVM 21 例 + Go 契约 11 例）
- [x] A3-P2：token Keystore 加密迁移 + challenge 全链（JVM 单测 5 例）
- [x] API 34 模拟器全链路验证，三处阻断级缺陷修复（95570d7，gradle 22/22）
- [x] 共用核心下沉 libs/agentcore + libs/androidagent（3ab3b7a）

### 桌面 GUI（Tauri）与 Dashboard

- [x] GUI 六页面 + IPC 连接重试 + events 轮询 + 托盘 + 主题 + 全局快捷键
- [x] GUI scripts 页文件树/预览/新建/删除（Rust 9 单测 + vitest 18 例）
- [x] Dashboard 9 页面 + 路由权限守卫 + WS 事件全对接 + 8 语言 i18n（约 530 处文案）
- [x] Dashboard API 前缀契约断裂修复 + 26 例回归锁定（apiContracts.test.ts）
- [x] 远控集成 Apache Guacamole：P0 票据/网关 + 剪贴板/文件传输/会话录制 + 会话审计 + e2e 容器栈（20fdb30 起，jest 261→351）
- [x] 浏览器内录像回放（jest 401→420）
- [x] SSH/SFTP 文件浏览器 v2：分页/进度/重试/文件操作审计（v1 为 cc7d094）
- [x] flaky 根治：RTL 窗口随 CPU 超售自适应 + 固定 sleep 清除（load 108 下全量 420/420）

### 测试与覆盖率

- [x] Go server 覆盖率 stmt/branch/func 100%（597 测试函数，vet 全清）
- [x] C++ Linux 覆盖率 TOTAL 90%（2509 注册：2478 passed + 31 环境 skip，余量逐行论证登记）
- [x] 覆盖率收口系列：agentcore 28 例 / IPC-RPC 52 例 / AgentConfig 24 例 / LuaScriptEngine 31 例 / lua_marshal 28 例 / Agent 主类 35 例 / StandaloneMode 12 例 / module_helpers 15 例 / Clipboard 3 例 / ResourceLoader 7 例 / 收官 13 例
- [x] 测试副产品真缺陷修复 11 处：TcpClient 重连 terminate / EventBuffer 死锁 / macro.play SIGFPE / config_handler 悬垂 / 沙箱 preload 崩溃 / EventLogSink 过滤反写 / system.shutdown 自死锁 / 事件 sink 悬空 / standalone stop 泄漏 / saveToFile 漏写 [performance] 节 / 心跳不可打断
- [x] flake 根治系列：X11 WM 测试静默期 / TestRunHTTPEndpoints 共享 deadline / 工作流取消写序 / guacd 契约 macOS-Windows 适配

### 构建与发布

- [x] CI 矩阵：C++ Windows 全量+覆盖率 / Linux full tests(+Python) / compat、Go 三平台、Dashboard 打包、Android APK、平台边界守卫
- [x] 一键安装三件套：install.sh / install.ps1 / agent 构建矩阵接入 nightly（shellcheck 干净 + ps1 AST + 防御分支实测）
- [x] Release 上传健壮性：publish-assets 统一重试 + 远端大小校验
- [x] libuuid vendor 预缓存（sourceforge 单点修复，--no-downloads 禁网实证）
- [x] 文档站 VitePress：构建绿 + 站内死链 0
- [x] 打包分发：Windows zip/NSIS、Linux tar.gz/AppImage/deb、macOS tar.gz/dmg/app、Docker 镜像

### 文档

- [x] 快速开始 / Dashboard / Runtime GUI / 脚本开发指南 / 协议规范 / 贡献指南
- [x] 文档-代码一致性审计：docs/审计-文档一致性.md（2026-10-08 批次 273 项，后续批次逐页对账）
- [x] Android 设计文档 / 保活指引 / 受限设置指引 / 手动验证指引 / 教程分镜脚本
- [x] Dependabot 清零（2026-10-21，#1/#2/#308/#309/#311/#312/#313/#314）：docs pnpm 锁 vite 6.4.4、esbuild 0.25.x；dashboard pnpm 锁 hono 4.13.11；docs:build PASS（62.16s）、dashboard build PASS（22.64s）；commit 8ab069b

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

## 审核发现

- [x] 审核发现：本次已更新 2026-10-28 的环境阻塞状态，但文件头“最后更新”仍停留在 2026-10-20，元数据与正文不一致（todo.md:3）— 已修复：头部日期更新为 2026-10-28
- [x] 审核发现：c7730da..28f492f 未发现问题 — 属实：区间含 e73fbbd（docs: fix todo header date）、b4eb5f3（docs: verify c7730da..HEAD audit finding）、d980b0e（docs: bump todo header date to 2026-10-29）、28f492f（docs: verify e73fbbd..HEAD audit finding），均为 docs 改动，无代码/测试改动，无问题（git log --oneline c7730da..28f492f）
- [x] 审核发现：e73fbbd..28f492f 新增审核发现行断言 c7730da..HEAD 区间仅 e73fbbd，但 c7730da..28f492f 实际包含 e73fbbd（docs: fix todo header date）、b4eb5f3（docs: verify c7730da..HEAD audit finding）、d980b0e（docs: bump todo header date to 2026-10-29）、28f492f（docs: verify e73fbbd..HEAD audit finding）四个提交，git 历史事实与记录不一致（todo.md:116）— 已修正：按实际提交列表更新该行表述，并附 git log --oneline 证据
- [x] 审核发现：新增审核记录仍把 `c7730da..HEAD` 的提交清单写成仅 3 个且遗漏当前提交 28f492f，与 git log 实际 4 个提交不一致（todo.md:116）— 属实：`git log --oneline c7730da..28f492f` 实为 4 个提交；已改用固定区间并把缺失的 28f492f 补入清单
- [x] 审核发现：todo.md:117 仍断言 `c7730da..HEAD`（当时 HEAD=28f492f）仅含 e73fbbd、b4eb5f3、d980b0e 三个提交，遗漏 28f492f，与 git rev-list --count c7730da..28f492f=4 不符；本区间修复仅更正 todo.md:116，未同步修正同源描述 todo.md:117（todo.md:117）— 属实：`git rev-list --count c7730da..28f492f`=4，该行由 28f492f 落地、自身落在区间内；已同步修正 todo.md:117：改用固定区间 c7730da..28f492f 并补入 28f492f（git log --oneline c7730da..28f492f）
