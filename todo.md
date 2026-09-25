# Wingman 项目待办事项

> 最后更新: 2026-09-25
> 状态: 收尾阶段（P0/P1 全部完成；2026-09-14 完成「声明完成但实际不可用」类缺陷修复——Go Team/Inbox 三断链、C++ ml.run 推理入口、GUI scripts 页文件管理——并推进测试覆盖率，见「2026-09-14 功能修复与覆盖率冲刺」；同日新增 agent 分组与批量操作，见「Agent 分组与批量操作」）

> ⚠️ 本文档已于 2026-06-21 依据代码实际状态重新校准。之前的版本严重低估了 Go orchestrator
> （工作流引擎、Agent 心跳、审计均已实现）并错误描述了 dashboard 位置。

---

## 📅 2026-09-25 VNC/SSH/RDP 远控集成（Guacamole，P0 + 阶段二）

第三方远控方案选型 **Apache Guacamole（guacd 1.5.5）**：浏览器侧 guacamole-common-js 像素面 + Go server 反代 WS 网关 + guacd 协议翻译（RDP/VNC/SSH 三协议客户端在 guacd 内实现）。runtime 零参与（像素面与控制面正交，架构硬约束不破）。设计文档 `docs/remote-gateway-guacamole-design.md`（§9 备选方案：myrtille/Apache 老栈、websockify+novnc、自研三协议客户端均否决的理由）。

