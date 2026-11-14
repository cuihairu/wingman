# Wingman 项目待办

> 最后更新: 2026-11-13
> 本文件是需求清单；路线图见 ROADMAP.md，过程记录见 CHANGELOG.md 与 git log。

## 未完成

- [x] 视频教程 EP01 机器侧素材预采（2026-11-09 imac 192.168.5.117 完成：装齐 ffmpeg 9.0.2 + ffprobe 与公开 nightly wingman-agent.exe（sha256 2beeca9f… 与官方资产一致，不 clone 不构建），agent 自跑脚本零注入预采成功——产物 D:\wingman-tutorial\capture\ep01-script-demo-take5.mp4：34.0s / 1920×1080 / 60fps CFR / 2040 帧，抽帧中文渲染正常非锁屏非空桌面，脚本=hello.lua+game_profile.lua（均 grep 验证零 input 调用）；实测修正版配方已同步 docs/zh/tutorials/video-storyboard.md（ddagrab+dup_frames=0+fps_mode cfr+UTF-8 控制台+ES_DISPLAY_REQUIRED 保活；color_detect.lua 因内含 input.click 真注入与零注入规则矛盾已替换为 game_profile.lua）；openclaw 因虚拟 GPU 竖屏 1440×2560 且 ChangeDisplaySettings 不生效判不可用，imac 为实际采集机）
- [ ] 视频教程 EP01-EP06 人工成片（BLOCKED：待用户档期；2026-11-04 复核：仍缺档期证据，保留 BLOCKED；复核 2026-11-06 仍缺档期证据，保留 BLOCKED；复核 2026-11-06 仍缺用户确认可用档期）
- [x] 配置开关效果动态图：全仓开关枚举 + off/on 截图与切换 GIF 流水线，docs「配置效果」双语页嵌入，可重跑兼作回归（c301206：apps/gui/scripts/capture-config-gifs.mjs、docs/guide/config-effects.md、docs/zh/guide/config-effects.md；vitest 529/529、docs build 绿、采集重跑成功）
- [ ] 文档站语言自动切换脚本挂错页（P2，G2 审计 2026-11-14）：`docs/zh/index.md:47-64` 的 `<script setup>` 有 navigator.language 探测，但编译进 `/zh/` 页且 guard 只放行 `/wingman/` 根路径，主用例「中文浏览器首访 `/`」加载不到该脚本——实现挂而不生效；另 `EN_PATH=/wingman/index.en.html` 在 Pages 站点下不存在，2026-10-18 注释「本页（/wingman/ 根）内容是中文」的前提已随 root=en 布局翻转失效。修法=脚本移到 root 页（`docs/index.md`）并改跳板目标（`/zh/` 已存在）。是否动手、是否随其它仓统一推广待拍板；取证见 docs/zh/审计-文档一致性.md「2026-11-14 G2 轮」段
- [ ] nightly 原生安装包从未真正落到 release 资产（P1，代码侧，G2 复核 2026-11-15）：`build-agent.yml:233`（deb/rpm）与 `:292`（msi）的产物被 `Upload artifacts`（`:305` glob `wingman-agent-<ver>-<os>-<arch>.*`）带出，但 `nightly.yml` `cleanup-assets`（`:151-189`）按 `^wingman-(agent-)?(\d{4}\.\d{2}\.\d{2})-nightly-([0-9a-f]+)-` 分组保留**同一 sha 的整组**，而 `build` 与 `build-agent` 是并行 job（`.github/workflows/nightly.yml:61-82`），各自 `resolve-version` 取 `${{ github.sha }}` 时 sha 不同 → 两组不同 sha → 保留 build 组时把 build-agent 整组（含 deb/rpm/msi）删光。实测 release `nightly` 现存 10 个资产全是整包与 agent 单体 zip/tar.gz，无 deb/rpm/msi（`8e42c83` 前后同）。后果：`README.md:106` / `README.zh.md:105` 与 `nightly.yml:107-108` 声明的 `.deb/.rpm/.msi` 在站点上取不到，本仓 docs 已在 platforms 矩阵补口径。修法=cleanup-assets 分组键去掉 sha（只按日期）或让两 job 共享同一 sha 解析；属代码侧，待拍板

## 已完成

### MCP stdio server（2026-11-13）

- [x] wingman-agent 新增 `mcp` 子命令：MCP（Model Context Protocol）stdio 传输，newline-delimited JSON-RPC 2.0（不用 Content-Length 帧），AI 客户端（Claude Code/Cursor/Hermes）本地拉起直调 wingman UI 自动化能力（351839f）。协议层 + 7 tool 在 `lib/wingman/src/mcp/`（mcp_server.cpp 协议分发：版本协商回显受支持版本否则回退 2025-11-25/capabilities.tools 显式空对象——裸 `{}` 序列化成 null 会被客户端读成未声明能力，冒烟实测抓过；mcp_tools.cpp：uia_tree/uia_find/uia_click/uia_action/screen_shot/input_click/script_run，ToolContext 三指针注入缝缺省走生产单例），apps/agent 只接线（commands/mcp_command.cpp + cli.cpp 分支 + main.cpp mcp 模式 sinks().clear() 后挂 stderr_color_sink_mt——stdout 只准 JSON-RPC 帧逐行 flush，日志全走 stderr）。失败语义：tool 内失败 = result.isError（协议层 crash catch 兜 "tool crashed" 不炸进程），未知 tool = -32602，缺依赖明确降级——Linux 无 UIA 后端报 not supported 不假装成功、uia_click 失败绝不静默转坐标点击、screen_shot 无 OpenCV 构建 tool 级 isError（vision 构建返回 MCP image content block + text 元数据）、script_run 沙箱 + 复用 ScriptManager 协作超时（运行时 getAvailableLanguages 探测 lua——WINGMAN_HAS_LUA 宏 lib 编译期不可见）。测试：core_tests mcp_server_test.cpp 41 例（协议矩阵/帧环回 CRLF+空行零输出+-32700 id:null/工具面注入 MockInput+手写 MockUIAManager/role 名全枚举往返/crash 注入经 McpServer ToolContext 构造缝/script 腿真 Lua 引擎含超时腿）；agent_tests mcp_e2e_test.cpp 3 例（in-process 环回跨平台 + POSIX fork/execvp 真子进程腿断言 stdout 逐行合法 JSON——日志漏进 stdout 即炸 + mcp 拒收参数 exit1/stdout 空/stderr 报错）。验证：core_tests 全量 2385 例 2346 过 33 skip（与基线 2344 差值=新增 41 全过；6 败全为 X11PlatformTest 环境既存——单进程隔离跑仍败、用例进程内 mcp 代码零参与无副作用、:99 上 Openbox 状态被并行会话驱动过，非本轮回归）+ agent_tests 237/237 + 平台边界守卫过 + 手动冒烟（initialize/tools_list/uia isError/input_click isError 三帧 stdout 纯 JSON）+ docs/mcp.md（client 配置/FAQ）+ README 双语入口行。覆盖率口径注记：gcovr 工具口径漂移（pipx venv/gcov 行归属变化）在无 mcp 代码上实测 90.27%（18020 行）vs 基线轮记录 92.1%（15933 行），差 1.8pp/行数 +2087 非本轮代码所致；同口径下 mcp 生产三文件 90.3%（401/444 行）与存量 90.3% 持平未拉低，分项 mcp_command 100%/mcp_server 96%/mcp_tools 87%（未盖大头为 gcov 对 roleFromString map 表/roleToString 全枚举 switch 兜底的行归属瑕疵与结构性不可达分支）。

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
- [x] codecov 口径 C++ 覆盖率收口（2026-11-08/09 四批，ed02492/bdd6ebf/f522d3c/dc550cf）：86.61% → 93.40%（12536/13422，Windows CI OpenCppCoverage 为 codecov 唯一数据源）。批次内容：transport/inbox/systemwatch 三文件跨平台移植（asio acceptor 选端口 + ping.exe 子进程工厂，补上 Windows 口径长期缺失的模块主体）、uia 查找家族/元素方法、vision aiSetup 校验矩阵、fastFindImage 金字塔路径、db 绑定越界/约束违反/事务回滚/stoll 兜底、script_manager 停止事件回调/运行中卸载/INI 缺文件。**98% 目标经核算不可达**：剩余 886 miss 中约 800 行为 Windows CI 不可达面（recorder.cpp X11/XRecord 71、crypt.cpp OpenSSL 调用错误分支 78（需故障注入）、notify_module 白名单死代码 44（setAllowedHosts/setWebhooksEnabled 无调用方）、X11 剪贴板/filewatcher 平台代码、私有防御分支（db prepare/bindParams/fetchResults 句柄守卫、isPathAllowed 前缀拒绝仅构造函数喂词法内路径）、线程创建失败/引擎初始化失败等强制不可行路径）+ 次级可回收余量约 60 行；任务纪律禁改生产行为，不为凑数字硬凑

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

