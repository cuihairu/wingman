# Wingman 项目待办事项

> 最后更新: 2026-10-04
> 状态: 收尾阶段（P0/P1 全部完成；2026-09-14 完成「声明完成但实际不可用」类缺陷修复——Go Team/Inbox 三断链、C++ ml.run 推理入口、GUI scripts 页文件管理——并推进测试覆盖率，见「2026-09-14 功能修复与覆盖率冲刺」；同日新增 agent 分组与批量操作，见「Agent 分组与批量操作」）；移动端 Android A1/A2/A3 已落地（含 A3 可靠性与受限设置引导，见「2026-09-30 Android Agent 现状登记」及其后两条 A3 实施条目）、A3-P2 与 A4 未排期；2026-09-30 平台验证收口——macOS/XRecord（runner+Xvfb）与 Android API 34 模拟器全链路验证，模拟器腿抓出三处阻断级缺陷已修（见同日「Android 模拟器验证」条），剩余真机项见该条清单；同日 C++ 覆盖率扫描收官——v13 基线清账 TOTAL 90%，余量全部带论证登记（见「C++ 覆盖率扫描收官」条）；2026-10-01 ScriptManager 状态机死锁环与 stop 数据竞争修复（2026-09-29 登记的独立任务落地，见同日条目）、task pause/resume 落地（见同日条目）；2026-10-02 wingman agent 一键安装三件套落地（install.sh / install.ps1 / agent 构建矩阵接入 nightly 分发，见同日条目）；**2026-10-04 架构决策加载与任务重排——四层模型/Capability/Execution ADR 四条落地文档（0c2bbe6），代码侧 Execution v1 + Capability v1 落地（4d4b6c3，见下方同日条目）；任务队列按新架构决策重排（P0 ADR 闭环：runtime capabilities 上报 / Execution v2 batch+workflow 接线 / Dashboard 视图；P1 缺口清单按原语边界归位：文件 IO → hotkey → notify tray）**

---

## 2026-10-04 架构决策加载与任务重排：四层模型 / Capability / Execution ADR 四条 + 缺口清单按新架构归位 + 过时条目清理

### 架构决策（docs/architecture-decisions.md 追加四条，0c2bbe6）

- **四层模型**：Automation Core（lib/wingman，不得感知 Agent/Dashboard/Workflow/User/Team）→ Runtime（Execution Plane）→ Agent（身份 + 能力集）→ Control Plane（Go server）；Control/Execution Plane 硬边界。
- **原语边界**：Trigger（事件→动作）< Script（过程）< Behavior Tree（实时决策）< Workflow（跨 Agent 长生命周期）< Team（协同）；轻者优先，不得跨级叠造。
- **Capability System**：dotted 词汇表（screen.capture / input.touch / ml.onnx …13 项，server internal/agent/capabilities.go）；agent.register 上报 → 注册/持久化/展示；workflow 步骤 requires 调度匹配；未知能力存而标 unverified（不拒绝）。
- **Execution 平台核心对象**：统一执行对象 + 状态机 pending→queued→running→{succeeded|failed|cancelled|timeout|lost}；Artifact 一等子对象；executions 表是日志/审计/Artifact 的统一挂载点；**统一前不得再新增执行形态功能**。
- 收敛表（docs/architecture.md，2026-10-04 快照）：Agent/Workflow/Control Plane/Execution Plane ✅；Capability/Execution/Artifact 🔶 演进中。

### 代码侧落地（4d4b6c3，本日）

- **Execution v1**：models.Execution + AutoMigrate（executions 表）；GET /api/executions（page/status/agentId 过滤 + /:id 详情，登录即可，与 /api/agents 同级）；run_script 下发前落 running → 命令返回后终态（succeeded/failed + finishedAt + Result JSON 摘要），审计补新规范 executionId；响应体保持 `executionId: scriptName` 兼容不变。
- **Capability v1**：KnownCapabilities 词汇表；agent.register 解析 capabilities → Registry 内存持有 + CapabilityStore 持久化（与 TagStore 同构，DB IO 锁外、重连保留内存值）；ToJSON 输出 capabilities / unknownCapabilities（词汇表外项原样展示）。
- **WorkflowStep.Requires**：selectAgent 先按 platform（空值归一 desktop）+ capabilities 全命中过滤，再负载均衡；无候选错误列出在线节点与缺失项；无要求时沿用既有文案（既有断言不破）。
- **测试**：agent（持久化/恢复/重连保留/未知标记/register 透传 6 例）、handlers（capstore roundtrip、run_script 生命周期成功/失败终态、查询分支 401/400/404）、workflow（requires 平台/能力/组合/错误诊断 5 例）。全量 `go test ./...` 全绿，vet 全清。

### 按新架构决策重排的任务队列（2026-10-04 起）

**P0 — ADR 闭环（收敛表 Capability/Execution 🔶 → ✅）**

1. ✅ **runtime 侧 capabilities 实际上报**（Capability System 闭环）：desktop runtime 于 initRemoteClient 注入 `platform=desktop` + 词汇表对齐的能力清单（ml.onnx 仅 WINGMAN_ENABLE_ML 构建声明；不虚报 input.touch/screen.stream，AgentLoopbackTest.RegisterReportsDesktopCapabilityVocabulary 钉定）；Android agent 经 capabilitiesJson 配置已在位。
2. ✅ **Execution v2：batch + workflow 接线**（8a9997b）：batch 接口（handlers/batch.go fan-out 后串行落库、offline 跳过与单发语义对齐）与 workflow 引擎步骤（每轮 attempt 一条，WorkflowID/StepID/AgentID 挂载；errStepTimeout 哨兵 + errors.Is 区分 timeout/cancelled 终态）；v1 已接线 run_script。
3. ✅ **Dashboard 视图**（aa2202f + 本笔）：executions 页（ProTable 列表/status 过滤/详情 Drawer：result 展开 + artifacts）+ Agents 页 capabilities / unknownCapabilities 展示（server 字段已出）。v1 范围：executions 页只读；screenshot 步骤未接 Execution、wait/condition 非 agent 下发不接（记入后续）。

**P1 — 缺口清单按原语边界归位（Runtime 能力层，轻者优先）**

4. ✅ **文件 IO 工具**（file 模块：本笔落地——`script/modules/file_module.cpp` 十三函数全部 std::filesystem 真实现非 stub（read/write/append/exists/isFile/isDir/size/move/copy/remove/removeAll/mkdir/listDir），move 带跨文件系统 copy+remove 回退；file_module_test 16 用例；filewatcher 已提供监控腿）。
5. ✅ **hotkey 模块**（本笔落地——`wingman/hotkey.hpp` + `script/modules/hotkey_module.cpp`：轮询式全局键态监听，后台线程按固定间隔读 IInput 键态、主键+Ctrl/Shift/Alt 组合上升沿触发回调；组合文本解析（大小写不敏感、Win/Meta 显式拒绝）；注册/注销自动启停线程、回调锁外触发按 ID 复核；脚本侧 register/unregister 带 callableThreadSafe 门控（Lua 拒收走 hotkey.error 事件）；9 用例 + 全量回归 2085 passed。v1 限制：轮询间隔内按下又弹起可能漏检；macOS 权限行为待真机）。
6. **notify tray**（本笔落地——脚本层 `trayShow()/trayHide()/traySetBadge(text?)/traySetTooltip(text?)` 四函数：不直达系统托盘，只向 EventHub 发 `notify.tray.{show,hide,badge,tooltip}` 意图事件（source "notify"，附 timestamp）；runtime 侧 `notify_bridge` 订阅转投 EventBuffer（method 去 "notify." 前缀 → `tray.*`，installNotifyBridge 按 subscriptionByName 幂等防订阅泄漏），GUI events.ts 轮询分发 → tauri `tray_control` 命令 → TrayIcon set_visible/set_badge_label/set_tooltip。分层诚实：托盘本体归 GUI，无 GUI 附着时事件仅入有界缓冲（EventBuffer 1000 条）。v1 限制：badge 仅 Windows/macOS 生效，Linux 底层 no-op。测试：notify_module_test 4 用例 + notify_bridge_test 2 用例 + events.test.ts 3 用例）。

**P2 — 收敛与登记**

7. ✅ 收敛表更新（本笔）：docs/architecture.md 核心概念表 Capability/Execution 🔶→✅（v1 范围内嵌）；ADR 两条 Status 段更新为 "v1 landed (2026-10-04)" 并登记 v1 缺口（Capability：模块级词汇不入词汇表；Execution：screenshot 步骤未接、wait/condition 服务端内部步骤不接、artifact 存储未建仅记引用）。Artifact 维持 🔶。
8. （登记不排期）WebhookSender worker、UIA 后端、XRecord 真机、NullClipboard 等结构性盲区，见 2026-09-30 覆盖率收官登记与 development-todo.md 引用。

### 过时条目清理

- 注意事项「测试基线 #6」：C++ `ctest -N` 计数 1705 → **2509 注册**（2026-09-30 收官：2478 passed + 31 环境 skip，TOTAL 90%）；Go server 测试函数 355 → **597**（同日实测 `grep -rc '^func Test'`）。
- 底部 Sprint A–E 档案段为历史完成记录，保持原样不改（改动会破坏档案口径）。
- 状态行「最后更新」滚动至 2026-10-04；全文无与硬约束冲突的遗留条目（「Dashboard 不直连 runtime」「runtime 禁 HTTP/WS server」等注意事项即是硬约束本身，保留）。

---

## 2026-10-02 wingman agent 一键安装三件套（install.sh / install.ps1 / agent 构建矩阵）

一条命令（`curl … | bash` / `irm … | iex`）从 nightly 分发面安装 wingman-agent 单二进制，细节详见 CHANGELOG 同日 feat 条目：

- **agent 构建矩阵（build-agent.yml，新）**：linux/macos/windows × x64/arm64 六腿只构建 `wingman-agent` 打 `wingman-agent-<ver>-<os>-<arch>` 包（整包矩阵不动）；publish-assets 同款重试 + 远端大小校验；nightly.yml 接线（job + 资产表 + cleanup 正则兼容 `wingman-agent-*` 前缀、同 sha 一组保留）；armv7 无 runner 不产出，安装侧明确报错。
- **install.sh（Linux/macOS）**：OS/架构自动检测（不认识的输入明确报错含 issue 指引）、零依赖 JSON 解析（grep/cut 提取 browser_download_url）、版本选择=release 倒序取第一个含本平台 agent 资产（正式版优先、nightly 兜底）、HTTP 失败按 403/000/404 分型报错、幂等覆盖升级（rm+install 防 ETXTBSY）、`--version` 验证（失败 ldd 列缺库）、`--service` 注册 systemd user unit / launchd。
- **install.ps1（Windows，PS 5.1+）**：Desktop 视为 Windows + TLS1.2 强制、param 惰性求值（非 Windows 会话 LOCALAPPDATA null 会先抛——真跑抓到）、查询统一 IWR+ConvertFrom-Json（**pwsh 7.4 `Invoke-RestMethod` 对 JSON 数组返回嵌套不枚举、5.1 平铺——本机实测分叉后统一**）、用户 PATH 幂等追加（2047 上限防御）、`-Service` 注册 Windows 服务（Session 0 警告）。
- **cli 验证入口**：`wingman-agent --version`/`-V`/`version` 三拼写等价（+1 测试，全树注册 2518 → 2519）。
- **验证**：install.sh 防御分支全实测（--help / armv7l / SunOS / 未知参数 / 真 API 无资产 / 404）+ shellcheck 干净；ps1 AST 解析 + Linux 防御分支真跑 + 带 token 真 API 遍历（命中 nightly linux-x64 资产、tag 提取正确）。**边界**：ps1 Windows 主流程未真跑（无 Windows 机），矩阵资产上线后本机 Linux 真装走查另行汇报。

---

## 2026-10-01 wingman.task pause/resume 落地（缺口清单条目，Lua/Python 双侧同步）

`docs/development-todo.md` 缺口清单「pause(taskId) / resume(taskId)（未实现）」落地，细节详见 CHANGELOG 同日 feat 条目；清单中「API 形状统一」「命名风格统一」两项范围模糊，按任务指示留后：

- **协作式暂停四检查点**：开工前驻留不执行、work 完成后扣住结果不落账（resume 才提交）、重试间隙停试、超时时钟暂停期间停走（deadline 顺延暂停时长）。仅 pending/running 可暂停、仅 paused 可恢复。
- **pausedFrom_ 状态恢复**：resume 恢复暂停前状态（开工前→pending、执行中→running），避免「worker 启动前 pause+resume」使 execute 入口误判重入、work 永不执行。
- **双侧落地**：C++ ModuleDescriptor 注册（Lua 即得）+ Python 自动绑定零运行时代码，仅补 task.pyi；新增 `task.paused`/`task.resumed` 事件；wait() 对 paused 只等待不改写、cancel 仅状态转换时发事件、shutdown 先 cancel-all 再 join。
- **测试与文档**：TaskModuleTest 42 → 47 例（5 新例 ×10 连跑全绿）；docs/api/task.md 补暂停/恢复章节与事件表；development-todo.md 勾选。全量两树 ctest **2518 注册两树 100% 全绿、0 failed**。

---

## 2026-10-01 CI「C++ Linux (full tests, Python engine)」sourceforge 单点修复（libuuid vendor 预缓存）

CI「下载脆弱性」独立小任务收口，根因/验证详见 CHANGELOG 同日 ci 条目：

- **恒红根因**：依赖图 python3 → libuuid（`vcpkg_from_sourceforge`）在 SF 事故窗口源站 522 + 全镜像坏内容，configure 三次重试全灭；全图 28 端点核查 sourceforge 托管仅此一个。
- **修复**：vendor tarball（SHA512 与 baseline portfile 钉值一致）+ `seed-vcpkg-downloads.sh` 预置 vcpkg downloads（先 `sha512sum --strict -c` 后拷贝），vcpkg 安装时按 portfile SHA512 独立复验、命中零网络；本地 `--no-downloads` 禁网实证全链通过。
- **登记不修**：Linux files 二进制缓存 ABI 恒 0 恢复 + GitHub cache key 不可变致 downloads 化石（每轮全量源码构建）——滚动 key 只摊薄首建、首建仍需本预缓存；Windows 走 NuGet 不受影响。

---

## 2026-10-01 ScriptManager 状态机死锁环 + stop 数据竞争修复（2026-09-29 登记的独立任务）

登记缺陷（2026-09-29 覆盖率扫描条「结构性不可达登记」）落地修复，根因/修法/验证详见 CHANGELOG 同日 fix 条目：

- **死锁环解扣**：runScriptInternal 从不赋值 running（唯一赋值点在 resumeScript，resume 前置 paused、pause 前置 running）——pause/resume 成功腿、批量计数增量、running/paused 状态映射全不可达（真机同样）。修复：执行线程起动后迁 running，全链 start→running→pause→paused→resume→running→stop 可达；pause 明确为簿记态（无引擎级暂停钩子，底层执行继续）。
- **stop 数据竞争根治**：旧 stopScript 对执行中引擎直接 `shutdown()+reset()`（shutdown 销毁执行线程正在使用的 lua_State——跨线程 UAF）。修复：协作停止（ScriptInfo 新增原子 stopRequested，等待循环 ≤50ms 收尾、终态 loaded）+ 只释放 manager 侧引擎引用（shared_ptr 保活，线程跑完自然销毁，同超时路径 detach 模型）。
- **连带收口**：重入防护（running/starting/paused/stopping 重跑一律拒绝——旧「先 stop 再重启」会双执行线程共享引擎）；unload/reload/checkReload 的先停判定扩到全活跃态；线程创建失败不再卡死 starting；新增 runGeneration 运行代号防并发 reload 重启时新旧两代运行互写状态。
- **回归钉 4 例**：ScriptManager 级全链 + 停止后重跑（script_manager_exec_coverage_test）；StandaloneMode 级全链（含事件序列）+ 批量计数增量（standalone_mode_coverage_test，旧「结构性不可达登记」注释同步改写）。
- **登记未做（后续独立项）**：引擎级协作停止钩子（Lua 指令钩子 / Python trace·PendingCall）——被停/超时脚本本体仍继续在后台跑到自然结束；callFunction 的并发安全调用通道（同步模型下 running 窗口引擎被执行线程独占，当前无生产调用方，维持 running 前置不变）。

---


> [本文档已于 2026-06-21 依据代码实际状态重新校准。之前的版本严重低估了 Go orchestrator]
> （工作流引擎、Agent 心跳、审计均已实现）并错误描述了 dashboard 位置。

---

## 2026-09-30 C++（Linux）覆盖率扫描收官：v13 基线剩余缺口三分类清账——13 例 + 1 断言，可测缺口归零、结构性盲区带论证登记

任务口径：v13 基线（miss 1252）剩余缺口逐文件三分类——可测 now / 结构性不可达（逐行论证）/ **行归因伪影**——后两类登记不硬凑。伪影判定实证：transport 362/368/503/521 报 miss 但其错误文案由既有通过用例（TcpListenPortConflictFails / UdpErrorBranches）直接断言，重跑该 2 用例后 miss 列表逐行不变（排除陈旧 gcda），定性为 gcov 对多行 braced-init 内层行/收尾行的零归因。**C++ 覆盖率扫描任务至此收官**（Go 侧 100%、前端 100%/99.7%、C++ TOTAL 90% 且余量全部带论证登记）。