- **P0（commit 20fdb30，2026-09-23）**：一次性 5 分钟票据（`remoteticket.Manager`）+ RBAC（desktop:view 监看 / desktop:control 接管）+ 网关 WS 隧道（`/api/remote/guacamole`，票据即凭证）+ Dashboard RemoteDesktopModal + 三协议 e2e（`WINGMAN_GUACD_E2E=1` 门控，需容器栈）。
- **阶段二（本轮，DG-7/8/9）**：
  - **剪贴板（§14）**：common-js `onclipboard` 收（逐块 ack）+ `createClipboardStream` 发；监看模式隐藏发送 UI；仅 text/*。
  - **文件传输（§15）**：SSH 经 SFTP（`enable-sftp=true`）、RDP 经驱动器重定向（`enable-drive`+`drive-path`）、VNC 无通道（RFB 协议层没有，UI 整块隐藏）；上传 `createFileStream`+`BlobWriter`，下载 `onfile` 聚合 Blob。
  - **会话录制（§16）**：record 票据 → connect 注入 `recording-path/name`；安全默认 `recording-include-keys=false`（按键永不入录像）；检索 API `/api/remote/recordings`（list/download=desktop:view，delete=desktop:control）+ Dashboard 录像管理；`deployments/guacd` 增加 drive/recordings 共享卷。
  - 服务端新增 mock-guacd 握手测试（讲线协议的假 guacd 验证 connect 按位注入）+ recordings handler 全路径测试；Swagger 注解补齐全部 remote REST 端点并再生成。
- **验证基线**：Go 14 包 `go vet` + `go test -race` 全过；Dashboard jest 261/261、tsc 0 错、eslint 仅存量 2 警告、prettier 干净。
- **剩余（P1/远期）**：cockpit 共享像素面组件、SSH 文件浏览器（`onfilesystem` SFTP 树）、浏览器内录像回放（session-player 非 npm 分发）、i18n 接线（阶段二 UI 文案暂为特性内中文硬编码，沿用 P0 先例）。

## 📅 2026-09-25 远控 P1 推进（公共件抽取 / 会话审计 / e2e 容器栈）

设计 §11 P1 三项中**可独立完成**的部分（cockpit 实际接入需对方仓库，不在本轮）：

- **前端公共组件抽取**（设计 §7.3 落地，记 §7.1）：新建 `orchestrator/dashboard/src/components/RemoteDesktop/`，四件边界一一对应——`useGuacamoleSession`（连接生命周期 hook，票据一次性故无自动重连）、`TicketClient` 接口 + `createWingmanTicketClient`（票据客户端，接口化以便第二方换 API base）、`RemoteErrorNotice` + `classifyRemoteError`（错误与降级，权限/未配置类不渲染重试）、`RemoteDesktopToolbar`（监看接管与工具栏）。`RemoteDesktopPanel` 为容器无关合成件，`RemoteDesktopModal` 降级为 Modal 容器适配器；`RemoteProtocol`/`RemoteSessionParams` 收敛到公共件 `types.ts` 唯一定义（消除协议枚举分叉）。jest 261 → 328，新/改文件四项覆盖率 100%。
- **会话审计落库与查询接口**（设计 §17）：新增 `models.RemoteSessionAudit`（表 `remote_session_audits`）——**不复用 AuditLog**（事件流水的 meta 是 JSON，聚合既慢又脆；两者受众不同，缺一不可）。只写终态（closed/failed），进行中不落行（否则报表把未结束会话算进时长、崩溃留不闭合脏行）；`RecordRemoteSession` 唯一落库入口（UTC 归一/时长口径/终态枚举是契约），写失败只记日志不阻断会话关闭。`GET /api/remote/sessions`（desktop:view，**不新增 RBAC 码**——报表只读、无接管能力）一次返回列表 + 汇总 + 维度聚合 + 时间趋势四块视图，四条查询共用同一过滤器，汇总/分组/分桶基于全量而非当页；`groupBy` 白名单化杜绝 SQL 注入。录像名与会话同源可关联；会话 ID 提前到拨号前生成，故建连失败也有唯一标识。Dashboard 新增 `RemoteSessionReportModal`（Agents 页「会话审计」入口，录像管文件、报表管行为）。jest 328 → 351，新文件四项覆盖率 100%。


---

## 📅 2026-09-22 真机验证自动化（架构盘点第九轮）

两项真机验证遗留项从「纯人工多步操作」升级为「真机各跑一条命令」，自动化链路本身已在本机验证：

- **XRecord 正向端到端用例**（`recorder_x11_e2e_test.cpp`，仅 Linux 编译，注册于 tests CMake `UNIX AND NOT APPLE` 块）：XTest 注入按键（优先 F13 防误触焦点窗口，键码缺失回退 'a'）→ 轮询捕获计数 → `saveToJSON` 内容精确断言（`"type": 5` 即 KeyDown——x11_recorder 回调只把 KeyPress 映射为 KeyDown、KeyRelease 丢弃；`"keyCode": <注入键码>` 带字段名匹配防 timestamp 数字误命中）。Xvfb 下 EnableContext 必然失败 → GTEST_SKIP；本机双场景实测（无 DISPLAY、Xvfb :99）均正确 skip 且不误报失败，Xvfb 场景 ~300ms 耗时证明真实走过 EnableContext 尝试路径。与 XTest 注入共用 `X11ServerLockGuard`，ctest -j 下与窗口/剪贴板测试串行化。
- **一键验证脚本**：`scripts/verify-xrecord-desktop.sh`（Linux + DISPLAY 守卫；用例全 skip 判「未验证」exit 2 不放绿；`--build` 强制重建）；`scripts/verify-macos-runtime.sh`（darwin + VCPKG_ROOT 守卫——缺失即报错不回退系统库，triplet 按 uname -m 自动选，跑 Clipboard/FileWatcher/Screen/Input/UnixSocketChannel 五套件，附 CGEvent 授权等人工观察项提示）。脚本三态自测通过（本机 SKIP、Xvfb SKIP 均不放绿）。
- **回归确认**：core_tests 增量编译通过；既有 `MacroRecorder*`/`*Recorder*` 用例无回归（19 个 skip 为录制后端不可用的预期降级，其余全 OK）。

剩余：真桌面 Linux 跑 `scripts/verify-xrecord-desktop.sh`、macOS 跑 `scripts/verify-macos-runtime.sh`；回放时序手感、CGEvent 辅助功能授权、录屏授权弹窗等仍属真机人工观察。

---

## 📅 2026-09-22 死代码清理（架构盘点第一轮）

架构盘点结论：宏架构（双控制面 / platform 抽象 / apps+lib / orchestrator 边界）合理且执行到位；主要问题为死代码撑起的虚假复杂度、lib/libs 边界失效、Android 源码级耦合。本轮完成第一优先级：

- **protobuf 链移除**：`protobuf/`（3 个 .proto 无人消费）+ `libs/proto`（`PROTO_PATH` 指向不存在的根目录 `proto/`，protoc 从未生成代码；孤儿文件 proto_wrapper_json.cpp）。实际协议 = 16 字节头 + JSON 体（C++ `MessageHeader` 与 Go `pkg/agent/client.go` 手写对齐，由 `integration/protocol_test.go` 兜底）。若未来需要 IDL，须双侧生成后一并落地。
- **libs/debug 移除**：EmmyAdapter 无任何调用方（runtime 仅链接未使用，自带测试也不链接它）；`WINGMAN_ENABLE_EMMY`/`WINGMAN_HAS_DEBUG` 宏零源码消费者。实际调试链路 = 脚本运行时 require `emmy_core`。
- **clasp 移除**：submodule + vcpkg overlay port 双落位、全仓库零引用。
- 同步清理：根/runtime/lua CMake 选项与链接、`vcpkg.json` protobuf、CI compat 目标、`build-scripts` 安装列表、platform_boundary_allowlist（-1 条）、BUILD.md / setup / DEVELOPMENT / project-structure / architecture / remote_protocol / debugging 文档、CHANGELOG Unreleased。
- Go 侧 `google.golang.org/protobuf // indirect` 为 gin/swag 传递依赖，非死代码，保留。

---

## 📅 2026-09-22 M4/M5 收尾校准（架构盘点第八轮）

**基线验证（三套 UI 测试全绿）**：Dashboard jest 235/235、GUI vitest 506/506、Tauri Rust cargo test 14/14（含 IPC 集成 5 用例）。

**M4 三层契约审计（跨端对齐，无缺口）**：
- 命令方向（server → runtime）：server 生产代码下发 `run_script`/`stop_script`/`list_windows`/`get_status`/`screenshot.capture`/`system.shutdown`/`trigger.*` 共 7 类，`agent.cpp handleRemoteCommand` 全部支持（trigger.* 前缀经 remoteDispatcher 复用本地 RPC handler）。
- 事件上行（runtime → server）：runtime 仅转发 `trigger_fired`/`script_state`/`script_output` 三类（`log.line`/`connection.state_changed` 有意不转发——防高频日志淹没 agent 上行链路，agent.cpp:385 注释明确），server `handleEvent` 三 case 全接并广播。
- 广播下行（server → Dashboard）：server 广播 agent connected/disconnected/status_changed、workflow submitted/status_changed/progress、script output/state_changed、trigger_fired、screenshot 共 9 类，Dashboard websocket.ts 全部消费。

**文档校准**：ROADMAP M4/M5 状态 🚧 → ✅（附审计与测试证据）；删除 M4 RBAC 条目过时的「⚠️ PermissionRequired 已实现未接线」注（已接线 8 权限码）；todo.md 里程碑表 M4 100% / M5 ~95%，完成度表权限系统 95% → 100%（Swagger 47 端点已全注解）。

**剩余**（用户确认跳过）：macOS 真机运行时验证、Linux XRecord 真桌面验证——均需真机人工执行。

---

## 📅 2026-09-22 文档站构建验证与死链修复（架构盘点第七轮）

第五轮改 VitePress sidebar 后未实际构建——本轮补上：`npm run docs:build` 成功（83.6s，仅 chunk 体积警告）；`ignoreDeadLinks: true` 会静默放过死链，故另写全量站内链接扫描（206 条链接，排除 node_modules/dist/锚点/外链）：发现 4 条真死链并修复——`guide/getting-started.md` 的架构决策链接多跳一级（`../../` → `../`）、`guides/database.md` 与 `guides/configuration.md` 引用不存在的 `api/storage.md`/`api/serialization.md`（与第五轮 docs/README 同款历史错误，改指 api/db.md、api/serialize.md）。复扫真死链 0。另将「测试」段两项实质完成（C++ runtime 保持水准、三条集成测试全 ✅）按事实勾选。剩余未勾项仅 macOS 真机验证与 Linux XRecord 真桌面验证两项，均需真机人工执行。

---

## 📅 2026-09-22 handlers 评估与辅助函数收口（架构盘点第六轮）——⑤ 关闭

**评估结论：不拆 Go 子包，⑤ 关闭。** 实测数据：46 文件全部 `package handlers` 单包、一域一文件（21 个生产文件命名即导航）、仅依赖 gin + gorm + 内部 models/middleware/rbac/security；测试 6300+ 行全为黑盒 HTTP 测试（经 gin 路由发请求，coverage_*×11 引用 handler 符号数为 0），天然依附路由装配点 routes.go。拆包成本 = 9 个 setup helper 重排 + coverage 跨域文件拆散归属 + 共享辅助抽包 + routes.go import 全部子包，收益仅目录观感——单包 HTTP 层是 Go 惯用模式（net/http 同例），维持现状。

**顺手收口**：跨文件共享的辅助函数归位新建 `helpers.go`——`parsePositiveInt`（audit/messages/users 三域共用）、`actorName` + `isUniqueConstraint`（roles/users 共用）；`WriteAuditLog` 留 audit.go（横切领域函数，语义归属正确）。单文件私有辅助不动。

验证：gofmt 0、go vet 0、14 包全过（含 integration 完整跑）。

---

## 📅 2026-09-22 文档去重与会话产物清理（架构盘点第五轮）

共删除 15 个文件，全仓交叉引用清零校验通过：

- **根目录会话产物删除**：`macOS_SESSION_SUMMARY.md`、`macOS_VERIFICATION_REPORT.md`（2026-06-22 macOS 验证会话产物，零引用）；`docs/superpowers/`（5 份 2026-06-27/28 会话计划/交接文档，仅自引用）。
- **时令文档删除**：`docs/pending-changes.md`（2026-06-21 一次性改动分析，todo.md 引用改纯文字）、`docs/project-improvements-2026-05-06.md`、`docs/project-improvements-plan.md`（零引用）、`docs/architecture-improvement-plan.md`（自标"历史设计草案，部分内容已过期"）。
- **getting-started 去重**：根级 `docs/getting-started.md` 删除（内容较旧），保留 `docs/guide/getting-started.md`（站点 sidebar 主文档，含 CLI 参数/运行模式/agent.toml 等新架构内容）；README×2、api/overview、api/core、docs/README 共 5 处引用改指新路径。
- **安装文档三份收口为 BUILD.md**：删 `docs/installation.md`（312 行，2026-06-23 旧版）与 `docs/setup.md`（131 行，零引用）；installation 独有的 macOS/Linux 故障排除（Xcode-select、权限、系统依赖、vcpkg 失败）并入 BUILD.md；README 安装入口改指 BUILD.md（CONTRIBUTING/project-structure 原已指向它）。
- **user-guide.md 删除**：385 行大全式手册，安装/配置/API/调试各章均已被站点对应专篇覆盖且更新。
- **guides 三孤儿挂上站点**：`guides/configuration.md`、`guides/database.md`、`guides/triggers.md` 加入 VitePress sidebar「进阶指南」组——与 `guide/config.md` 主题正交（前者是 wingman.config API 实践教程，后者是 config.json 配置文件参考），去重结论为"挂出来"而非合并。
- **死链修复**：docs/README.md 索引引用的 `api/storage.md`、`api/serialization.md`、`api/debugging.md` 三个不存在的文件，改指实际存在的 kv/db、serialize/json/ini、debugger。

---

## 📅 2026-09-22 Go 包收敛与路由装配收口（架构盘点第四轮）

- **`pkg/agent` 并入 `internal/agent`**：同名词包分居两处（listener/team/client 在 pkg，registry/types 在 internal），实为同一条 runtime 接入链路的两半，靠接口跨包解耦。合并后单包 20 文件（线协议类型 / FrameListener / TeamManager / Registry / types），`pkg/agent` 消失；`Broadcaster`/`AgentRegistrar` 接口保留（依赖倒置，注释已更新为准确表述）。消费方 import 与 `agentPkg` 别名全部清理（main.go 同包双别名导入一并消除）。
- **main.go 路由装配收口**：gin 中间件、静态资源与全部 API 路由（~230 行）从 main.go 抽至 `internal/handlers/routes.go`（`RegisterRoutes(r, RouterDeps)`），main.go 415 → 178 行，回到"配置 + 组件生命周期"职责；装配点集中是后续 handlers 按域拆子包的前置。**决策**：handlers 不立即拆多包——21 文件已一域一 Handler，拆包主要成本在 5900 行测试与 coverage_* 跨域用例重排，当前收益不足，列为后续评估项。
- gofmt 全仓修齐（含 3 个历史遗留未格式化文件）；go vet 0、全部 Go 测试通过（integration / handlers / workflow / agent 等 14 包）。

---

## 📅 2026-09-22 平行实现合并（架构盘点第三轮）

盘点假设"双 TCP 通道"经查证**不成立**：`lib/wingman` 的 `TcpChannel` 是本地 IPC（IIpcChannel 家族）的显式 fallback（`allowTcpFallback` 安全闸、纯 JSON 无帧头），与 `libs/transport`（远程链路、16 字节头 + JSON）职责正交，保留。真正合并的两处：

- **双 system_handler 删除**：lib/wingman 的 `wingman/rpc/system_handler` 是 stub 版（`system.getStatus` 返回硬编码假数据），在 LocalIpcServer 注册后立即被 runtime 版静默覆盖（`registerHandler` 为 map 赋值，后注册覆盖先注册）——假数据陷阱。整层删除；`system.getVersion` 并入 runtime 版统一提供，协议方法不减；用例自 rpc_test 迁移为 `apps/runtime/tests/system_handler_test.cpp`（3 用例，覆盖 providers 空注入回退值）。
- **XOR 混淆遗留移除（breaking）**：`SecurityManager::encryptString/decryptString` + Lua `security.encryptString/decryptString` 删除（XOR 非真实加密，已 deprecated + 运行时告警多年，生产 C++ 零调用）；加密统一走 `crypto.encryptAES/decryptAES`（AES-256-GCM）。**手写 SHA-256 收口**：security.cpp 内 60 行手写实现删除，`hashString` 改调 `wingman::crypt::sha256`（OpenSSL EVP），输出格式不变。security_test 清 14 个 XOR 用例（保留 GenerateRandomStringsAreDifferent）、script_function_test 清 1 个、docs/api/security.md 同步。

---

## 📅 2026-09-22 Android agent 核心下沉（架构盘点第二轮）

消除 `apps/android/cpp` 对 `apps/runtime/src` + `lib/wingman/src` 私有树的源码摘编，共用核心改为正经库目标：

- **`libs/agentcore`（新）**：`remote_client` / `event_buffer` / `remote_client_config` 自 `apps/runtime` git mv 下沉，命名空间保持 `wingman::runtime`；仅依赖 transport + spdlog/nlohmann（硬约束：不得依赖 lib/wingman 本体，NDK 侧不编 core）。`apps/runtime/config.hpp` 引入下沉后的 `RemoteClientConfig` 维持 `AgentConfig` 完整定义。
- **`libs/androidagent`（新）**：`script_runner` / `android_script_api` 自 `apps/android/cpp/agent` git mv 下沉；并收口全仓库唯一的 lib/wingman cherry-pick 清单（bitmap / screen / image_analyzer / platform/android 租户），消费方不得再自行向 `lib/wingman/src` 加 include。OpenCV 判定与 core 严格一致，重复编译 TU 配置相同、链接器仅取其一。
- **消费方收口**：`apps/android/cpp`（NDK 壳）只链 transport/agentcore/androidagent 三库；`libs/lua/tests` 经 `wingman::androidagent` 桌面同源编译全部 androidagent 源（script_runner_test / android_api_test 22 用例，替代 NDK 验证所有可宿主 TU）。
- 边界检查通过（223 文件，allowlist 仅路径更新）；runtime_tests 29/29；NDK 专属 TU（jni_bridge / android_screenshot）依赖 NDK sysroot 头，由 CI build-android 覆盖。

---

## 📌 位置澄清（重要）

| 路径 | 实际内容 |
|------|----------|
| `orchestrator/server/` | Go 远程中控（HTTP API + WebSocket + Agent TCP 监听） |
| `orchestrator/dashboard/` | **真正的 wingman 远程 Dashboard**（React/Umi/Ant Design Pro，含 Agents/Monitor/Scripts/Workflows 页面 + wsService + wingman.ts） |
| `apps/gui/` | 本地 Tauri GUI（Svelte 5），通过 local IPC 控制 runtime |
| `dashboard/`（仓库根） | ⚠️ 另一个产品 **Croupier** 的副本，已被 `.gitignore` 排除，与 wingman 无关，勿改 |
| ~~`dashboard_old/`~~ | 已删除（空目录） |

---

## 🎯 里程碑规划（已校准）

| 里程碑 | 目标 | 状态 | 完成度 |
|--------|------|------|--------|
| M1: 核心功能 | 基础屏幕捕获、输入模拟、Lua 脚本 | ✅ 完成 | 100% |
| M2: 触发器系统 | 条件触发、自动化配置 | ✅ 完成 | 100% |
| M3: 宏系统 | 录制回放 | ✅ 完成 | 100% |
| M4: 远程编排 | Orchestrator 中控、Agent 通信、工作流引擎 | ✅ 完成 | 100% |
| M5: GUI 界面 | 本地控制台 (Tauri) + 远程 Dashboard (React) | ✅ 完成 | ~95%（macOS 真机验证为独立遗留项） |
| M6: 人性化模拟 | 防检测、随机化 | ✅ 完成 | 100% |
| M7: 调试器集成 | EmmyLua 调试支持 | ✅ 完成 | 100% |

---

## 🔴 P0 - 必须完成（阻塞交付）

### RBAC 权限系统（Go orchestrator）— ✅ 已完成 (2026-06-20)

实现：`internal/rbac/`（种子+解析）、`internal/models/role.go`（Role/Permission 多对多）、
`internal/middleware/auth.go` `PermissionRequired`、`internal/handlers/users.go`/`roles.go`、
`/api/admin/{users,roles,permissions}` 路由、dashboard `services/api/admin.ts` + `Admin/Users`、`Admin/Roles` 页面。
内置角色 `admin`(*通配)/`operator`/`viewer`，权限码 `resource:action` 形式，与 dashboard `access.ts` 对齐。

- [x] **数据模型**：Role/Permission（many2many role_permissions）+ AutoMigrate + User.Active
- [x] **后端 API**：用户 CRUD + 重置密码、角色 CRUD + 权限分配、权限目录
- [x] **中间件**：`PermissionRequired`（admin 旁路 + DB 解析 + 请求级缓存）；`HandleGetPermissions` 返回真实 permissionIDs
- [x] **Dashboard 前端**：`Admin/Users`、`Admin/Roles` 页面 + 路由 + `admin.ts` 服务
- [x] **测试**：`rbac_test.go` + `handlers/rbac_test.go`（种子幂等、权限解析、inactive、CRUD、内置保护）
- [x] 可选增强：将现有 `RoleRequired("admin")` 写路由渐进迁移到 `PermissionRequired`（✅ 已接线，2026-06-21：agents/workflows/scripts/users/roles/settings 写操作均按权限码鉴权）；Swagger 文档待补

### Runtime IPC 事件推送（C++ runtime → Tauri GUI）— ✅ 机制完成 (2026-06-20)

现状：runtime 通过 RPC `events.drain` 暴露缓冲事件，GUI 轮询拉取并分发。采用 **pull 模型**
（非 type=2 push），避免 Rust IPC 客户端在 Windows 阻塞 IO 下引入异步读取循环导致帧错位
（设计决策见 `docs/architecture-decisions.md` "Runtime-to-UI Event Delivery"）。

- [x] **事件缓冲**：`apps/runtime/.../event_buffer.hpp`（线程安全有界队列，上限 1000）
- [x] **日志事件**：spdlog `EventLogSink` → `log.line`（main.cpp 已挂载）
- [x] **触发器事件**：`TriggerManager::setOnFired` 回调（win32+posix）→ `trigger.fired`（local_ipc_server 已接 EventBuffer）
- [x] **drain RPC**：`events.drain` handler（local_ipc_server 已注册）
- [x] **Rust 命令**：`commands::events::drain_events`（main.rs 已注册）
- [x] **GUI 分发**：`stores/events.ts` 轮询器（500ms）→ `logs.addRuntime` + `triggers.markFired`；App.svelte 按连接启停
- [x] **logs 页面**：`logs.ts` 增加 `addRuntime` + 容量上限（1000 条）
- [x] **脚本状态**：`script.state_changed`（StandaloneMode 各状态转换已接 EventBuffer）
- [x] **截图事件**：`screenshot.frame` —— 经评估**不接入 drain 缓冲**（全屏 base64 大负载会淹没有界缓冲；截图保持按需 `screenshot.capture`，见 architecture-decisions.md）
- [x] IPC 调用超时处理（Rust `IpcClient` 已有 30s 超时；GUI `connection.refresh` 捕获错误并置 disconnected）

### Debugger 端点实现（Go orchestrator）— ✅ 完成（直连模式，2026-06-20）

现状：EmmyLua 调试由 VSCode 直连 runtime:9966，Go server 不中转调试协议（双向流不适合
dashboard → server → agent 请求/响应模型）。原裸 501 stub 已替换为结构化「直连模式」契约。

- [x] `GET /api/debugger/info`：返回调试模式说明 + 各 agent 调试端点（host:9966）+ VSCode launch.json 片段
- [x] `connect/command/breakpoints`：返回结构化 501，指向直连模式（不再是裸 stub）
- [x] 测试：`debugger_test.go`（info 返回 direct_attach + agent 端点；connect 返回指引）
- [x] README 文档更新

---

## ✅ 代码缺陷（2026-06-21 分析发现，已全部修复）

> 来源：2026-06-21 工作区未提交改动分析（原文档已随文档去重删除）。
> 全部 10 项已修复（含回归测试 `TestBroadcastMessagePerUserReadState`）。

### 高优先级

- [x] **Dashboard `listPermissions` 导出重名冲突**：`admin.ts` 版本重命名为 `listPermissionCatalog` / `ListPermissionCatalogResponse`，Roles 页已改用新名；`permissions.ts` 保留为 Profile 页使用的规范化版本
- [x] **广播消息已读状态污染（`handlers/messages.go`）**：新增 `MessageRead` per-user 关联表（`message_reads`，AutoMigrate 已接入）；列表/未读数/标记已读全部改用 `EXISTS` 子查询判定 per-user 状态，不再更新共享行 `status`。回归测试 `TestBroadcastMessagePerUserReadState` 已加

### 中优先级

- [x] **路由重复注册（`main.go`）**：移除 `/api/v1` 组中重复的 messages/feedback，仅保留 `/api`（dashboard 兼容）组
- [x] **`PermissionRequired` 为死代码**：已接线——`agents:manage` / `workflows:run` / `scripts:edit` / `scripts:run` / `users:manage` / `roles:manage` / `settings:view` / `settings:edit` 分组鉴权（admin 自动放行），取代原粗粒度 `RoleRequired("admin")`
- [x] **Runtime jitter 粒度粗（`remote_client.cpp`）**：`computeBackoff` 返回毫秒、`sleepInterruptible` 接受毫秒，抖动改为 0~999ms 精确粒度
- [x] **Dashboard Settings 端点不一致**：Go server 在 `/api` 组补充 `/settings`（GET `settings:view` / PUT `settings:edit`），Settings 页改用 `/api/settings`，与其余 dashboard 调用前缀统一
- [x] **Dashboard `canUserManage`/`canRoleManage` 路由未用**：新增 `canAccessAdmin` 控制菜单可见性；`/admin/users` 用 `canUserManage`、`/admin/roles` 用 `canRoleManage` 细粒度守卫

### 低优先级

- [x] **`triggers.ts` `markFired` 注释与实现不符**：新增 `last_triggered_at` 字段，`markFired(id,name,timestamp)` 记录命中时间戳；events.ts 传入 `event.timestamp`；注释与实现一致
- [x] **`script.state_changed` 未覆盖 error**：`StandaloneMode::start()` 订阅 `ScriptManager` error 事件，异常退出推送 `state:"error"` + 错误信息
- [x] **EventLogSink 无 payload 大小截断**：消息超 4096 字节截断并追加 `...[truncated]`
- [x] **EventBuffer 跨类型 FIFO 公平性**：容量超限时优先丢弃高频 `log.line`，保护低频重要事件（trigger/connection/script）；`events.drain` 响应新增 `dropped` 累计丢弃计数，GUI 可感知

### Windows 平台修复（2026-06-21）

- [x] **CRLF 警告刷屏**：仓库无 `.gitattributes` + `core.autocrlf=true` → 每次 git 操作 48 文件警告。新增 `.gitattributes`（源码统一 LF、`.bat/.cmd/.ps1/.sln/.vcxproj` 保留 CRLF、二进制标记 binary）并 renormalize，警告归零
- [x] **`ClipboardTest.Clear` 偶发失败**（Win32 剪贴板）：①`clear()` 绕过 `openClipboard()` 重试逻辑直接调 `OpenClipboard`，锁竞争时静默失败 → 改用重试版本；②`openClipboard` 重试预算 5×10ms=50ms 太紧 → 提升到 20×25ms=500ms；③剪贴板测试改为 `TEST_F` + `SetUp` 探测可用性，OS 拒绝访问时 `GTEST_SKIP`（环境问题）而非误报失败。全套件 1705/1705 稳定

---

## 🟡 P1 - 高优先级（功能完善）

### Orchestrator Dashboard（`orchestrator/dashboard/`）收尾

**已完成页面**（已对接真实 API + WebSocket，勿重做）：
- ✅ Welcome、Agents（getAgents + shutdownAgent + WS 事件）
- ✅ Scripts（Monaco 编辑器 + CRUD + run/stop/logs）
- ✅ Workflows（submit/cancel + WS submitted/status_changed/progress + Steps 可视化）
- ✅ Admin/LoginLogs、Admin/OperationLogs（listAudit + CSV 导出）
- ✅ User/Login、Profile（7 tab，最完整）

**待完善**：
- [x] **Monitor 页面**（`Monitor/index.tsx`）：mock 已移除（trigger 列表改为 WS 事件驱动上限 20，无 agent 时空态 + Alert）；远程 `trigger.list` API 已暴露并接入；触发器 CRUD（新增/编辑/删除表单，经 `POST/PUT/DELETE /api/agents/:id/triggers`，agents:manage）
- [x] **Settings 页面**（独立 `pages/Settings/index.tsx`，读写 server 键值，admin 可编辑）
- [x] Dashboard 截图实时推送（Monitor 监听 `screenshot` 事件，runtime 按需 capture——架构决策：不进 drain）

### Dashboard 审核与修复记录（2026-09-04）

对照声明逐文件审核 `orchestrator/dashboard/`（React/Umi）+ Go server 路由，发现并修复：

**P0 — 前后端 API 前缀契约断裂（已修复）**

- 前端 `src/utils/api.ts` 曾把所有 `/api/*` 全局改写为 `/api/v1/*`（requestErrorConfig 拦截器 + core/http 双生效点），而 server 把 agents/workflows/messages/feedback/audit/admin 路由注册在无 v1 前缀的 `/api` 组 → Agents、Workflows、站内消息、反馈、审计、Admin/Users、Admin/Roles 页面运行时全部 404
- 修复：删除 `utils/api.ts` 重写层与全部调用点（service 路径字符串本就与 server 注册一致，server 零改动）；`POST /api/scripts/delete` 404 与 scripts/settings 写操作绕过细粒度权限码（落到 v1 admin-only）两个次生问题随重写层移除一并消除
- 回归防线：新增 `tests/apiContracts.test.ts`（26 用例逐函数锁定 service → URL 映射，禁止前缀漂移）

**P1/P2 — 事件与页面缺陷（已修复）**

- [x] Monitor 触发器事件监听 `type='trigger'` 与 server 广播 `type='agent'+event='trigger_fired'` 不匹配 → 永远收不到；新增 `wsService.onTriggerFired` 接线 + `tests/websocket.test.ts` 单测
- [x] `WSMessageType` 死枚举（与实际二级协议不符）删除；WS 心跳空转 → 实现死链检测（75s 无下行消息主动断开重连）
- [x] LoginLogs 双重分页（后端分页结果再本地 slice → 第 2 页起空白；翻页不触发请求）→ 移除本地分页、useEffect 随 page/size 重新拉取；login-logs/operation-logs 路由补 `canAccessAdmin` 守卫
- [x] Welcome 假状态 Tag（硬编码「系统正常运行」）→ 改中性功能导览；Monitor 设置按钮（无 onClick）→ 跳转 /settings；事件流「刷新」假按钮删除；「运行中/已暂停」假开关 → 真实控制本页事件记录；script 事件 error 级别判断修正
- [x] Login「自动登录」无消费者 checkbox 删除；登录背景图脚手架外链 → 本地渐变

**P3 — 工程卫生（已修复）**

- [x] 脚手架残留清理：`mock/` 目录（823 行假数据）、`EXAMPLE_USERS_PAGE.tsx`、Footer/Question 的 ant design pro 外链（Footer 同时修正 GitHub 仓库地址 cuihaitao → cuihairu）
- [x] 死代码清理：wingman.ts `getAgent`/`getWorkerStatuses`/`getStepStatus`、admin.ts `getUser`/`getRole`、auth.ts 兼容层 6 函数 + 2 死类型（保留 `createSession`/`fetchCurrentUserGames`）
- [x] 死测试清理：`tests/workspace/` 6 个引用已删模块的测试；jest.config 移除 testPathIgnorePatterns 与指向不存在 `tests/umi/` 的 moduleNameMapper（css mock 改指 `tests/mocks/styleMock.js`）
- [x] i18n 修复：zh-CN menu 补齐 11 个缺失菜单 key；zh-CN 权限模板 fallback 替换 workspaces/functions/ops 残留 key 为代码实际引用的 6 组（消除中英错位）。注：业务页面硬编码中文的全面 i18n 接线不在本次范围（8 套语言文件仅 4 个文件使用 intl，属独立任务）
- [x] Dashboard 业务页面全面 i18n 接线（2026-09-15 完成，上述「独立任务」闭环）：18 个页面/组件约 530 处硬编码中文全部接线 umi intl（Agents、Scripts、Workflows、Monitor、Settings、Admin Users/Roles/登录日志/操作日志、Account、Messages、Support、Feedback、Welcome、403、404、Login、Profile 头像段）；8 语言（zh-CN/zh-TW/en-US/ja-JP/fa-IR/id-ID/pt-BR/bn-BD）pages.* key 全量补齐（新增约 400 key × 8 语言，含为非中英 6 语言补齐既有 profile.avatar.modal.* 段消除裸 key）；ICU 参数化（{count}/{name} 等），模块级纯函数改双参兜底（提取错误消息）；校验：tsc 0 错误、源码引用 493 key 对照 zh-CN 零缺失（校验脚本）、eslint 0、jest 235/235、生产构建通过

**验证基线**：`tsc --noEmit` 0 错误；jest 47/47（新增 apiContracts 26 例 + websocket 5 例）；eslint 仅剩脚手架 `service-worker.js` 历史 warning。

**遗留边界（已知，不阻塞）**：server 端 `/api/v1/status`、`/api/v1/health`、`/api/v1/windows`、`POST /api/v1/screenshot`、`/api/debugger/*` 前端未消费（debugger 为有意直连模式）；Welcome 仍为静态页（不接后端状态）。

### Tauri GUI（`apps/gui/`）收尾**已完成**：dashboard/scripts/screen/triggers/settings/logs 六页面 + IPC 连接重试 + 全部 Tauri 命令 + events 轮询（logs/trigger/script 事件已接）。
（注：`editor/` 空占位目录已删除）

- [x] `scripts/+page.svelte`：**启动器**定位（按路径加载/运行/停止 + 状态可视化），**不做内置编辑器**——脚本编辑统一用 VS Code（EmmyLua 补全 + wingman.d.lua + launch.json 调试，见 `docs/development-environment.md`）；`editor/` 空占位目录已删除
- [x] logs 页面接收 IPC 推送的 runtime 日志（events.ts → logs.addRuntime）
- [x] dashboard 截图实时推送（按需 screenshot.capture + events 轮询）

### Go orchestrator 增强

**已完成**：工作流引擎（DAG + 环检测 + 持久化 + WS 事件）、Agent 心跳（30s/90s 超时）、
JWT auth（bcrypt + 限流）、审计日志、Team/投票/Inbox。

- [x] **工作流引擎增强**（引擎已可用，重试+负载均衡+模板已加，2026-06-20）
  - [x] 指数退避重试策略（`WorkflowStep.MaxRetries`/`RetryBackoffSeconds`，取消不重试）
  - [x] Agent 负载均衡（`selectAgent` 选在执行步骤最少的 agent；显式 worker 优先）
  - [x] 工作流模板库（`GET /api/workflow-templates`：5 内置模板 + dashboard 模板选择器）
  - [x] 独立步骤类型（`wait`/`condition`/`screenshot` 已实现；`screenshot` 通过远程 `screenshot.capture` 命令复用 runtime 截图 handler 并广播 Dashboard WS）
- [x] **Agent 管理**
  - [x] 负载均衡（`selectAgent` 选在执行步骤最少的 agent；显式 worker 优先）
  - [x] Agent 分组/标签（`PUT /api/agents/:id/tags` + 注册表 SetTags + Dashboard 标签列/Popover 编辑）
- [x] **通知/历史**（子项全部完成，2026-09-17 补勾）
  - [x] WebSocket 事件广播完善（runtime `EventBuffer` 远程 sink → `RemoteClient::sendAgentEvent` → server `handleEvent` 广播 `trigger_fired`/`script_state`/`script_output` 到 Dashboard WS；listener.go 新增 `script_state` case）
  - [x] 通知历史持久化（Message 模型已入库；per-user 已读见代码缺陷修复）
  - [x] script_output 转发（`IScriptEngine::setOutputCallback`：Lua override `print`→`tostring`；ScriptManager 接 `logScriptOutput`；StandaloneMode 推 `script.output` 到 EventBuffer；远程 sink 转发 `script_output`；GUI events.ts 显示。Python stdout/stderr 重定向已完成——`libs/python/src/python_script_engine.cpp` stdout/stderr proxy 注入 `sys.stdout`，经同一回调链路下发）
- [x] **API 文档**：Swagger/OpenAPI — swaggo + `/swagger/index.html` 实时 UI（`swag init` 生成 `docs/`）；已注解 11 个关键端点（auth/profile/messages/users/roles/agents/workflow-templates），其余端点可渐进补注解

### Runtime Agent Outbound（C++ — 基本完成）

**已完成**：`remote_client.cpp` 心跳线程（30s `agent.heartbeat`）、重连线程、注册流程、**指数退避重连**（2026-06-20）。

- [x] 断线重连指数退避（`reconnectLoop` + `computeBackoff`：base×2^attempt 封顶 + 抖动；`max_reconnect_interval` 配置；连接成功重置计数）
- [x] 持久重连循环（同时处理初始失败与断线，可被 stop 打断）
- [x] 命令队列 / 离线缓存（有界 outbox：断线缓冲 Notify 类，重连后 flush；Response 类丢弃避免陈旧）
- [x] 连接状态回调通知 GUI（`RemoteClient::onEvent` → EventBuffer `connection.state_changed` → events.ts → connection store `remote` 字段 + 日志；GUI 可显示远程链路状态）

---

## 🟢 P2 - 中优先级（功能增强）

### 跨平台验证

> 编译跨平台已由 CI 矩阵保证（C++ Ubuntu/macOS、Go 三平台、Dashboard 三平台打包）。
> 以下为各平台**运行时功能**的人工/集成验证（需在实际 OS 上执行）：

- [ ] **macOS**（基础实现已存在，装配断链已全部接线 2026-09-16）：UDS / Clipboard / CGWindowList 截图 / FileWatcher / CGEvent 输入 — 验证已自动化（2026-09-22）：macOS 真机执行 `scripts/verify-macos-runtime.sh`（五项对应五套件；VCPKG_ROOT 守卫、triplet 按 arch 自动选、`--build` 强制重建），剩真机跑该脚本 + 人工观察项（CGEvent 需辅助功能授权、录屏授权弹窗、activate 后台激活语义）
- [x] **macOS 三后端装配断链**（2026-09-16 接线，2026-09-17 编译验证闭环）：`cocoa_clipboard.cpp`/`cocoa_window.cpp`/`fsevents_filewatcher.cpp` 均为「实现完整但无工厂导出、全库零消费者」，顶层 `Clipboard`/`Window`/`FileWatcher` 在 macOS 恒落 Null/空 stub（与 Linux 同款缺陷，2026-09-14/15 Linux 侧已修）。三平台源文件补工厂导出（同 linux x11_factory 模式：new + initialize() 后交 unique_ptr，无公开头文件，facade 经前向声明消费）；`clipboard.cpp`/`window.cpp`/`filewatcher.cpp` Apple 分支接入——`window.cpp` 删除 macOS 恒空 stub 段改 `windowBackend()` 统一分派（X11/Cocoa/其余平台恒空）。**接线后实现审查**（首次获得真实消费者，2026-09-17）：① `fsevents_filewatcher.cpp` 回调空壳（P0——构造 FileChange 后仅打日志从不调用回调，watch 假成功）已修：回调载荷移入堆上 shared_ptr 控制块（CF 回调只给 void*，与 map 生命周期解耦）、每 stream 专用串行队列保序、锁内摘条目锁外停流释放、补 Start 返回值检查，全模式对齐 win32/inotify；② `cocoa_clipboard.cpp` MRC 泄漏两处（getHTML NSString/setImage NSImage alloc 无 release，本文件无 fobjc-arc）+ setImage 补 buffer 尺寸校验（防 CGBitmapContext 读越界）；③ `cocoa_window.cpp` 质量可接受（activate 后台激活常无效、hide 实为最小化等语义疑点归真机验证）。验证：Linux 侧全量无回归（vision 构建 Xvfb 真跑 1929/1929、stub 构建通过、主线 CI 全绿）；macOS 分支 CI `cpp-compat` 只编译 proto/transport 不含 lib，**真实编译验证由 Nightly 三平台全量构建完成**——接线提交与修复提交两轮 Nightly 的 macOS job Build 步骤均 success；运行时行为（CGEvent 需 Accessibility 权限、CGDisplay 非 GUI 会话受限）仍待真机
- [x] **Linux 截图装配断链**（2026-09-15 修复）：`screen.cpp` 删除两个恒 nullptr 的 stub Screen 段（有/无 vision 重复），合并为单个 `#if __linux__` 实现接线 X11Capture——`Screen::capture/capture(region)/getPixel/findColor/findColors/getScreen*` 全部可用（Xvfb 验证，j4 全量 1894/1894）；X11Capture 加宽容 X error handler（越界坐标 BadMatch 不再杀进程）。遗留 `findImage`/screenshot JPEG 已于 2026-09-15 解决（见下条 vision 接线）
- [x] **Linux 窗口管理装配断链**（2026-09-15 修复）：`window.cpp` 非 Windows 分支恒空 stub，X11Window 有完整实现却全库零消费者（与 Clipboard/Screen 同款缺陷）。Linux 分支改为经工厂转发 X11Window——`Window::enumerate/find/findAll/getForeground/getTitle/getBounds/setBounds/move/resize/activate/minimize/maximize/restore/close/waitFor/waitClose/isVisible/isValid/isForeground` 全部可用，消费者 `agent.cpp list_windows` 与 Lua `getWindows` 自动受益；语义对齐 Windows 分支（enumerate 只列可见顶层窗口、写操作对无效句柄返回 false 不再假成功）；X11Window 加宽容 X error handler（旧句柄 BadWindow 不再 exit 杀进程）。Xvfb 验证（测试进程直写根窗口 EWMH 属性模拟 WM，新增 4 用例 + flock 串行化，j4 全量 1898/1898）。真实 WM 集成测试补齐（2026-09-15，`X11WmIntegrationTest` 3 用例：minimize↔show 状态翻转、maximize/restore 原子与宽度断言、activate→_NET_ACTIVE_WINDOW 前台轮询 + WM 自维护 _NET_CLIENT_LIST 枚举；环境自起自毁 Xvfb+openbox 子进程，flock 串行化 + display 归属校验 + 失败换号整体重建；openbox 启动窗口静默 600ms 规避外来连接竞态——50ms 高频探测轮询实测 ~40% 间歇失败，静默后 40/40 稳定；压测 40 轮零 flake，DISPLAY=:99 全量 1901/1901）。macOS 同款断链已于 2026-09-16 接线（见上条三后端装配断链）
- [ ] **Linux 宏录制（XRecord）真桌面验证**：Xvfb 的 RECORD 扩展存在但 EnableContext 必然失败（XRecordBadContext）；产品已改为优雅降级（`isRecording()` 回落 false，不再 exit 进程，2026-09-14）。正向捕获闭环已自动化（2026-09-22）：`RecorderX11E2E.*` 端到端用例（XTest 注入→捕获→JSON 精确断言，Xvfb 自动 SKIP 防误报绿），真桌面执行 `scripts/verify-xrecord-desktop.sh` 即完成验证；剩回放（playback）时序手感人工观察
- [x] **Linux 运行时功能自动化验证**（2026-09-14，Xvfb 1280x800x24 真跑，无 X 环境 GTEST_SKIP；j4 并行与串行、带/不带 DISPLAY 四套矩阵全绿 1890/1890）：UDS IPC（`ipc_test`/`unix_socket_channel_test` 真 backend 真跑）；inotify FileWatcher（12 用例）；X11 三件套 + 装配（`platform_x11_test.cpp` 7 用例：createPlatformScreen 显示器元数据 / X11Capture XGetImage 真捕获（全屏=显示器 bounds、区域 64x64）/ XTest 鼠标移动 XQueryPointer 回读 + 按键 XQueryKeymap 状态 / 顶层 Clipboard 装配为 X11/xclip / **xclip 文本回读真跑通过**（xclip 已装；未装环境 setText 优雅失败、测试 skip））。附带修复三个实测暴露的缺陷：xclip daemon 继承输出管道致 EOF 挂起、XRecord 坏环境下 exit 进程、剪贴板并行测试缺跨进程锁（flock 守卫）。桌面人工验证（多显示器/真实键鼠/XRecord 正向路径）待真机
- [x] **Linux OpenCV vision 构建接线**（2026-09-15 完成）：vcpkg 新增 `vision` feature 承载 Linux opencv4（`-DVCPKG_MANIFEST_FEATURES="tests;vision"` 启用，源码编译 ~15-30 分钟；Windows 顶层依赖与 CI Linux job 零变化）；`screen.cpp` 抽 `matchTemplateOnBitmap` 共享 helper（imread → BGRA→BGR → matchTemplate TM_CCOEFF_NORMED → minMaxLoc 阈值判定），Windows findImage 改薄委托（行为不变）、Linux `#ifdef WINGMAN_ENABLE_VISION` 同款接入、无 vision 构建保持 stub 恒 false。测试三面：`X11PlatformTest.ScreenFindImageLocatesDrawnPattern`（Xvfb 根窗口画非均匀红绿图案→capture→save 模板→findImage 找回坐标；纯色模板 TM_CCOEFF_NORMED 零方差未定义，必须非均匀）；`vision_test.cpp` findImage 系改双模式断言（不存在路径 vision/stub 一致 not found + 合成模板 vision 下真匹配不命中，stub 构建 GTEST_SKIP；SaveImage 改真回读断言，废弃裸 return 假 pass）；`screenshot_handler_test.cpp` RPC 端到端（vision 下 JPEG data URI + base64 解码首两字节 FF D8 SOI 硬证据 + width/height/region 回显；无 vision 下错误信封含 WINGMAN_ENABLE_VISION）。验证：build-runtime（vision）全量 **1929/1929**、build-novision（stub）全量 **1902/1902**（`performance_test` 首入 Linux vision 构建；一次 `X11WindowPlatformFeatures` 间歇失败为共享 :99 的既有低频 flaky——单跑通过、复跑全量全绿，与本改动无关，新用例均持 flock 锁或纯文件操作）。遗留：macOS 侧 vision 构建无环境验证

### 配置和协议统一

- [x] 统一 IPC 协议格式（JSON envelope over Named Pipe/UDS，已就位，见 `docs/protocols.md`）
- [x] 统一 Agent-Orchestrator 通信协议文档（见 `docs/protocols.md`，TCP 二进制帧 16B header + JSON body）
- [x] 统一配置文件格式和路径（TOML：`config.hpp` 已支持 `loadFromString(TOML)`/`loadFromFile`/`saveToFile` + `apps/runtime/config/agent.toml` 在用）
- [x] 清理 `pkg/agent/client.go` 已废弃的 dialing Client/Pool（架构违规代码已移除，保留共享协议类型）

### 高级功能

- [x] OCR 支持（引擎已实现：`lib/wingman/src/ocr.cpp` + `ocr_stub.cpp` + test；build flag `WINGMAN_ENABLE_OCR`；可选增强：runtime RPC 暴露 + 多语言）
- [x] ML/AI 支持（引擎已实现：`lib/wingman/src/ml.cpp` + `ml_stub.cpp` + test；build flag `WINGMAN_ENABLE_ML`；`docs/guides/yolo-guide.md`；2026-09-14 补齐脚本推理入口 `ml.run(modelId, inputs)`——Tensor↔ScriptValue 转换见 `module_helpers.hpp`，示例 `examples/lua_scripts/onnx_object_detection.lua`，类型存根 `libs/python/typing/wingman/ml.pyi`）
- [x] 宏系统 UI（引擎 `recorder.hpp` + win32/cocoa/x11；**引擎修复**：低层钩子加消息泵线程 + 线程安全；runtime `macro_handler` RPC：start/stop/play/status/save/load/clear；Tauri 命令；GUI 宏录制页：录制/停止/回放/速度/重复/保存载入 + 侧栏入口）
- [x] 游戏配置模板库（导入/导出/版本管理）：C++ `GameProfileManager` 已具备 export/import JSON+package/createTemplate/scan/validate/version 能力；**补全脚本 API 暴露**——`gameprofile.createTemplate/scan/setProfilesDirectory/getProfilesDirectory/exportJson/importJson/exportPackage/importPackage/delete`；示例 `examples/lua_scripts/game_profile.lua` 现可用

### 用户体验

- [x] 全局快捷键（`hotkeys.rs`：profile 热键 + F5/F6/F7/F12 默认；`reload_hotkeys` Tauri 命令已暴露供 profile 变更后刷新）
- [x] 系统托盘（`tray.rs`：左键切换窗口显隐 + 右键菜单「显示/隐藏」「退出」；Cargo `tray-icon` feature）
- [x] 主题系统（亮/暗双主题：`app.css` CSS 变量 + `stores/theme.ts` 持久化 + TopBar 日/月切换按钮）

---

## 🔵 P3 - 低优先级（工程优化）

## 📅 Agent 分组与批量操作（2026-09-14 完成）

集中管理能力强化：agent 标签从内存态升级为持久化，并新增基于标签/ID 的批量操作。纯 Go server + Dashboard，runtime 不改（决策记录见 `docs/architecture-decisions.md`「Agent Groups & Batch Operations」）。

| 能力 | 实现 | 验证 |
|------|------|------|
| 标签持久化 | `models.Agent.tags`（JSON 文本列）+ `Registry` 注入 `TagStore` 回调接口（`internal/agent` 保持零 DB 依赖，DB IO 一律锁外）；`SetTags` 写穿落库、`Register` 重启恢复/重连保留内存值 | `registry_tags_test.go`（恢复/重连/清洗/失败不回滚）+ `tagstore_test.go`（roundtrip/不重复建行/非法 JSON）+ 跨注册表连通测试 |
| 批量 API | `POST /api/agents/batch/run-script`、`/stop-script`（scripts:run）、`/trigger`（agents:manage）；选择器 agentIds/tags 并集去重（皆空 400、上限 500、无匹配 total=0）；信号量并发 8 逐台下发既有命令，离线记 "agent offline"，部分失败一律 200 + 逐台结果；脚本路径先服务端 Resolve；审计 `script.batch_run`/`script.batch_stop`/`agent.batch_trigger_add`（meta 含选择器与逐台摘要） | `handlers/batch_test.go` 10 用例 + `integration/batch_test.go`（打标→按标签批量运行→批量触发器→RBAC viewer 403/operator 200）；swagger 已再生成 |
| Dashboard 批量 UI | `TriggerFormModal` 从 Monitor 抽取为共享组件（回调式 onSubmit）；Agents 页 rowSelection（权限门控）+ 批量工具栏 3 按钮（选中 0 台禁用）+ 标签筛选 + 结果弹窗（Alert 汇总 + 逐台明细）；`wingman.ts` 新增 batch 服务函数；`access.ts` 新增 `canScriptRun` | jest 235 用例全绿（含 wingman batch 4 例 + 组件 10 例，组件覆盖率 100%）；tsc/eslint/prettier 干净 |

## 📅 2026-09-14 功能修复与覆盖率冲刺

**功能修复（「声明完成但实际不可用」类缺陷，经全库完成度分析裁定）**：

| 缺陷 | 修复 | 验证 |
|------|------|------|
| Go Team/Inbox 三断链：消息入队无投递、断连无清理、无创建具名团队入口 | `MessageNotifier` 实时下发 + `RemoveAgent` 清理/解散通知 + `CreateTeamNamed`；新增 `POST /api/teams` | 新增 `team_delivery_test.go`(11)、`teams_test.go`、`team_inbox_test.go` 集成；Go server 覆盖率 **100.0%**（stmt/branch/func，14 包） |
| C++ ML 引擎无脚本推理入口 | `ml.run(modelId, inputs)` 模块方法 + Tensor↔ScriptValue 转换 + ONNX 示例 + pyi 存根 | `script_modules_test.cpp` +5、`ml_test.cpp` +7 全绿 |
| GUI scripts 页无文件管理（只能启动器式按路径加载） | `script_files.rs` 6 命令（list/read/write/delete/rename/exists + 路径安全校验）+ scripts 页文件树/预览/新建/删除 | Rust 9 单测（本机 glib/gtk/webkit2gtk 齐备，`cargo test` 可跑；Windows 为目标平台）+ vitest 18 用例 |
| runtime 误执行 Lua 字节码无提示 | `resource_loader` `looksLikeLuaBytecode` 检测（Lua 5.x `\x1bLua` / LuaJIT `\x1bLJ`）报可读错误 | `cli_test.cpp` +6 全绿 |

**设计决策（裁定不修，已记录）**：Debugger 501 直连模式为有意契约；GUI 不做内置编辑器（VS Code 统一）；PBKDF2 加密资源两端一致禁用。

**覆盖率**：

| 模块 | stmt | branch | func | 说明 |
|------|------|--------|------|------|
| Go server | **100.0%** | 100.0% | 100.0% | 431 测试函数；`-race` 全绿；vet 干净；含 main() subprocess 重执行 |
| Dashboard (React) | 99.70% | 98.85% | 100% | 220/220 绿；tsc 0 错；jest 95% 门禁通过；余 7 处死防御代码逐条论证 |
| C++ (Linux) | 57.8% | — | — | 支持矩阵不含 Lua/X11（vcpkg 平台限制）；定向改动子集 201/201 绿；Windows 基线 ~90% 需 MSVC 真机 |
| GUI (Svelte) | 见提交 | — | — | vitest + @vitest/coverage-v8（2026-09-14 新装）；30 测试文件 |



- [x] **Go orchestrator 单元测试**
  - [x] rbac（种子/解析/admin 旁路/inactive）+ handlers 用户/角色 CRUD
  - [x] workflow engine（DAG 环检测/步骤校验/selectAgent 负载均衡/executeStep 成功-重试-取消-无 agent/Submit 端到端）
  - [x] handlers：script（创建/校验/保存/读取/运行/日志）、agent（列表/查询/shutdown）、screenshot（广播/校验/超限）、audit（过滤/空态）、debugger（info 直连模式）
  - [x] websocket hub（注册/广播/rooms/agent 事件形状）
  - [x] registry 心跳（注册/列表/状态更新/超时判定离线/Stop 幂等）
  - [x] middleware auth（JWT 校验）
  - [x] 修复 listener_test.go 既有 vet 警告（goroutine 内 Fatalf → channel 回传测试 goroutine）
  - [x] handlers（script/agent/audit/screenshot）补全
- [x] **C++ runtime**：当前 ~1705 测试 / 90%+ 覆盖（保持水准；2026-06-23 已修复 `runtime_tests` 链接缺源、`gtest_discover_tests(... PRE_TEST)` 多配置发现，以及 `WINGMAN_BUILD_TESTS` 自动联动 core/runtime/transport/proto/debug 标准测试集）
- [x] **集成测试**：✅ server HTTP 链路（`TestIntegrationAuthAndPermissionFlow`：JWT 签发→AuthRequired→PermissionRequired admin 旁路/viewer 拒绝→handler，8 断言）；✅ Agent→Orchestrator 跨语言集成（6 文件 3 测试全通过）；✅ GUI↔IPC↔Runtime 跨语言集成（2026-09-14：Rust 侧 spawn 真 runtime 子进程，5 用例走 UDS 帧协议 + 二进制缺失优雅 skip；C++ `ipc_test` 移除 Linux 跳过经 UnixSocket 真跑）
- [x] **性能测试**（Go server 基准）：`go test -bench=. -benchmem ./internal/rbac/ ./internal/workflow/`；rbac 非admin 解析 160µs/689 allocs、admin 旁路 17µs（~9×，验证短路 + 请求级缓存有效）、DAG 环检测 44µs（100 节点）、selectAgent 9µs（20 agent）；C++ 截图/WS 基准待续
- [x] **Go Team/Inbox 三断链修复**（2026-09-14，此前「有 API 无投递」不可用）：① 消息入队后经 `MessageNotifier` 回调实时下发在线 agent（`listener.go deliverInboxMessage`，帧契约与 runtime inbox 模块对齐）；② 断连/RemoveAgent 清理收件箱与团队关系（空团队解散 + 锁外发 `team.member_left`，含重连竞态防护 `hasOtherConnForAgent`）；③ 补 `CreateTeamNamed` 供 handler 创建具名团队；收件箱 FIFO 保序（`InboxMessage.Seq` + 显式排序，修复 map 遍历乱序 flaky）
- [x] **Go 收件箱 FIFO flaky 修复**：`GetMessages` 按 `Seq` 升序输出（原 map 遍历无序导致 3 跑 1 挂）；单测 15 连跑 + 包级 5 连跑 + race 全绿
- [x] **Swagger API 文档**：✅ 全部 47 个端点已添加 swaggo 注解（2026-09-09，16 个 handler 文件 41 个端点）；swag init 生成成功（2026-09-14 新增 teams 端点后已重新生成）

### 文档

- [x] 快速开始指南（`docs/guide/getting-started.md`：环境/vcpkg/编译/第一个脚本 + 三种运行模式）
- [x] Dashboard 使用教程（`docs/guide/dashboard.md`：远程 Dashboard 各页面功能、权限系统、WebSocket 事件）
- [x] Runtime GUI 使用教程（`docs/guide/runtime-gui.md`：本地 Tauri GUI 各页面功能、配置管理、系统托盘）
- [x] 脚本开发指南（`docs/guide/script-development.md`：Lua/Python 对比、核心模块速览、典型模式、运行模式、EmmyLua 调试；本地编辑统一用 VS Code）
- [x] IPC 协议规范、Agent-Orchestrator 协议规范（`docs/protocols.md`）
- [x] 贡献指南（`CONTRIBUTING.md`：架构硬约束、vcpkg 规则、构建验证矩阵、Conventional Commits、PR 流程；README 已链接）

### 构建和部署

- [x] CI/CD：GitHub Actions — C++（Windows 全量+覆盖率 / Ubuntu+macOS proto/transport）、Go（Ubuntu/Windows/macOS vet+build+test-race）、Dashboard（3 OS 打包）
- [x] 自动发布（tag→release）— ✅ release.yml 触发 `v*` tag，调用 build-package.yml 三平台构建 + softprops/action-gh-release@v3 上传
- [x] **Release 上传健壮性**（2026-09-17 修复）：同一晚两次 nightly 在 "Upload to GitHub Release" 步骤抖动失败（Windows `Headers Timeout Error`、macOS 网关 HTML 错误页），rerun 即恢复，且 Windows 超时曾在 release 上留下 3.65 MB 半截资产（完整包 31.6 MB）。build-package.yml 三平台 job 移除并发 softprops 上传（保留 artifact 上传），新增 `publish-assets` 统一 job：`gh release upload --clobber` + 3 次指数退避重试 + **远端资产大小校验**（mismatch 自动重传，防半截资产静默发布）+ release 缺失兜底创建；三平台不再并发写 release，nightly/release.yml 两条调用路径同时受益，release notes 仍由调用方写入。控制流经本地 mock 验证（上传失败重试、size mismatch 重传两条路径）
- [x] 打包分发：Windows zip+InnoSetup+Tauri NSIS / Linux tar.gz+Tauri AppImage/deb / macOS tar.gz+Tauri dmg+app / Docker（orchestrator/server/Dockerfile + orchestrator/dashboard/Dockerfile）

---

## 📅 2026-09-15 X11 WM 测试间歇失败根因排查（X11WmIntegrationTest flaky）

**现象**：自起 Xvfb+openbox 的 WM 集成测试 ~30-50% 间歇启动失败，且呈负载相关的 episodic 簇状（同机 10 个 peer 会话高负载时恶化）。加自愈重试（归属校验+换号整体重建）后压测反而 12/20 失败，遂转入系统性根因排查。

**两种失败形态**（后续被统一解释）：
- **A（REG-FAIL）**：openbox 死于 "Failed to open the display"，但 strace/连接垫片证实 connect() 本身成功（rc=0），失败在 setup 交换；
- **B（NOT-MANAGED）**：openbox 存活且 ~80ms 完成 EWMH 注册（`_NET_SUPPORTING_WM_CHECK` 在），gdb 活体解剖主线程 `g_main_loop_run → ppoll` 完全健康，但 canary 窗口永卡 IsUnmapped；`ss` 显示 openbox X socket Recv-Q=0——**服务器从未投递 MapRequest**。

**排查路径（证伪链，方法可复用）**：

| 假设 | 实验 | 结论 |
|------|------|------|
| 残留 server 应答 display | lock 文件 pid 归属校验通过后仍失败 | ❌ 证伪 |
| openbox 自身不稳定 | 常驻 Xvfb 上 20 连发零崩溃 | ❌ 证伪 |
| 测试二进制问题 | 纯 python ctypes 复刻同样 ~25% 失败 | ❌ 无罪 |
| server reset 竞态 | poll+`-noreset` 反而 28/45 更糟 | ❌ 证伪 |
| 重定向被幽灵持有 | 楔死现场新连接试选 SubstructureRedirectMask 无 BadAccess | ❌ 空闲 |
| **openbox 启动窗口内的外来探测连接** | 三模式**同时段交替**对照（见下） | ✅ 坐实 |

**决定性实验**——三模式交替各 45 轮（消除负载漂移）：poll（spawn 后 50ms 连断轮询注册）18/45 失败；quiet（spawn 后静默 600ms 单查）**0/45**；poll+noreset 28/45。楔死现场探针补齐机制：root 重定向空闲 + canary map_state=0（MapWindow 从未被执行）+ openbox 健康 ⇒ openbox 的连接被 server 摘出了事件分发。

**根因**：`wmRegistered()` 以 50ms 间隔「连接→查属性→断开」轮询，恰好落在 openbox 启动窗口（connect + EWMH 注册 + grab 初始化，亚秒级）内。高负载下外来连接的断开与 openbox 连接建立竞态：踢掉其 setup（形态 A）或将其摘出分发（形态 B——连接活着但永不被投递事件）。`displayAccepts()` 探测在 openbox spawn 之前（无害）；`waitForWmManaging()` 是长连接轮询（无害）——唯一毒源即注册轮询的高频连断。

**修复**（`platform_x11_test.cpp`）：spawn openbox 后静默 600ms 覆盖启动窗口，再以 500ms 低频探测注册；归属校验/换号整体重建自愈保留兜底。验证：真实二进制压测 **40/40 零 flake**（修复前 12/20），DISPLAY=:99 全量 1901/1901，无 DISPLAY 环境优雅 skip。

**方法论沉淀**：① 海森 bug（strace/LD_PRELOAD/gdb 观察均使失败消失）下，单次实验无意义——用**同时段交替 A/B/C 对照**做统计判定；②「进程健康 + Recv-Q=0 + 资源空闲」的组合比逐层猜内部状态更快收敛；③ 第三方组件（Xvfb/openbox）的启动窗口对并发连接敏感是常见 flaky 源，探测一律「先沉降、后低频、长连接轮询」。

---

## 📅 2026-09-16 Go Server CI race 失败修复（FrameListener.teamMgr 数据竞争）

**现象**：CI Go Server (ubuntu-latest) 连续两轮在 race detector 下失败（run 34976763718 / 35034266626），`TestTeamStatusReportUnknownTeamNoop` 报 `WARNING: DATA RACE`；同 workflow 三平台的 windows/macos job 通过（race 时序依赖调度）。

**根因**（64d642b 引入的既有缺陷）：`FrameListener.teamMgr` 字段写读不对称——`SetTeamManager()` 持 `l.mu` 写，但 readLoop goroutine 的 8 个 team/inbox handler（ack / report / join / leave / vote×2 / status_report / broadcast）及断连清理路径全部**裸读**该字段，无任何同步。触发时序即测试自身：发 `team.status_report` 后立即 `SetTeamManager(nil)`——readLoop 异步处理前一条消息读到半写状态的指针。

**修复**（`orchestrator/server/pkg/agent/listener.go`，4c7ddd9）：handler 入口统一经 `GetTeamManager()`（RLock）取指针快照到局部变量，后续全用快照。顺带消除 TOCTOU——原「nil 检查后再读一次」模式下，检查与使用之间 `SetTeamManager(nil)` 可换入空指针（比 race 本身更接近真实崩溃）。

**验证**：`go vet` 通过；`pkg/agent` `-race -count=1` 全绿；nil 防御两用例 `-race -count=20` 压测稳定；全仓 `-race -count=3` 各包通过（`internal/handlers` 单轮 216s、三轮叠加 ~648s 超包默认 600s 超时属压测参数问题而非缺陷，CI 口径 `-count=1` 通过）；推送后 CI 全绿（run 35054666493，七 job 全过）。

**教训**：给可变字段加 setter 并持锁时，grep 该字段的**全部**读点同步收口——「写字段加了锁」常给人已同步的错觉，而读侧散布在多个 handler 里最易漏；`-race` 本地默认不跑，CI 才是唯一防线，本地验证 Go 改动应至少对涉及包跑一次 `go test -race`。

---

## 📊 各模块实际完成度（已校准 2026-09-04，含 Dashboard 审核修复）

| 模块 | 子功能 | 完成度 | 说明 |
|------|--------|--------|------|
| **Runtime (C++)** | 核心引擎 | 95% | screen/input/trigger/vision/btree/macro/human 全部完成 |
| | IPC Server | 92% | Named Pipe + UDS + events.drain（pull 模型，log/trigger/script/connection 事件已接 + dropped 计数） |
| | RPC Handlers | 85% | trigger/system(×2)/script/screenshot/event/macro 已接通（实为这些 handler；无独立 window handler——window 能力经 system 与脚本模块暴露） |
| | Agent Outbound | 100% | 心跳 + 重连 + 指数退避（毫秒抖动）+ 离线 outbox + 连接状态回调通知 GUI |
| | 脚本引擎 | 100% | Lua (sol2) + Python (pybind11) 双语言 |
| **GUI (Tauri)** | 框架 | 100% | Tauri 2.0 + Svelte 5 |
| | 页面实现 | 92% | 六页面可用 + events 轮询；scripts 页已补文件树/预览/新建/删除（2026-09-14） |
| | IPC 通信 | 92% | 连接重试/命令/事件轮询/30s 超时已通 |
| **Orchestrator (Go)** | HTTP API | 92% | auth/agent/script/workflow/audit/profile/settings/status/window/workflow-templates/admin(RBAC) |
| | WebSocket | 90% | Hub + rooms + agent/workflow/debugger 事件广播 |
| | Agent 监听 | 100% | FrameListener TCP 协议 + 心跳 + Team/Inbox 全链路（2026-09-14 修复投递/清理/建团三断链） |
| | 工作流引擎 | 100% | DAG/环检测/持久化/取消/超时/重试/负载均衡/模板/独立步骤类型（wait/condition/screenshot） |
| | 权限系统 | 100% | ✅ RBAC（模型+中间件+API+Dashboard 页面 + PermissionRequired 已接线 8 权限码）；Swagger 47 端点全注解（2026-09-09） |
| | Debugger | 100% | `/api/debugger/info` 直连模式契约 |
| | 测试 | 100% 覆盖 | 431 个测试函数；覆盖率 stmt/branch/func 均 100.0%（2026-09-14）；vet 全清、race 全绿 |
| **Dashboard (React)** | 页面框架 | 95% | 9 页面（+Settings）+ 路由 + admin 子页 access 守卫全覆盖 |
| | 组件实现 | 92% | Agents/Scripts/Workflows/Admin/Login/Profile/Settings/Support 完成；Monitor WS 事件驱动 + 假 UI 全部清理 |
| | WebSocket | 95% | wsService + 自动重连 + 死链检测 + agent/workflow/trigger/script/screenshot 事件全对接 |
| | API 对接 | 95% | 前缀契约断裂已修复并加 26 例回归测试锁定；前后端路由一一对应（见「Dashboard 审核与修复记录」） |

---

## 🗓️ 迭代计划（已校准）

### Sprint A (2周) — RBAC 权限系统 ✅ 已完成 (2026-06-20)
- [x] Role/Permission 模型 + AutoMigrate
- [x] 用户/角色管理 API + PermissionRequired 中间件
- [x] Dashboard 权限页面 (Admin/Users、Admin/Roles) + 路由守卫

### Sprint B (2周) — Runtime IPC 事件推送 ✅ 完成 (2026-06-20)
- [x] runtime → local IPC 事件缓冲 + `events.drain` RPC（EventBuffer + spdlog sink）
- [x] GUI 事件轮询分发（events.ts → logs.addRuntime + triggers.markFired）
- [x] trigger.fired 事件源（TriggerManager::setOnFired → EventBuffer）
- [x] script.state_changed 事件源（StandaloneMode → EventBuffer）
- [x] screenshot.frame 评估：不接入 drain（按需 capture）
- [x] IPC 调用超时（Rust 30s + GUI 错误处理）

### Sprint C (1周) — Dashboard 收尾 ✅ 完成
- [x] Monitor triggers/macros 去 mock（WS 事件驱动 + 空态）、指标空态
- [x] 独立 Settings 页面
- [x] 截图链路（按需 capture + events）

### Sprint D (1周) — Debugger + 增强（完成）
- [x] Debugger 端点明确降级为直连模式（`/api/debugger/info` + 结构化 501）
- [x] 工作流重试策略（指数退避）+ Agent 负载均衡（selectAgent 最少在执行）
- [x] Agent 断线指数退避（reconnectLoop + computeBackoff）
- [x] Agent 离线命令队列（有界 outbox + flush）
- [x] 工作流模板库（4 内置模板 + dashboard 选择器）
- [x] Agent 分组/标签（PUT tags API + Dashboard 标签编辑）

### Sprint E (2周) — 测试 + 跨平台 + 收尾
- [x] Go orchestrator 测试覆盖（handlers/engine/hub/registry/rbac — 355 个测试函数，vet 全清）
- [x] 跨平台 CI 矩阵：C++（Windows 全量 + Ubuntu/macOS proto/transport）、Go（Ubuntu/Windows/macOS vet+build+test-race）、Dashboard（3 OS 打包）
- [x] 文档（使用教程）+ 自动发布（tag→release）— ✅ 已完成，见上方构建和部署章节

---

## 📝 注意事项

1. **架构优先**：修改 runtime/orchestrator 前，先看 `docs/architecture-decisions.md`
2. **IPC 边界**：GUI 只能通过 local IPC 控制 runtime，禁止 runtime 开 HTTP/WS server
3. **远程链路**：Dashboard → Go server → runtime (outbound)，Dashboard 不直连 runtime
4. **Dashboard 位置**：真正的 dashboard 在 `orchestrator/dashboard/`，根目录 `dashboard/` 是无关的 Croupier 副本
5. **vcpkg 约束**：所有 C++ 依赖必须走 vcpkg x64-windows-static
6. **测试基线**：C++ 当前 `ctest -N -C Debug` 可发现 1705 个测试；`WINGMAN_BUILD_TESTS` 自动启用 core/runtime/transport/proto/debug 标准套件，Go server 355 个测试函数（rbac/workflow/handlers/hub/registry/middleware/debugger/integration/security/scripts），vet 全清

---

## 🔗 相关文档

- [ROADMAP.md](./ROADMAP.md) — 项目开发路线图
- [docs/architecture.md](./docs/architecture.md) — 架构设计文档
- [docs/architecture-decisions.md](./docs/architecture-decisions.md) — 架构决策记录（硬约束）
- [docs/API.md](./docs/API.md) — API 文档
- [docs/development-environment.md](./docs/development-environment.md) — VS Code 开发环境