## 审核发现（625504ca..HEAD，只审不改，2026-11-03）

覆盖范围：git diff 625504ca..HEAD 全部 5 个 commit（522f34e fix / 8a95106 docs / c301206 feat / 347f4de docs / 8ae79dd build）共 29 个文件。代码类已逐行审：apps/gui/src-tauri/src/commands/{profiles,scripts}.rs、apps/gui/src-tauri/src/hotkeys.rs、orchestrator/server/internal/agent/{client,listener}.go、orchestrator/server/internal/agent/listener_gaps_test.go、orchestrator/server/integration/{guacd_e2e_script_test,guacd_e2e_test}.go、orchestrator/server/integration/testdata/{guacd-e2e-compose.yml,Dockerfile.sshd,Dockerfile.vnc,Dockerfile.xrdp}、apps/gui/scripts/capture-config-gifs.mjs、docs/guide/config-effects.md、docs/zh/guide/config-effects.md；其余为 package.json/pnpm-lock/.vitepress 配置与 png/gif 产物（抽查路径、命名、双语互链与导航项一致，未见问题）。容器镜像 digest 本环境无外网，未能向 registry 实测核对（注释称实测取得）。

- [x] 审核发现：TestWriteFrameLockedBoundsHeaderWrite 的模拟 conn 把 SetWriteDeadline 实现为空操作、Write 无视截止时间固定 sleep 后返回 os.ErrDeadlineExceeded，故即便 writeFrameLocked 完全不设写截止（旧缺陷：header 写无界持锁）测试同样通过，无法防护该回归；测试只验证「Write 返回错误即返回」，未验证截止机制本身（orchestrator/server/internal/agent/listener_gaps_test.go:195-207）— 属实并已修复：shortBlockingConn 改为按 SetWriteDeadline 实际截止睡眠（deadlineSet 标记），并断言 writeFrameLocked 确实设了写截止、耗时上界收紧到 300ms（listener_gaps_test.go:159-171,196-208；go test -race ./internal/agent 绿）
- [x] 审核发现：blockingConn 类型（注释「模拟对端窗口为零」）定义后从未被任何测试实例化，仅 shortBlockingConn 被使用，属死代码（orchestrator/server/internal/agent/listener_gaps_test.go:155-171）— 属实并已修复：删除未使用的 blockingConn 类型与其实例方法（listener_gaps_test.go:155-171 移除；go test -race ./internal/agent 绿）
- [x] 审核发现：profile_path_accepts_safe_id 与 get_active_profile_returns_error_on_unsafe_active_id 并发改写同一进程全局环境变量 XDG_DATA_HOME（std::env::set_var，无 Mutex/serial_test 串行化），cargo test 默认并行线程下两测试交错会使后者读取到另一测试的数据目录、找不到其写入的 active_profile.txt 而返回 Ok(null)，assert!(result.is_err()) 偶发失败（apps/gui/src-tauri/src/commands/profiles.rs:280,289）— 已修复：EnvGuard 持有全局 MutexGuard 串行化环境变量改写（profiles.rs:248-269），cargo test 5×profiles::tests 绿 + cargo test 17/17 绿
- [x] 审核发现：profiles.rs 新增两个环境相关测试仅对 Linux 有效——local_data_dir 在 Windows（LOCALAPPDATA/APPDATA）与 macOS（~/Library/Application Support）分支均不读 XDG_DATA_HOME，故 Windows/macOS 下该环境变量被忽略：get_active_profile_returns_error_on_unsafe_active_id 会读真实应用目录（可能返回 Ok(null) 而非 Err），profile_path_accepts_safe_id 的 ends_with("profiles/profile_123.json") 在 Windows 反斜杠路径下必然失败；GUI 交叉构建 Windows/macOS/Linux（build-package.yml），测试平台假设与构建矩阵不符（apps/gui/src-tauri/src/commands/profiles.rs:282,291-296）— 已修复：测试改用平台对应环境变量（DATA_DIR_ENV：Windows LOCALAPPDATA / macOS HOME / Linux XDG_DATA_HOME），断言改用 local_data_dir() 构造期望路径，摆脱反斜杠分隔假设（profiles.rs:254-260,301-313）；cargo test 17/17 绿 + cargo test --no-run --target x86_64-pc-windows-gnu 编译绿
- [x] 审核发现：自建 fixture 用可变本地 tag wingman-e2e-*:latest 且 verify-guacd-e2e.sh 的 up 用 compose up -d（不带 --build），Dockerfile 改动后复跑不会重建、继续用旧镜像，e2e 会静默测到过期 fixture（改端口/凭据时最易踩坑），与「镜像变更是显式可评审改动」的目标相悖（scripts/verify-guacd-e2e.sh:154；deployments/guacd/README.md「首次 compose up 会触发仓内构建，属预期」）— 已修复：do_up 改 compose up -d --build，自建端点每次从当前 Dockerfile 重建（verify-guacd-e2e.sh:153-157）；bash -n + shellcheck 绿 + go test ./integration/ 全绿
- [x] 审核发现：todo.md「配置开关效果动态图…」已勾选，但交付的 capture-config-gifs.mjs SWITCHES 仅 3 项；设置页同页另有第 4 个布尔开关「Challenge 注册鉴权（A3-P2）」（真 checkbox）未采集，交付范围与代码注释「设置页布尔开关一一对应」不符（apps/gui/scripts/capture-config-gifs.mjs:33-36；apps/gui/src/routes/settings/+page.svelte:351）— 属实并已修复：SWITCHES 增加 challenge-auth 第 4 项，重跑采集生成 challenge-auth off/on/gif（capture-config-gifs.mjs:33-38；docs/guide/config-effects.md + docs/zh/guide/config-effects.md 新增 Challenge Auth 段落；4/4 开关 off/on/gif 采集绿）
- [x] 审核发现：findChrome 在 ~/.cache/ms-playwright 目录不存在时 fs.readdirSync 直接抛 ENOENT；且仅探测 chrome-linux64/chrome，Windows/macOS 下默认路径无效（apps/gui/scripts/capture-config-gifs.mjs:43-52）— 属实并已修复：findChrome 先检查 root 存在再 readdir，按平台探测 Linux chrome-linux64/chrome、macOS Chromium.app、Windows chrome-win64/chrome.exe，并支持 PLAYWRIGHT_BROWSERS_PATH 回退（capture-config-gifs.mjs:44-62；Linux 下 node --check + findChrome 实测返回 chromium-1243/chrome-linux64/chrome 绿）
- [x] 审核发现（本轮复核 625504ca..HEAD 新增）：Dockerfile.vnc 的 /root/.vnc/config 显式 `localhost=no`，sshd(Port 2222)/xrdp(3389) 在 network_mode: host 下亦默认绑 0.0.0.0，guacd 同宿主经 127.0.0.1 连目标，改绑回环即可消除暴露（orchestrator/server/integration/testdata/Dockerfile.vnc:28）— 属实并已修复：VNC config 改 localhost=yes，sshd 增加 ListenAddress 127.0.0.1，xrdp.ini port 改 tcp://127.0.0.1:3389（Dockerfile.vnc:26-28；Dockerfile.sshd:23-25；Dockerfile.xrdp:29-31；三镜像 docker build 绿 + 容器内 cat config / xrdp -n 运行日志确认「listening to port 3389 on 127.0.0.1」）
- [x] 审核发现（本轮复核 625504ca..HEAD 新增）：capture-config-gifs.mjs 的 SWITCHES[].setting 键从未被脚本引用（采集循环只用到 key/label），属死数据易与真实设置键漂移（apps/gui/scripts/capture-config-gifs.mjs:34）— 属实并已修复：删除所有 SWITCHES[].setting 键，数组仅保留 key/label（capture-config-gifs.mjs:33-38；node --check 绿 + 4/4 开关采集重跑绿）