- **新增 13 例 + 1 断言（5 文件）**：① `glue_modules_coverage_test.cpp`（新）3 例——debugger/orchestration stub 契约（start 恒 false、breakpoint 串 `"a.lua:12"`、`DEBUG_BREAK_HERE`、orchestration 三 stub null/false/空数组）+ security 直通契约（hashString 64-hex 稳定、generateRandomString 长度、filterSensitive 整段替换 `***`），三模块胶水体此前零驱动；② transport_inbox +1——udpSendTo 非 IP 地址（asio::make_address 抛 → catch → false）；③ unix_socket_channel +3——server/client socket() 创建 EMFILE 注入 ×2（新共享注入器 `fd_exhaustion.hpp`：RLIMIT_NOFILE soft 压「当前占用+4」逐个占满 /dev/null，毫秒级窗口，探测先行不成立即 GTEST_SKIP）+ 断连后重启接收语义（listenFd_ 首个 accept 后关闭、serverAccepted_ 不复位 → 重启即早退不挂，钉住既有语义）；④ platform_x11 +1 断言——findByClassName 无命中腿返回 NullWindowHandle；⑤ cli_test +6——script 命令执行面（成功/带参 env/运行时错误/不可编译/目录不可读源）+ build 命令 stub 全链（resolveStubPath 命中 → 图标日志 → 选项装配 → create_directories → Linux ELF 嵌入不支持 → 优雅退出 1）。
- **覆盖率**：TOTAL **90%（15238 → 15290/16931，+52 真覆盖行）**；transport_module 89→**90%**、unix_socket_channel 94→**97%**、x11_window 99%（余 1 行伪影）、script_command 13→**83%**、build_command 49→**87%**、debugger/orchestration/security 胶水体全驱动（余量全为收尾行伪影）。
- **结构性不可达登记（34 行，逐行论证）**：transport 17（UDP 阻塞 receive_from catch——close 不唤醒阻塞 recv、UDP 无 shutdown；创建后句柄 null 防御 ×2——工厂单调计数永不失败；`start()` 失败腿——transport.hpp 内联恒 true）；usc 6（listen() 失败防御——fd 已持有无注入口）；script_command 5（loadScript TOCTOU 双检 31-32——loadScript 仅 `!exists` 失败而命令已前置检查；48-50 no-throw catch）；build_command 6（86-88 PE-only 成功腿——Linux ELF 嵌入明确不支持；95-97 no-throw catch）；ml_module 34 行沿用前轮登记（onnxruntime 为 vcpkg Windows 平台专属依赖，Linux gate 恒跑 ml_stub）。
- **伪影登记（30 行）**：transport 13（braced-init 内层/收尾行，其中 4 行错误文案被既有用例直接断言）+ usc 1（catch 收尾行）+ debugger/orchestration/security 15（各导出函数 `}, "sig"});` 收尾行）+ x11_window 1。
- **flake 收口（2 处）**：收官门禁首跑 load 68-104 抓红 `ClipboardModuleGlue.HtmlImageAndFilesBehavior`——同用例 HTML/text 段均已按轮询纪律改写，唯 files 段漏网（setFiles 读回与 clear 后 hasFiles 两处 t=0 直断，与 xclip 异步接管窗口竞争）。改正向轮询后单测 ×20（load 93）全绿，0.33-0.39s 慢轮次即轮询真实吸收竞态的证据。
- **无真缺陷暴露**：全部新驱动路径行为符合既有契约，本轮零生产代码改动。
- **验证**：全量两树 ctest（xvfb-run 串行 + --timeout 300）**2509 注册 = 2478 passed + 31 环境 skip + 0 failed**（两树一致，含 +13）；gcovr TOTAL 90%（历轮口径）。零 Go/JS 改动。
- **推送后 CI 插曲（同批收口）**：C++ Windows job 唯红本批新文件的 glue 三用例——`findModuleFunction` 在 `getAllModules()` 临时 vector 上 `return &f` 悬垂指针（Linux 释放块侥幸未复用全绿，Windows Debug 堆加毒假红）；按全库既有 `getModule`-by-value 模式修复 + 补空指针防护，门禁重跑全绿重推。同时修 `run-windows-coverage.ps1` 盲区：失败时只回显日志尾 40 行，断言明细在日志中段永远看不到——补 per-test FAILED 行前 15 行上下文（详见 CHANGELOG fix 条目）。

---

## 2026-09-30 C++（Linux）覆盖率扫描续：Clipboard 门面与 X11 后端——故障注入 3 例，可触达缺口归零、结构性盲区全登记

任务基线（clipboard.cpp 58.1%/26 行、x11_clipboard 71.8%/37 行）为 1f0f976 前旧树口径——彼时 Linux 文件列表用例按能力守卫 SKIP；修复落地后复采基线：**clipboard.cpp 58%（36/62）、x11_clipboard.cpp 74%（98/132）**。基线先验证 X 环境存活（Xvfb :98 在跑、xclip/gcovr 齐备）。

- **测试**：新增 `lib/wingman/tests/clipboard_fault_coverage_test.cpp` 3 例（CMake 接 `UNIX AND NOT APPLE` gate——x11_clipboard.cpp 本就不在 Windows 编译面；无 X 环境语义自洽：坏 DISPLAY 注入不依赖真 X，fd/fork 注入在单例未初始化时同样走 false/空契约）：① `UninitializedBackendDegradesByContract`——坏 DISPLAY（:9999）经工厂直连取未初始化实例，全接口降级契约逐项断言（写恒 false、读恒空、isEmpty true、getBackendInfo().isInitialized=false；覆盖 initialize 失败分支）；② `PipeExhaustionFailsGracefully`——RLIMIT_NOFILE soft 压到「当前占用+4」再逐个占满（直接占满默认百万额度是秒级窗口、全量跑实测 1.4s，压限后毫秒级），覆盖 setText/getText 两条 pipe 失败防御分支；③ `ForkFailureFailsGracefully`——/proc 扫描统计本 uid 进程数后 RLIMIT_NPROC 压限（按 uid 计数），fork 探测确认 EAGAIN 前提成立，覆盖两条 fork 失败防御分支。
- **注入纪律（共享机）**：三条注入全部前置探测（pipeStillFails / forkStillFails / rlimit 可用性），前提不成立一律 GTEST_SKIP 不误报代码失败；剪贴板 flock 按纪律持有（注入失效走通成功路径时不与并行进程竞态）；rlimit/fd/env 改动全部 RAII 恢复（含「未成功 apply 不恢复」防把限额写成垃圾值）。
- **在案 flake 收口（断言方向 5 处）**：全量门禁首跑在 load ~27 下抓红 `ClipboardTest.IsEmpty`——`setText` 后 `EXPECT_FALSE(waitFor(isEmpty))` 等价于要求「首次读取即为终态」，而 clear 建立的空态会一直可读到 xclip daemon 异步接管为止，t=0 读取必然与接管窗口竞争。同型 5 处（HasText/HasHTML/HasFiles 的 clear 后、Clear/IsEmpty 的 setText 后）一并改为正向等待终态 `EXPECT_TRUE(waitFor(终态谓词))`：断言契约不变（终态最终出现即通过、始终不出现即失败），只去掉不可达成的「t=0 起持续为终态」硬要求；`clipboard_poll.hpp` 头注释登记该方向纪律。改后 Clipboard 全家族 + 故障注入 19 例 ×3 轮全绿。
- **覆盖率（gcovr 行）**：`x11_clipboard.cpp` 74%（98/132）→ **84%**（112/132）；`clipboard.cpp` 维持 58%（36/62）——**两文件可触达缺口归零**（余量见下登记，均结构性）。
- **结构性盲区登记（46 行，不写假用例）**：x11_clipboard 子进程分支 19 行（63-65/70-74/78/81 与 119-121/124-127/131/134）——fork 后 exec(xclip) 替换进程镜像或失败 _exit(1)，子进程 gcov 计数器永不落盘：父进程行可故障注入、子进程行是工具不可观测（路径本身每次 setText/getText 真实执行）；226 为 getAvailableFormats 收尾行归因伪影（函数体 222-225 已全驱动）；clipboard.cpp 26 行 = NullClipboard 类体（13-46）+ 工厂 null 兜底（88）——两平台工厂（createX11Clipboard/createCocoaClipboard）恒无条件 new+initialize+return，该回退在 Linux/macOS 恒不可达，Windows 侧 #else 分支不参与编译（直接 typedef Win32Clipboard）。
- **无真缺陷暴露（如实登记）**：三条防御路径行为全部符合既有契约（优雅 false/空、不崩、未初始化如实上报），本轮零生产代码改动。
- **验证**：新 3 例 ×5 连跑稳定（102/100/10ms）；全量两树 ctest（CI 口径：xvfb-run 串行 + --timeout 300）**2496 注册 = 2465 passed + 31 环境 skip + 0 failed**（两树一致，含本轮 +3）；gcovr TOTAL **90%（15238/16931）**（历轮口径剔除 vcpkg 头与 tests/，上轮 89.9%）。首跑插曲：两树各挂 1 例 `AgentLifecycleTest.ApplyRemoteConfigWhileRunningReportsReconnectAndPersistFailure`——Android 模拟器任务遗留 host Go server 仍占 :8888、打穿其「初始地址不可达」前提，杀遗留进程后恢复（与本轮零生产改动无关）；同轮 load ~27 抓出 ClipboardTest.IsEmpty 断言方向 flake，同型 5 处已收口（见上条）。零 Go/JS 改动。

---

## 2026-09-30 Android 模拟器验证（API 34 AVD 全链路）：三处阻断级缺陷修复；「无真机可验证项」四条全部销账

任务口径：真机验证两项里能在模拟器/runner 上验的全部验掉，确需物理硬件的如实列清单。macOS 腿（Actions macos-latest runner）与 XRecord 腿（本机 Xvfb :98）已于前两条勾销，本条收 Android 腿：**本机 Android SDK 模拟器 emulator-5554 = AVD test34（API 34，sdk_gphone64_x86_64，userdebug 可 adb root）**，host 侧 Go server（agent 0.0.0.0:8888，经 10.0.2.2 loopback alias 回连）+ `WINGMAN_AGENT_TOKENS` token 白名单口径复现生产链路。

- **三处阻断级缺陷（修复提交 95570d7，全部模拟器实测抓出）**：① MainActivity 启动即崩——AppCompatActivity + androidx AlertDialog 必须 Theme.AppCompat 后代主题，framework Theme.Material 在 setContentView 抛 IllegalStateException，**UI 自 A1 起从未在任何构建上真正启动过**（此前只验过 `am startforegroundservice` 服务路径，没起过 Activity）；② API 34 纯核心启动 100% 崩——startForeground 无条件带 mediaProjection 类型，未取得投屏授权（appop project_media 未授予）即 SecurityException，改为纯核心 dataSync / 投屏分支叠加 mediaProjection；③ 崩溃闹钟腿自 A3 落地从未触发——PendingIntent.getForegroundService 指向 BroadcastReceiver，系统按 service 组件解析恒 not found（被 START_STICKY 兜底掩盖），改 getBroadcast 后 am crash 实测触发。
- **验证全链路（修复构建）**：安装 → MainActivity 稳定前台（修复①）→ ACTION_START → FGS dataSync（修复②）→ nativeStart=1（配置 prefs→JSON 全链）→ TCP 建立 → server `[Registry] Agent registered`（16 次注册含断网自动重连）；`am crash` → START_STICKY ~1s 拉起 + 闹钟腿（修复③）幂等重跑 nativeStart，crashCount/crashLastAt/lastCrashMessage 落 prefs；开机自启正向 BOOT_COMPLETED（prefs autoStartOnBoot=true + 重启 → receiver 触发 → nativeStart → server 重注册）与 MY_PACKAGE_REPLACED（`adb install -r` 覆盖安装 → 同链路 ~10s）均通；开机自启默认关（干净安装不勾选 → 重启无进程，既有验证）；restricted-settings 脚本真机路径 check(FAIL/default) → allow(PASS 幂等) → status(allow) → revoke(PASS) → status(default) 真实 exit code 语义。
- **前条「无真机可验证项」四条全部销账（A3 可靠性条登记的 BOOT_COMPLETED/MY_PACKAGE_REPLACED 广播到达、豁免名单内 startForegroundService、覆盖安装自启、勾选后重启全链路）**：模拟器已逐一实证。
- **误诊澄清（登记，防再犯）**：中途曾据「广播已发出 + receiver 在 query-receivers 解析列表 + 60s 内无进程」误判 exported=false 拦系统广播——实为共享机高负载下广播队列积压，BOOT_COMPLETED 发出到 receiver onReceive 实测延迟 **47s**；复查（完整 logcat 缓冲 + 更长等待窗口）确认 `exported="false"` 不拦系统保护广播送达，CHANGELOG 既有口径正确，manifest 零改动。教训：验证「广播未送达」必须等过队列积压窗口且不得先行 `logcat -c` 抹证据。
- **加固登记（未修）**：冷启动 13s 窗口内 Activity 焦点被夺（同模拟器他 App 抢焦点 → uidState SVC）时 `startForegroundService` 链路抛 `ForegroundServiceStartNotAllowedException` 未捕获 → 进程崩溃。单用户真机常规流程不踩（共享模拟器多会话环境特有触发面）；后续加固方向：捕获该异常按退避重试或降级提示。
- **剩余真机项（如实清单，均需物理硬件）**：macOS TCC 人工观察三项（CGEvent 辅助功能授权 / 录屏授权弹窗 / activate 后台激活语义，指引 docs/guides/manual-verification.md）；XRecord 回放手感；Android 真机保活与厂商后台限制逐机型实测（五厂商 ROM ⋮ 菜单验收）；MediaProjection 真机授权弹窗流（模拟器未走用户授权流）。
- **验证**：`gradle :app:testDebugUnitTest` 22/22 全绿；本地模拟器构建（abiFilters x86_64，本地 gradle.properties/build.gradle.kts 改动不入库）；零 Go/C++ 改动（门禁沿用本轮同代码态已验结果）。

---

## 2026-09-30 C++（Linux）覆盖率扫描续：ResourceLoader 实例接口 68% → 87%（7 例）

todo 仅剩 macOS 与 Linux 真机验证两项（本机无真机、不可做），按既定规则转覆盖率缺口。双端对账：Go 侧全包覆盖率复测（`go test -coverprofile -count=1`）**全部 ≥95.6%**（最低 internal/remoteticket 95.6%、internal/handlers 96.6%、internal/workflow 99.7%，余均 100%），无 85% 以下缺口；Android 侧 gradle 无覆盖率插件基建（任务「如适用」条件不成立，登记不适用）。C++ 侧 gcovr 全量报告（TOTAL 89.9%，15213/16921）按未覆盖行数重排、逐项复核既有登记排除项（misc_modules UIA 平台耦合 / lua_engine 零引用遗留 / crypt OpenSSL 内部失败分支 / packer 私有+PE 平台 / x11_recorder 真桌面 / screenshot_handler VISION 变体门 / notify WebhookSender 配置性死代码 / ml_stub 恒失败的模型路径 / agent.cpp 84.6% 的 46 行余量已全部登记）后，85% 线下最大可离线测缺口锁定 `apps/agent/src/resource_loader.cpp` **68.3%（40 行未覆盖）**——既有 43 例只直测静态字节级入口（loadScriptFromBytes / Packer 往返），实例接口面（构造探测/错误回调/资源信息/loadScript）零驱动。

- **测试**：新增 `apps/agent/tests/resource_loader_interface_test.cpp` 7 例（runtime_tests 接线；纯实例方法 + std::filesystem，无 POSIX/X11 符号，Windows CI 全量参与；测试二进制非 Packer 产物、不含 PACK_PE_RESOURCE_ID 资源，Linux 与 Windows 探测结果一致为「无嵌入脚本」）：构造探测无嵌入、ResourceInfo 默认值全字段、getExecutablePath 解析到存在的文件（Linux 走 /proc/self/exe 生产路径）、loadScript 无嵌入时错误回调收「No embedded script found」、无回调失败路径安全不崩、setErrorCallback 二次替换后旧回调不再接收、重复 loadScript 持续失败且探测态不漂移。
- **余量 16 行登记（不写假用例）**：265-279（11 行）loadScript 有嵌入分支——`hasEmbeddedScript()` 在 Linux 恒 false，PE FindResource/LoadResource 为 Windows 产物路径本机结构性不可达，其解析/解压/解密核心由 loadScriptFromBytes 共用实现（unpackResource）覆盖；309 looksLikeLuaBytecode 的 size<3 早退——前置 size>=4 检查已蕴含 size>=3，恒假死防御；332 readlink 失败兜底——/proc/self/exe 恒可读；51 decompress 收尾行归因伪影（函数体已被往返用例驱动）；91/114 readResourceData 的 `#else return {}` 与函数开行——唯一调用点随有嵌入分支一同不可达。
- **覆盖率**（gcovr 行）：`resource_loader.cpp` 68.3%（86/126）→ **87.3%**（110/126），函数 12/13；**TOTAL 维持 89.9%**（15217/16922）——本轮 +24 为真实增量（agent.cpp 顺带 +2），X11 家族 −23（x11_recorder −18 / x11_window −2 / x11_clipboard −2 / platform_types −1）为高负载窗口采集漂移：瞬态连接拒绝使 XRecord/X11 走早期失败腿、健康路径行未执行到，与本轮改动无关（零生产代码改动）。
- **验证**：新增 7 例全绿（连跑多轮稳定）；runtime_tests 整二进制单进程 **209/209 ×2**（共享机 load 88 下 12s 稳定）；插桩 build-cov 全量 ctest 三轮 2493 例、常规 build/ 编译对齐且两树注册数一致 2493——三轮各 1-2 例负载型时序假红且**失败集合逐轮漂移**（ConfigSetRemoteAppliesAndPersists / MissingMethodIsRejected+InputMouseFullSweep / TextRoundtripAndClear，单跑均绿，均为在案 flake 家族：X11 瞬态连接拒绝、IPC 就绪时序、xclip 异步接管；采集窗口外部负载 17→88，无法取得干净全量窗口），与本轮改动无关，CI 推送后在专用 runner 串行复跑；Go 零改动沿用本轮覆盖率复测（全绿）。

---

## 2026-09-30 Android A3 开机自启默认值校正（默认关、显式开启）

todo「Android 现状登记」未完成清单第一项——A3 可靠性剩余项之「开机自启」。**对账结论：实现已于 6cecfad 落地**（BootCompletedReceiver + BootStartGate 纯逻辑 + JVM 单测 + build-android CI 门禁），本轮任务有实质增量的部分是**默认值口径**：

- **冲突与取舍（登记）**：任务指定「默认关闭、显式开启」，而设计 §7 原文与 6cecfad 实现均为「默认开」——后者是落地轮登记的**假设**（CHANGELOG：「A3 设计目标即无人值守，UI 可关」），非用户既定决策。按指令显式口径执行：**默认关**。特权行为（自动化 Agent 开机自启）以显式勾选为准是更保守且可辩护的安全默认；既有显式勾选过的用户不受影响（prefs 已存 true），从未动过开关的安装从「装完即自启」变为「须显式开启」。
- **实现**：默认值收敛单一来源 `BootStartGate.DEFAULT_ENABLED = false`——Receiver 与 MainActivity 两处 prefs 读取都改经该常量，禁止字面量（`AgentPrefs` 注释同步）；BootStartGate/BootCompletedReceiver KDoc 语义更新。
- **回归钉**：BootStartGateTest 新增 `bootAutoStartDefaultsToOff`——钉住常量 false 且「未显式开启（prefs 缺键）+ 已配置地址」不放行，默认值再变必须过显式决策。
- **文档**：android-agent-design §7（默认开→默认关、显式开启，注明校正缘由）；android-keep-alive 指引表；apps/android/README A3 验证步骤（「显式勾选」）；本节勾销现状登记陈旧条目。
- **无真机可验证项（登记）**：BOOT_COMPLETED/MY_PACKAGE_REPLACED 真机广播到达与豁免名单内 startForegroundService 实际行为、覆盖安装后自启、勾选后重启设备全链路——均需真机，本机仅 JVM 单测 + 编译验证；验证步骤已写入 apps/android/README.md A3 节。
- **验证**：`gradle :app:testDebugUnitTest` 22/22 全绿（+1）；零 Go/C++ 改动（Go/C++ 门禁沿用 1a2acb5 已验结果）；docs:build 随文档改动复跑。

