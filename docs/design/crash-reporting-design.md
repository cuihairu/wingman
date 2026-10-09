# 崩溃采集设计：Crashpad 直集成（简档）

> 设计日期：2026-10-08。范围：桌面 C++ runtime（`apps/agent`）。
> 选型已定：Google **Crashpad**，C++ 直集成；Breakpad 只作历史对照（§2）。
> 实现拆批：文档 → 构建接入 → 库与 runtime 接线 → 验收脚本。

---

## 1. 选型与依赖口径

| 项 | 决定 | 依据 |
|----|------|------|
| 采集器 | Crashpad（上游 gn 构建，不接入） | 独立 handler 进程模型、minidump 生态延续 |
| 构建接入 | [TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake)（Apache-2.0）以 **git 子模块钉版**引入；只取其 `cmake/` 模块文件直驱（本仓 `cmake/crashpad.cmake` 设 `*_git_SOURCE_DIR` 后按其根文件顺序 include），**不走其 FetchContent 根**；crashpad/mini_chromium/lss 以本仓子模块钉死 commit，配置期零网络 | 上游 crashpad 只支持 gn/depot_tools 构建；该包装 fork 是唯一覆盖 Win/mac/Linux 三平台的开源 CMake 包装（Windows 专用替代品 unidentifieddeveloper/crashpad 仅 MSVC 单流，不取）。绕开 FetchContent 根的原因：其 GIT_REPOSITORY 在配置期触网，且单参 `FetchContent_Populate` 在 CMake 4.x 已弃用 |
| 钉版 | crashpad `backtrace-labs/crashpad@7b9686b`（2021-07）、mini_chromium `chromium/mini_chromium@9cdc2a7`（2021-06）、lss `e1e7b0a`（取 [cpp-pm/linux-syscall-support](https://github.com/cpp-pm/linux-syscall-support) 镜像，commit 同上游 chromium.googlesource.com/linux-syscall-support） | 与包装 fork CI 实证组合一致（其 FetchContent GIT_TAG 原值），升级走子模块指针更新 |
| zlib | **vcpkg manifest**（crashpad util/net 依赖） | CLAUDE.md 依赖规则：能走 vcpkg 的走 vcpkg |
| 依赖规则例外声明 | crashpad 本体在 vcpkg 无 port（官方 registry 无 crashpad/breakpad），无法按 vcpkg 统一管理，经用户令批准以子模块钉版接入 | CLAUDE.md 第 3 条禁 FetchContent 临时拉取的意图是禁「绕过 vcpkg 的临时源」；子模块钉版 commit 是确定性来源，且构建期不触网 |
| 新工具链豁免 | 包装 INTERFACE 上追加 `-Wno-error`（钉版代码冻结 2021，新编译器新增告警不作闸门）与 `-include cstdint`（GCC 13+ 不再传递包含 `<cstdint>`，GCC 15 已实证需要） | 子模块源码保持原样不打补丁，豁免集中在本仓 glue |

**架构红线核对**：crashpad_handler 是采集端子进程，不开任何监听端口，不属控制面；runtime 依旧无 HTTP/WebSocket server，符合 architecture-decisions.md Forbidden Changes。

**平台分期**：`WINGMAN_ENABLE_CRASHPAD` 默认仅在 Linux 开（本批真机验证）；Windows/macOS 构建代码就位但默认关，待专门验证轮放开——避免 Windows CI 盲开红闸。

## 2. Breakpad 历史对照（不采用的理由）

- Breakpad 是 Crashpad 的前身，同出 Google，minidump 格式同源。Crashpad 官方文档自述定位为「the next generation of breakpad」：in-process 收集改为 **独立崩溃报告进程**（采集器自身崩溃不影响被采集进程）、写安全（handler 与被采集进程隔离）、支持 Windows 上真实异常（breakpad 在现代 Windows 上靠句柄快照等补丁路径）。
- 本设计不引入 breakpad 代码；唯二例外是**符号工具链**——`dump_syms`（DWARF→.sym）与 `minidump_stackwalk`（栈还原）仍是 breakpad 生态工具，crashpad 官方无对应物，作为开发机本机工具使用（§6），不入仓不入 CI。

## 3. 初始化点

- 位置：`apps/agent/src/main.cpp` 的 `main()` 内、事件日志 sink 挂上之后、命令分发之前——覆盖 `start`/`script`/`crash-test` 等全部命令路径与嵌入式脚本路径。
- 调用：`setupCrashReporting(argv[0])`（`apps/agent/src/crash_setup.cpp`），内部组装 `wingman::crash::CrashpadConfig` 后调 `wingman::crash::initialize()`。
- 语义：`CrashpadClient::StartHandler`（restartable=true, asynchronous_start=false）；**初始化失败只记 spdlog 警告并降级为无采集**，进程照常运行——崩溃采集不得成为可用性单点。
- 编译门：`#ifdef WINGMAN_HAS_CRASHPAD`（沿用 `WINGMAN_HAS_LUA` 的可选库门模式）。

## 4. dump 落盘目录

| 平台 | 默认目录 |
|------|----------|
| Windows | `%LOCALAPPDATA%\wingman\crashes` |
| Linux | `$XDG_DATA_HOME/wingman/crashes`，未设时 `$HOME/.local/share/wingman/crashes` |
| macOS | `~/Library/Application Support/wingman/crashes` |

- 目录由 `wingman::platform::appDataDir()`（本批新增，`lib/wingman/src/platform/` 平台层内实现——平台宏只允许出现在该层，边界守卫规则）拼接 `crashes` 得到；`libs/crash` 与 `apps/` 保持零平台宏。
- 结构（2026-10-09 实测确认，fork 数据库布局）：`settings.dat`（数据库设置，含上传关闭位）+ `new/`（写入中）→ `pending/`（完整落盘；无上传时即最终态）→ `completed/`（上传后）+ `attachments/`；metrics 同目录。完整 dump 判定按非空 `.dmp` 文件，`new/` 下的空壳不算。目录不存在时初始化前创建。
- 保留策略：本批不做自动清理；设计上 `pendingReports()` 只读枚举 `pending/`，清理策略后续按容量拍板。

## 5. 独立 handler 进程打包

- `crashpad_handler` 可执行文件随 runtime 产物分发，与 `wingman-agent` 同目录；`libs/crash` 默认按「本可执行文件同目录」解析 handler 路径（由 `argv[0]` 推导，PATH 直呼场景退化为按 PATH 查找，见 §3）。
- 构建期：`wingman-agent` POST_BUILD 把 `$<TARGET_FILE:crashpad_handler>` copy 到运行输出目录；CI 打包脚本收集运行输出目录即自然带上。
- 许可随附：crashpad / mini_chromium / lss 为 BSD 类许可、cmake 包装为 Apache-2.0，登记 `docs/dependencies.md`。

## 6. 符号表管理

- **构建侧**：debug 信息保留在构建产物（Debug/RelWithDebInfo 天然带 `-g`；Windows Release 为体积默认不带，发布符号包时单开带 `-Zi` 的符号构建）。发布产物可 `objcopy --only-keep-debug` 分离后 strip，分离出的 ELF/debug 对与发布二进制按 `版本+平台+commit` 归档。
- **还原侧**（开发机本机工具，不入仓）：
  1. `dump_syms <runtime-binary>` 产出 `*.sym`（breakpad 工具，构建命令见验收脚本 `scripts/verify-crashpad.sh` 头注释）；
  2. `minidump_stackwalk <dump.dmp> <symbols-dir>` 还原带符号调用栈；
  3. minidump 格式与 breakpad 同源，工具直接可用。
- 符号目录约定：`<symbols-root>/<version>-<platform>/`，`minidump_stackwalk` 支持传多符号根递归查找。

## 7. 上传留位（默认关）

- `CrashpadConfig.uploadUrl` 默认空串：`StartHandler` 不传 URL 且显式 `settings->SetUploadsEnabled(false)`（数据库层面双保险）。
- `uploadUrl` 非空才传 URL 并开上传——为将来自建 collector（crashpad 官方测试用 `generate_test_server`，生产自建按 minidump HTTP POST 约定）预留；是否部署 collector 属数据外发决策，默认关，另行拍板。
- 本批不实现任何网络上传路径。

## 8. 与既有日志/监控打通

- **日志**：初始化成功/失败各一条 spdlog（含 handler 路径与数据库目录）；启动时 `pendingReports()` 查询上一轮遗留崩溃报告，有则 `warn` 逐条列出路径——把「上次崩了」带进常规日志流（复用 `createEventLogSink` 同链路）。
- **注解（annotations）**：`version`（`WINGMAN_VERSION_FULL`）、`platform`（`wingman::platform::platformName()`）、`argv0`——dump 自描述，栈还原时能对上符号版本。
- **监控（后续拍板项，本批不做）**：崩溃报告元数据经 `agent.event` Notify 上报 Go server 供 dashboard 展示——涉及崩溃元数据出机，与 §7 同口径默认关。

## 9. 验收方案（③ 的展开）

1. `wingman-agent crash-test`：新 CLI 命令（独立文件 `commands/crash_test_command.cpp`，遵循命令拆分惯例），初始化 crashpad 后故意空指针解引用（`wingman::crash::testCrashNullPointer()`，写页 0 触发 SIGSEGV）。
2. 断言链：进程异常退出 → `crashes/pending/` 出现完整 `.dmp` → `dump_syms` 产出符号 → `minidump_stackwalk` 栈顶还原出 `testCrashNullPointer` 帧与源码行。
3. 固化为 `scripts/verify-crashpad.sh`（沿用 `verify-xrecord-desktop.sh` 范式：无 crashpad 构建或工具缺失判「未验证」而非通过）。
4. 验收证据（脚本输出摘录）回填本档 §10。

## 10. 验收记录

**2026-10-09（Linux 真机，GCC 15.2 / CMake 4.2.3 / kernel 7.0，`scripts/verify-crashpad.sh` 全链 PASS）**

- 环境：`XDG_DATA_HOME` 隔离目录，`wingman-agent crash-test` exit=139（SIGSEGV）。
- dump 落盘：`$XDG_DATA_HOME/wingman/crashes/pending/<uuid>.dmp`，14992–17392 bytes，连跑 6/6 成功（见下「偶发首跑」注）。
- 符号化（rust 实现工具链，与 §6 breakpad 工具同格式互通）：
  ```bash
  dump_syms --store <symroot> build/apps/agent/wingman-agent
  minidump-stackwalk --human <dump> <symroot>
  ```
  栈顶还原（源码行来自 Debug 构建的 `-g`）：
  ```
  Crash reason:  SIGSEGV / SEGV_MAPERR
  Crash address: 0x0000000000000000
  Thread 0  (crashed)
   0  wingman-agent!wingman::crash::testCrashNullPointer() [crash_client.cpp : 53 + 0x4]
  ```
- 排查记录：早期偶发失败为 `new/` 下 0 字节空壳或无文件——构建后首次运行的冷加载时窗内客户端 5s 等待超时（strace 佐证：PTRACE_ATTACH 被占位即失败属 strace 自身抢占 tracer，非产品问题）；稳态 6/6 全过。若 CI/脚本环境复现偶发，可在验收脚本加一次重跑。
- ptrace 授权链实证：客户端 `prctl(PR_SET_PTRACER, handler_pid)`（yama ptrace_scope=1 下必需）+ handler 双 fork 脱离进程组，均按设计工作。

## 11. 来源

- 上游与包装：[crashpad 源码（github 镜像）](https://github.com/chromium/crashpad)、[crashpad 文档（doxygen）](https://chromium.googlesource.com/crashpad/crashpad/+/master/doc/)、[TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake)（README 用例、CMakeLists 钉版值、Apache-2.0）
- 客户端 API：`client/crashpad_client.h`（StartHandler）、`client/crash_report_database.h`（Initialize/GetPendingReports）、`client/settings.h`（SetUploadsEnabled），以 vendored 源码为准
- Breakpad 对照：[breakpad 仓库](https://chromium.googlesource.com/breakpad/breakpad/)、[minidump_stackwalk](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/processor_design.md)
- 仓内：docs/architecture-decisions.md（Forbidden Changes）、scripts/check_platform_boundary.sh（平台宏边界）、docs/dependencies.md（依赖登记）