## 漏洞扫描遗留（2026-11-03 全舰队扫描，源 scratch/vuln-scan-*.md）

- [x] VULN-WINGMAN-001 [高危·GHSA-mrhx-c7xc-m3w5] zod 3.x→4.6.6【跨大版本需改代码】（orchestrator/dashboard，终扫残留 1）— 已修复：pnpm-workspace.yaml 增加 overrides zod: 4.6.6 强制所有 zod 解析为 4.6.6（pnpm-workspace.yaml:15；pnpm why zod 确认单一版本 4.6.6；pnpm audit zod 高危不再命中，vuln 10→9）；pnpm test 449/449 绿 + pnpm build 绿 + pnpm tsc --noEmit 绿

## 审核发现（8ae79dd..f9fac03，只审不改，2026-11-04）

覆盖范围：git diff 8ae79dd3..f9fac034 全部 7 个 commit（31fb24a fix / 61ff52d docs / 3e292e6 fix / 000095d fix / 44d16c1 fix / 80f0842 fix / f9fac03 docs）共 21 个文件。代码类已逐行审：apps/gui/scripts/capture-config-gifs.mjs、apps/gui/src-tauri/src/commands/profiles.rs、orchestrator/server/internal/agent/listener_gaps_test.go、orchestrator/server/integration/testdata/{Dockerfile.sshd,Dockerfile.vnc,Dockerfile.xrdp}、scripts/verify-guacd-e2e.sh、orchestrator/server/go.mod、orchestrator/dashboard/pnpm-workspace.yaml + pnpm-lock.yaml；其余为 Cargo.lock/go.sum 依赖更新与 png/gif/doc 产物（抽查命名与双语段落一致，未见问题）。

- [x] 审核发现：overrides 的 `zod: 4.6.6` 无版本范围限定，把 zod 4 强加给声明 zod ^3 的消费方（babel-plugin-react-compiler@0.0.0-experimental 声明 ^3.22.4、zod-validation-error@2.1.0 声明/peer ^3.18/^3.20、hono peer ^3.23.8），zod 3→4 跨大版本行为差异（如 ZodError.errors→issues、错误映射签名）在这些消费方未被任何定向测试覆盖，仅靠 pnpm test/build/tsc 全绿作间接兜底，属隐性运行时兼容风险（orchestrator/dashboard/pnpm-workspace.yaml:15；pnpm-lock.yaml:13746-13747,20287-20289,9106）— 属实并已修复：新增 override `zod-validation-error@2.1.0: 4.0.2` 把 zod-3-only 消费方升级到 zod-4 兼容版（4.0.2 支持 `^3.25 || ^4.0`），hono 4.13.11 实际无 zod peer（已核验 package.json peer=none）；实测 zod 4.6.6 + zve 4.0.2 `fromZodError/fromError` 均返回正常错误信息（此前 zve 2.1.0 下 `fromZodError` 报 `Cannot read properties of undefined (reading 'slice')`）；lock 只剩单一 zod 4.6.6，pnpm peers check 不再报 zod unmet peer；pnpm test 449/449 绿 + pnpm tsc --noEmit 绿 + pnpm audit zod 高危不再命中
- [x] 审核发现：EnvGuard 持 MutexGuard 跨越整个测试体，若某环境变量测试在 `lock().unwrap()` 之后、`drop(guard)` 之前的 assert/expect 失败，panic 展开时会毒化 env_mutex，后续所有调用 `.lock().unwrap()` 的环境变量测试连锁 panic（PoisonError），掩盖原始失败原因、产生误导性失败；应以 `unwrap_or_else(|e| e.into_inner())` 容忍中毒（apps/gui/src-tauri/src/commands/profiles.rs:269）— 属实并已修复：EnvGuard::set 改 `env_mutex().lock().unwrap_or_else(|e| e.into_inner())` 容忍 PoisonError（profiles.rs:269）；cargo test 17/17 绿（含 profile_path_accepts_safe_id / get_active_profile_returns_error_on_unsafe_active_id）
- [x] 审核发现：write deadline 测试仅覆盖「header 写阻塞」场景（shortBlockingConn.Write 恒返回 os.ErrDeadlineExceeded，header 必然失败），未模拟「header 写成功、body 写阻塞」路径——writeFrameLocked 同时约束 header+body 两段写，若回归只把 deadline 设/清在 header 段（body 段无界），现测试无法检出（orchestrator/server/internal/agent/listener_gaps_test.go:197-212；listener.go:963-976）— 属实并已修复：新增 shortBodyBlockingConn（首写成功、次写阻塞）与 TestWriteFrameLockedBoundsBodyWrite（listener_gaps_test.go:245-283），断言 header+body 均写且 body 受 100ms 截止约束（elapsed ≤300ms）；go test ./internal/agent -run 'TestWriteFrameLocked|TestSendNotifyWriteFailure|TestSendCommandWriteFailure' -v 5/5 绿，go test ./... 全包绿
- [x] 审核发现：go.mod 新增 `toolchain go1.27.3` 并抬 `go 1.26.10`，而 CI 各 Go 步骤固定 `go-version: '1.26.x'`；GOTOOLCHAIN=auto（默认）下 CI 与本地低版本 go 会在构建期静默联网下载并使用 1.27.3 工具链，实际编译器版本与工作流声明不一致、引入非受控的构建期下载（离线/受限网络环境直接构建失败）（orchestrator/server/go.mod:5；.github/workflows/ci.yml:591、build-package.yml:112,277,411）— 属实并已修复：删除 `toolchain go1.27.3` 指令（go.mod:5），保留 `go 1.26.10`（gin-contrib/cors 与 gin-contrib/sse 经 `go list -m` 核验确需 1.26.10，本地 go1.27.1 下可构建）；go mod tidy 后 go build ./... 绿，go test ./... 全包绿，无 GOTOOLCHAIN 静默下载
- [x] 审核发现：capture-config-gifs.mjs 的 findChrome 在非 darwin/linux/win32 平台上 `executables[process.platform]` 为 undefined，`for (const relativePath of undefined)` 直接抛 TypeError（原实现对未找到返回 null 走优雅 exit 3）；当前枚举仅覆盖三平台，未支持的平台会崩溃而非提示未找到 chromium（apps/gui/scripts/capture-config-gifs.mjs:50-54,62）— 属实并已修复：`for (const relativePath of executables[process.platform] ?? [])` 用空数组兜底（capture-config-gifs.mjs:62），未支持平台不再崩溃而是返回 null 走优雅 exit 3；node --check 绿
- [x] 审核发现：本轮 diff 未见问题（f9fac034..HEAD）— 属实并已复核：该范围即 aab50e2（上述 5 项修复：zod override / EnvGuard 容忍 PoisonError / body 写截止测试 / 删 toolchain / findChrome 空数组兜底）+ ebe60a3 qs 依赖 bump + 285f1c2 merge，无新增问题；复核证据：go build ./... 绿 + go test ./... 全包绿、cargo test 17/17 绿、node --check 绿、pnpm test 449/449 绿 + pnpm tsc --noEmit 绿 + pnpm build 绿
- [x] 审核发现：本轮 diff 未见问题（aab50e20..HEAD）— 属实并已复核：该范围即 ebe60a3 （qs 依赖 bump 6.15.2→6.17.1，锁文件单一 6.17.1，pnpm why 确认单版本）+ 285f1c2 merge + 0a4a441（todo 记录），无新增问题；复核证据：pnpm test 449/449 绿 + pnpm tsc --noEmit 绿 + pnpm build 绿
- [x] 审核发现：代码块强制自动换行后，带行号的示例在窄屏或长行折行时行号仍按固定行高排列，导致折行之后的行号与代码错位（docs/.vitepress/theme/custom.css:58）—— 属实并已修复：换行规则收窄，`.line-numbers-mode` 块回退 `white-space: pre`（行号列与代码行高一一对应，长行横向滚动），无行号块保留 `pre-wrap`（custom.css:60-69）；验证：vitepress build 绿，dist 中 hotkey.html 等 `:line-numbers` 页样式命中回退规则（2026-11-05）
- [x] 审核发现：本轮 diff 未见问题（28c3aa9ff1..d931cf510d）— 属实并已复核：该范围即 d931cf5（docs/.vitepress/theme/custom.css 行号块回退 white-space:pre + todo 记录），无新增问题；复核证据：vitepress build 绿，dist style.BqPCXreW.css 命中 `.line-numbers-mode pre{white-space:pre;word-break:normal}`，zh/api/hotkey.html 等 `:line-numbers` 页使用 line-numbers-mode（2026-11-05）
- [x] 审核发现：本轮 diff 未见问题（d931cf51..HEAD）— 属实并已复核：该范围即 cda3aa7（仅改 todo.md 记录一行，无代码改动），无新增问题；复核证据：git show --stat cda3aa7 仅 todo.md +1/-0，git diff --name-only d931cf51..HEAD 仅 todo.md（2026-11-05）；审核通过，无需改动
- [x] 审核发现：todo.md 头部「最后更新: 2026-11-05」未随本轮追加的「复核 2026-11-06」记录同步更新，元数据日期落后于正文内容日期（todo.md:3，追加记录见 todo.md:8-9）—— 属实并已修复：将头部更新为「最后更新: 2026-11-06」与正文 2026-11-06 复核记录对齐（todo.md:3）；验证：git diff todo.md 仅头部日期与勾选状态变化，无代码改动（2026-11-06）
- [x] 审核发现：本轮 diff 未见问题（eb57c49774c5a41a1071ec24bd99085313c9b09d..HEAD）— 属实并已复核：该范围即 4ddfdf9（仅改 todo.md 头部日期与追加一条复核记录，无代码改动），无新增问题；复核证据：git diff --stat eb57c497..HEAD 仅 todo.md +2/-1，git diff --name-only eb57c497..HEAD 仅 todo.md，git log --oneline eb57c497..HEAD 仅 4ddfdf9（2026-11-06，HEAD 4ddfdf9149d3ee698188c17c3c4e8b6aa59dbfa1）；审核通过，无需代码改动
- [x] 审核发现：本轮 diff 未见问题（4ddfdf91..HEAD）— 属实并已复核：该范围仅 db22e15（todo.md +1 行），`git diff --stat 4ddfdf91..HEAD` 为 todo.md +1/-0、`git diff --name-only` 仅 todo.md、`git log --oneline` 仅 db22e15；审核通过，无需代码改动
- [x] 审核发现：本轮 diff 未见问题（db22e153..HEAD）— 属实并已复核：该范围即 23cb0d9（仅改 todo.md 头部日期更新与追加一条复核记录，`git show --stat 23cb0d9` 为 todo.md +2/-1、`git diff --name-only db22e153..HEAD` 仅 todo.md、`git log --oneline db22e153..HEAD` 仅 23cb0d9）；文档纯记录变更，无代码改动；审核通过，无需代码改动