---

## 2026-09-30 Android A3 受限设置引导落地（Android 13+ 侧载开箱：手动允许 / adb 预授权 / Device Owner）

todo「Android 现状登记」A3 剩余项第二项（按 development-todo A3 节顺序，先于 asset.sync；触发器项属 A2 节残留且无设计成文章节，不属「A3 之后」）。Android 13（API 33）起侧载 App 的无障碍被「受限设置」默认屏蔽——开关打不开的根因、端侧开箱失败最高来源（mobile-support-feasibility.md §5.2，风险表评级：高）。

- **脚本**：`scripts/android-restricted-settings.sh check/allow/revoke/status`——幂等 adb 预授权（allow 后复核；无 adb/无设备 SKIP、包未装 FAIL、API<33 PASS 不受约束；`tr -d '\r'` 处理 adb shell CRLF；`WINGMAN_ANDROID_PKG` 贯穿）。adb 只出现在部署期（设计决策 D7）。
- **契约护栏**：`orchestrator/server/integration/android_restricted_settings_script_test.go` 11 例（假 adb 状态文件驱动 + calls 断言 + 净化 PATH，不碰真设备；同 guacd 脚本契约框架，bash 3.2 兼容）。
- **App 内引导**：`RestrictedSettingsPolicy` 纯逻辑（API≥33 且无障碍未启用且未确认 → 提示；诚实边界：公开 API 无法区分「被挡」与「未开启」）+ MainActivity onResume 自动弹一次（`restrictedHintAck` ack）+ 常驻「受限设置指引」按钮（三档解法全文）。
- **测试**：JVM 单测 +6（共 21 例全绿）；Go 契约 11 例全绿；shellcheck 干净。
- **文档**：新增 `docs/guides/android-restricted-settings.md`（症状识别/三档解法/验证/已知边界，入文档站进阶指南，与保活指引互链）；development-todo A3 两项勾销（可靠性首项补 6cecfad 对账）；mobile-support-feasibility §5.2/§7/§8；android-agent-design §5.4/§9/新增 §10''；apps/android/README（受限设置验证步骤节 + 里程碑）；ROADMAP M9。
- **遗留登记**：无障碍失效检测上报未做（依赖 device.capabilities 预留槽位）；五厂商定制 ROM 真机逐机型验收未做（本机无真机，手册按官方文档口径）；Device Owner 路径需设备纳管，仓库不附带 MDM 配置。

---

## 2026-09-30 Android A3 可靠性落地（开机自启 / 崩溃自重启 / 断连缓存自治 / 机型保活指引）

todo「Android 现状登记」未完成三项中的第一项（A3 剩余可靠性项优先于 A4）：

- **开机自启**：BootCompletedReceiver（BOOT_COMPLETED + MY_PACKAGE_REPLACED，均在系统后台 FGS 启动豁免名单）+ App 内开关（默认开）+ 已配置服务器地址才放行（BootStartGate 纯逻辑）；显式启动同时清零崩溃退避串。
- **崩溃自重启**：WingmanApplication 安装进程级 CrashRestartHandler——崩溃时记录状态并经 AlarmManager 按指数退避（1s→60s 封顶）调度重启（CrashAlarmReceiver，best-effort，API 31+ 后台 FGS 限制下让位于 START_STICKY 系统路径）；10 分钟窗口连崩 5 次放弃防风暴；coreRunning 门控只在「崩溃前服务在跑」时复活；核心看门狗（服务存活但 C++ 核心不在跑时 30s 幂等重拉 nativeStart）。
- **断连缓存自治**：C++ RemoteClient 重连/outbox/脚本断连自治自 A1 起现成（agentcore_test 28 例覆盖），本轮零 C++ 改动；App 侧以看门狗补进程内自愈；asset 缓存归 A4。
- **机型保活指引**：docs/guides/android-keep-alive.md（五厂商 ROM 步骤 + 验证方法 + 已知边界）+ App 内指引对话框。
- **测试**：纯逻辑对象（RestartPolicy/BootStartGate/WatchdogPolicy）Kotlin JVM 单测 15 例（app/src/test，JUnit4 + Maven 真 org.json——android.jar 对其只部分真实实现）；本机 `gradle :app:testDebugUnitTest` 15/15 全绿；build-android CI job 在打 APK 前先跑该单测（此前 Kotlin 逻辑无任何 CI 门禁）。
- **文档同步**：android-agent-design.md §1.2/§5.4/§6.3/§7/§8/§9 + 新增 §10'（A3 实施摘要）；apps/android/README.md（标题/环境说明/里程碑/A3 验证步骤）；ROADMAP M9。
- **遗留登记**：MediaProjection 授权崩溃后不可恢复（Android 14 单会话一次性，系统约束）；闹钟重启受 Doze 节流影响精度；A3-P2 安全演进与 A4 未动。

---

## 2026-09-30 Android Agent 现状登记（A1/A2/A3-P1 已落地；ROADMAP 补移动端里程碑）

问询「android 的 agent 什么时候实现」并指认文档未更新，本轮核查并对账：**Android agent 并非未实现**——链路与能力均已落地，滞后的是路线图（ROADMAP 仅有桌面 M1-M8、平台说明未提 Android、todo 无移动端条目），不是代码。

- **已落地（以提交时间线为准）**：
  - A1 链路打通（2026-09-19 设计，ce4a648 落地）：Dashboard→Go Server `run_script{content}`→设备端 NDK C++ 核心执行 Lua→`agent.event` 日志回传；Kotlin 壳（WingmanService/MainActivity/JNI 窄接口）；复用 16B 帧头 + JSON 协议、零新消息类型、零监听端口（架构硬约束不破）；nightly CI 自 2026-09-20 打 arm64 APK。
  - A2 能力闭环（2026-09-21，c1df705/c0a67fe）：`platform/android` 宿主桥——dispatchGesture 手势注入、MediaProjection 采集、找色找图、`screenshot.capture` 远程截图 + 反向 JNI 桥。
  - A3-P1 token 认证（2026-09-20，4c9a8f4）：register token 白名单 `WINGMAN_AGENT_TOKENS`（server+桌面+Android 三端，默认关闭、完全向后兼容，见 `docs/agent-token-auth-design.md`）。
  - 工程基建：共用核心下沉 `libs/agentcore` + `libs/androidagent`（2026-09-22，3ab3b7a），桌面同源编译、单测桌面跑。
- **未完成（设计成文、未排期，供任务派发对账）**：
  - ~~A3 可靠性剩余项：开机自启、崩溃自重启、断连缓存自治、机型保活指引~~（✅ 已落地 2026-09-30，6cecfad + 同日受限设置引导条目；开机自启默认值同日校正为「默认关、显式开启」，见前节）。
  - A3-P2 安全演进：~~per-agent token + Dashboard 管理与审计~~（✅ 已落地 2026-10-10：`models.AgentToken` 入库 + `agent.TokenStore`（sha256 哈希、可选 agentId 绑定、明文仅签发时返回一次）+ listener 双源并存（env 白名单 ∪ DB 源，存在签发记录即启用、fail-closed）+ `/api/agent-tokens` 管理面（`agenttokens:manage` 默认仅 admin，签发/吊销落审计）+ Dashboard「注册 Token」页；agent 侧零改动）；~~token 迁移 Android Keystore~~（✅ 已落地 2026-10-10：`SecretStore` 抽象 + `EncryptedSecretStore`（Keystore 主密钥 EncryptedSharedPreferences）+ `migrateToken` 一次性明文迁移（JVM 单测 5 例；初始化失败回退明文库保可用），Service/MainActivity 读写全走加密库）；challenge-response（需 NDK 引入 OpenSSL）与 TLS 仍未实施（agent-token-auth-design.md §6）。
  - A4 多设备编排：Dashboard 设备视图、批量下发、asset.sync 模板分发（协议预留）。
- **本轮文档收口**：ROADMAP.md 新增 Milestone 9「移动端 Agent（Android）」（A1-A4 状态表 + 工程基建注记）+ 平台说明补 Android 实验性 + 时间估算/下一阶段行动表登记未排期项；docs/android-agent-design.md §1.2 A3 行补注 P1 已落地（与 §8 口径对齐）。
- **验证**：纯 .md 改动，主 CI 按路径规则跳过、Docs workflow 随推送运行；无代码/测试变更。

---

## 2026-09-29 C++（Linux）覆盖率扫描续：StandaloneMode 78% → 92%（12 例），ScriptManager 状态机结构性缺陷登记

module_helpers 收口后继续。重跑 gcovr 全量（xvfb 门禁 2474/2474 全绿后出报），排除项复核不变，remote_client 与 standalone_mode 并列 51 miss；standalone_mode 为纯编排层（进程级 ScriptManager + EventBuffer，确定性可离线驱动），锁定 **78%（51 行）**。

- **测试**：新增 `apps/agent/tests/standalone_mode_coverage_test.cpp` 12 例（runtime_tests 接线 + registerLuaEngine() 惰性注册同 rpc_ipc_test；全平台编译）：start 幂等/scriptDir 被文件占据失败、autoStart 装配（输出回调空串跳过实证、completed→Unknown 现状钉）、缺失脚本跳过、loadScript 登记（unloaded→Stopped 现状钉、reload 后 Loaded）、失败脚本 error 态 + 事件推送全链、unknown-id 防御、manager 失同步降级三腿（getScript 默认/unloadScript false/stop 不崩）、批量操作闲置计数 0 契约、getConfig 镜像。
- **结构性不可达登记（19 行）——~~ScriptManager 状态机缺陷~~（✅ 已修复 2026-10-01，见顶部「ScriptManager 状态机死锁环 + stop 数据竞争修复」条）**：runScriptInternal 从不赋值 running（唯一赋值点在 resumeScript，resume 前置 paused、pause 前置 running——死锁环）→ pause/resume 成功腿、批量计数增量、running/paused 状态映射均不可达（**真机同样不可达**，修正 round-4「真机观察」归类）；stopScript 成功腿仅 starting 态并发 stop 可达但伴生 engine->shutdown() 与执行线程的数据竞争，不触发。修复 = 补 running 赋值 + 引擎级协作停止（指令钩子），建议独立任务。另登记 167-168（manager 失败腿为 TOCTOU 外不可达的防御双检）、46（-O2 行归因伪影）。
- **覆盖率**（gcovr 行）：standalone_mode.cpp 78%（189/240）→ **92%**（221/240）；**TOTAL 14849→14878/16600**（89%，+29 行）。
- **验证**：新增 12 例全绿（连跑 3 轮稳定）；build/ 与插桩 build-cov 全量 ctest **2486/2486**（两树一致，含 +12；xvfb 口径 0 failed）；Go/JS 零改动，CI 随推送复跑。

---

## 2026-09-28 C++（Linux）覆盖率扫描续：module_helpers 张量转换层 68% → 96%（15 例，dtype 全矩阵直测）

marshal 收口后继续扫缺口。gcovr 全量报告（xvfb 口径）按未覆盖行数重排并逐一验证可达性：packer.cpp 62% 的 63 行缺里 `compileToBytecode`/`replaceIcon`/`setVersionInfo` 均为 private 且 `build()` 不调用、Linux 成功路径被 PE 写入卡死，离线仅 ~10 行可达（暂记）；notify_module.cpp 75% 的 52 行缺几乎全落 WebhookSender——**`setAllowedHosts`/`setWebhooksEnabled` 全仓零调用方**，白名单默认空 → 每次连接都被 SSRF 防护拒绝，URL 解析/worker 管线/并发上限/shutdown join 在现网行为下均不可达（配置性死代码，登记；既有用例已钉住拒绝路径事件）；remote_client/standalone_mode 各 51 行散布重连/错误腿（暂记）；crypt 前轮既定口径。锁定 `module_helpers.hpp` **68%（58 行）**——缺口几乎全部是张量 dtype 矩阵（11 枚举仅 4 个被顺带踩过）。

- **测试**：新增 `lib/wingman/tests/module_helpers_tensor_test.cpp` 15 例（core_tests 接线；纯头文件内联直测，无 I/O/平台分支，Windows CI 全量参与）：elementSize 全 11 dtype+越界兜底；typeFromString 全 11 名+未知/大小写敏感拒绝；**dtype 全矩阵往返**（spec→TensorData→ModelOutput→ScriptValue 逐 dtype 断言，一次驱动全部 appendBytes<T> 实例化与两侧 switch 分支）；int 经 asFloat 转换路径；bool 0→false；spec 错误矩阵（非对象/缺 data/空 data/非数组 data/未知与非字符串 dtype/shape 非数组）；shape 缺省一维与显式保留；未知 dtype 兜底 3（ModelOutput 空数据、直读 null、appendElement 直调不追加）。
- **余量 7 行登记（不写假用例）**：106-111（static map 初始化）+ 213（聚合初始化）= -O2 行归因伪影——全部行为已被用例真实驱动（全 11 名解析 + 15+ 次 ModelOutput 输出），编译器将初始化代码合并至相邻行。
- **覆盖率**（gcovr 行）：module_helpers.hpp 68%（128/186）→ **96%**（179/186）；**TOTAL 89%→89%**（14865/16600，+51 行）。
- **验证**：新增 15 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2474/2474**、插桩 build-cov 全量 ctest **2474/2474**（两树注册数一致，含 +15；均 xvfb 口径 0 failed，31 例 xclip/XRecord 条件 skip 与既往一致）；Go/JS 零改动，CI 随推送复跑。

---

## 2026-09-28 C++（Linux）覆盖率扫描续：lua_marshal 40% → 97%（28 例，双向转换契约直测）

上轮 Agent 收口后继续扫缺口。gcovr 全量报告（本轮起本地门禁沿用 CI 的 xvfb-run 口径）按未覆盖行数排序：misc_modules 的 UIA OO 区块（Linux 无 UIAManager 后端、平台耦合）、lua_engine.cpp 0%（零引用遗留类）、screenshot_handler（VISION 编译变体门）、crypt OpenSSL 失败分支（既定口径）、x11/clipboard 平台后端、main/start_command 入口胶水均剔除，锁定 `libs/lua/src/lua_marshal.cpp` **40%（62/104）**——ScriptValue↔Lua 双向转换层，契约零直测。

- **测试**：新增 `lib/wingman/tests/lua_marshal_test.cpp` 28 例（core_tests 既有接线；纯嵌入式 sol::state，Windows CI 全量参与）：toLuaObject 10（null→nil、integer 性保持、容器/嵌套往返、C++ callable 五类实参 marshaling 被 Lua 调用）；toScriptValue 10（invalid→null、lua_isinteger 分流、Lua function→非线程安全 callable 实调、error() 函数实证 protected 调用落 null 按现状钉、userdata 兜底）；tableToScriptValue 8（数组/对象判别矩阵：空表落对象、混合键丢数字键、非正整数键取消数组资格、稀疏数组保形补 Null、嵌套递归）。
- **余量 3 行登记（不写假用例）**：48 = switch 全枚举兜底恒不可达；57-58 = valid-nil 防御（真实 nil 均以 invalid 对象到达 52-53 已覆盖腿）。
- **覆盖率**（gcovr 行）：lua_marshal.cpp 40%（42/104）→ **97%**（101/104）；**TOTAL 88%→89%**（14814/16600）。
- **验证**：新增 28 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2459/2459**、插桩 build-cov 全量 ctest **2459/2459**（均 xvfb 口径，两树注册数一致）；Go `-race` 14 包全绿、jest 420、vitest 全绿（零 Go/JS 改动沿用今日同一代码态已验结果，CI 随推送复跑）；CI 结果见提交对应 run。

---

## 2026-09-28 C++（Linux）覆盖率收口：Agent 主类 0% → 84%（35 例），两缺陷根治：system.shutdown 死锁 / shutdown 悬空事件 sink

todo 仅剩两条真机人工验证项（headless 不可执行），按既定规则转覆盖率缺口。gcovr 全量报告（口径沿用 `--gcov-ignore-parse-errors negative_hits.warn`，TOTAL 87.1%）剔除真机/平台耦合项（x11_recorder/x11_clipboard）、入口胶水（main.cpp）与 OpenSSL 内部失败分支后，行覆盖最低且可离线测的自有模块锁定 `apps/agent/src/agent.cpp` **297 行 0%**——runtime 编排核心（initialize 能力分支、start 组件装配、applyRemoteConfig 热重建、handleRemoteCommand 全命令面、EventBuffer 远程转发）此前只被间接编译、无任何测试驱动。

- **测试**：新增 `apps/agent/tests/agent_loopback_test.cpp` 35 例 / 2 套件（tests/CMakeLists 接线；TCP 回环沿用 agentcore 测试 harness 模式，IPC 客户端沿用 rpc_ipc_test 模式）。AgentLifecycleTest 17 例全平台（能力→组件派生矩阵 4、配置文件首跑写默认+读回 2、生命周期契约 4——含 start 失败 running 仍置位的降级契约、applyRemoteConfig 矩阵 7）；AgentLoopbackTest 18 例 POSIX（`#ifndef _WIN32`，XDG_RUNTIME_DIR/TMPDIR 重定向到用例私有目录后走 Agent::start() 真实装配路径：本地 IPC 面 8——getVersion 全链、getStatus providers、getRemote 镜像、setRemote 校验矩阵+应用落盘全链+不可写路径部分成功、EventBuffer 三类事件转发过滤+摘 sink 回归钉；远程命令面 10——get_status 脚本落定/error 态、list_windows 信封、run_script 四腿、stop_script 三错误腿、unknown、trigger.* Dispatcher Reuse、screenshot.capture 信封、system.shutdown）。
- **两缺陷根治（均由本轮用例暴露）**：① system.shutdown 远程命令自我死锁——命令回调内联运行在 RemoteClient 消息处理线程上，同步 `stop()` 回收正在执行回调的线程自身（实测 `Resource deadlock avoided`），ack 永远发不出、server 侧超时；改为先回 ack、stop 移交独立线程收尾，`SystemShutdownCommandStopsAgent` 回归钉（轮询组件全停再收尾）。② `Agent::shutdown` 不摘除 EventBuffer 远程 sink——lambda 捕获 `this` 而 EventBuffer 是进程级单例，Agent 析构/重建后 push 事件即悬空回调 UB；shutdown 补 `setRemoteSink(nullptr)`，`ShutdownClearsRemoteEventSink` 回归钉。
- **登记假设与余量 46 行（不写假用例）**：stop_script 成功腿与脚本 running 态需长驻脚本协作停止（真机观察，同 rpc_ipc_test 口径）；list_windows 循环体 Xvfb 无窗口不执行；scriptStateToString 的 Loaded 过渡态不停驻、Running/Paused 同前、Unknown 防御兜底；触发器 onFired→EventBuffer 推送需真实屏幕命中；screenshot/trigger 的 dispatcher 空指针防御与 initRemoteClient/initStandaloneMode 失败腿恒不可达；encodeWindowHandle `_WIN32` 分支非激活编译侧；多行 braced-init 与 spdlog 双行语句的行归因伪影。
- **覆盖率**（gcovr 行）：`agent.cpp` 0%（0/297）→ **84%**（252/298）；**TOTAL 87.1%→88%**（14740/16600）。
- **验证**：新增 35 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2431/2431**、插桩 build-cov 全量 ctest **2431/2431**（两树终版注册数一致）；Go `-race -count=1 -timeout 90m` 14 包全绿（integration 包 1868s）；dashboard jest 420、GUI vitest 全绿。CI 注记：前笔 c97a57a 为加固未完成的中间态提交，其 Linux full-tests 两作业失败的 6 例即本笔修复对象（断言与实测契约错配 4、trigger.update 数值 id 的 JSON 解析歧义 1、system.shutdown 死锁超时 1），最终 CI 结果见本提交对应的 workflow run。

---

## 2026-09-27 C++（Linux）覆盖率扫描续：LuaScriptEngine 55% → 86%（31 例，余量全部登记不可归因/防御分支）

上轮扫描的次低自有可测模块。既有覆盖仅来自 script_manager/script_module 胶水的间接驱动，`executeString`/`callFunction`/`getGlobal`/`setGlobal`/沙箱开关/`getLanguageName` 等公开面整段零直测；round-1 修复的 `package.preload` 钩子（非沙箱分支）此前从未被任何测试驱动。

- **测试**：新增 `lib/wingman/tests/lua_script_engine_test.cpp` 31 例（core_tests 直链 wingman::lua 既有接线；纯引擎 API，Windows CI 全量参与）：initialize 6（沙箱危险全局清除全集、非沙箱全库、require("wingman") 非沙箱解析、沙箱 require 报错、env→`_ENV_*`、幂等）；execute 4（语法/运行时错误 lastError、文件成败、未初始化拒绝）；callFunction 6（未找到、参数+整数返回、error 进 lastError、nil→Null、混合标量、Lua 5.4 整数性 Int/Float 分流）；registerModule 4（被 Lua 调用+返回、实参传递、C++ 异常→sol::error 传播、未初始化 no-op）；global 3（四标量往返、未知名 Null、未初始化 no-op）；沙箱开关 3（事后剥离、disableSandbox 只重开 io/os/debug——package/require 不恢复按现状钉、未初始化 no-op）；print 捕获 4（简单、变参制表符+tostring 匹配 Lua 原生、空 print 空串、**initialize 前安装回调不生效按现状钉死**）；shutdown 幂等 1。
- **余量 22 行全部登记（不写假用例）**：① 9 行 `open_libraries` sol 变参模板实参行——两分支均被真实驱动（沙箱另经 StandaloneMode），gcov 变参展开不落行归因，工具盲区；② 4 行 initialize 防御 catch——无故障注入不可达（登记口径同 crypt.cpp）；③ 9 行 executeFile/String 的 `!result.valid()` 腿——实证不可达：本轮全部失败用例在该 sol 版本下均走 catch 异常腿，invalid-result 为死防御。
- **覆盖率**（gcovr 行）：lua_script_engine.cpp 55%（93/168）→ **86%**（146/168）；**TOTAL 86.5%→87.1%**（14457/16599）。工具注记：gcov 触发 gcc#68080（smart_trigger switch 负值），按 gcovr 文档以 `--gcov-ignore-parse-errors negative_hits.warn` 出报告；smart_trigger 行归因 235→197（98%→100%）为跳过记录伪影、真实覆盖未变（TOTAL 影响 ≈0.03pp）。
- **验证**：新增 31 例全绿；build/ 全量 ctest **2386/2386**（2355+31）；插桩 build-cov 全量 ctest 2386/2386；Go `-race -count=1` 全绿；dashboard jest 420、GUI vitest 511 全绿；CI 结果见提交对应 run。

---

## 2026-09-27 C++（Linux）覆盖率扫描续：AgentConfig 45% → 100%（24 例 + saveToFile 丢 [performance] 节根治）

todo.md 真机两项继续搁置。双端扫描：Go 侧 `go test -cover` total **98.5%**（<80% 仅 7 个函数：`remoteticket.NewManager` 一行包装 0%——测试走 `newManagerWithSweep` 短周期 seam；guacamole/recordings 6 个 70~75% 分支需 guacd 基础设施，环境依赖），不构成实质缺口；C++ 侧 gcovr（TOTAL 86.0%）最低且无平台耦合的自有模块锁定 `apps/agent/src/agent_config.cpp` **45%（78 行未覆盖）**——能力/模式派生、loadFromFile、debugger/logging/performance 节解析、saveToFile 整段零覆盖，而它们是 agent.cpp 的真实生产路径（`Agent::initialize` 派生 RunMode、远程配置写回 `saveToFile`）。

- **测试**：新增 `apps/agent/tests/agent_config_test.cpp` 24 例（tests/CMakeLists 接线，Windows CI 全量参与——纯文件 I/O 无平台分支、无 loopback 跳过、无真机观察项）：能力派生 6 例（默认 Hybrid、各单能力派生、仅 LocalIpc 与全关 → Unknown、Hybrid 优先于 Standalone 判定序）；loadFromString 13 例（[global] 别名、remote/debugger/logging/performance/standalone 全键、引号内 `#` 保留、未配对引号不剥、未知节/键静默忽略、类型不匹配忽略（当前容错契约）、超范围整数抛出由 `Agent::initialize` catch 兜底、文件不存在抛错带路径、不可写目录 false）；文件往返 5 例（全节段 save→load 相等、默认配置首跑写盘可读回、模式一致）。
- **连带根治——saveToFile 漏写 `[performance]`**：解析端支持该节三整型键而写盘端不输出，用户手调的性能配置会在 runtime 首次配置落盘（`Agent::applyRemoteConfig` → `saveToFile`，agent.cpp:326/336）时被**静默抹掉**。补写 [performance] 节与解析端对称；回归钉 `SaveToFilePersistsPerformanceSection` 同时断言文件内容与读回值。
- **覆盖率**（gcovr 行）：agent_config.cpp 45%（64/142）→ **100%**（147/147，含修复新增 5 行）；**TOTAL 86.0%→86.5%**（14366/16599）。
- **验证**：新增 24 例全绿；build/ 全量 ctest **2355/2355**（2331+24）；插桩 build-cov 全量 ctest 2355/2355；Go `-race -count=1 -timeout 90m` 14 包全绿；dashboard jest 420、GUI vitest 511 全绿。

---

## 2026-09-27 C++（Linux）覆盖率缺口收口：IPC/RPC 控制面约 405 行 0% → 52 例单测（顺带根治六个真实缺陷）

todo.md 剩余两条真机观察项继续搁置，覆盖率方向继续推进。上轮收掉 agentcore 后，gcovr 报告里剔除「真机/人工观察」与「OpenSSL 内部失败分支不可达（crypt.cpp，无故障注入无解，登记假设）」后剩余最大缺口锁定 GUI ⇄ runtime 控制面集群：local_ipc_server / script_handler / macro_handler / config_handler / event_log_sink 合计约 340 行 **0%、零测试**（早已编进 runtime_tests 二进制但从未被驱动），system_handler 余 29 行 55%。

- **测试**：新增 `apps/agent/tests/rpc_ipc_test.cpp` 52 例 / 7 套件（tests/CMakeLists 接线 wingman::lua）。Linux loopback 真链路（UnixSocket 帧 + 真 StandaloneMode + 真 Lua 文件）；Windows 侧 loopback 套件 `#ifndef _WIN32` 跳过（NamedPipe 语义本机不可验证，登记假设；仅由 Windows CI 编译检查）。覆盖：script.* 全命令参数/错误信封与同步执行契约、events.drain 上限与 remaining、config.getRemote/setRemote 往返与 apply 失败透传、macro.* status/save/load/坏文件/越界钳制、system.getStatus 注入 provider 反映、EventLogSink 级别过滤与 4096 截断、LocalIpcServer 起/停/幂等/未知方法/坏 JSON/缺 method/Error 型/客户端断开事件/重连/带客户端干净停机。
- **意外收获——六个真实缺陷**（四个生产行为级）：① `StandaloneMode::stop()` 提前 return，仅 LocalIpc 能力的 runtime 上 GUI 加载的脚本在全局 ScriptManager **永久泄漏**（去早退，无条件清登记）；② `macro.play` speed 未校验直传 recorder——**speed=0 SIGFPE、负数无符号下溢挂死**，RPC 边界单点钳制一次收口三平台实现；③ config_handler **引用捕获悬垂 → 段错误**（调用方传临时对象即 use-after-free，新测稳定复现），改按值捕获；④ Lua 引擎初始化无条件装 `package.preload` 钩子，沙箱模式 package 为 nil 直接抛错——**沙箱脚本（GUI script.start 唯一路径）100% 启动失败**，钩子改 `!sandboxed` 才装；⑤ LocalIpcServer 停机双 disconnect 竞态——server 线程与 stop() 都对同一通道 disconnect，并发进入双重 join 同一 receiveThread（UB，审读确认的硬化收口；实测挂死混有同机并发跑测干扰，诚实定性），channelMutex 串行 + 锁内复查 stopping；⑥ **EventLogSink 级别过滤方向写反**（`>` 应为 `<`）——info 下限时 warn/error 全被滤掉，GUI 日志面板永远收不到告警与错误（回归钉三连）。
- **登记的产品契约与真机观察项**：ScriptManager 同步执行模型——`script.start` 阻塞至脚本跑完，顺序 RPC 打不进 running 窗口，**GUI 无法停运行中脚本**（stop 必报 Failed to stop script），pause/resume 成功路径同属真机观察；wingman::unloaded→runtime Stopped→JSON "stopped" 映射；macro.start 录制平台相关（无头只断言信封）。
- **覆盖率**（gcovr 行覆盖）：local_ipc_server 0%→80%（余 34 行全为创建/连接失败与停机分支）、script_handler 0%→78%（余量即真机观察项）、macro_handler 0%→89%、config_handler 0%→**100%**、event_log_sink 0%→94%、system_handler 55%→75%、standalone_mode 70%；**TOTAL 80%→86%**（14278/16594）。
- **验证**：新增 52 例 2.1s 全绿；loopback 压力 10/10 轮；runtime_tests 全量 128/128；插桩 build-cov 全量 ctest **2331/2331**（上轮 2279 + 本轮 52）；Go `-race -timeout 90m` 全仓绿；dashboard jest 420、GUI vitest 511 全绿。

---

## 2026-09-27 C++（Linux）覆盖率缺口收口：agentcore 0% → 28 例单测（顺带根治五个真实缺陷）+ 文档语法修复

todo.md 仅剩两条需真机人工验证项（macOS 真机脚本、XRecord 真桌面），headless 不可执行，按优先级转覆盖率缺口。

- **缺口定位**：gcovr 全量报告（TOTAL 行覆盖 80%）中行覆盖最低且不依赖 Lua/X11/平台限制的模块为 `libs/agentcore`（`remote_client.cpp` 390 行 + `event_buffer.cpp` 59 行 **0%**、零测试）；依赖仅 transport + nlohmann_json + spdlog，可纯 loopback 单测（Android 约束不破坏）。
- **测试**：新增 `libs/agentcore/tests/agentcore_test.cpp` 28 例——EventBuffer 11 例（容量驱逐 / log.line 优先 / sink 重入 / dropped 增量）+ RemoteClient 17 例走真 loopback TcpServer（注册身份/token、ack 成败、命令回环六形态、1s 心跳 link 五元组、断线 outbox 冲刷、超容量丢弃恰好 100 条）；根 CMakeLists 与模块 CMake 接线 `BUILD_AGENTCORE_TESTS`（镜像 transport 模式）。覆盖率：EventBuffer 100%、RemoteClient 86%。
- **意外收获——五个真实缺陷**（三个生产行为级）：① `TcpClient::connect()` 重连对 joinable IO 线程赋值 → `std::terminate`，**服务端一断链 agent 进程重连必崩**（transport 层收口：connect 入口隐式 disconnect）；② EventBuffer `push()` sink 持锁回调，违反自身「锁外回调」契约，sink 内查询/重入即自死锁（锁作用域收口）；③ 心跳线程整段 30s 睡眠不可打断，stop 与每次重连的 join 被放大一个心跳周期（改 100ms 分段睡眠）；④ stop 与重连线程拉起新心跳的生命周期竞争 → joinable 线程析构 terminate（heartbeatMutex 串行化 + 先停重连后收心跳 + 无条件幂等 stop）；⑤ markConnected 双计使首连 `reconnects=1`（删直接调用，统一由 SessionEvent::Connected 记账）。
- **顺带**：全量 ctest 首红 `HumanMouseTest.PathRandomness` 定性为统计型 flake（int 向零截断零桶双倍宽 → 两路径可量化全等），改 20 轮分布断言 + 修 `size()-1` 无符号下溢，连跑 100 次绿；`TestRunGracefulShutdownOnSIGINT` 15s 就绪窗口为 d22abe3 同族第四处，收口至 `waitTCPUp` 60s。
- **文档语法修复（用户指认）**：`docs/development-todo.md` Phase 7 请求块 `#### 请求消息结构` 标题 + ` ```json ` 开围栏自 c400bd6f 丢失（孤立闭合围栏吞掉响应节渲染），对照原提交补回；`docs/api/core.md` 两处 `#### `…)**` 标题反引号误写 `**`。docs:build 校验通过。
- **验证**：agentcore 28 例 ×4 轮全绿；全量 ctest 2279 例（见提交）；Go `-race -timeout 90m` 全仓绿；dashboard jest 420、GUI vitest 511 全绿。

---

## 2026-09-27 事件与状态收尾：wingman.event 监听器查询与按事件名清理（listener / listeners / clear(type?)）

`docs/development-todo.md`「事件与状态」小节仅剩的两条未完成项，按登记口径实现收口。

- **核心层（event.hpp/event.cpp）**：`EventHub` 新增 `SubscriptionInfo{id,type,name,once}` 快照与 `subscription(id)` / `subscriptionByName(name)` / `subscriptionsForType(type)` 三个查询 + `clear(type)` 按事件名清理重载。确定性约定：同名订阅取**最早注册者**（unordered_map 遍历无序，不能交给它决定）；`subscriptionsForType` 按订阅 ID 升序（注册顺序）；匿名订阅（name 为空串）不可按名查询；`clear(type)` 对未注册事件是无副作用空操作。无参 `clear()` 全量清理语义不变。
- **脚本层（event_module.cpp，Lua/Python 经 ModuleDescriptor 自动可用）**：`listener(id|int|name|string)` 返回 `{id,type,name,once}` 或 nil；`listeners(type)` 返回数组（未注册事件为空数组）；`clear(type?)` ——无参/nil 全量清理、传事件名只清理该事件、**其他类型参数返回 false 而非静默全量清理**（防 `clear(123)` 误清全部）。缺参/坏类型返回 false，与 `on`/`off` 既有约定一致。`event.pyi` 同步 `ListenerInfo` TypedDict 与签名。
- **测试**：`EventHubTest` +4、`EventModuleTest` +12（38→50）：按 ID/按名查询、once/匿名快照、同名取最早、排序、按名清理只影响目标事件（emit 不触发、其余事件原样）、全量清理兼容、坏参数 false。
- **验证**：`EventHubTest.*:EventModuleTest.*:EventHubGuardTest.*` 74/74 绿；全量 ctest 与 `go test ./...` 见提交记录/CHANGELOG。

---

## 2026-09-27 X11WindowCloseCenterAndWaitFamily flake 根治（X server 断开→重连瞬态拒绝 → openX11Display 重试 + 建窗 XSync + 护栏随超售缩放）

上轮按指示未动的登记欠账（「高负载 flake 二轮根治」条目末尾），本轮单独收掉。登记过的两种失败形状（①forceClose 后 holder 5s 护栏报警；②center/close/isInitialized 成片 false）本轮都追到了同一个上游。