## 源码分层分析 2026-11-07

- 应用表现层：连接本地 Runtime 与远端编排面，维护用户界面状态（代表：`orchestrator/dashboard/src/app.tsx:36`）。
- 边缘网关层：集中 HTTP/权限/静态路由与 WebSocket 状态扇出（代表：`orchestrator/server/internal/handlers/routes.go:38`）。
- 领域编排层：拥有工作流定义/执行/模板与 Agent 注册表等持久领域状态（代表：`orchestrator/server/internal/workflow/engine.go:39`）。
- Agent 链路层：维持外连注册、心跳、命令/通知与安全重连（代表：`libs/agentcore/src/remote_client.cpp:79`）。
- Runtime 应用层：管理 Agent 进程生命周期、命令门面与本地/远程桥接（代表：`apps/agent/src/agent.cpp:191`）。
- 脚本引擎层：加载用户脚本，通过可插拔引擎和模块描述符暴露能力（代表：`lib/wingman/src/script/modules/module_registry.cpp:101`）。
- 设备能力层：隔离屏幕/输入/窗口/剪贴板/视觉/Android 宿主的 OS 差异（代表：`lib/wingman/src/platform/screen_backend.cpp:47`）。

- [x] LAY-001 [P1] 拆分 `lib/wingman/tests/orchestration_module_test.cpp`（2190 行）——编排脚本模块的注册、转发、收敛与错误路径共用一个测试文件，适合按场景拆分（`lib/wingman/tests/orchestration_module_test.cpp:16`）。已按场景拆为 5 文件 + 共享头（06528be：orchestration_module_test.cpp 收窄至提交校验+历史账本、orchestration_dependency_test.cpp 依赖图调度/传播矩阵/取消、orchestration_condition_test.cpp 条件分支、orchestration_parallel_test.cpp 独立并发/并发控制、orchestration_flow_test.cpp 查询面/流程状态/产出聚合、orchestration_test_helpers.hpp 共享 findFn/spinUntil/EventLog/taskStatus/callable/ScopedFlag）；纯重组无逻辑改动，OrchestrationModuleTest.* 40/40 绿、core_tests 全量 2333 例 2237 过 0 败。
- [x] LAY-002 [P1] 拆分 `assets/vscode-wingman-lua/wingman.d.lua`（2075 行）——Lua 类型提示把所有 API 命名空间放在一个描述文件，适合按模块拆成导入集合（`assets/vscode-wingman-lua/wingman.d.lua:12`）。已拆（eda65ae）：该文件是生成物，拆分做在生成器 scripts/gen_lua_hints.py——输出改为 42 个按模块导入集合 `defs/<模块名>.d.lua`（class+local+函数+return，自洽可独立解析）+ `wingman.d.lua` 命名空间聚合面（---@class wingman + 每模块一条 ---@field 组合，不跨文件 require）；含 stale 清理（模块下线后残留 defs 文件自动删除，修过 Path.stem 只去单后缀的坑）。验证：API 面与旧单文件逐一等价（415 函数行数一致、---@param/@return/function/["保留字"] 面逐字 diff IDENTICAL）；luac -p 43/43 过（Lua 5.4.8）；重跑幂等（二次生成 0 diff）；audit_pyi_drift.py 42 模块 42 pyi 0 门禁失败；CI api-typing-audit 门禁改 diff 整目录，仓库级 .vscode settings 的 Lua.workspace.library 指目录递归索引 defs/ 无需改，README/PR 模板/development-environment/script-development/debugging/architecture-decisions 双语文档同步两出口径。
- [x] LAY-003 [P1] 拆分 `lib/wingman/tests/script_function_test.cpp`（2064 行）——脚本函数断言覆盖多模块和异常路径，单文件难以维护（`lib/wingman/tests/script_function_test.cpp:14`）。已按模块簇拆为 6 文件 + 共享头（245d9a6：script_function_test.cpp 收窄至 registry 面（perf/ml/debugger/http/util/node/模块在位校验 42 例）、script_data_test.cpp json/kv/config/security 37 例、script_action_test.cpp system/human/input/process/gameprofile 45 例、script_ui_test.cpp screen/window/vision/uia 36 例、script_bt_test.cpp bt 节点构造桥接 25 例、script_module_flow_test.cpp orchestration/team/smarttrigger/verification 21 例、script_function_test_helpers.hpp 共享 findFunction/hasModule/result_is_string_or_null/HumanConfigGuard）；纯重组无逻辑改动，六文件 TEST 计数合计 206=原文件逐项一致，core_tests 全量 2237 过 0 败（/tmp/core_full.log [PASSED] 2237 tests）。
- [x] LAY-004 [P2] 拆分 `lib/wingman/tests/platform_x11_test.cpp`（1726 行）——X11 截屏、窗口、剪贴板和文件监听混在同一套平台测试中（`lib/wingman/tests/platform_x11_test.cpp:16`）。已按子系统拆为 6 文件 + 共享头（52b4eeb：platform_x11_screen_test.cpp 显示器元数据/截图/顶层装配/X11Screen 全扫描/窗口捕获/查询回退+RESOURCE_MANAGER DPI+无显示降级 20 例、platform_x11_input_test.cpp XTest 注入与全方法 6 例、platform_x11_clipboard_test.cpp 剪贴板 3 例、platform_x11_window_test.cpp 窗口管理 8 例、platform_x11_wm_test.cpp 自起 Xvfb+openbox 真实 WM 集成 3 例、platform_x11_inotify_test.cpp 文件监听 2 例、platform_x11_test_common.hpp 共享 include/工厂前向声明/TestX11Window/X11PlatformTest fixture，ResourceMgrGuard/BrokenDisplayGuard 归 screen 文件）；纯重组无逻辑改动，42 例逐文件合计与原文件一致，core_tests 全量 2333 例 2237 过 0 败（无 display 时 X11PlatformTest 用例按设计 GTEST_SKIP）；首轮全量出现 X11WmIntegrationTest.ActivateForegroundAndEnumerate 一次超时（pollUntil 3s 满载抖动，与 LAY-001 MinimizeAndShowRoundtrip 同族），单跑 WM 套件 3/3 绿、全量重跑 0 败，证实为负载型时序抖动非拆分回归；docs/DEVELOPMENT.md 双语与 recorder_x11_e2e_test.cpp 顶部注释的文件名引用同步。
- [x] LAY-005 [P2] 拆分 `lib/wingman/tests/script_manager_test.cpp`（1647 行）——脚本加载、执行、暂停、恢复、配置与热重载断言集中（`lib/wingman/tests/script_manager_test.cpp:16`）。已按关注点拆为 5 文件 + 共享头（a65d79d：script_manager_value_test.cpp 值类型默认值/枚举/拷贝语义 7 例、script_manager_query_test.cpp 空状态构造/配置/查询/不存在操作/语言探测 28 例、script_manager_load_test.cpp ScriptManagerFileTest 加载卸载/INI/JSON/CFG 配置文件面 41 例、script_manager_config_test.cpp saveConfig/triggerEvent/checkReload/env 回退/暂停恢复生命周期 47 例、script_manager_exec_test.cpp 真实 Lua runScript/callFunction 七型返回矩阵/文件变更热重载 19 例、script_manager_test_common.hpp 共享 include+ScriptManagerFileTest fixture）；纯重组无逻辑改动，142 例逐文件合计与原文件一致，core_tests 全量 2333 例 2237 过 0 败。
- [x] LAY-006 [P1] 拆分 `orchestrator/dashboard/src/pages/Agents/index.tsx`（1565 行）——页面同时承载列表、筛选、WebSocket 订阅、批量操作和多组弹窗状态（`orchestrator/dashboard/src/pages/Agents/index.tsx:298`）。已按弹窗簇拆为 4 组件 + 共享文件（513f94f：index.tsx 收窄至 1027 行（统计卡/列表/详情/批量栏/ws 订阅/批量运行/停止/触发器弹窗），DesktopConnectModal.tsx 282 行（连接表单+保险箱探测/内联解锁+RemoteDesktopModal 建连，表单态组件内部持有、父级只传初值）、RecordingsModal.tsx 205 行（列表/下载/删除/浏览器内回放，open 变更即拉取）、BatchResultModal.tsx 74 行（汇总+逐台明细纯受控展示）、shared.tsx 60 行（PlatformTag/CapabilityTags/extractErrorMessage）；纯重组无行为改动，props 下行/事件上行不共享可变状态）。门禁全绿：tsc --noEmit 0 错、eslint 0 问题、prettier clean、jest 34 套 449/449 过、max build 成功。
- [x] LAY-007 [P1] 拆分 `lib/wingman/src/script/modules/db_module.cpp`（1521 行）——SQLite 连接、语句封装、安全路径解析与 db.* 模块注册在同一翻译单元（`lib/wingman/src/script/modules/db_module.cpp:63`）。已按类边界拆（f713fec）：DbConnection/DbTable/QueryBuilder 三个类实现（含 SQLite 路径校验/标识符白名单/语句 RAII）移入新文件 db_connection.cpp（705 行）——公开类声明本就在 include/wingman/script/modules/db_connection.hpp，拆分纯为实现搬家零接口面变化，#ifdef WINGMAN_HAS_SQLITE 结构原样随迁；db_module.cpp 收窄至 824 行（安全路径解析 getScriptDataDir/getDatabasePath + 连接注册表 + db.*/table.*/query.* 模块函数 + ModuleDescriptor 注册面，mod.functions.push_back 扫描面不受影响），死 include（regex/sstream/unordered_set/chrono/algorithm 随实现迁走）同步清出。验证：Db 套件 87/87 绿、core_tests 全量 2333 例 2237 过 0 败。
- [x] LAY-008 [P2] 拆分 `orchestrator/dashboard/src/components/RemoteDesktop/index.test.tsx`（1491 行）——票据、连接、工具条、剪贴板和审计交互测试集中（`orchestrator/dashboard/src/components/RemoteDesktop/index.test.tsx:32`）。已按被测边界拆为 5 测试文件 + 2 共享模块（bed3949：index.test.tsx 收窄至 117 行（base64 工具/协议能力/公共出口契约/createWingmanTicketClient 7 例）、useGuacamoleSession.test.tsx 704 行（连接生命周期/剪贴板文件流编排/卸载竞态 33 例）、RemoteDesktopToolbar.test.tsx 261 行（监看隐藏入口/VNC 无文件块/上传复位/SSH 文件浏览 11 例）、RemoteErrorNotice.test.tsx 97 行（错误四类判定+降级渲染 9 例）、RemoteDesktopPanel.test.tsx 310 行（主面板状态/下载/复制/文件浏览器 10 例）、index.testMocks.ts（guacamole-common-js mock 工厂单一来源，named+default 双出口形状与拆分前逐字一致）、index.testHelpers.ts（fakeTicketClient/lastClient/fakeInputStream/BASE_PARAMS/waitForUI）；纯重组无逻辑改动，70 例逐文件合计与原文件一致。门禁全绿：tsc 0 错、eslint 0 问题、prettier clean、jest 全量 38 套 449/449 过、max build 成功。
- [x] LAY-009 [P1] 拆分 `orchestrator/dashboard/src/pages/Profile/index.tsx`（1382 行）——登录态资料、游戏范围、活动记录和偏好表单聚合在一个路由页面（`orchestrator/dashboard/src/pages/Profile/index.tsx:92`）。已按弹窗边界拆出 3 组件 + 1 共享类型（89cf92e）：PasswordChangeModal.tsx 100 行（改密表单/提交态内置，open 即 resetFields）、AvatarEditModal.tsx 139 行（头像表单/上传态内置，保存后经 onSaved 回调刷新）、PermissionApplyModal.tsx 128 行（申请文案生成/复制兜底/反馈渠道提交内置）、shared.ts（PermissionApplyItem 类型单一来源，目录项与兜底模板同形）；index.tsx 收窄至 1074 行——父级只持三个 open 开关 + selectedApplyPermission + profile 值，删去 passwordForm/applyForm/avatarForm 三个表单实例与 passwordLoading/avatarUploading/applySubmitting 提交态、8 个内联处理函数与 190 行弹窗 JSX（改密入口改内联 setPasswordModalVisible(true)，头像预填改由 initialValues+avatar prop 承担）；props-down/events-up 纯重组无逻辑改动。门禁全绿：tsc 0 错、eslint 0 问题、prettier clean、jest 全量 38 套 449/449 过、max build 成功。
- [x] LAY-010 [P1] 拆分 `orchestrator/server/internal/workflow/engine.go`（1330 行）——状态迁移、DAG 调度、步骤执行和持久化记录共享一个 Go 文件（`orchestrator/server/internal/workflow/engine.go:39`）。已按职责边界同包拆为 4 新文件（add2ddd）：selection.go 127 行（agent 在执行计数 reserve/release/inflight + selectAgent 负载均衡与 requires 过滤 + missingRequirement/noCandidateError 诊断）、dag.go 272 行（execute 调度环 + findReadySteps 依赖就绪 + detectCycle 环检测 + validateSteps 步骤校验）、step_exec.go 475 行（executeStep 分发/重试/退避 + runScriptOnce 脚本下发 + wait/condition/screenshot 三类无脚本步骤及其辅助）、condition.go 192 行（evaluateCondition + 操作符归一化/数值比较/包含判定/toBool/toFloat 求值辅助）；engine.go 收窄至 304 行——Engine/Execution 类型与状态访问器（setStepState/StepSnapshot/stepStatusIn/anyStepStatus）+ NewEngine + Submit/Cancel/Get*/List 生命周期 + failStep/broadcastStepProgress/workflowToJSON 广播序列化；同包纯搬家零签名变化，死 include（errors/maps/reflect/strconv/strings 随实现迁走）同步清出。验证：go build ./... + go vet 全过、workflow 包 27.9s ok、orchestrator/server 全量 16 包 ok（含 integration 69.7s、handlers 58.8s）。
- [x] LAY-011 [P2] 拆分 `lib/wingman/tests/event_module_test.cpp`（1355 行）——事件订阅、缓冲、过滤和并发路径断言过长（`lib/wingman/tests/event_module_test.cpp:16`）。已按文件自带分节注释拆为 4 测试文件 + 1 共享头（800ec2e）：event_module_test.cpp 保留基础节 315 行（模块函数面/on/once/emit/off/clear/message 基础与参数校验 17 例）、event_module_test_callback.cpp 240 行（回调触发/自动移除/payload 传递 8 例）、event_module_test_json.cpp 238 行（toJson/fromJson 序列化覆盖 10 例）、event_module_test_exception.cpp 98 行（回调异常捕获 3 例）、event_module_test_listener.cpp 452 行（listener/listeners 查询与按事件名清理 12 例）、event_module_test_common.hpp 21 行（include + createEventModule 前置声明 + using 声明逐字共享）；纯重组无断言改动，50 例逐文件合计与原文件一致（17+8+10+3+12=50）。验证：core_tests 构建绿、EventModuleTest.* 单跑 50/50 过、全量 2333 例 2237 过 0 败 96 skip（与基线一致）。
- [x] LAY-012 [P2] 拆分 `libs/transport/tests/stream_transport_test.cpp`（1345 行）——TCP 帧、粘包、会话与错误路径共用文件（`libs/transport/tests/stream_transport_test.cpp:14`）。已按分节注释拆为 6 测试文件 + 1 共享头（4979049）：stream_type_test.cpp 51 行（StreamType 命名/默认参数 5 例）、stream_channel_test.cpp 461 行（StreamChannel 生命周期/连接错误路径/FdExhaustionGuard 描述符耗尽/StreamChannelEnv 回环矩阵/StreamChannelPair 31 例）、stream_manager_test.cpp 90 行（StreamManager 创建/替换/按类型查询/全清 9 例）、stream_session_test.cpp 287 行（Message 序列化/SessionEnv 双向收发/RawPeerSessionEnv 超大帧·截断体·写失败·发送队列上限 11 例）、channel_manager_test.cpp 94 行（Channel 请求挂起/ChannelManager 生命周期 7 例）、transport_client_server_test.cpp 258 行（TransportEnv TcpClient/TcpServer 回环矩阵 16 例 + TransportFactoryTest 1 例）、stream_transport_test_common.hpp 108 行（include/findFreePort（去匿名命名空间改 inline 防未用告警）/FdExhaustionGuard/DataSink 逐字共享），原文件删除；纯重组无断言改动，80 例逐文件合计与原文件一致（5+31+9+11+7+17=80）。验证：transport_tests 构建绿、全量 118 例 118 过 0 败（含其余既有套件）。
- [x] LAY-013 [P1] 拆分 `orchestrator/server/internal/agent/listener.go`（1224 行）——帧编解码、连接注册、鉴权、命令转发与 proxy 中继耦合（`orchestrator/server/internal/agent/listener.go:232`）。已按职责边界同包拆为 4 新文件（a441f38）：agent_conn.go 191 行（SendCommand/SendCommandWithTimeout 命令转发 + SendNotify/sendNotify 代理协议面 + writeNotifyFrameLocked/writeFrameLocked/writeMsgHeaderLocked/readMsgHeader 帧编解码）、conn_handlers.go 317 行（readLoop 单读者分发 + dispatchResponse/handleMessage/handleNotify/handleProxyData/handleProxyClose/handleProxyError/handleRequest proxy 中继处理 + handleHeartbeat/handleEvent/sendPong）、listener_auth.go 175 行（handleRegister/completeRegistration/rejectRegister 注册收口 + challengeTimeout/verifyChallengeHMAC/tokenMAC/sha256Hex/hmacMatchesEnv HMAC 挑战鉴权）、inbox_team_handlers.go 226 行（inbox 注册/心跳/ack/report + team join/leave/vote/status/broadcast 十个处理器）；listener.go 收窄至 342 行——Broadcaster/AgentRegistrar/AgentConnection 接口与 FrameListener/agentConn 类型 + 鉴权开关配置（SetAgentTokens/SetTokenStore/SetTeamManager 等）+ Start/Stop/acceptLoop + getConnByAgent/hasOtherConnForAgent/deliverInboxMessage 连接查询推送；同包纯搬家零签名变化，死 include（crypto/hmac-sha256/base64/hex/json/io/maps/strings 随实现迁走）同步清出。验证：go build ./... + go vet 全过、agent 包 7.6s ok、orchestrator/server 全量 16 包 ok 无 FAIL。
- [x] LAY-014 [P2] 拆分 `libs/agentcore/tests/agentcore_test.cpp`（1222 行）——远程客户端注册、心跳、重连和出站缓冲测试集中（`libs/agentcore/tests/agentcore_test.cpp:16`）。已按文件自带分节注释拆为 4 测试文件 + 1 共享头（59ed303）：event_buffer_test.cpp 190 行（容量驱逐/sink 转发/drain pull 模型 11 例）、remote_client_test.cpp 465 行（loopback TcpServer 真连：注册元数据/ack 成败与畸形 JSON/命令分发回路/心跳链路统计/断线 outbox 排队与重连冲刷 19 例）、remote_client_auth_test.cpp 117 行（challenge-response A3-P2 §6.1：hmac_sha256 已知向量/挑战模式注册/挑战应答 7 例）、proxy_tunnel_test.cpp 320 行（proxy.* 裸 TCP 拨号分支：双向回环/target EOF/拨号失败/迟到 new 的 proxy.error/坏 target 与坏 base64 5 例）、agentcore_test_common.hpp 136 行（include 前言 + testHexEncode/findFreePort 去匿名命名空间改 inline 防未用告警 + RemoteClientTest fixture 跨 TU 共享——auth 文件复用同 fixture），原文件删除；纯重组无断言改动，42 例逐文件合计与原文件一致（11+19+7+5=42）。验证：agentcore_tests 构建绿、全量 42 例 42 过 0 败。
- [x] LAY-015 [P2] 拆分 `apps/agent/tests/agent_loopback_test.cpp`（1217 行）——Runtime 初始化、IPC、RPC、脚本与关闭链路做全回路断言（`apps/agent/tests/agent_loopback_test.cpp:16`）。已按文件自带分节注释拆为 2 测试文件 + 1 共享头（83c8fd1）：agent_lifecycle_test.cpp 281 行（生命周期/applyRemoteConfig 全平台无 socket 腿：初始化模式与组件装配/默认配置文件落盘/Start 契约/applyRemoteConfig 持久化与运行中延迟重连矩阵/Shutdown 收口 17 例，无平台宏包裹）、agent_loopback_test.cpp 792 行（Agent 装配回环 POSIX 腿，#ifndef _WIN32 结构原样保留：本地 IPC version/状态提供者/config get-set 校验矩阵与持久化/注册能力词汇/事件转发/脚本出错与 link 状态/ListWindows/remote-only 命令腿/trigger 与 screenshot 复用 RPC 分发器/system.shutdown 19 例）、agent_loopback_test_common.hpp 145 行（include 前言 + fs 别名 + processId/tempPath/findFreePort/writeScriptFile/makeApiConfig 五个 helper 去匿名命名空间改 inline 防未用告警）；纯重组无断言改动，36 例逐文件合计与原文件一致（17+19=36）。验证：agent_tests 构建绿、两套件单跑 36/36 过、二进制全量 234 例 234 过 0 败。
- [x] LAY-016 [P2] 拆分 `lib/wingman/tests/fsm_module_test.cpp`（1078 行）——状态机模块的正向、超时与异常测试过长（`lib/wingman/tests/fsm_module_test.cpp:16`）。已按文件自带分节注释拆为 2 测试文件 + 1 共享头（80b3f0d）：fsm_module_test.cpp 保留基础节 350 行（模块函数面/create/current/state/transition/dispatch/reset 正向与参数校验 21 例）、fsm_guard_test.cpp 271 行（Guard/Action/Callback 节：guard 放行与拒绝/action 回调/from-state 不符/payload+event 链/多转移链/状态回调 9 例）、fsm_json_test.cpp 427 行（toJson/fromJson 覆盖节 5 例 + 9 个小覆盖节：Cleanup/Guard 返回非 bool/空 event/无回调 state/多机共存/新机 Reset/guard 数组 payload/onEnter 数组 payload/嵌套对象 payload 14 例）、fsm_module_test_common.hpp 35 行（include + createFsmModule/cleanupFsmModule 前置声明 + findFsmFunction 去匿名命名空间改 inline + FsmModuleTest fixture（TearDown cleanup）跨 TU 共享）；纯重组无断言改动，44 例逐文件合计与原文件一致（21+9+14=44）。验证：core_tests 构建绿、FsmModuleTest.* 单跑 44/44 过、全量 2333 例 2237 过 0 败 96 skip（与基线一致）。
- [x] LAY-017 [P2] 拆分 `apps/agent/tests/rpc_ipc_test.cpp`（1037 行）——本地 IPC 信封与 RPC 方法契约测试集中（`apps/agent/tests/rpc_ipc_test.cpp:16`）。已按文件自带分节注释（RPC 域）拆为 5 测试文件 + 1 共享头（e8a71f8）：rpc_script_test.cpp 197 行（script.* 真实 StandaloneMode+真实 Lua：list/start/pause/resume/restart/unload 生命周期与错误腿 15 例）、rpc_event_config_test.cpp 106 行（events.drain 限额与空缓冲计数器 2 例 + config.getRemote/setRemote 权限与 apply 传播 5 例）、rpc_macro_test.cpp 161 行（macro.* save/load/play 往返与 speed/repeat 钳制、零除与负速度不挂死 13 例）、rpc_system_test.cpp 138 行（system.* providers 注入与批量 noop 分支 3 例 + EventLogSink 过滤/截断/过滤下限 3 例）、rpc_loopback_test.cpp 344 行（LocalIpcServer Unix 回环：生命周期幂等/version/status 信封/未知方法/畸形 payload/缺 method/断连事件与重连/带客停机 11 例 + TestIpcClient 专享客户端随迁）、rpc_ipc_test_common.hpp 104 行（include 前言 + processId/tempPath/dispatchCall/clearEventBuffer/drainEventBuffer/writeTwoTickMacro 六 helper 去匿名命名空间改 inline 防未用告警），原文件删除；纯重组无断言改动，52 例逐文件合计与原文件一致（15+7+13+6+11=52）。验证：agent_tests 构建绿、全量 234 例 234 过 0 败（与拆分前同数）。
- [x] LAY-018 [P2] 拆分 `lib/wingman/tests/task_module_test.cpp`（1026 行）——任务编排、并发取消和失败恢复断言集中（`lib/wingman/tests/task_module_test.cpp:16`）。已按文件自带分节注释拆为 3 测试文件 + 1 共享头（af44010）：task_module_test.cpp 保留基础节 281 行（submit/cancel/status/result/error/wait/retry 正向与参数校验、嵌套与扁平 retry options、WaitTimeout 不死锁 20 例）、task_retry_test.cpp 188 行（Retry and Error Path 节：抛异常触发重试/未知异常/sync 任务/metadata/完成后 wait-cancel-error-retry 腿 9 例）、task_coverage_test.cpp 270 行（toJson 覆盖节 5 例 + 9 个小覆盖节：async 非线程安全拒绝/执行中取消/成功后对象结果/零超时/失败 error 消息/默认超时 wait/无 options submit/带 options retry 13 例）、task_pause_test.cpp 237 行（pause/resume 节：运行中暂停挂起完成/resume 前非法目标/取消暂停任务丢弃挂起结果/暂停冻结超时时钟/重试间暂停延迟下次尝试 5 例）、task_module_test_common.hpp 57 行（include + createTaskModule 前置声明 + findTaskFunction/spinUntil/waitForTaskStatus 三 helper 去匿名命名空间改 inline 防未用告警）；纯重组无断言改动，47 例逐文件合计与原文件一致（20+9+13+5=47）。验证：core_tests 构建绿、TaskModuleTest.* 单跑 47/47 过、全量 2333 例 2237 过 0 败 96 skip（与基线一致）。
- [x] LAY-019 [P2] 拆分 `orchestrator/dashboard/src/services/wingman.ts`（1023 行）——类型定义、HTTP 调用、批量选择器和 WebSocket 桥接共享一个门面文件（`orchestrator/dashboard/src/services/wingman.ts:31`）。已按域拆为 wingman/ 目录 7 模块 + 兼容门面（2595576）：wingman/types.ts 350 行（ApiResponse + asRecord/asArray/toNumber/toStrictNumber/parseMaybeJson/unwrapResponseData/withData/normalizeApiResponse 求值辅助加 export、AgentStatus/WorkflowStatus/StepStatus 枚举、Agent/Workflow/Worker 全部 interface 与 normalizeWorkflow/normalizeWorkflowInstance 归一器、RawRecord 导出）、wingman/agents.ts 233 行（agent 列表/关停/标签 + Execution 域类型与 listExecutions/getExecution/getExecutionStatusColor + trigger 归一与 CRUD 五口）、wingman/batch.ts 78 行（批量选择器 BatchAgentSelector/BatchSummary/normalizeBatchSummary/requestBatch + 三个 batch.* 口）、wingman/teams.ts 105 行（TeamView/TeamMessageSummary 归一 + getTeams/getTeam/pushTeamMessage）、wingman/workflows.ts 72 行（workflow 列表/详情/模板/submit/cancel）、wingman/scripts.ts 134 行（ScriptInfo/ScriptLog 归一 + 脚本 CRUD/run/stop/logs 八口）、wingman/format.ts 71 行（formatBytes/formatDuration + 三类状态色映射）、wingman.ts 收窄至 77 行纯兼容门面——export * 汇聚七模块具名导出 + default 聚合对象保持原 19 口不变，16 个消费方 import 路径零改动；跨模块依赖仅 batch→agents（AgentTriggerConfigInput）与 batch/scripts/workflows→types 两向、无环。门禁全绿：tsc 0 错、eslint 0 问题、prettier clean、jest 全量 38 套 449/449 过、max build 成功。

- 分类不一致：`apps/agent/src/resource_loader.cpp` 属于应用打包/资源能力，但根目录还有通用 `runtime` 等应用文件，资源路径逻辑应独立子目录或库（`apps/agent/src/resource_loader.cpp:42`）。
- 分类不一致：`apps/agent/src/local_ipc_server.cpp` 是传输/协议装配，`apps/agent/src/agent.cpp` 是应用组合；两者同层但生命周期与协议职责不同（`apps/agent/src/local_ipc_server.cpp:139`）。
- 分类不一致：`apps/android/cpp/agent/base64.hpp` 内联复制 base64 实现，而通用编解码在 `lib/wingman/src/crypt.cpp`；Android 约束成立但应建立共享轻量 codec 而非 agent 内副本（`apps/android/cpp/agent/base64.hpp:12`）。
- 分类不一致：`orchestrator/server/internal/handlers/tagstore.go` 和 `capstore.go` 是注册表持久化适配器，却放在 HTTP handlers 包；registry 依赖会反向经过 API 包（`orchestrator/server/internal/agent/registry.go:90`）。
- 分类不一致：`docs/zh/guide/architecture.md` 是完整架构文档，而 `docs/guide/architecture.md` 只有英文占位大纲；同一路径两侧深度不一致（`docs/guide/architecture.md:3`）。