- **定位（测试进程 + Xvfb 双侧 strace）**：单用例在 `xvfb-run -a` 下连跑 4 轮红 1（登记数据：load 55~61 → 10 跑 7 红），本轮捕获的每个红实例都以 `[error] X11Window: failed to open X display` 开场——门面的 `XOpenDisplay` 返回 NULL → `initialized_=false` → 后续每个方法静默 false → forceClose 落空 → holder 护栏报警，①②都是这一跳的级联。抓失败现场的 trace：门面那次连接 connect 成功、Xauthority 里的 cookie 也读到了，但 **Xvfb 在 accept 后读完 `SO_PEERCRED` 与 `/proc/<pid>/cmdline`，连客户端的 setup 请求都没读就 `shutdown`**（随后按 Xorg 惯例打开 `/etc/X<disp>.hosts` 与 `protocol.txt` 组织拒绝信息）；同一进程几毫秒后的下一次 open 完全正常。用不带任何 wingman 代码的裸 open→close 循环隔离验证：load≈40 下 11/3000 失败、**11/11 立即重试成功**；自造 load≈67 下 7/800、7/7。定性：X server 对「前一个本地连接刚断开 → 新连接立即到达」存在 accept 阶段的**瞬态拒绝**（对应 `os/access.c`/`os/client.c` 按 pid 缓存的本地凭据在断连清理窗口的竞态，ComputeLocalClient/DetermineClientCmd 一路），CPU 超售放大概率。全 fixture 复跑（load 38~91）在 screen/capture/input/window 各类都见过散片 `failed to open X display`（每轮 13~23 次）——不是某条用例的时序错误，但每个 open 都可能撞上，只能由调用方吸收。
- **修法（生产侧收口为主，测试侧补齐登记的另两项）**：① 新增 `src/platform/linux/x11_display.hpp`：`openX11Display(name, attempts=6, backoff_us=20000)`，瞬态拒绝重试，最坏 120ms 只在失败路径付出；6 个 Linux 门面（`x11_window`/`x11_screen`/`x11_capture`/`x11_clipboard`/`xtest_input`/`x11_recorder` 的 control+data 双连接）的 `XOpenDisplay` 全部改走它，重试耗尽仍按原语义 false/nullptr。② 测试侧 `TestX11Window` ctor/`setActive`/析构与 fork holder 子进程的 `XFlush` 改 `XSync`——登记项①（子进程未 XSync、父侧 XKillClient 打在 server 建窗之前落空）是独立存在的次级隐患，本轮一并堵死：跨连接读回与强杀目标必须以 server 已处理为前提。③ holder 收割护栏 5s 改为随超售缩放（`clamp(load1/nproc, 1, 8)` × 5s，同 dashboard 二轮口径），报警信息带实际秒数与因子——固定窗口报警的其实是负载。WM 集成用例的 `displayAccepts`/`wmRegistered` 等就绪探测保持裸 open：它们本身就在等待循环里，瞬态失败由循环自己吸收。
- **回归钉**：新增 `X11PlatformTest.WindowInitializeSurvivesConnectionChurn`——25 轮「探测连接 open→close 紧接门面 open」逐轮复刻实证的竞态形状，断言门面每轮必须靠重试站起来（`isInitialized`），末轮还要真实可用（center/close 生效）。修复前这正是 ~1/4 红的形状；循环里的裸 open 失败不作断言（那是压力本身）。
- **验证（本机自造负载）**：裸循环在 load≈67 确认竞态真实（7/800 瞬态失败、全部可重试）；`X11WindowCloseCenterAndWaitFamily` + 回归用例连跑 **10/10 绿**（负载 31→83 全程覆盖登记区间）；`ctest -R 'X11PlatformTest\.|Recorder|X11'` 66 用例 **×3 轮全绿**（load 60~77）。全量 ctest 与 CI `C++ Linux (full tests)` 结果见提交记录/CHANGELOG 本条目末尾补充。

---

## 2026-09-27 根包 TestRunHTTPEndpointsAndScriptOutput 负载 flake 根治（共享 deadline 耗尽 → nil conn SIGSEGV → waitTCPUp 收口）

工作流取消根治同日全仓 `-race` 复跑红过一次而登记的欠账，本轮收掉；压测还揪出了 SIGSEGV 一直掩盖着的第二层事实。

- **根因（一处共享 deadline，三处后果）**：15s HTTP 就绪循环与 agent 拨号循环复用同一个 `deadline`。负载下就绪等待耗完 15s 后，拨号循环 `for time.Now().Before(deadline)` 的循环体**一次都不执行**——`agentConn` 保持 nil、循环外的 `err` 也没被赋值（还是 `os.Getwd` 留下的 nil），`if err != nil { Fatalf }` 形同虚设，nil conn 一路传进 `writeAgentFrame` 才 SIGSEGV，崩掉的栈指向不了「server 没起来」这个真根因。就绪循环自身超时不收口，server 没起来时后续请求只产生 connection refused 的 `Errorf` 假信号。同测试还有一处同族缺陷：落库轮询 `time.Now().Before(time.Now().Add(5s))` 恒真，事件不落库就挂到测试全局超时而非干净失败。
- **压测揪出的第二层（SIGSEGV 掩盖的）**：`-race` 构建在 load 50-70 的共享机上，server 启动实测要 **30s 上下**——失败实例的日志里 Fatalf 之后才出现 route 注册 → seed → `Server starting`。原 15s 窗口本身就低于慢启动机器的真实需要，这是 load 46 那次红的直接触发条件。
- **修法（waitTCPUp 一个收口点 + 窗口只作失败上界）**：新增 `waitTCPUp(t, name, addr, timeout)`——每次调用独立起算窗口、到点带名字与地址 `Fatalf`；HTTP 就绪（60s）与 frame listener 拨号（15s）都走它，nil conn 结构上不可能再往下漏。落库轮询改先算好的 `dbDeadline`（15s）。窗口就绪即返回，绿路径零成本。
- **验证**：空闲单跑 10s 绿；28 个 CPU burner（load 50-70）下 `-race -count=10` **10/10 全绿**（263s）。中途 30s 窗口版在压测下 4/10 **干净红**、错误信息直指 server 未就绪（收口与诊断价值同时验证）——正是这次干净红暴露了 30s 启动事实，随后 60s 复跑全绿。`go vet` 干净、全仓 `go test -race -count=1 ./...` 全绿；工作流取消根治同轮未回退（workflow 与 handlers 包均绿）。X11WindowClose 那条 flake 按指示另立一轮，未动（同日下一轮已根治，见顶部）。

---

## 2026-09-27 工作流取消被终态回写覆盖（TestCancelRunningWorkflow 的写序竞争根治）

上一轮末尾登记的欠账，本轮单独收掉。

- **定性方法**：先用临时探针把取消动作同步到「步骤行已 `running`」之后（`waitFor` 轮询 `step_statuses.status`，此刻 runner 必已越过 `execute` 循环顶部的 ctx 早退检查），修复前 **60/60 全红** `expected cancelled, got completed`；`-race` 全程无报告、无共享变量误用 → **状态机真缺陷**，不是测试自身问题。原用例平时绿只是运气：它只等「进了 running 表」，而那是 `Submit` 同步写的，等于不等待，取消多半落在 runner 进循环之前——那条路径直接 `return`、不回写终态，缺陷不露面；CI 负载把窗口撑开才红。
- **机制（两层，第二层由自建验收 gate 的 `-race -count=50` 抓出）**：取消让步骤以 `cancelled` 结束并返回 `ctx.Err()` → 调度循环按 `failed` 出环 → 但 `finalStatus` 只看 `anyStepStatus("failed")`，被取消的步骤状态是 `cancelled` 不是 `failed`，于是**误算成 `completed`**。第一层：两侧无条件 `Updates`，后落库者把 `Cancel` 刚写的 `cancelled` 覆盖。只加条件更新收口后，count=50 又红 3 次 + handlers 包 `TestWorkflowCancelLifecycle` 1 次：旧 `Cancel` **先 `exec.cancel()` 再落库**，被惊醒的 runner 带着误算的 `completed` **抢先**落库——守卫把「后到覆盖」变「先到赢」，先到者写的仍是错值，`Cancel` 反向让位回 400（该用例工作流是 `wait 30s`，不可能真跑完）。反方向（已完成被改写成 `cancelled`）同样由条件更新堵死。
- **修法（终态收口 = 条件写 + 先落库后惊醒 + 诚实取值，不加锁）**：① 两侧终态回写都改条件更新 `WHERE id = ? AND status = 'running'`——终态只能从 running 迁移，后到让位（`RowsAffected == 0`），任何交错收敛到同一终态；② `Cancel` 改为**先落库、成功后才 `exec.cancel()`**——runner 被惊醒前终态已安装，取消必然生效、必然返回 nil，runner 的回写只会让位；③ runner 的 `finalStatus` 把 `ctx.Err() != nil` 判成 `cancelled`（优先于 failed 判定）——未来任何新增 cancel-ctx 路径（停机、超时策略）下取值也诚实。让位方不做后续动作：runner 不广播与库不一致的 `status_changed`；`Cancel` 让位时不 cancel ctx、不标 skipped、不广播，按既有契约返回 `workflow not running`。选 SQL 条件与调用顺序而非内存互斥：更小，且覆盖「ctx 检查与写库之间」这类 check-then-act 窗口。
- **回归用例四条**：① `TestCancelAfterStepStartedKeepsCancelledStatus`（探针转正，同步点后取消必生效必 nil、终态恒 cancelled）② `TestExecuteFinalStatusHonoursCancelledContext`（绕开 `Cancel` 直接 cancel runner ctx，锁取值层：finalStatus 必须认 ctx 取消，不误算 completed）③ `TestTerminalWriteYieldsWhenRowNotRunning`（直接驱动 `execute`，锁让位条件）④ `TestCancelYieldsWhenWorkflowAlreadyTerminal`（对称方向：不改写终态、不标 skipped、返回错误）。原 `TestCancelRunningWorkflow` 无同步点，`Cancel` 撞「工作流真已跑完」让位分支时改为只要求行处于合法终态。
- **验证**：`-race -count=50 ./internal/workflow` 全绿；`go vet ./...` 0 告警、`gofmt -l` 干净；全仓 `go test -race -count=1 ./...` 全绿（integration 包共享机负载下需 `-timeout 90m`）。上一轮 CI 根治未回退（脚本仍无 `declare -A`，10 个 shell 契约用例同轮绿）。


---

## 2026-09-27 修 main 长期红：guacd e2e 脚本契约测试在 macOS/Windows 恒红

**为什么动别人的文件**：CI 自 `fcf7cf9`（2026-09-25）起连续 9 个 push 红（上一个绿的是 `4f97996`），只红 `Go Server (macos-latest)`/`(windows-latest)` 两个 job，失败集合恒定为 `scripts/verify-guacd-e2e.sh` 的 8 个 shell 契约用例。长期红让「全绿才推送」的门禁失去判定力——此后任何人的真回归都混在这 9 个红里。诊断已做完且修复局部，故顺手收掉；两处都是真缺陷，不是口味问题。

- **macOS（被测脚本的真缺陷）**：`declare -A READY_PORTS` 是 bash 4 语法，macOS 自带 `/bin/bash` 停在 3.2 → `[guacd]=4822` 被当作给未变量 `$guacd` 赋值，`set -u` 下当场崩；崩点在引擎探测之后、任何 SKIP 分支之前，于是「环境不具备 → SKIP(2)」变 exit 1。改法：`READY_NAMES`/`READY_PORTS` 两条平行数组 + `ready_port()` 下标查表（其余 `verify-*.sh` 一律不用关联数组，本脚本是全仓唯一例外），`do_status` 的服务名也改取同表，端口映射回到单一来源。
- **实测复现与对拍（不靠推理）**：`docker run --rm bash:3.2` 跑改前脚本 → `line 60: guacd: unbound variable`，与 CI 日志逐字相同；改后在 bash 3.2 与本机 bash 5.3 下逐路径对拍一致——`--help`(0)、无引擎 `up/test/run/status`(SKIP 2)、未知子命令/未知 flag(2)、假引擎 `up` 端口未就绪(1 且四行端口报告正确)、假引擎 `status`(1，NOT READY×4)。Linux 全量 10 用例绿。
- **Windows（契约测试自己的缺陷）**：用例靠往临时目录软链 coreutils 拼「净化 PATH」，而 `os.Symlink` 的错误被 `_ =` 吞掉；Windows runner 默认无建符号链接权限 → PATH 目录为空 → 脚本里每个命令 exit=127。改法：软链失败退化为复制，复制也失败则带原因 SKIP（不再静默）；POSIX bash 脚本的契约测试在 Windows 显式 SKIP，守卫集中在 `linkCoreutils`（所有跑脚本的用例必经）；宿主缺单个命令（macOS 无 `timeout`）仍按原语义继续，避免把 macOS 覆盖一起 skip 掉。
- **同轮 CI 另抓到的两处（都不在本轮改动内）**：① `apps/agent/src/packer.cpp` 的 `const std::vector<uint8_t> resourceData` 传给 `UpdateResourceA` 的 `LPVOID` 参数，MSVC 报 C2664——该段是 `_WIN32` 专属，Linux 编译看不见，只有 Windows CI 会红（本轮改动引入，已随本轮修复：去掉 `const`；本地用同签名的探针 TU 复现并验证：const 版报 `no known conversion from 'const unsigned char *' to 'LPVOID'`，非 const 版干净）。② `internal/workflow` 的 `TestCancelRunningWorkflow` 在 ubuntu runner 上偶发红（`engine_extra_test.go:409: expected cancelled, got completed`；此前三个 push 同 job 均为绿，本轮 Go 文件零改动）。机制是可复现的写序竞争而非玄学：`Engine.Cancel`（engine.go:249）先写 `status=cancelled`，而 runner goroutine 收尾处（engine.go:358-361）**无条件**写 `completed`/`failed`，最后写者赢；工作流极快跑完时二者只差几毫秒。修法是一条条件更新（收尾 UPDATE 加 `WHERE status = 'running'`，并据此决定是否广播），但那是 workflow 引擎的语义改动、不属本轮，留给独立一轮。**该欠账已由同日「工作流取消被终态回写覆盖」一节收掉**（定性为真缺陷：把取消同步到步骤行 running 之后，修复前 60/60 复现覆盖）。
- **待观察**：macOS 维度这 8 个用例是首次真跑，若仍有红，最可能的下一处是 `/dev/tcp` 在 macOS 系统 bash 上的可用性（`port_open` 已留 `nc` 退化路径，两个都没有则脚本按设计报 SKIP 2，而 `status` 用例期望 1）。本轮无 macOS 机器，只能靠 CI 判定。

---

## 2026-09-27 加密资源打包→加载闭环（PBKDF2 口令派生 + PACK_HEADER v2）

全仓唯一代码 TODO（`apps/agent/src/resource_loader.cpp` 的「Encrypted resources are not supported yet」）落地：加密资源从「两端一致禁用」变成「打包→加载真闭环」，并且第一次在 Linux 上跑通同一条链路。

- **两套分叉实现收敛为一份**：`PACK_HEADER` 原先在 packer 与 loader 各抄一份 struct，两侧各有一份 `_WIN32` CryptAPI 加解密；非 Windows 分支只剩「XOR 假哈希 + `verifyHash` 恒返回 true + 一律拒绝加密」——即 Linux 上跑的加解密与完整性校验全是假的。现格式与 KDF 定义唯一化到新头 `apps/agent/include/wingman/runtime/resource_pack.hpp`（`sizeof(PACK_HEADER)==160` 的 static_assert、reserved[] 布局、le32 读写、参数合法性判定都在一处），密码学统一走 `wingman::crypt`（OpenSSL EVP，新增原始字节 + 调用方给密钥的 `aesGcmEncrypt/aesGcmDecrypt`，供不能自描述的二进制格式用）→ Linux/macOS 上的哈希校验与加解密从此是真的。PE 资源 ID 也从两处字面量收敛为 `PACK_PE_RESOURCE_ID`。
- **v2 格式与兼容性论证**：口令 → PBKDF2-HMAC-SHA256（100000 轮、随机 16B salt、随机 12B IV）→ AES-256-GCM（密文 || tag）；salt/IV/迭代次数写进 `reserved[0,32)`，尾字节清零。为什么升 v2 不破坏任何存量：v1 的加密密钥是打包时随机生成、只留 `sha256(key)`，构造上不可恢复，而当年 `build()` 又直接拒绝 encrypt → **合法产物里不存在 v1 加密包**；未加密包照旧写 v2（v1 读侧不解释 version/reserved，新写未加密包仍可被旧读侧加载）。v1 未加密包继续可读；未知版本一律拒绝（`Unsupported pack format version N`）；v1+ENCRYPTED 明确判为不可恢复并要求重打包，不做任何「猜密钥」回退。
- **完整性分三层（各有不可替代的职责）**：GCM 标签是权威（错口令、改密文、改 IV 都拒）；`keyHash = sha256(派生密钥)` 只是解密**前**的口令指纹，作用是给出一条能区分「口令错」与「数据损坏」的信息（改 reserved 里的 salt → 派生密钥变 → 指纹先拒，省一次无用 AES 且不误报成磁盘损坏）；`dataHash = sha256(原始明文)` 在解密解压**后**校验，兜住「密文合法但头部被改写」这一类（把 COMPRESSED 位清掉或谎称压缩：GCM 与指纹都过，但解出的字节对不上 originalSize/dataHash）。有专门用例锁「伪造自洽 keyHash 绕过预检后仍被 GCM 拒」——预检不是安全边界。
- **读侧 DoS 护栏**：迭代次数为 0 或 > 5000000 一律拒绝（0 让 PBKDF2 直接失败，超大值等于让加载方替打包方烧 CPU）；负载长度先按 `compressedSize` 核对，解码后按 `originalSize` 核对，头部说谎早失败早报错。
- **让 Linux 真测到生产代码**：新增平台无关的字节级公共入口 `Packer::buildResourceBytes()` 与 `ResourceLoader::loadScriptFromBytes()`（PE 读写只是给这份字节流换容器，`BeginUpdateResource*`/`FindResourceA` 本身 Windows-only）→ 往返、错口令、篡改三类用例在非 Windows 上执行的就是生产实现本身，而不是另抄的测试副本。
- **顺手修真缺陷：载荷变换顺序改为 compress → encrypt**（读侧镜像 decrypt → decompress）。旧顺序先加密，而此简化压缩器的匹配模型只认「同一字节连出现 ≥4 次」（`data[i - j]` 左下标不随 count 前进），密文里永远找不到字节连串 → `--encrypt --compress` 静默退化成「只加密」。v2 是新版本且历史上 encrypt 被禁，无存量产物受影响。压缩比只泄露「明文可压缩程度」这一弱信号，且是静态一次性压缩、无攻击者可参与的自适应压缩，不构成 CRIME 类面。
- **口令来源与 CLI 语义**：打包侧 `--password <p>`（隐含 `--encrypt`）或环境变量 `WINGMAN_PACK_PASSWORD`；`--encrypt` 无口令在复制 stub **之前**硬拒（没有口令的加密产物再也打不开，宁可不产出），`--no-encrypt` 在 `--password` 之后出现即取消加密并清掉口令。运行加密产物侧读 `WINGMAN_SCRIPT_PASSWORD`（口令一律不进命令行，避免落进 shell 历史与进程列表）。`cli_test.cpp` 里原 `PackerRejectsEncryptedResourcesUntilLoaderSupportsThem` 的语义作废，改写为「有口令放行 / 无口令硬拒」两条；`main.cpp` 顺带修掉「Version/Size 恒打 0」的存量显示 bug（ResourceInfo 要解头之后才填）。
- **本轮自决假设（非交互）**：salt 16B、IV 12B（GCM 推荐 96-bit）、100000 轮、AES-256、`keyHash` 沿用 v1 字段位与语义；ELF/Mach-O 侧的资源嵌入仍明确失败（要产出可分发单文件得另设容器方案，不在本轮范围，不静默写「看着成功其实没嵌脚本」的产物）；不加新依赖（openssl 已在 vcpkg.json）。
- **验证**：新增 `apps/agent/tests/resource_pack_test.cpp` 43 例（格式布局/KDF/写侧标志/参数化往返 明文+加密/负路径口令与篡改与版本）+ `lib/wingman/tests/crypt_test.cpp` +10 例（AES-GCM 原始字节对）+ cli 测试语义改写；全量 ctest **2234 例 0 红**（31 例平台性 skip；共享机 load 33~47 下复跑，Go 门 `go vet` + `go test -race ./...` 同轮全绿，integration 包在高负载下需把 `-timeout` 放到 90m 才跑得完）。
- **本轮暴露的存量 flake（已修，见顶部 2026-09-27 X11 根治轮）**：`X11PlatformTest.X11WindowCloseCenterAndWaitFamily` 在 CPU 超售时不稳（load 55~61 时 10 跑 7 红，两种形状：① `forceClose` 后子进程 5s 护栏超时——fork 子进程 `XFlush` 后未 `XSync`，父进程可能在 X server 处理建窗请求前就 XKillClient，落空则子进程困在自己的 10s select 里；② `center/close/isInitialized` 成片返回 false 的无 WM 时序依赖）。与本轮改动无关（该用例不链接也不触达打包/密码学路径），修法属独立一轮：子进程建窗后补 `XSync`、并把 5s 护栏随超售缩放（同 2026-09-27 dashboard 那轮的口径）。复测数据：load 38~48 → 6 跑 1 红；load 55~61 → 10 跑 7 红；同日另一轮全量 ctest（load ~18）同一用例为绿。根治轮补充定性：两种形状的共同上游是 X server 对断开→重连的 accept 阶段瞬态拒绝，先致门面 `initialized_=false`，①②都是级联；XSync（②→①的次级隐患）与护栏缩放按登记口径落实，主修为 `openX11Display` 生产侧重试。