## 审核发现（33ec1f83..HEAD，只审不改，2026-11-10）

- [x] 审核发现：todo.md 头部「最后更新: 2026-11-09」落后于正文内容日期——LAY-001～LAY-019 勾选及 EP01 机器侧素材预采记录均在 2026-11-09 18:13 至 2026-11-10 06:01 之间追加/勾选，头部元数据日期未随本轮追加内容同步更新（todo.md:3）。—— 属实并已修复：将头部更新为「最后更新: 2026-11-10」与正文 2026-11-10 勾选记录对齐（todo.md:3）；验证：git diff todo.md 仅头部日期与勾选状态变化，无代码改动（2026-11-10）。

- [ ] 审核发现：本轮 diff 未见问题（15e6dc8c..HEAD）

## Go 覆盖率基线推进（2026-11-11，目标 98%）

- [x] orchestrator/server 总覆盖率 91.6% → 95.5%（+3.9pp，7356 语句；go test ./... -count=1 全绿 16 包，go vet 0 错）。补测三个最低包（不改生产代码）：internal/vault 39.3%→90.8%（LocalProvider 主口令状态机 setup/unlock/change/lock 全哨兵错误、换口令后凭据可解、Upsert 秘密留空保留旧值、Delete 归属校验、Export 无明文契约、Lookup 端口回退+库改密文 ErrDecrypt，d17cff2）；internal/models 73.4%→100%（ToView 时间格式化与 Result/Artifacts JSON 展开/降级，b4158e9）；internal/agent 89.4%→98.8%（guac_relay 回环端到端含 base64 载荷与拆链语义、send 失败/非 EOF 读错误/整条中继拆、handleProxyData/Close/Error 路由分支、SendNotify 导出口写失败语义、ListTeams 排序视图，a7200c3）。
- [ ] 覆盖率冲 98% 不可达，如实记录上限与原因：剩余 333 条未盖语句（95.5%），其中 handlers HTTP 错误分支长尾 ~139 条散布 10 个文件（guacamole/vault/update/asset/routes 等）、assets/store 19 条、vault DB 失败分支 18 条（内存 sqlite 天然不炸，需故障注入机制）、security 防御分支 10 条、杂项 ~30 条。到 98% 需覆盖 ≥186 条 ≈ 几乎清空全部 handler 错误分支 + 引入故障注入——超出「补 2-3 个包、不写凑数测试」的巡检范围；不硬凑。后续若继续推进，建议按 handlers 错误分支逐文件补 HTTP 层用例（每文件 10-30 条语句），预计可磨到 ~96.5%。

## C++ 覆盖率基线推进（2026-11-12，目标 ≥96%）

- [x] lib/wingman 总覆盖率 92.0% → 92.1%（14672/15933 行；Xvfb :99 下 core_tests 全量 2311 过 0 败 33 skip，build-cov 树 gcovr 8.6 口径）。巡检单点名短板逐文件补测（不改生产代码，Windows CI 是 codecov 唯一数据源 → 新测试文件全部无条件编译、平台差异收进文件内 `#ifdef`）：
  - script/modules/hotkey_module.cpp 24%→**96%**：新 lib/wingman/tests/hotkey_module_test.cpp（6 例——模块在位、register 参数校验矩阵（空参/非 string combo/非 callable）、非线程安全 callable 拒绝 + hotkey.error 事件、非法/合法 combo 注册注销簿记与 unregister 参数校验、XTEST 合成 Ctrl+Alt+F9/F10 真触发正常回调与 std::exception/… 两条 catch（Linux 腿））。
  - script/modules/systemwatch_module.cpp 37%→**93%**：新 lib/wingman/tests/systemwatch_module_test.cpp（5 例——processWatch/windowWatch 参数校验矩阵 + systemwatch.error、watchCount 全分支（缺省/process/window/bogus）、clearAll、进程腿受控子进程真触发 started/exited payload 构建 + EventHub 广播 + 两条 catch（priming 纪律：逐轮起探针证明 worker 已 primed 再断言事件）、窗口腿 TestX11Window 直写 _NET_CLIENT_LIST 驱动 opened/changed/closed 两轮 + 进程 canary 证 primed（Linux 腿，无 X skip））。
  - recorder.cpp 85%→**94%**：recorder_test.cpp 补 PlaybackDispatchesAllEventTypesOffline（recordEvent 离线注入七事件类型驱动 playback 分派 Click/Scroll/Key/Type 腿 + Type delay 睡眠；断言 saveToJSON 整型类型编码 + loadFromJSON 七条往返）。
  - 附带 system_watcher.cpp 82%→92%（systemwatch 模块测试的受控子进程/窗口腿捎带覆盖）。