---

## 2026-09-27 高负载 flake 二轮根治（窗口随 CPU 超售自适应 + 固定 sleep 清除）

上一轮「固定 5s/15s 窗口」口径在更极端负载下再度假红（load ~89、20 会话并行：4 套件 11 用例，login/RemoteFileBrowser 两个新套件也红）。本轮分类复现 + 三处修复：

- **分类（单跑 vs 全量）**：4 个失败套件 `--runInBand` 单跑各 2 次共 8/8 全绿（负载 82~93）→ 纯负载时序；失败形状全部是窗口/预算超时（jest 15s 预算、RTL 5s 窗口），零断言/逻辑失败。4 套件 4-worker 并行在 load ~60 也全绿——红线场景 = 全量 13 worker × 外部负载的超售。
- **根因**：固定 wall-clock 窗口按「独占机器」标定，机器超售 6~7 倍时「条件正确、只是慢」也会超窗。窗口必须随超售程度缩放，而非再取一个更大的固定值。
- **修复 1（窗口自适应，单一来源 `src/testSupport/rtlWindow.ts`）**：窗口 = 5s × 超售系数（1 分钟 loadavg / 核数，向上取整、下限 1、封顶 60s）；`tests/setupRTL.jsx` 的全局 `configure` 与测试文件的显式 `WAIT_TIMEOUT` 都从这里取值；`jest.config.ts` 的 `testTimeout` = 3 × 窗口（下限 15s、封顶 180s，内联同款公式——config 由 jest 原生 ESM 加载，不能走 moduleNameMapper import）。空闲/CI 系数=1，行为与固定 5s/15s 逐位一致；绿路径零额外耗时（条件满足即返回，窗口只是失败上界）。
- **修复 2（真缺陷：固定 sleep）**：`src/pages/User/Login/login.test.tsx` 原来固定 `sleep 200ms` 后直接断言（不走 waitFor，负载下必假红）→ 改条件化 `waitFor` 同时等 token 写入与跳转；死掉的 `waitTime` helper 一并删除。
- **修复 3（显式超时绕过全局）**：RemoteFileBrowser / RemoteDesktop / RemoteDesktopModal 三个测试文件的 `WAIT_TIMEOUT = 5000` 显式超时会绕过全局 configure（永远吃不到自适应值）→ 改引共享常量；RemoteSessionReportModal 的同名常量是零引用死代码，删除。
- **实测**：load 108~129（9 倍超售，超过用户红线场景的 89）全量 jest **420/420**；tsc 0 错、eslint 仅存量 2 警告、prettier 干净（同负载下跑）。**残余风险**：系数在进程启动时采样一次，若运行中负载再涨数倍（起跑后从 129 涨到 250+）理论上仍可能超窗——封顶 60s/180s 是有意为之（防病态挂死），届时重跑即可。

## 2026-09-26 存量 flaky 测试根治（loginPage / triggerFormModal 的 load flake）

上轮远控重构批次遗留的独立任务（该批只放宽了 `RemoteDesktop*` 两文件的等待窗口，未触碰这两个文件）。**根因：纯时序，非逻辑回归**——不需要改任何产品代码。

- **复现（基线，14 核共享机自起 14 个 CPU burner + 外部负载 ~52）**：`npx jest tests/loginPage.test.tsx tests/triggerFormModal.test.tsx` 重复 10 次 → **0/10 通过**。失败形状两段：10/10 命中 jest 默认 **5s 单测预算**（`Exceeded timeout of 5000 ms for a test`）；其中 4/10 另外命中 RTL 默认 **1s `waitFor`/`findBy` 窗口**（`Unable to find an element with the text: Wingman`）。两条都是「wall-clock 猜测」在 CPU 超售下不够用，断言条件本身都指向真实异步条件（antd Form/Modal 校验与提交、登录跳转），无固定 sleep、无 fake timer。
- **修复口径（只放宽窗口，不放宽断言；不做 fake timers）**：`jest.config.ts` 加 `testTimeout: 15000`；新增 `tests/setupRTL.jsx` 里 `configure({ asyncUtilTimeout: 5000 })` 全局放宽 RTL 等待窗口——它同时覆盖 `tests/` 与 `src/` 下全部 `waitFor`/`findBy` 站点（24 处），无需逐文件改写。**否决 fake timers**：antd Form/Modal 内部异步链在 fake timers 下需手工推进会侵入组件行为，且等待条件本就指向真实条件，不增确定性只增脆弱。
- **踩过的坑（必须记，别再踩）**：`configure` 最初写在 `setupFiles`（`setupTests.jsx`）里 → 5 个用例**确定性**失败（弹窗跨用例堆积、`Found multiple elements`、按钮点到上一个用例的旧弹窗）。根因：RTL 在**首次 import 时**以 `typeof afterEach === 'function'` 决定是否注册自动 cleanup，而 `setupFiles` 阶段 jest 测试框架尚未安装、`afterEach` 不存在——在那里提前 import 会**永久禁用自动 cleanup**。故 `configure` 必须挂在 `setupFilesAfterEnv`（新文件 `tests/setupRTL.jsx`），头注释已写死这条约束。
- **验证**：同款高负载下（实测 load average 68，高于基线的 52）重复跑 10 次 → **10/10 全绿，每次 21/21 用例**。随后全量 dashboard 套件：jest **420/420**（28 套件，75.5s）、`tsc --noEmit` 0 错、eslint 仅存量 2 警告、prettier 干净。本轮未动 C++/Go，未重跑。

## 2026-09-26 浏览器内录像回放（设计 §16「回放」第二版）

录像列表加「回放」入口（desktop:view 同级，取回即 `desktop.recording_download` 审计）→ `RemoteRecordingPlayer` 拉取 Blob → `guacamole-common-js` 自带 `SessionRecording` 本地解析回放（播放/暂停/拖动进度 + 画面等比缩放，纯浏览器本地行为不经网关、无注入面）：

- **上游缺陷绕行（关键）**：1.5.0 `SessionRecording` 的 Blob 直连分支把从未赋值的 `recordingBlob` 交给解析器（缺 `recordingBlob = source`），构造即抛 TypeError，npm 无更新版本。`recordingPlayer.ts` 的 `BlobRecordingTunnel` 把 Blob 转成隧道源（官方 Parser 解析 → 逐条喂 → 读完发 CLOSED），走官方播放器「边下边播」的隧道分支；测试用真实库 + 样例录像锁契约，含「Blob 直连构造即抛」防退化用例。
- **样例录像**：本机无任何真实 .mjs（recordings 卷恒空），按用户许可造最小样例 `deployments/guacd/recordings/sample-session.mjs`（`gen-sample-recording.js` 生成，12 帧/4.4s/1280x720 移动色带）——栈启动后回放面板即有真实条目；录像格式 = 裸指令流、`sync,<毫秒>` 分帧的契约经 dist/esm 源码与真实解析双重验证。
- **顺手修一处存量缺陷**：`display.scale` 在 1.5.0 是方法而非可写属性，`useGuacamoleSession` 原来赋值式调用让监看/接管的画面自适应缩放从未生效（静默顶掉方法）；改调用式并对齐类型声明。不做倍速：1.5.0 无变速 API。
- 服务层 `fetchRecordingBlob`（下载复用同一取回路径）；jest 401 → 420（新增回放封装 8 用例 + 组件 9 用例 + 服务 2 用例），录制语法/播放状态机/组件三态全锁。

## 2026-09-26 阶段二页面层 i18n 接线

Agents 页的阶段二文案（连接表单「会话录制」勾选 + 会话录像面板 19 条）改走 umi intl（`pages.agents.recordings.*`），八份 locale（zh-CN/zh-TW/en-US/ja-JP/pt-BR/bn-BD/fa-IR/id-ID）全量补齐，`{name}` 插值沿用同页先例；纯文案接线，zh-CN 渲染文本逐字不变。

- **组件层明确不接（留档）**：`RemoteDesktop/*` 是给 cockpit 复用的公共件，全目录零 `@umijs/max` 依赖是既成设计；umi `useIntl` 在 jest 下不可用（`.umi` 不入库、react-intl 未 hoist），接 intl 要么加 react-intl 直依赖要么文案 props 化 + 补测试基建——架构代价大于收益。回放面板/文件浏览器/剪贴板等组件文案维持中文硬编码，多语言留给消费方。

## 2026-09-26 文件浏览器第二版（分页/进度/重试/文件操作审计）

SSH/SFTP 文件浏览器在第一版（commit cc7d094，设计 §15.1）之上继续收口「协议内可做」的四件事（设计 §15.2）：

- **大目录**：协议一次 `get` 返回整个目录体（**无服务端分页**），分页做在浏览器侧（每页 50 行、小列表自动隐藏翻页）；目录体聚合加 8 MiB 护栏，超限拒绝并停止 ack——ack 纪律即流控，guacd 自然停发。
- **关键协议修正（v1 遗留缺陷）**：1.5.0 `BlobWriter` 对错误 ack 只停发不报错（`onerror` 仅本地读文件失败触发），v1 上传失败路径是死代码——v2 从 `writer.onack`（`status.code !== 0`）判定失败 + 无进展看门狗（默认 60s、可注入、0 关闭）兜底；下载侧停滞检测同靠看门狗。进度：下载逐块上报已收字节、上传走 `BlobWriter.onprogress`；`withRetry` 有限重试（默认 2 次按序退避），尝试次数进审计。
- **文件操作审计**：新增 `POST /api/remote/file-ops`（desktop:view 组；upload 内联追加 desktop:control，监看可下载/上传仅接管的矩阵在审计通道同样成立）。服务端按票据反解会话快照（WS 建连入库、closeSession 清除；请求体同类字段不采信），反解不到落 `no_session` 降级行；审计 kind `desktop.file_download`/`desktop.file_upload`；列表高频不审计。
- **协议边界不动**：删除/重命名仍不可行（对象流只有 get/put）、服务端分页不存在，不硬做；远期项（cockpit 接入、回放、i18n）不受影响。

## 2026-09-25 VNC/SSH/RDP 远控集成（Guacamole，P0 + 阶段二）

第三方远控方案选型 **Apache Guacamole（guacd 1.5.5）**：浏览器侧 guacamole-common-js 像素面 + Go server 反代 WS 网关 + guacd 协议翻译（RDP/VNC/SSH 三协议客户端在 guacd 内实现）。runtime 零参与（像素面与控制面正交，架构硬约束不破）。设计文档 `docs/remote-gateway-guacamole-design.md`（§9 备选方案：myrtille/Apache 老栈、websockify+novnc、自行开发三协议客户端均否决的理由）。