- [x] 巡检单点名平台头文件口径核实（勿误判）：iscreen.hpp 0%、ifilewatcher.hpp 50% 是 codecov（Windows）视图缺失——本地 gcovr 实测 iscreen.hpp 50%（唯一未盖行 = `virtual ~IScreen() = default;` 纯虚接口析构、ifilewatcher.hpp 66%（唯一未盖行同理 `= default` 析构）、platform_types.hpp 100%。纯虚接口类无逻辑可测，无需 ignore。
- [x] 修复自查引入的测试顺序毒化：hotkey_module_test 曾在用例尾调 `defaultSharedInput()->shutdown()`——shared input 是进程级单例，关掉后依赖其已初始化态的后续用例（MacroRecorderOffline.PlaybackInjectsThroughXTest）静默空转注入失败（全量跑必挂、单独跑必过）。已改为不关；顺带定位法值得记：全量 vs 单跑 diff + 逐套件二分。
- [ ] 剩余缺口如实记录（不硬凑）：clipboard.cpp 58% 的缺口全部是 NullClipboard（TU-private 空实现，仅在平台工厂返回空时装配，X11 工厂永不下空 → 本地不可达）；crypt.cpp 75% 缺 79 行（Base64/HMAC 大量错误分支矩阵，属历史基线非本轮点名短板）；其余 <95% 文件（ui_automation.cpp 80%、hotkey.cpp 83%、update.cpp 86% 等）为平台/网络错误路径长尾。本轮 92.1% 是本地 Linux 口径的天花板；codecov（Windows 视图）预计显著更高（script/modules 点名三件均已 87%+，本地 96%/93%/87%）。