- **P0（commit 20fdb30，2026-09-23）**：一次性 5 分钟票据（`remoteticket.Manager`）+ RBAC（desktop:view 监看 / desktop:control 接管）+ 网关 WS 隧道（`/api/remote/guacamole`，票据即凭证）+ Dashboard RemoteDesktopModal + 三协议 e2e（`WINGMAN_GUACD_E2E=1` 门控，需容器栈）。
- **阶段二（本轮，DG-7/8/9）**：
  - **剪贴板（§14）**：common-js `onclipboard` 收（逐块 ack）+ `createClipboardStream` 发；监看模式隐藏发送 UI；仅 text/*。
  - **文件传输（§15）**：SSH 经 SFTP（`enable-sftp=true`）、RDP 经驱动器重定向（`enable-drive`+`drive-path`）、VNC 无通道（RFB 协议层没有，UI 整块隐藏）；上传 `createFileStream`+`BlobWriter`，下载 `onfile` 聚合 Blob。
  - **会话录制（§16）**：record 票据 → connect 注入 `recording-path/name`；安全默认 `recording-include-keys=false`（按键永不入录像）；检索 API `/api/remote/recordings`（list/download=desktop:view，delete=desktop:control）+ Dashboard 录像管理；`deployments/guacd` 增加 drive/recordings 共享卷。
  - 服务端新增 mock-guacd 握手测试（讲线协议的假 guacd 验证 connect 按位注入）+ recordings handler 全路径测试；Swagger 注解补齐全部 remote REST 端点并再生成。
- **验证基线**：Go 14 包 `go vet` + `go test -race` 全过；Dashboard jest 261/261、tsc 0 错、eslint 仅存量 2 警告、prettier 干净。
- **剩余（P1/远期）**：cockpit 接入像素面公共组件（公共件已抽出，wingman 侧先行 ✅）、浏览器内录像回放（✅ 2026-09-26 实现，见「浏览器内录像回放」）、i18n 接线（✅ 2026-09-26 页面层完成，组件层经裁定不接，见「阶段二页面层 i18n 接线」）。

## 2026-09-25 远控 P1 推进（公共件抽取 / 会话审计 / e2e 容器栈）

设计 §11 P1 三项中**可独立完成**的部分（cockpit 实际接入需对方仓库，不在本轮）：

- **前端公共组件抽取**（设计 §7.3 落地，记 §7.1）：新建 `orchestrator/dashboard/src/components/RemoteDesktop/`，四件边界一一对应——`useGuacamoleSession`（连接生命周期 hook，票据一次性故无自动重连）、`TicketClient` 接口 + `createWingmanTicketClient`（票据客户端，接口化以便第二方换 API base）、`RemoteErrorNotice` + `classifyRemoteError`（错误与降级，权限/未配置类不渲染重试）、`RemoteDesktopToolbar`（监看接管与工具栏）。`RemoteDesktopPanel` 为容器无关合成件，`RemoteDesktopModal` 降级为 Modal 容器适配器；`RemoteProtocol`/`RemoteSessionParams` 收敛到公共件 `types.ts` 唯一定义（消除协议枚举分叉）。jest 261 → 328，新/改文件四项覆盖率 100%。
- **会话审计落库与查询接口**（设计 §17）：新增 `models.RemoteSessionAudit`（表 `remote_session_audits`）——**不复用 AuditLog**（事件流水的 meta 是 JSON，聚合既慢又脆；两者受众不同，缺一不可）。只写终态（closed/failed），进行中不落行（否则报表把未结束会话算进时长、崩溃留不闭合脏行）；`RecordRemoteSession` 唯一落库入口（UTC 归一/时长口径/终态枚举是契约），写失败只记日志不阻断会话关闭。`GET /api/remote/sessions`（desktop:view，**不新增 RBAC 码**——报表只读、无接管能力）一次返回列表 + 汇总 + 维度聚合 + 时间趋势四块视图，四条查询共用同一过滤器，汇总/分组/分桶基于全量而非当页；`groupBy` 白名单化杜绝 SQL 注入。录像名与会话同源可关联；会话 ID 提前到拨号前生成，故建连失败也有唯一标识。Dashboard 新增 `RemoteSessionReportModal`（Agents 页「会话审计」入口，录像管文件、报表管行为）。jest 328 → 351，新文件四项覆盖率 100%。
- **三协议 e2e 容器栈补强**（设计 §13.4）：四个目标端点镜像**全部钉 digest**（除 guacd 外原为 `latest`——上游重建会静默换内容，e2e 在无人改代码时变红，排查方向先怀疑网关）；新增一键脚本 `scripts/verify-guacd-e2e.sh`（up/test/run/down/status，解决 `compose up -d` 早于端口监听返回导致「栈没起好」与「代码有 bug」形状无法区分）；四服务补 healthcheck（bash `/dev/tcp`，不依赖 curl/nc）。契约由 10 个 Go 用例锁住（假引擎 + 净化 PATH，0.5s 跑完，不碰真容器）。
- **验证基线**：Go 14 包 `go vet` + `go test -race` 全过；Dashboard jest 351/351、tsc 0 错、eslint 仅存量 2 警告、prettier 干净；Go 侧新增/改动文件覆盖率 100%（remote_session.go 全函数 100%）。
- **未完成（需真机/容器环境）**：C++ 全量测试本机无预编译产物（`core_tests_NOT_BUILT`），本轮未改 C++；e2e 三协议链路实测需 `scripts/verify-guacd-e2e.sh run`（本机 docker 无 compose 插件，脚本正确报 SKIP）。


---

## 2026-09-22 真机验证自动化（架构盘点第九轮）

两项真机验证遗留项从「纯人工多步操作」升级为「真机各跑一条命令」，自动化链路本身已在本机验证：

- **XRecord 正向端到端用例**（`recorder_x11_e2e_test.cpp`，仅 Linux 编译，注册于 tests CMake `UNIX AND NOT APPLE` 块）：XTest 注入按键（优先 F13 防误触焦点窗口，键码缺失回退 'a'）→ 轮询捕获计数 → `saveToJSON` 内容精确断言（`"type": 5` 即 KeyDown——x11_recorder 回调只把 KeyPress 映射为 KeyDown、KeyRelease 丢弃；`"keyCode": <注入键码>` 带字段名匹配防 timestamp 数字误命中）。Xvfb 下 EnableContext 必然失败 → GTEST_SKIP；本机双场景实测（无 DISPLAY、Xvfb :99）均正确 skip 且不误报失败，Xvfb 场景 ~300ms 耗时证明真实走过 EnableContext 尝试路径。与 XTest 注入共用 `X11ServerLockGuard`，ctest -j 下与窗口/剪贴板测试串行化。
- **一键验证脚本**：`scripts/verify-xrecord-desktop.sh`（Linux + DISPLAY 守卫；用例全 skip 判「未验证」exit 2 不放绿；`--build` 强制重建）；`scripts/verify-macos-runtime.sh`（darwin + VCPKG_ROOT 守卫——缺失即报错不回退系统库，triplet 按 uname -m 自动选，跑 Clipboard/FileWatcher/Screen/Input/UnixSocketChannel 五套件，附 CGEvent 授权等人工观察项提示）。脚本三态自测通过（本机 SKIP、Xvfb SKIP 均不放绿）。
- **回归确认**：core_tests 增量编译通过；既有 `MacroRecorder*`/`*Recorder*` 用例无回归（19 个 skip 为录制后端不可用的预期降级，其余全 OK）。

剩余：真桌面 Linux 跑 `scripts/verify-xrecord-desktop.sh`、macOS 跑 `scripts/verify-macos-runtime.sh`；回放时序手感、CGEvent 辅助功能授权、录屏授权弹窗等仍属真机人工观察。

---

## 2026-09-22 死代码清理（架构盘点第一轮）

架构盘点结论：宏架构（双控制面 / platform 抽象 / apps+lib / orchestrator 边界）合理且执行到位；主要问题为死代码撑起的虚假复杂度、lib/libs 边界失效、Android 源码级耦合。本轮完成第一优先级：

- **protobuf 链移除**：`protobuf/`（3 个 .proto 无人消费）+ `libs/proto`（`PROTO_PATH` 指向不存在的根目录 `proto/`，protoc 从未生成代码；孤儿文件 proto_wrapper_json.cpp）。实际协议 = 16 字节头 + JSON 体（C++ `MessageHeader` 与 Go `pkg/agent/client.go` 手写对齐，由 `integration/protocol_test.go` 兜底）。若未来需要 IDL，须双侧生成后一并落地。
- **libs/debug 移除**：EmmyAdapter 无任何调用方（runtime 仅链接未使用，自带测试也不链接它）；`WINGMAN_ENABLE_EMMY`/`WINGMAN_HAS_DEBUG` 宏零源码消费者。实际调试链路 = 脚本运行时 require `emmy_core`。
- **clasp 移除**：submodule + vcpkg overlay port 双落位、全仓库零引用。
- 同步清理：根/runtime/lua CMake 选项与链接、`vcpkg.json` protobuf、CI compat 目标、`build-scripts` 安装列表、platform_boundary_allowlist（-1 条）、BUILD.md / setup / DEVELOPMENT / project-structure / architecture / remote_protocol / debugging 文档、CHANGELOG Unreleased。
- Go 侧 `google.golang.org/protobuf // indirect` 为 gin/swag 传递依赖，非死代码，保留。

---

## 2026-09-22 M4/M5 收尾校准（架构盘点第八轮）

**基线验证（三套 UI 测试全绿）**：Dashboard jest 235/235、GUI vitest 506/506、Tauri Rust cargo test 14/14（含 IPC 集成 5 用例）。

**M4 三层契约审计（跨端对齐，无缺口）**：
- 命令方向（server → runtime）：server 生产代码下发 `run_script`/`stop_script`/`list_windows`/`get_status`/`screenshot.capture`/`system.shutdown`/`trigger.*` 共 7 类，`agent.cpp handleRemoteCommand` 全部支持（trigger.* 前缀经 remoteDispatcher 复用本地 RPC handler）。
- 事件上行（runtime → server）：runtime 仅转发 `trigger_fired`/`script_state`/`script_output` 三类（`log.line`/`connection.state_changed` 有意不转发——防高频日志淹没 agent 上行链路，agent.cpp:385 注释明确），server `handleEvent` 三 case 全接并广播。
- 广播下行（server → Dashboard）：server 广播 agent connected/disconnected/status_changed、workflow submitted/status_changed/progress、script output/state_changed、trigger_fired、screenshot 共 9 类，Dashboard websocket.ts 全部消费。

**文档校准**：ROADMAP M4/M5 状态 🚧 → ✅（附审计与测试证据）；删除 M4 RBAC 条目过时的「⚠️ PermissionRequired 已实现未接线」注（已接线 8 权限码）；todo.md 里程碑表 M4 100% / M5 ~95%，完成度表权限系统 95% → 100%（Swagger 47 端点已全注解）。

**剩余**（用户确认跳过）：macOS 真机运行时验证、Linux XRecord 真桌面验证——均需真机人工执行。

---

## 2026-09-22 文档站构建验证与死链修复（架构盘点第七轮）

第五轮改 VitePress sidebar 后未实际构建——本轮补上：`npm run docs:build` 成功（83.6s，仅 chunk 体积警告）；`ignoreDeadLinks: true` 会静默放过死链，故另写全量站内链接扫描（206 条链接，排除 node_modules/dist/锚点/外链）：发现 4 条真死链并修复——`guide/getting-started.md` 的架构决策链接多跳一级（`../../` → `../`）、`guides/database.md` 与 `guides/configuration.md` 引用不存在的 `api/storage.md`/`api/serialization.md`（与第五轮 docs/README 同款历史错误，改指 api/db.md、api/serialize.md）。复扫真死链 0。另将「测试」段两项实质完成（C++ runtime 保持水准、三条集成测试全 ✅）按事实勾选。剩余未勾项仅 macOS 真机验证与 Linux XRecord 真桌面验证两项，均需真机人工执行。

---

## 2026-09-22 handlers 评估与辅助函数收口（架构盘点第六轮）——⑤ 关闭

**评估结论：不拆 Go 子包，⑤ 关闭。** 实测数据：46 文件全部 `package handlers` 单包、一域一文件（21 个生产文件命名即导航）、仅依赖 gin + gorm + 内部 models/middleware/rbac/security；测试 6300+ 行全为黑盒 HTTP 测试（经 gin 路由发请求，coverage_*×11 引用 handler 符号数为 0），天然依附路由装配点 routes.go。拆包成本 = 9 个 setup helper 重排 + coverage 跨域文件拆散归属 + 共享辅助抽包 + routes.go import 全部子包，收益仅目录观感——单包 HTTP 层是 Go 惯用模式（net/http 同例），维持现状。

**顺手收口**：跨文件共享的辅助函数归位新建 `helpers.go`——`parsePositiveInt`（audit/messages/users 三域共用）、`actorName` + `isUniqueConstraint`（roles/users 共用）；`WriteAuditLog` 留 audit.go（横切领域函数，语义归属正确）。单文件私有辅助不动。

验证：gofmt 0、go vet 0、14 包全过（含 integration 完整跑）。

---

## 2026-09-22 文档去重与会话产物清理（架构盘点第五轮）

共删除 15 个文件，全仓交叉引用清零校验通过：

- **根目录会话产物删除**：`macOS_SESSION_SUMMARY.md`、`macOS_VERIFICATION_REPORT.md`（2026-06-22 macOS 验证会话产物，零引用）；`docs/superpowers/`（5 份 2026-06-27/28 会话计划/交接文档，仅自引用）。
- **时令文档删除**：`docs/pending-changes.md`（2026-06-21 一次性改动分析，todo.md 引用改纯文字）、`docs/project-improvements-2026-05-06.md`、`docs/project-improvements-plan.md`（零引用）、`docs/architecture-improvement-plan.md`（自标"历史设计草案，部分内容已过期"）。
- **getting-started 去重**：根级 `docs/getting-started.md` 删除（内容较旧），保留 `docs/guide/getting-started.md`（站点 sidebar 主文档，含 CLI 参数/运行模式/agent.toml 等新架构内容）；README×2、api/overview、api/core、docs/README 共 5 处引用改指新路径。
- **安装文档三份收口为 BUILD.md**：删 `docs/installation.md`（312 行，2026-06-23 旧版）与 `docs/setup.md`（131 行，零引用）；installation 独有的 macOS/Linux 故障排除（Xcode-select、权限、系统依赖、vcpkg 失败）并入 BUILD.md；README 安装入口改指 BUILD.md（CONTRIBUTING/project-structure 原已指向它）。
- **user-guide.md 删除**：385 行大全式手册，安装/配置/API/调试各章均已被站点对应专篇覆盖且更新。
- **guides 三孤儿挂上站点**：`guides/configuration.md`、`guides/database.md`、`guides/triggers.md` 加入 VitePress sidebar「进阶指南」组——与 `guide/config.md` 主题正交（前者是 wingman.config API 实践教程，后者是 config.json 配置文件参考），去重结论为"挂出来"而非合并。
- **死链修复**：docs/README.md 索引引用的 `api/storage.md`、`api/serialization.md`、`api/debugging.md` 三个不存在的文件，改指实际存在的 kv/db、serialize/json/ini、debugger。

---

## 2026-09-22 Go 包收敛与路由装配收口（架构盘点第四轮）

- **`pkg/agent` 并入 `internal/agent`**：同名词包分居两处（listener/team/client 在 pkg，registry/types 在 internal），实为同一条 runtime 接入链路的两半，靠接口跨包解耦。合并后单包 20 文件（线协议类型 / FrameListener / TeamManager / Registry / types），`pkg/agent` 消失；`Broadcaster`/`AgentRegistrar` 接口保留（依赖倒置，注释已更新为准确表述）。消费方 import 与 `agentPkg` 别名全部清理（main.go 同包双别名导入一并消除）。
- **main.go 路由装配收口**：gin 中间件、静态资源与全部 API 路由（~230 行）从 main.go 抽至 `internal/handlers/routes.go`（`RegisterRoutes(r, RouterDeps)`），main.go 415 → 178 行，回到"配置 + 组件生命周期"职责；装配点集中是后续 handlers 按域拆子包的前置。**决策**：handlers 不立即拆多包——21 文件已一域一 Handler，拆包主要成本在 5900 行测试与 coverage_* 跨域用例重排，当前收益不足，列为后续评估项。
- gofmt 全仓修齐（含 3 个历史遗留未格式化文件）；go vet 0、全部 Go 测试通过（integration / handlers / workflow / agent 等 14 包）。

---

## 2026-09-22 平行实现合并（架构盘点第三轮）

盘点假设"双 TCP 通道"经查证**不成立**：`lib/wingman` 的 `TcpChannel` 是本地 IPC（IIpcChannel 家族）的显式 fallback（`allowTcpFallback` 安全闸、纯 JSON 无帧头），与 `libs/transport`（远程链路、16 字节头 + JSON）职责正交，保留。真正合并的两处：

- **双 system_handler 删除**：lib/wingman 的 `wingman/rpc/system_handler` 是 stub 版（`system.getStatus` 返回硬编码假数据），在 LocalIpcServer 注册后立即被 runtime 版静默覆盖（`registerHandler` 为 map 赋值，后注册覆盖先注册）——假数据陷阱。整层删除；`system.getVersion` 并入 runtime 版统一提供，协议方法不减；用例自 rpc_test 迁移为 `apps/agent/tests/system_handler_test.cpp`（3 用例，覆盖 providers 空注入回退值）。
- **XOR 混淆遗留移除（breaking）**：`SecurityManager::encryptString/decryptString` + Lua `security.encryptString/decryptString` 删除（XOR 非真实加密，已 deprecated + 运行时告警多年，生产 C++ 零调用）；加密统一走 `crypto.encryptAES/decryptAES`（AES-256-GCM）。**手写 SHA-256 收口**：security.cpp 内 60 行手写实现删除，`hashString` 改调 `wingman::crypt::sha256`（OpenSSL EVP），输出格式不变。security_test 清 14 个 XOR 用例（保留 GenerateRandomStringsAreDifferent）、script_function_test 清 1 个、docs/api/security.md 同步。

---

## 2026-09-22 Android agent 核心下沉（架构盘点第二轮）

消除 `apps/android/cpp` 对 `apps/agent/src` + `lib/wingman/src` 私有树的源码摘编，共用核心改为正经库目标：

- **`libs/agentcore`（新）**：`remote_client` / `event_buffer` / `remote_client_config` 自 `apps/agent` git mv 下沉，命名空间保持 `wingman::runtime`；仅依赖 transport + spdlog/nlohmann（硬约束：不得依赖 lib/wingman 本体，NDK 侧不编 core）。`apps/agent/config.hpp` 引入下沉后的 `RemoteClientConfig` 维持 `AgentConfig` 完整定义。
- **`libs/androidagent`（新）**：`script_runner` / `android_script_api` 自 `apps/android/cpp/agent` git mv 下沉；并收口全仓库唯一的 lib/wingman cherry-pick 清单（bitmap / screen / image_analyzer / platform/android 租户），消费方不得再自行向 `lib/wingman/src` 加 include。OpenCV 判定与 core 严格一致，重复编译 TU 配置相同、链接器仅取其一。
- **消费方收口**：`apps/android/cpp`（NDK 壳）只链 transport/agentcore/androidagent 三库；`libs/lua/tests` 经 `wingman::androidagent` 桌面同源编译全部 androidagent 源（script_runner_test / android_api_test 22 用例，替代 NDK 验证所有可宿主 TU）。
- 边界检查通过（223 文件，allowlist 仅路径更新）；runtime_tests 29/29；NDK 专属 TU（jni_bridge / android_screenshot）依赖 NDK sysroot 头，由 CI build-android 覆盖。

---

## 位置澄清（重要）

| 路径 | 实际内容 |
|------|----------|
| `orchestrator/server/` | Go 远程中控（HTTP API + WebSocket + Agent TCP 监听） |
| `orchestrator/dashboard/` | **真正的 wingman 远程 Dashboard**（React/Umi/Ant Design Pro，含 Agents/Monitor/Scripts/Workflows 页面 + wsService + wingman.ts） |
| `apps/gui/` | 本地 Tauri GUI（Svelte 5），通过 local IPC 控制 runtime |
| `dashboard/`（仓库根） | ⚠️ 另一个产品 **Croupier** 的副本，已被 `.gitignore` 排除，与 wingman 无关，勿改 |
| ~~`dashboard_old/`~~ | 已删除（空目录） |

---

## 里程碑规划（已校准）

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

## P0 - 必须完成（阻塞交付）

### RBAC 权限系统（Go orchestrator）— [已完成 (2026-06-20]

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

### Runtime IPC 事件推送（C++ runtime [Tauri GUI）— ✅ 机制完成 (2026-06-20]

现状：runtime 通过 RPC `events.drain` 暴露缓冲事件，GUI 轮询拉取并分发。采用 **pull 模型**
（非 type=2 push），避免 Rust IPC 客户端在 Windows 阻塞 IO 下引入异步读取循环导致帧错位
（设计决策见 `docs/architecture-decisions.md` "Runtime-to-UI Event Delivery"）。

- [x] **事件缓冲**：`libs/agentcore/include/wingman/agentcore/event_buffer.hpp`（线程安全有界队列，上限 1000）
- [x] **日志事件**：spdlog `EventLogSink` → `log.line`（main.cpp 已挂载）
- [x] **触发器事件**：`TriggerManager::setOnFired` 回调（win32+posix）→ `trigger.fired`（local_ipc_server 已接 EventBuffer）
- [x] **drain RPC**：`events.drain` handler（local_ipc_server 已注册）
- [x] **Rust 命令**：`commands::events::drain_events`（main.rs 已注册）
- [x] **GUI 分发**：`stores/events.ts` 轮询器（500ms）→ `logs.addRuntime` + `triggers.markFired`；App.svelte 按连接启停
- [x] **logs 页面**：`logs.ts` 增加 `addRuntime` + 容量上限（1000 条）
- [x] **脚本状态**：`script.state_changed`（StandaloneMode 各状态转换已接 EventBuffer）
- [x] **截图事件**：`screenshot.frame` —— 经评估**不接入 drain 缓冲**（全屏 base64 大负载会淹没有界缓冲；截图保持按需 `screenshot.capture`，见 architecture-decisions.md）
- [x] IPC 调用超时处理（Rust `IpcClient` 已有 30s 超时；GUI `connection.refresh` 捕获错误并置 disconnected）

### Debugger 端点实现（Go orchestrator）— [完成（直连模式，2026-06-20]

现状：EmmyLua 调试由 VSCode 直连 runtime:9966，Go server 不中转调试协议（双向流不适合
dashboard → server → agent 请求/响应模型）。原裸 501 stub 已替换为结构化「直连模式」契约。

- [x] `GET /api/debugger/info`：返回调试模式说明 + 各 agent 调试端点（host:9966）+ VSCode launch.json 片段
- [x] `connect/command/breakpoints`：返回结构化 501，指向直连模式（不再是裸 stub）
- [x] 测试：`debugger_test.go`（info 返回 direct_attach + agent 端点；connect 返回指引）
- [x] README 文档更新

---

## 代码缺陷（2026-06-21 分析发现，已全部修复）

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

## P1 - 高优先级（功能完善）

### Orchestrator Dashboard（`orchestrator/dashboard/`）收尾

**已完成页面**（已对接真实 API + WebSocket，勿重做）：
- [Welcome、Agents（getAgents + shutdownAgent + WS 事件）]
- [Scripts（Monaco 编辑器 + CRUD + run/stop/logs）]
- [Workflows（submit/cancel + WS submitted/status_changed/progress + Steps 可视化）]
- [Admin/LoginLogs、Admin/OperationLogs（listAudit + CSV 导出）]
- [User/Login、Profile（7 tab，最完整）]

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
  - [x] 工作流模板库（`GET /api/workflow-templates`：6 内置模板 + dashboard 模板选择器）
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

## P2 - 中优先级（功能增强）

### 跨平台验证

> 编译跨平台已由 CI 矩阵保证（C++ Ubuntu/macOS、Go 三平台、Dashboard 三平台打包）。
> 以下为各平台**运行时功能**的人工/集成验证（需在实际 OS 上执行）：

- [x] **macOS**（基础实现已存在，装配断链已全部接线 2026-09-16）：UDS / Clipboard / CGWindowList 截图 / FileWatcher / CGEvent 输入 — **已验证（2026-09-30，GitHub Actions macos-latest runner 即真实 macOS 硬件）**：`.github/workflows/verify-platforms.yml`（workflow_dispatch，可选 ref）跑 `scripts/verify-macos-runtime.sh --build`（vcpkg manifest 构建 core_tests + 五套件）。首跑抓出真缺陷——文件列表用例喂挂空路径，macOS 后端 `fileURLWithPath:` 生成无效 file URL、写入声明成功读回为空（已修：改真实临时文件，1f0f976）；修复分支复跑 **78/78 全绿**（run 36715059719）。仍属人工观察项（无人值守 runner 无法授予 TCC）：CGEvent 辅助功能授权、录屏授权弹窗、activate 后台激活语义——**操作指引见 [docs/guides/manual-verification.md](docs/guides/manual-verification.md)**
- [x] **macOS 三后端装配断链**（2026-09-16 接线，2026-09-17 编译验证闭环）：`cocoa_clipboard.cpp`/`cocoa_window.cpp`/`fsevents_filewatcher.cpp` 均为「实现完整但无工厂导出、全库零消费者」，顶层 `Clipboard`/`Window`/`FileWatcher` 在 macOS 恒落 Null/空 stub（与 Linux 同款缺陷，2026-09-14/15 Linux 侧已修）。三平台源文件补工厂导出（同 linux x11_factory 模式：new + initialize() 后交 unique_ptr，无公开头文件，facade 经前向声明消费）；`clipboard.cpp`/`window.cpp`/`filewatcher.cpp` Apple 分支接入——`window.cpp` 删除 macOS 恒空 stub 段改 `windowBackend()` 统一分派（X11/Cocoa/其余平台恒空）。**接线后实现审查**（首次获得真实消费者，2026-09-17）：① `fsevents_filewatcher.cpp` 回调空壳（P0——构造 FileChange 后仅打日志从不调用回调，watch 假成功）已修：回调载荷移入堆上 shared_ptr 控制块（CF 回调只给 void*，与 map 生命周期解耦）、每 stream 专用串行队列保序、锁内摘条目锁外停流释放、补 Start 返回值检查，全模式对齐 win32/inotify；② `cocoa_clipboard.cpp` MRC 泄漏两处（getHTML NSString/setImage NSImage alloc 无 release，本文件无 fobjc-arc）+ setImage 补 buffer 尺寸校验（防 CGBitmapContext 读越界）；③ `cocoa_window.cpp` 质量可接受（activate 后台激活常无效、hide 实为最小化等语义疑点归真机验证）。验证：Linux 侧全量无回归（vision 构建 Xvfb 真跑 1929/1929、stub 构建通过、主线 CI 全绿）；macOS 分支 CI `cpp-compat` 只编译 proto/transport 不含 lib，**真实编译验证由 Nightly 三平台全量构建完成**——接线提交与修复提交两轮 Nightly 的 macOS job Build 步骤均 success；运行时行为（CGEvent 需 Accessibility 权限、CGDisplay 非 GUI 会话受限）仍待真机
- [x] **Linux 截图装配断链**（2026-09-15 修复）：`screen.cpp` 删除两个恒 nullptr 的 stub Screen 段（有/无 vision 重复），合并为单个 `#if __linux__` 实现接线 X11Capture——`Screen::capture/capture(region)/getPixel/findColor/findColors/getScreen*` 全部可用（Xvfb 验证，j4 全量 1894/1894）；X11Capture 加宽容 X error handler（越界坐标 BadMatch 不再杀进程）。遗留 `findImage`/screenshot JPEG 已于 2026-09-15 解决（见下条 vision 接线）
- [x] **Linux 窗口管理装配断链**（2026-09-15 修复）：`window.cpp` 非 Windows 分支恒空 stub，X11Window 有完整实现却全库零消费者（与 Clipboard/Screen 同款缺陷）。Linux 分支改为经工厂转发 X11Window——`Window::enumerate/find/findAll/getForeground/getTitle/getBounds/setBounds/move/resize/activate/minimize/maximize/restore/close/waitFor/waitClose/isVisible/isValid/isForeground` 全部可用，消费者 `agent.cpp list_windows` 与 Lua `getWindows` 自动受益；语义对齐 Windows 分支（enumerate 只列可见顶层窗口、写操作对无效句柄返回 false 不再假成功）；X11Window 加宽容 X error handler（旧句柄 BadWindow 不再 exit 杀进程）。Xvfb 验证（测试进程直写根窗口 EWMH 属性模拟 WM，新增 4 用例 + flock 串行化，j4 全量 1898/1898）。真实 WM 集成测试补齐（2026-09-15，`X11WmIntegrationTest` 3 用例：minimize↔show 状态翻转、maximize/restore 原子与宽度断言、activate→_NET_ACTIVE_WINDOW 前台轮询 + WM 自维护 _NET_CLIENT_LIST 枚举；环境自起自毁 Xvfb+openbox 子进程，flock 串行化 + display 归属校验 + 失败换号整体重建；openbox 启动窗口静默 600ms 规避外来连接竞态——50ms 高频探测轮询实测 ~40% 间歇失败，静默后 40/40 稳定；压测 40 轮零 flake，DISPLAY=:99 全量 1901/1901）。macOS 同款断链已于 2026-09-16 接线（见上条三后端装配断链）
- [x] **Linux 宏录制（XRecord）真桌面验证**：**已验证（2026-09-30，本机 Xvfb :98 实跑 `scripts/verify-xrecord-desktop.sh` PASS exit 0）**——旧口径「Xvfb 的 RECORD 扩展存在但 EnableContext 必然失败（XRecordBadContext）」已过时：2026-09-24 修复录制启动时序缺陷（清零标志误在 EnableContextAsync 之后）后 Xvfb（RECORD 1.13）实测可用，正向捕获闭环 `RecorderX11E2E.*` 在 Xvfb 下真实执行而非 SKIP。产品对坏环境的优雅降级不变（`isRecording()` 回落 false，不再 exit 进程，2026-09-14）。剩回放（playback）时序手感人工观察——**操作指引见 [docs/guides/manual-verification.md](docs/guides/manual-verification.md)**（前置条件、焦点注意事项、预期输出与通过判定）
- [x] **Linux 运行时功能自动化验证**（2026-09-14，Xvfb 1280x800x24 真跑，无 X 环境 GTEST_SKIP；j4 并行与串行、带/不带 DISPLAY 四套矩阵全绿 1890/1890）：UDS IPC（`ipc_test`/`unix_socket_channel_test` 真 backend 真跑）；inotify FileWatcher（12 用例）；X11 三件套 + 装配（`platform_x11_test.cpp` 7 用例：createPlatformScreen 显示器元数据 / X11Capture XGetImage 真捕获（全屏=显示器 bounds、区域 64x64）/ XTest 鼠标移动 XQueryPointer 回读 + 按键 XQueryKeymap 状态 / 顶层 Clipboard 装配为 X11/xclip / **xclip 文本回读真跑通过**（xclip 已装；未装环境 setText 优雅失败、测试 skip））。附带修复三个实测暴露的缺陷：xclip daemon 继承输出管道致 EOF 挂起、XRecord 坏环境下 exit 进程、剪贴板并行测试缺跨进程锁（flock 守卫）。桌面人工验证（多显示器/真实键鼠/XRecord 正向路径）待真机
- [x] **Linux OpenCV vision 构建接线**（2026-09-15 完成）：vcpkg 新增 `vision` feature 承载 Linux opencv4（`-DVCPKG_MANIFEST_FEATURES="tests;vision"` 启用，源码编译 ~15-30 分钟；Windows 顶层依赖与 CI Linux job 零变化）；`screen.cpp` 抽 `matchTemplateOnBitmap` 共享 helper（imread → BGRA→BGR → matchTemplate TM_CCOEFF_NORMED → minMaxLoc 阈值判定），Windows findImage 改薄委托（行为不变）、Linux `#ifdef WINGMAN_ENABLE_VISION` 同款接入、无 vision 构建保持 stub 恒 false。测试三面：`X11PlatformTest.ScreenFindImageLocatesDrawnPattern`（Xvfb 根窗口画非均匀红绿图案→capture→save 模板→findImage 找回坐标；纯色模板 TM_CCOEFF_NORMED 零方差未定义，必须非均匀）；`vision_test.cpp` findImage 系改双模式断言（不存在路径 vision/stub 一致 not found + 合成模板 vision 下真匹配不命中，stub 构建 GTEST_SKIP；SaveImage 改真回读断言，废弃裸 return 假 pass）；`screenshot_handler_test.cpp` RPC 端到端（vision 下 JPEG data URI + base64 解码首两字节 FF D8 SOI 硬证据 + width/height/region 回显；无 vision 下错误信封含 WINGMAN_ENABLE_VISION）。验证：build-runtime（vision）全量 **1929/1929**、build-novision（stub）全量 **1902/1902**（`performance_test` 首入 Linux vision 构建；一次 `X11WindowPlatformFeatures` 间歇失败为共享 :99 的既有低频 flaky——单跑通过、复跑全量全绿，与本改动无关，新用例均持 flock 锁或纯文件操作）。遗留：macOS 侧 vision 构建无环境验证

### 配置和协议统一

- [x] 统一 IPC 协议格式（JSON envelope over Named Pipe/UDS，已就位，见 `docs/protocols.md`）
- [x] 统一 Agent-Orchestrator 通信协议文档（见 `docs/protocols.md`，TCP 二进制帧 16B header + JSON body）
- [x] 统一配置文件格式和路径（TOML：`config.hpp` 已支持 `loadFromString(TOML)`/`loadFromFile`/`saveToFile` + `apps/agent/config/agent.toml` 在用）
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

## P3 - 低优先级（工程优化）

## Agent 分组与批量操作（2026-09-14 完成）

集中管理能力强化：agent 标签从内存态升级为持久化，并新增基于标签/ID 的批量操作。纯 Go server + Dashboard，runtime 不改（决策记录见 `docs/architecture-decisions.md`「Agent Groups & Batch Operations」）。

| 能力 | 实现 | 验证 |
|------|------|------|
| 标签持久化 | `models.Agent.tags`（JSON 文本列）+ `Registry` 注入 `TagStore` 回调接口（`internal/agent` 保持零 DB 依赖，DB IO 一律锁外）；`SetTags` 写穿落库、`Register` 重启恢复/重连保留内存值 | `registry_tags_test.go`（恢复/重连/清洗/失败不回滚）+ `tagstore_test.go`（roundtrip/不重复建行/非法 JSON）+ 跨注册表连通测试 |
| 批量 API | `POST /api/agents/batch/run-script`、`/stop-script`（scripts:run）、`/trigger`（agents:manage）；选择器 agentIds/tags 并集去重（皆空 400、上限 500、无匹配 total=0）；信号量并发 8 逐台下发既有命令，离线记 "agent offline"，部分失败一律 200 + 逐台结果；脚本路径先服务端 Resolve；审计 `script.batch_run`/`script.batch_stop`/`agent.batch_trigger_add`（meta 含选择器与逐台摘要） | `handlers/batch_test.go` 10 用例 + `integration/batch_test.go`（打标→按标签批量运行→批量触发器→RBAC viewer 403/operator 200）；swagger 已再生成 |
| Dashboard 批量 UI | `TriggerFormModal` 从 Monitor 抽取为共享组件（回调式 onSubmit）；Agents 页 rowSelection（权限门控）+ 批量工具栏 3 按钮（选中 0 台禁用）+ 标签筛选 + 结果弹窗（Alert 汇总 + 逐台明细）；`wingman.ts` 新增 batch 服务函数；`access.ts` 新增 `canScriptRun` | jest 235 用例全绿（含 wingman batch 4 例 + 组件 10 例，组件覆盖率 100%）；tsc/eslint/prettier 干净 |

## 2026-09-14 功能修复与覆盖率冲刺

**功能修复（「声明完成但实际不可用」类缺陷，经全库完成度分析裁定）**：

| 缺陷 | 修复 | 验证 |
|------|------|------|
| Go Team/Inbox 三断链：消息入队无投递、断连无清理、无创建具名团队入口 | `MessageNotifier` 实时下发 + `RemoveAgent` 清理/解散通知 + `CreateTeamNamed`；新增 `POST /api/teams` | 新增 `team_delivery_test.go`(11)、`teams_test.go`、`team_inbox_test.go` 集成；Go server 覆盖率 **100.0%**（stmt/branch/func，14 包） |
| C++ ML 引擎无脚本推理入口 | `ml.run(modelId, inputs)` 模块方法 + Tensor↔ScriptValue 转换 + ONNX 示例 + pyi 存根 | `script_modules_test.cpp` +5、`ml_test.cpp` +7 全绿 |
| GUI scripts 页无文件管理（只能启动器式按路径加载） | `script_files.rs` 6 命令（list/read/write/delete/rename/exists + 路径安全校验）+ scripts 页文件树/预览/新建/删除 | Rust 9 单测（本机 glib/gtk/webkit2gtk 齐备，`cargo test` 可跑；Windows 为目标平台）+ vitest 18 用例 |
| runtime 误执行 Lua 字节码无提示 | `resource_loader` `looksLikeLuaBytecode` 检测（Lua 5.x `\x1bLua` / LuaJIT `\x1bLJ`）报可读错误 | `cli_test.cpp` +6 全绿 |

**设计决策（裁定不修，已记录）**：Debugger 501 直连模式为有意契约；GUI 不做内置编辑器（VS Code 统一）；~~PBKDF2 加密资源两端一致禁用~~（**2026-09-27 已推翻**：加密资源改走「口令派生密钥 + 打包头携带参数」，packer/loader 闭环打通、跨平台单实现，见「2026-09-27 加密资源打包→加载闭环」）。

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

## 2026-09-15 X11 WM 测试间歇失败根因排查（X11WmIntegrationTest flaky）

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

## 2026-09-16 Go Server CI race 失败修复（FrameListener.teamMgr 数据竞争）

**现象**：CI Go Server (ubuntu-latest) 连续两轮在 race detector 下失败（run 34976763718 / 35034266626），`TestTeamStatusReportUnknownTeamNoop` 报 `WARNING: DATA RACE`；同 workflow 三平台的 windows/macos job 通过（race 时序依赖调度）。

**根因**（64d642b 引入的既有缺陷）：`FrameListener.teamMgr` 字段写读不对称——`SetTeamManager()` 持 `l.mu` 写，但 readLoop goroutine 的 8 个 team/inbox handler（ack / report / join / leave / vote×2 / status_report / broadcast）及断连清理路径全部**裸读**该字段，无任何同步。触发时序即测试自身：发 `team.status_report` 后立即 `SetTeamManager(nil)`——readLoop 异步处理前一条消息读到半写状态的指针。

**修复**（`orchestrator/server/pkg/agent/listener.go`，4c7ddd9）：handler 入口统一经 `GetTeamManager()`（RLock）取指针快照到局部变量，后续全用快照。顺带消除 TOCTOU——原「nil 检查后再读一次」模式下，检查与使用之间 `SetTeamManager(nil)` 可换入空指针（比 race 本身更接近真实崩溃）。

**验证**：`go vet` 通过；`pkg/agent` `-race -count=1` 全绿；nil 防御两用例 `-race -count=20` 压测稳定；全仓 `-race -count=3` 各包通过（`internal/handlers` 单轮 216s、三轮叠加 ~648s 超包默认 600s 超时属压测参数问题而非缺陷，CI 口径 `-count=1` 通过）；推送后 CI 全绿（run 35054666493，七 job 全过）。

**教训**：给可变字段加 setter 并持锁时，grep 该字段的**全部**读点同步收口——「写字段加了锁」常给人已同步的错觉，而读侧散布在多个 handler 里最易漏；`-race` 本地默认不跑，CI 才是唯一防线，本地验证 Go 改动应至少对涉及包跑一次 `go test -race`。

---

## 各模块实际完成度（已校准 2026-09-04，含 Dashboard 审核修复）

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

## 迭代计划（已校准）

### Sprint A (2周) — RBAC 权限系统 [已完成 (2026-06-20]
- [x] Role/Permission 模型 + AutoMigrate
- [x] 用户/角色管理 API + PermissionRequired 中间件
- [x] Dashboard 权限页面 (Admin/Users、Admin/Roles) + 路由守卫

### Sprint B (2周) — Runtime IPC 事件推送 [完成 (2026-06-20]
- [x] runtime → local IPC 事件缓冲 + `events.drain` RPC（EventBuffer + spdlog sink）
- [x] GUI 事件轮询分发（events.ts → logs.addRuntime + triggers.markFired）
- [x] trigger.fired 事件源（TriggerManager::setOnFired → EventBuffer）
- [x] script.state_changed 事件源（StandaloneMode → EventBuffer）
- [x] screenshot.frame 评估：不接入 drain（按需 capture）
- [x] IPC 调用超时（Rust 30s + GUI 错误处理）

### Sprint C (1周) — Dashboard 收尾 [完成]
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

## 注意事项

1. **架构优先**：修改 runtime/orchestrator 前，先看 `docs/architecture-decisions.md`
2. **IPC 边界**：GUI 只能通过 local IPC 控制 runtime，禁止 runtime 开 HTTP/WS server
3. **远程链路**：Dashboard → Go server → runtime (outbound)，Dashboard 不直连 runtime
4. **Dashboard 位置**：真正的 dashboard 在 `orchestrator/dashboard/`，根目录 `dashboard/` 是无关的 Croupier 副本
5. **vcpkg 约束**：所有 C++ 依赖必须走 vcpkg x64-windows-static
6. **测试基线**：C++ 当前 `ctest -N` 可发现 **2509 个测试**（2026-09-30 收官：2478 passed + 31 环境 skip，TOTAL 90%；`WINGMAN_BUILD_TESTS` 自动启用 core/runtime/transport/proto/debug 标准套件）；Go server **597 个测试函数**（2026-10-04 实测 `grep -rc '^func Test'`：rbac/workflow/handlers/hub/registry/middleware/debugger/integration/security/scripts），vet 全清

---

## 相关文档

- [ROADMAP.md](./ROADMAP.md) — 项目开发路线图
- [docs/architecture.md](./docs/architecture.md) — 架构设计文档
- [docs/architecture-decisions.md](./docs/architecture-decisions.md) — 架构决策记录（硬约束）
- [docs/API.md](./docs/API.md) — API 文档
- [docs/development-environment.md](./docs/development-environment.md) — VS Code 开发环境
