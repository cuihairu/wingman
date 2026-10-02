# 变更日志 (Changelog)

本项目所有显著变更记录于此文件。

- 格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)；
- 提交信息遵循 [Conventional Commits](https://www.conventionalcommits.org/zh-hans/)（feat / fix / test / docs / chore / refactor / perf / ci）；
- 条目按时间倒序排列，commit 哈希链接至 GitHub；
- `M1`–`M8` 前缀标注对应的 [ROADMAP](ROADMAP.md) 里程碑：M1 MVP / M2 触发器 / M3 宏系统 / M4 远程编排 / M5 GUI / M6 人性化模拟 / M7 调试器 / M8 发布准备。

## [Unreleased]

自 v0.1.1 以来共 446 个提交（feat 79 / fix 147 / docs 79 / test 65 / ci 22 / refactor 9 / chore 20 / style 4 / security 1 / build 1 / 其他 19）。

### feat（2026-10-02，wingman agent 一键安装三件套——install.sh / install.ps1 / agent 独立构建矩阵接入 nightly 分发）

一条命令（`curl … | bash` / `irm … | iex`）从每日构建分发面安装 wingman-agent 单二进制：自动检测 OS 与 CPU 架构、匿名下载（release 资产直链天然免登录，Actions artifacts 需登录故不作分发面）、解包放 PATH、`wingman-agent --version` 验证、幂等（重跑即覆盖升级）、失败即停带明确报错。README 新增「一键安装 Agent」节（三平台命令一行复制 + 参数形态 + 支持矩阵表）。

- **agent 分发矩阵（.github/workflows/build-agent.yml，新）**：linux/macos/windows × x64/arm64 六腿，只构建 `wingman-runtime` target 打包为 `wingman-agent-<ver>-<os>-<arch>.{tar.gz,zip}`（整包矩阵 build-package.yml 不动——GUI/server/Dashboard 构建贵，agent 单体轻）。windows-arm64 用 windows-11-arm runner + `arm64-windows-static-md` triplet（configure-windows.ps1 增 `-Platform`/`-Triplet` 参数，向后兼容默认）；armv7 无 runner 不产出，安装侧明确报错不猜测。publish-assets 与 build-package.yml 同款纪律：`gh release upload --clobber` + 3 次指数退避重试 + 远端大小逐文件校验；release_tag 为空时只出 artifacts 不发布。
- **nightly 接线（nightly.yml）**：build-agent job 复用 resolve-version 与同一 `nightly` 滚动 tag；nightly-notes 资产表补 agent 行；cleanup-assets 修剪正则扩展为 `^wingman-(agent-)?<date>-nightly-<sha>-`（整包与 agent 单体按同 sha 一组保留，避免旧正则把 agent 资产当遗留格式误删）。
- **install.sh（Linux/macOS，~300 行，bash + 零依赖）**：OS/架构检测（`Linux|linux`→linux、`Darwin`→macos、MINGW/MSYS 指向 install.ps1；`x86_64|amd64|x64`→x64、`aarch64|arm64`→arm64；armv7/armv6 明确报错含 issue 指引；不认识的主机/架构报错引用实际输入值）；JSON 解析零依赖（`grep -oE` + `cut -d'"'` 提取 browser_download_url，紧凑/缩进两形态通吃）；版本选择 = `/releases?per_page=30` 时间倒序取第一个含本平台 agent 资产的 release（正式版优先，正式版尚无 agent 产物时自然落到 nightly），`--version TAG` 走 `/releases/tags/`；HTTP 失败按状态码分型报错（403 限流含 token 提示 / 000 网络含可达域名说明 / 404 tag 不存在）；幂等升级 = `rm -f` + `install -m 0755`（防运行中二进制 ETXTBSY）+ 新旧版本对照打印；`--version` 验证失败时 Linux 侧 `ldd` 列出缺失动态库；`--service` 注册 systemd user unit / launchd LaunchAgent（先停旧实例幂等；headless 需 linger 的明确指引）；PATH 缺失打印 export 提示不代改 rc。
- **install.ps1（Windows，PS 5.1+ 兼容）**：Desktop edition 视为 Windows（无 `$IsWindows`）+ 强制 TLS 1.2；`PROCESSOR_ARCHITECTURE` 检测 AMD64/ARM64，x86 与未知值明确报错；param 默认值惰性求值（绑定阶段 `$env:LOCALAPPDATA` 在非 Windows 会话为 null 会先抛——真跑抓到）；查询统一 `Invoke-WebRequest -UseBasicParsing` + `ConvertFrom-Json`（**pwsh 7.4 的 `Invoke-RestMethod` 对 JSON 数组响应返回嵌套数组不枚举、5.1 又平铺展开——本机实测分叉后统一 IWR 路径**）；用户 PATH 幂等追加（2047 字符上限防御）；`-Service` 注册 Windows 服务（管理员检测 + Session 0 无桌面访问的明确警告，建议 headless 专用）；下载与查询分离——token 仅用于 API 提额，二进制走资产匿名直链。
- **cli 验证入口**：`wingman-agent --version` / `-V` / `version` 三拼写等价（cli.cpp dispatch + RuntimeCliTest 新用例，全树注册 2518 → 2519）。
- **验证**：install.sh `bash -n` + shellcheck 干净；防御分支全实测（--help / armv7l 报错引用实际输入 / SunOS / 未知参数 / 真 API 无 agent 资产时明确报错 / --version 404）；URL 与 tag 提取链对真 API 数据验证；install.ps1 AST 解析干净 + Linux 下非 Windows 防御分支真跑（exit 1 指向 install.sh）+ 带 token 真实 API 遍历（跳过 0 资产正式版、命中 nightly linux-x64 资产、tag 提取正确）。**验证边界如实说明**：ps1 Windows 主流程未在本机真跑（无 Windows 环境），Windows/macOS/arm64 构建腿与安装链靠 CI 矩阵收口；本机 Linux 对真实 nightly 资产的端到端真装在资产上线后执行。

### feat（2026-10-01，wingman.task pause/resume 落地——协作式暂停四检查点 / 超时时钟悬挂 / pausedFrom_ 状态恢复 / task.paused·resumed 事件）

`docs/development-todo.md` 缺口清单条目「pause(taskId) / resume(taskId)（未实现）」落地，Lua/Python 双侧 API 同步（清单中「API 形状统一」「命名风格统一」两项范围模糊，按任务指示留后）。

- **协作式暂停语义（无法挂起正在执行的 work 本身，暂停在生命周期检查点生效）**：① 开工前（execute 入口）驻留不执行；② work 执行中被暂停则完成后结果扣住不落账（resume 才提交 succeeded，cancel/timeout 打断则丢弃）；③ 重试间隙不进入下一次尝试；④ **超时时钟在暂停期间停走**（timeout 线程驻留、deadline 顺延暂停时长）。状态机 `pending → running → paused → running` 恢复，仅 pending/running 可 pause、仅 paused 可 resume（其余含不存在返回 false）。
- **pausedFrom_ 状态恢复（自查抓出的真缺陷）**：resume 一律恢复 running 会使「worker 启动前完成 pause+resume」时 execute 入口误判重入直接返回——work 永不执行且无超时监控。改为 pause 记录 `pausedFrom_`、resume 恢复原状态（开工前→pending，执行中→running），入口为「驻留环 + 仅 pending 开工」。
- **配套收口**：重试检查点驻留醒来后复查 cancel/timeout（否则 cancel 打断后会带着 canceled 状态落入 try 执行 work）；超时线程 `wait_until` 改谓词化消除 pause 的 lost-notify 竞态；wait() 将 paused 视为未完成继续等待、等待者超时对 paused 只返回 false 不改写状态不发事件；cancel() 改为仅状态真正转换时发 `task.canceled`（终态任务不再发误导事件）；TaskManager::shutdown 先 cancel-all 再 join（暂停中任务超时时钟已停走、可能无限驻留）；pause/resume 与 cancel 同款「锁外调 task 方法」防事件死锁纪律。
- **双侧落地最小面**：C++ ModuleDescriptor 注册 pause/resume（Lua 侧即得）；Python 侧经 registerModule + camelToSnake 自动绑定零运行时代码，仅补 `task.pyi`（TaskStatus Literal 加 `"paused"` + 两函数签名）。新增 `task.paused`/`task.resumed` 生命周期事件（载荷 taskId, metadata）。
- **测试（TaskModuleTest 42 → 47 例）**：PauseRunningTaskHoldsCompletionUntilResume（扣留+wait 契约+事件计数）、PauseResumeInvalidTargetsReturnFalse（不存在/缺参/非字符串/终态）、CancelPausedTaskDiscardsHeldResult、PauseSuspendsTimeoutClock（timeoutMs=300 下暂停睡 600ms 仍 paused 对照钉时钟停走）、PauseBetweenRetriesDelaysNextAttempt（backoffMs=300 下暂停睡 400ms attempts 仍 1 对照钉停试）。
- **验证**：新 5 例 ×10 连跑全绿；全量两树 ctest（CI 口径 xvfb-run 串行 --timeout 300）**2518 注册（2513+5）两树各 100% 全绿、0 failed**。文档同步：docs/api/task.md（暂停/恢复章节 + status 返回加 paused + 接口表 + 事件表）与 development-todo.md 勾选。

### ci（2026-10-01，C++ Linux (full tests, Python engine) 网络下载脆弱性修复——libuuid vendor 预缓存旁路 sourceforge 单点）

- **现象与根因链**：7071a54 CI 唯红该 job 的 Configure CMake 步。依赖图 python3 → libuuid 走 `vcpkg_from_sourceforge`；sourceforge 事故窗口内源站 522（vcpkg 判非瞬态不重试）、~20 个镜像全部返回坏内容（unexpected hash）→ workflow 层 configure 3 次重试全灭。仓库无 ExternalProject/FetchContent；哈希钉死的对应机制即 vcpkg port 的 SHA512（随 builtin-baseline 固定，`vcpkg_from_sourceforge` 内置）——真正缺失的是「预缓存」腿。全图核查：该 job 依赖面里 sourceforge 托管的下载**仅 libuuid 一个**（28 端口逐一遍历 portfile 实证），其余全在 github/lua.org/sqlite.org。
- **放大器（登记，不修）**：actions/cache 两维虽命中（397MB 恢复），但 Linux files 二进制缓存 ABI 恒不匹配（全部 Linux/macOS job 每轮 `Restored 0 package(s)`、全量源码构建），且 GitHub cache key 不可变（对已存在 key 的 save 必失败，downloads 缓存成化石、不含 libuuid）→ libuuid 每轮重下、SF 挂即恒红。Windows job 走 NuGet（恢复 20 包）不受影响。滚动 cache key 只摊薄首建成本、首建仍需本预缓存，不另立项。
- **修复**：vendor `build-scripts/ci/vcpkg-downloads/libuuid-1.0.3.tar.gz`（318KB；deac-riga 镜像取货，SHA512 与 baseline portfile 钉值逐位一致）+ `sha512.manifest`（`sha512sum --check --strict` 口径）+ `seed-vcpkg-downloads.sh`（先验后拷进 `${VCPKG_ROOT}/downloads`）+ ci.yml 该 job 在 Prepare/Configure 间接线。vcpkg 下载前先查本地文件并按 portfile SHA512 独立复验，命中即零网络——脚本清单与 vcpkg portfile 双层哈希即供应链边界；baseline 升级若改端口哈希，清单先行失败给出明确报错（脚本头注释含更新口径）。
- **验证**：本地 `vcpkg install libuuid:x64-linux --no-downloads`（禁网）全链通过（SHA512 校验→构建→二进制缓存提交）；种子脚本 shellcheck 干净、篡改腿实测拒绝（FAILED 即不拷贝）、恢复件哈希复验一致；ci.yml YAML 解析通过；CI 结果见提交对应 run。

### fix（2026-10-01，ScriptManager 状态机死锁环修复 + stop 数据竞争根治——running 赋值 / 协作停止 / 重入防护 / 运行代号）

2026-09-29 覆盖率扫描登记的结构性缺陷（当时明确「建议独立任务」，本条即该任务）：**runScriptInternal 从不赋值 `ScriptState::running`**（全文件唯一赋值点在 resumeScript，而 resume 前置 paused、pause 前置 running——死锁环），导致 pause/resume 成功腿、pauseAll/resumeAll/stopAll 批量计数增量、running/paused 状态映射（standalone_mode / script_module 两处消费方）与对应 state_changed 推送**全部不可达（真机同样不可达）**；stopScript 成功腿仅 starting 窗口并发 stop 可达，且伴生 `engine->shutdown()` 与执行线程的数据竞争（shutdown 做 `lua_ = sol::state()` 销毁 lua_State，与 execThread 内 executeFile 并发即 use-after-free）。

- **running 赋值（死锁环解扣）**：runScriptInternal 在执行线程起动后、加锁复查仍处本次运行的 starting 时迁移到 running——`start→running→pause→paused→resume→running→stop` 全链语义正确可达。pause 明确为**簿记态**：底层脚本线程不被挂起（无引擎级暂停钩子），执行继续到自然完成/超时/停止，簿记态影响前置判定与状态上报。
- **stop 协作化（数据竞争根治）**：stopScript_Locked 对 {running, paused, starting} 只置 `ScriptInfo::stopRequested`（新原子标志）+ state=stopping + **释放 manager 侧引擎引用**（shared_ptr 引用计数操作，线程安全），不再 `engine->shutdown()`——引擎对象由执行线程持有的 shared_ptr 保活、跑完自然销毁（与超时路径既有的 detach 模型一致）。runScriptInternal 等待循环轮询 scriptDone / stopRequested / deadline：停止请求 ≤50ms 内收尾，终态 **loaded**（区别于 completed/error）、lastError 保持空（停止不是错误）；脚本本体继续在后台跑到自然结束。**引擎级中断（Lua 指令钩子 / Python trace）工程量超本轮，登记为后续独立项**——与登记口径一致，本条只修状态机与数据竞争最小面。
- **重入防护**：旧实现 running 态重跑「先 stop 再重启」会与执行线程并发复用同一引擎（且 running 不可达、实为死代码）；改为 running/starting/paused/stopping 一律拒绝（false + lastError "already running"），从根上杜绝两个执行线程共享一个引擎。unload/reload/checkReload 的「运行中先停」判定从仅 running 扩到全活跃态集合（paused/starting 态 reload 旧实现不 reset 引擎，重跑会复用执行中引擎——同型竞争，一并收口）。线程创建失败不再停在 starting（会永久卡死重入防护），改记 error。
- **运行代号（runGeneration）**：reload 对执行中脚本 stop 后立即重启会产生两代运行并存（旧执行线程尚未退出）；ScriptInfo 新增每次运行自增的代号，等待线程收尾时若已被更新的运行超越（或条目被替换），不写状态、不动属于新运行的引擎——状态机在并发 reload/restart 交错下收敛。
- **回归钉（4 例）**：`script_manager_exec_coverage_test.cpp` +2——PauseResumeStopFullChainIsReachable（全链逐态迁移 + getRunningScripts 映射 + 重复 pause/resume 拒绝 + 重入防护 + 停止终态 loaded/lastError 空/引擎引用已释放）与 RerunAfterStopCreatesFreshRun（停止后重跑创建新引擎完整执行至 completed）；`standalone_mode_coverage_test.cpp` +2——FullChainRunningPauseResumeStopReachable（经 StandaloneMode 编排面全链 + running/paused 状态映射 + paused→running→stopped 事件序列）与 BulkOperationsCountRunningScripts（双运行脚本 pauseAll/resumeAll/stopAll 计数增量 2）。脚本用 `wingman.timer.sleep` 驻留（沙箱下 wingman 全局表可用、全平台、零 CPU），被停后 detached 执行线程睡完剩余时长自然退出。standalone_mode_coverage_test 文件头与批量闲置用例的「结构性不可达登记」注释同步改写为新语义。
- **callFunction 登记（不动）**：同步执行模型下 running 窗口 = 引擎正被执行线程独占，跨线程调用即数据竞争；该接口面向异步模型设计、当前无生产调用方，running 前置保持不变（paused 态直接拒绝已由新用例钉住），引擎级并发安全调用通道随协作停止钩子一并归后续独立项。
- **验证**：新 4 例 ×10 连跑稳定；ScriptManager/script_module/timer/rpc_ipc/agent_loopback 相关 161+47 例、CLI script/build 命令面 34 例全绿；全量两树 ctest（CI 口径 xvfb-run 串行 --timeout 300）**2513 注册（2509+4）两树各取得 100% 全绿**（31 例环境 skip 与既往一致）。门禁插曲如实登记（load 60~69，两例均与本轮改动零交集——剪贴板/transport 不触达 ScriptManager）：首跑两树各 1 例 `ClipboardTest.HasText/HasHTML` 假红（clear 后空 owner 接管超 waitFor 窗口，2026-09-30 已登记的 xclip 异步接管家族残余负载风险，隔离 ×10 全绿）；复跑 cov 树全绿、build 树漂移出 1 例 `TcpE2ECoverageTest.ZeroLengthFrameTerminatesLoop`（零长帧断开异步收尾的 t=0 直断，负载时序形状，隔离 ×20 全绿）；build 树再复跑 100% 全绿。C++ 改动无平台分支（`std::atomic` + 显式拷贝构造保持 ScriptInfo 值语义），Windows CI 编译与 Debug 堆验证随推送。

### fix（2026-10-01，glue 三用例悬垂指针修复——Windows CI 红根因定位 + Windows 覆盖率脚本补失败断言明细回显）

- **CI 红**：收官批次推送后 C++ Windows job 唯红 `GlueDebuggerModuleTest.StubContract` / `GlueOrchestrationModuleTest.StubContract` / `GlueSecurityModuleTest.PassthroughContract`（同 commit Linux/macOS 全绿）。根因在本批新文件 `glue_modules_coverage_test.cpp`：`findModuleFunction` 直接在 `getAllModules()` 返回的**临时 vector** 上 range-for 并 `return &f`——函数指针逃逸到已析构对象（悬垂指针，UB）。Linux glibc tcache 释放块内容未复用，侥幸全绿；Windows Debug 堆复用/加毒后读到垃圾 `std::function` → AV 被 gtest SEH 兜底捕获 → 三用例均匀 ~50ms 假红，且恰好只有走了该 helper 的三例中招。
- **修复**：对齐全库既有 glue 用例的 `getModule`-by-value 模式（先把模块拷贝到局部稳定对象再取函数指针），顺带补 `if (!fn) return ScriptValue::null()`——缺函数时先记断言失败、不再解引用空指针。全库审计 `getAllModules()` 消费方：其余 14 处全部按值拷贝或返回 by-value，仅此一处踩坑（含 script_function_test 返回 FunctionEntry by-value、platform_x11 先 `mod = m` 再取址）。
- **CI 可诊断化（盲区修复）**：`run-windows-coverage.ps1` 失败路径此前只回显汇总行 + 日志尾 40 行——gtest 断言明细（file/line/Expected/Actual）打印在日志中段 per-test FAILED 行之前，从 CI 输出永远看不到，本次根因只能靠代码审查反推。补 per-test `"[  FAILED  ] … (N ms)"` 行前 15 行上下文回显，Windows 失败从此可直接从 CI 日志定位。
- **验证**：3 例单跑绿；全量两树 ctest 2509 注册 0 failed（连同本修复重跑门禁）；悬垂期间 Linux 覆盖率计数路径真实执行（函数调用均落到合法对象），gcda 数字不受影响。

### test（2026-09-30，C++（Linux）覆盖率扫描收官：v13 基线剩余缺口三分类清账——13 例 + 1 断言，可测缺口归零、结构性盲区带论证登记）

- **方法（先分类后动手）**：v13 基线剩余 miss 逐文件三分类——①可测 now（补用例）②结构性不可达（逐行论证登记，不写假用例）③**行归因伪影**。伪影判定实证：报告 miss 的 transport 362/368/503/521 其错误文案由既有通过用例（TcpListenPortConflictFails / UdpErrorBranches）直接断言——重跑该 2 用例后 miss 列表逐行不变，排除陈旧 gcda；定性为 gcov 对多行 braced-init 内层行/收尾行的零归因（语句计数归首行，其余行记录 0-hit）。此判定把「报告面缺口」压缩到真实可测集合，避免对着伪影硬凑用例。
- **新增 13 例 + 1 断言（5 文件）**：① `glue_modules_coverage_test.cpp`（新）3 例——debugger/orchestration stub 契约（start 恒 false、断点串 `"a.lua:12"`、`DEBUG_BREAK_HERE`；orchestration 三 stub null/false/空数组）+ security 直通契约（hashString 64-hex 稳定、generateRandomString 长度、filterSensitive 整段替换 `***`），三模块胶水体此前零驱动；② transport_inbox +1 `UdpSendToInvalidAddressFailsGracefully`——非 IP 字符串经 asio::make_address 抛 → catch → false（transport_module 154-157）；③ unix_socket_channel +3——server/client socket 创建 EMFILE 注入 ×2（`fd_exhaustion.hpp` 共享注入器：RLIMIT_NOFILE soft 压「当前占用+4」逐个占满 /dev/null，毫秒级窗口；探测先行不成立即 GTEST_SKIP）+ 断连后重启接收语义（listenFd_ 首个 accept 后关闭、serverAccepted_ 不复位 → 重启即早退退出不挂，钉住既有语义）；④ platform_x11 +1 断言——findByClassName 无命中腿返回 NullWindowHandle（既有用例只测过命中腿）；⑤ cli_test +6——script 命令执行面 5 例（成功/带参 env/运行时错误/不可编译/目录不可读源：引擎惰性注册 + loadScript/runScript 主体 + 卸载收尾）+ build 命令 stub 全链 1 例（resolveStubPath CWD 命中 → 图标日志行 → PackerOptions 装配 → create_directories → Linux ELF 嵌入明确不支持 → 优雅失败退出 1）。
- **在案 flake 收口（断言方向 2 处）**：收官全量门禁首跑在 load 68-104 下抓红 `ClipboardModuleGlue.HtmlImageAndFilesBehavior`——该用例 HTML/text 段均已按轮询纪律改写，唯 files 段漏网：setFiles 读回与 clear 后 hasFiles 两处 t=0 直断，与 xclip daemon 异步接管窗口竞争（`clipboard_poll.hpp` 头注释在案的同族）。改正向轮询终态后单测 ×20（load 93）全绿、0.33-0.39s 慢轮次即轮询真实吸收竞态窗口的证据；全量复跑（load 150）两树全绿。
- **覆盖率（gcovr 行）**：TOTAL **90%（15238 → 15290/16931，+52 真覆盖行）**；transport_module 89%→**90%**（274→278）、unix_socket_channel 94%→**97%**（238→245）、x11_window 99%（275→276，余 1 行伪影）、script_command 13%→**83%**（4→25）、build_command 49%→**87%**（24→43）、debugger/orchestration/security 胶水体全驱动（余量全为收尾行伪影，见下）。
- **结构性不可达登记（34 行，逐行论证）**：transport 17 行——UDP 阻塞 receive_from catch 4 行（close() 不唤醒阻塞 recv、UDP 无 shutdown 语义，不可确定性中断）+ tcpConnect/tcpListen 创建后句柄 null 防御 8 行（manager 工厂单调计数永不失败）+ `server->start()` 失败腿 5 行（transport.hpp 内联实现恒返回 true）；usc 6 行——listen() 失败防御（listenFd_ 已持有，Linux 无确定性注入口）；script_command 5 行——31-32 loadScript 失败腿（ScriptManager::loadScript 仅 `!exists` 返回 false，命令已前置存在性检查，属 TOCTOU 双检防御）+ 48-50 no-throw catch（引擎错误经返回值不走异常）；build_command 6 行——86-88 PE-only 成功腿（packer.cpp 明示 Linux ELF 资源嵌入不支持）+ 95-97 no-throw catch（Packer::build 无抛契约）；ml_module 34 行为前轮既证（onnxruntime 是 vcpkg Windows 平台专属依赖，Linux gate 恒跑 ml_stub，loadModel 恒 false）。
- **行归因伪影登记（30 行，工具行为非缺口）**：transport 13（279,287,293,320,328,362,368,418,443,454,469,503,521——braced-init 内层/收尾行，其中 4 行的错误文案被既有用例直接断言）+ usc 1（380 deserializeMessage catch 收尾行，坏 JSON 体已覆盖）+ debugger/orchestration/security 15（各导出函数 `}, "sig"});` 收尾行，函数体由本批 3 例驱动）+ x11_window 1（134 findByClassName 收尾行）。
- **无真缺陷暴露（如实登记）**：全部新驱动路径行为符合既有契约（EMFILE 优雅 false + Error 态 + 错误回调、UDP 坏地址 false、断连重启不挂不重收、build 非 PE 宿主退出 1），本轮零生产代码改动。
- **验证**：全量两树 ctest（CI 口径 xvfb-run 串行 --timeout 300）**2509 注册 = 2478 passed + 31 环境 skip + 0 failed**（两树一致，含本轮 +13）；gcovr（历轮口径，剔除 vcpkg 头与 tests/，`grep -c tests/` = 0）TOTAL 90%。门禁插曲如实登记：首跑 build 树挂 1 例 HtmlImageAndFilesBehavior（见上 flake 收口），复跑全绿。零 Go/JS 改动。

### test（2026-09-30，C++（Linux）覆盖率扫描续：Clipboard 门面与 X11 后端——故障注入 3 例，可触达缺口归零）

- **基线校正**：任务口径的 clipboard.cpp 58.1%/26 行、x11_clipboard 71.8%/37 行为 1f0f976 前旧树数字（彼时 Linux 文件列表用例按能力守卫 SKIP）；修复树复采基线 clipboard.cpp 58%（36/62）、x11_clipboard.cpp 74%（98/132），采集前先验证 X 环境存活（Xvfb :98 + xclip + gcovr）。
- **新增 `lib/wingman/tests/clipboard_fault_coverage_test.cpp` 3 例**（`UNIX AND NOT APPLE` gate，Windows 编译面不含该文件；无 X 环境语义自洽）：`UninitializedBackendDegradesByContract`（坏 DISPLAY 经工厂直连，未初始化实例全接口降级契约——写 false/读空/isEmpty true/isInitialized 如实 false，覆盖 initialize 失败分支）；`PipeExhaustionFailsGracefully`（RLIMIT_NOFILE 压到「当前占用+4」再占满，毫秒级注入窗口——直接占满百万默认额度是秒级窗口，全量跑实测 1.4s）；`ForkFailureFailsGracefully`（/proc 统计 uid 进程数 + RLIMIT_NPROC 压限 + fork 探测确认 EAGAIN）。pipe/fork 两条防御分支（4+8 行）与 initialize 失败分支（2 行）全部落地。
- **注入纪律（共享机）**：前置探测不成立一律 GTEST_SKIP 不误报；剪贴板 flock 按纪律持有；rlimit/fd/环境变量全 RAII 恢复（含「未成功 apply 不恢复」防把限额写成垃圾值）。
- **在案 flake 收口（断言方向 5 处）**：全量门禁首跑在 load ~27 下抓红 `ClipboardTest.IsEmpty`——`setText` 后 `EXPECT_FALSE(waitFor(isEmpty))` 等价于要求「首次读取即为终态」，而 clear 建立的空态会一直可读到 xclip daemon 异步接管为止，t=0 读取必然与接管窗口竞争。同型 5 处（HasText/HasHTML/HasFiles 的 clear 后、Clear/IsEmpty 的 setText 后）一并改为正向等待终态 `EXPECT_TRUE(waitFor(终态谓词))`：断言契约不变（终态最终出现即通过、始终不出现即失败），只去掉与异步接管竞争的 t=0 硬要求；`clipboard_poll.hpp` 头注释登记方向纪律。
- **覆盖率（gcovr 行）**：`x11_clipboard.cpp` 74% → **84%**（112/132）；`clipboard.cpp` 维持 58%（36/62）——两文件可触达缺口归零。
- **结构性盲区登记（46 行，不写假用例）**：x11 子进程分支 19 行——fork 后 exec(xclip) 替换进程镜像或 _exit(1)，子进程 gcov 计数器永不落盘（父进程行可注入、子进程行工具不可观测，路径本身每次读写真实执行）；226 收尾行归因伪影（getAvailableFormats 函数体已全驱动）；clipboard.cpp 26 行 = NullClipboard 类体 + 工厂 null 兜底——两平台工厂恒无条件 new+initialize+return，回退恒不可达（Windows 侧 #else 分支不参与编译）。
- **无真缺陷暴露（如实登记）**：三条防御路径行为全部符合既有契约，本轮零生产代码改动。
- **验证**：新 3 例 ×5 连跑稳定（102/100/10ms）；全量两树 ctest（CI 口径 xvfb-run 串行 --timeout 300）**2496 注册 = 2465 passed + 31 环境 skip + 0 failed**（两树注册数一致、含本轮 +3；31 例 xclip/XRecord/能力守卫 skip 与既往一致）；gcovr（历轮口径，剔除 vcpkg 头与 tests/）TOTAL **90%（15238/16931）**（上轮 89.9%）。门禁插曲如实登记：首跑两树各挂 `AgentLifecycleTest.ApplyRemoteConfigWhileRunningReportsReconnectAndPersistFailure`——上一任务（Android 模拟器验证）遗留的 host Go server 仍占 :8888，打穿该用例「初始地址不可达」前提，杀遗留进程后恢复（与本轮改动无关）；同轮 load ~27 抓出 ClipboardTest.IsEmpty 断言方向 flake（见上条收口）。零 Go/JS 改动。

### fix（2026-09-30，Android 模拟器验证（API 34 AVD 全链路）抓出三处阻断级缺陷：主题 / FGS 类型 / 闹钟 PendingIntent）

- **验证环境**：本机 Android SDK 模拟器 emulator-5554（AVD test34，API 34，userdebug）+ host Go server（agent 0.0.0.0:8888，10.0.2.2 loopback alias 回连、`WINGMAN_AGENT_TOKENS` 白名单）复现生产链路；macOS 腿（Actions macos-latest）与 XRecord 腿（Xvfb）此前已勾销，本轮收 Android 腿。
- **① MainActivity 启动即崩（A1 起潜伏）**：AppCompatActivity + androidx AlertDialog 必须 Theme.AppCompat 后代主题，framework Theme.Material 在 setContentView 抛 `IllegalStateException`——UI 自 A1 起从未在任何构建上真正启动过（此前验证只走 `am startforegroundservice` 服务路径，未起过 Activity）。values/values-night 改 `Theme.AppCompat(.Light).NoActionBar`（布局仅框架控件，够用）。
- **② API 34 纯核心启动 100% 崩**：`startForeground` 无条件带 mediaProjection 类型，未取得投屏授权（appop project_media）即 `SecurityException`。`startForegroundWithTypes(mediaProjection: Boolean)` 按分支选类型集——纯核心 dataSync、投屏分支叠加 mediaProjection（服务已前台时二次 startForeground 原地更新类型集，升级路径安全）。
- **③ 崩溃闹钟腿自 A3 落地从未触发**：`PendingIntent.getForegroundService` 指向 BroadcastReceiver，系统按 service 组件解析恒 "Unable to start service … not found"（被 START_STICKY 兜底掩盖）。改 `getBroadcast`，`am crash` 实测触发。
- **全链路验证（修复构建）**：安装 → MainActivity 稳定前台 → ACTION_START → FGS dataSync → nativeStart=1 → TCP 建立 → server `[Registry] Agent registered`（含断网自动重连）；`am crash` → START_STICKY + 闹钟腿双拉起、crash 遥测落 prefs；BOOT_COMPLETED / MY_PACKAGE_REPLACED（覆盖安装）自启正路径与默认关负路径全通；restricted-settings 脚本 check/allow/status/revoke 真机路径 exit code 语义正确。**前条 A3 登记的「无真机可验证项」四条（广播到达/豁免名单 startForegroundService/覆盖安装自启/勾选后重启全链路）全部销账**。
- **误诊澄清（登记）**：exported=false 并不拦系统保护广播——中途误判源于共享机高负载下广播队列积压（BOOT_COMPLETED 发出到 receiver 实测延迟 47s）+ 检查窗口过短 + 提前 `logcat -c` 抹证据；manifest 零改动，既有口径正确。
- **加固登记（未修）**：冷启动窗口内焦点被夺时 `ForegroundServiceStartNotAllowedException` 未捕获致进程崩溃（共享模拟器多会话特有触发面，单用户真机常规流程不踩）；后续按捕获退避重试方向加固。
- **验证**：`gradle :app:testDebugUnitTest` 22/22 全绿；零 Go/C++ 改动。剩余真机项：macOS TCC 三项 / XRecord 回放手感 / Android 厂商保活逐机型 / MediaProjection 真机授权流。

### test（2026-09-30，C++（Linux）覆盖率扫描续：ResourceLoader 实例接口 68% → 87%（7 例），无嵌入早退链直测）

- **缺口定位**：todo 仅剩 macOS 与 Linux 真机验证两项（本机无真机、不可做）按既定规则转覆盖率。Go 侧全包覆盖率复测（`-count=1`）全部 ≥95.6%（最低 internal/remoteticket 95.6% / internal/handlers 96.6% / internal/workflow 99.7%，余均 100%），无 85% 以下缺口；Android 侧 gradle 无覆盖率插件基建（「如适用」条件不成立，登记不适用）。C++ 侧 gcovr 全量报告（TOTAL 89.9%）按未覆盖行数重排并逐项复核既有登记排除项（misc_modules UIA 平台耦合 / lua_engine 零引用遗留 / crypt OpenSSL / packer 私有+PE / x11_recorder 真桌面 / screenshot_handler VISION 变体门 / notify WebhookSender 配置性死代码 / ml_stub 模型路径 / agent.cpp 84.6% 的 46 行余量已全部登记）后，85% 线下最大可离线测缺口锁定 `apps/runtime/src/resource_loader.cpp` **68.3%（40 行未覆盖）**——既有 43 例只直测静态字节级入口（loadScriptFromBytes / Packer 往返），实例接口面（构造探测/错误回调/资源信息/loadScript）零驱动。
- **新增 `apps/runtime/tests/resource_loader_interface_test.cpp` 7 例**（runtime_tests 接线；纯实例方法 + std::filesystem 无 POSIX/X11 符号，Windows CI 全量参与；测试二进制非 Packer 产物不含 PACK_PE_RESOURCE_ID 资源，Linux 与 Windows 探测一致为「无嵌入」）：构造探测无嵌入、ResourceInfo 默认值全字段、getExecutablePath 解析到存在的文件（Linux /proc/self/exe 生产路径）、loadScript 无嵌入时错误回调收「No embedded script found」、无回调失败路径安全、setErrorCallback 二次替换后旧回调不再接收、重复 loadScript 持续失败且探测态不漂移。
- **余量 16 行登记（不写假用例）**：265-279（11 行）loadScript 有嵌入分支——Linux 恒不可达（`hasEmbeddedScript()` 恒 false，PE FindResource 为 Windows 产物路径），核心解析/解压/解密由 loadScriptFromBytes 共用实现覆盖；309 为恒假死防御（size>=4 蕴含 size>=3）；332 readlink 失败兜底（/proc/self/exe 恒可读）；51 为收尾行归因伪影；91/114 随有嵌入分支一同不可达。
- **覆盖率**（gcovr 行）：`resource_loader.cpp` 68.3%（86/126）→ **87.3%**（110/126），函数 12/13；TOTAL 维持 89.9%（15217/16922）——+24 为真实增量（agent.cpp 顺带 +2），X11 家族 −23 为高负载窗口采集漂移（瞬态连接拒绝走早期失败腿，与本轮零生产代码改动无关）。
- **验证**：新增 7 例全绿；runtime_tests 整二进制单进程 **209/209 ×2**（load 88 下稳定）；插桩树全量 ctest 三轮 2493 例、两树注册数一致 2493——三轮各 1-2 例负载型假红且失败集合逐轮漂移（单跑均绿，均为在案 flake 家族：X11 瞬态连接拒绝 / IPC 就绪时序 / xclip 异步接管；外部负载 17→88 无干净全量窗口），CI 推送后专用 runner 串行复跑；Go 零改动沿用本轮全绿复测。

### feat（2026-09-30，Android A3 开机自启默认值校正：默认开 → 默认关、显式开启）

- **对账**：任务点名的「开机自启」已于 6cecfad 落地（BootCompletedReceiver + BootStartGate 纯逻辑 + JVM 单测 + build-android CI 门禁）；本轮实质变更是**默认值口径**——任务明确「默认关闭、显式开启」，而 6cecfad 的「默认开」是登记假设（「A3 设计目标即无人值守，UI 可关」）非既定决策，按显式指令校正。
- **行为变更（未发布区间内，无存量 release 影响）**：开机自启默认关——特权行为须用户在 App 内显式勾选。显式勾选过的安装不受影响（prefs 已存 true）；从未动过开关的安装从「装完即自启」变为「须显式开启」。
- **实现**：默认值收敛单一来源 `BootStartGate.DEFAULT_ENABLED = false`；BootCompletedReceiver 与 MainActivity 两处 prefs 读取均改经该常量（禁字面量，`AgentPrefs` 注释同步）；KDoc 语义更新。
- **回归钉**：BootStartGateTest `bootAutoStartDefaultsToOff`——钉常量 false + 「prefs 缺键（未显式开启）且已配置地址」不放行；默认值再变必须过显式决策，无法静默漂移。
- **文档**：android-agent-design §7（默认开→默认关、显式开启，注明校正缘由与单一来源）；android-keep-alive 指引表；apps/android/README A3 验证步骤（显式勾选）；todo.md 勾销现状登记陈旧「未完成」条目 + 新条目。
- **无真机可验证项（登记）**：BOOT_COMPLETED/MY_PACKAGE_REPLACED 真机广播到达、豁免名单内 startForegroundService 实际行为、覆盖安装后自启、勾选后重启全链路——需真机；本机口径 JVM 单测 + 编译，真机步骤已在 apps/android/README.md A3 节。
- **验证**：`gradle :app:testDebugUnitTest` **22/22** 全绿（+1）；docs:build 复跑绿；零 Go/C++ 改动（相关门禁沿用 1a2acb5 同代码态已验结果）。

### feat（2026-09-30，Android A3 部署体验：Android 13+ 受限设置引导——手动允许 / adb 预授权 / Device Owner 三档落地）

- **范围**：development-todo A3 节「可靠性与部署体验」第二项（先于 asset.sync；触发器项属 A2 节残留且设计无成文章节，不属「A3 之后」顺位）。Android 13（API 33）起侧载 App 的无障碍被「受限设置」默认屏蔽——开关打不开的根因、端侧开箱失败最高来源（mobile-support-feasibility.md §5.2，风险表评级：高）。零 C++ 改动。
- **预授权脚本**：`scripts/android-restricted-settings.sh check/allow/revoke/status`——幂等 adb 预授权（allow 后复核；无 adb/无已授权设备判 SKIP(2) 不与「检查不通过」混同、包未装判 FAIL(1) 可行动、API<33 判 PASS「不受约束」；`tr -d '\r'` 处理 adb shell CRLF 行尾；取值用参数展开而非 sed——e8dc57c 教训；`WINGMAN_ANDROID_PKG` 贯穿全部命令）。adb 只出现在部署/一次性授权路径（设计决策 D7 复述），运行时链路零 adb。
- **契约护栏**：`orchestrator/server/integration/android_restricted_settings_script_test.go` 11 例——假 adb（状态文件驱动设备/包名/SDK/appops + calls 调用记录，shell 输出带 CRLF 对齐真 adb pty 行尾）+ 净化 PATH，三态/幂等/包名覆盖/revoke 复核/用法全路径不碰真设备；复用 guacd 脚本契约框架（Windows runner 显式 SKIP、macOS bash 3.2 兼容口径同守）。
- **App 内引导**：`RestrictedSettingsPolicy` 纯逻辑对象（API≥33 且无障碍未启用且未确认 → 提示）+ MainActivity 常驻「受限设置指引」按钮（三档解法全文对话框）+ onResume 自动弹一次（`restrictedHintAck` prefs 键，弹过即落 ack 不再自动弹）。诚实边界：公开 API 无法区分「被受限设置挡住」与「未开启」，系统不暴露该状态——按口径触发，Android 12- 不弹；「无障碍失效检测上报」依赖 device.capabilities 预留槽位，登记未做。
- **手册**：`docs/guides/android-restricted-settings.md`（背景/症状识别表/三档解法/验证/已知边界；入文档站「进阶指南」，与保活指引互链互补——本页解决「开不了」、保活页解决「被杀」）。
- **测试**：Kotlin JVM 单测 +6（RestrictedSettingsPolicy API 门槛边界 + 提示矩阵，共 21 例全绿）；Go 契约 11 例全绿（1.6s）；shellcheck 零 finding；`npm run docs:build` 绿。
- **文档同步**：development-todo A3 两项勾销（可靠性首项补 6cecfad 对账修正）；mobile-support-feasibility §5.2/§7 风险表/§8 分阶段（失效检测上报单列未做）；android-agent-design §5.4/§9 验证矩阵/新增 §10'' 实施摘要；apps/android/README（新增受限设置验证步骤节 + 里程碑行）；ROADMAP M9 A3 行与行动表；todo.md 新条目 + 状态行。
- **假设与登记**：五厂商定制 ROM（小米/华为/OPPO/vivo/三星）真机逐机型 ⋮ 菜单验收未执行（本机无真机，手册按各厂商官方文档口径编写，登记待真机抽样）；Device Owner 路径需设备已纳管，仓库不附带 MDM 配置（手册记边界）；本地验证口径为 JVM 单测 + Go 契约 + docs 构建（真机 adb 预授权实操归真机验收）。

### feat（2026-09-30，Android A3 可靠性落地：开机自启 / 崩溃自重启 / 断连缓存自治 / 机型保活指引）

- **范围**：todo.md「Android 现状登记」未完成三项中的第一项（A3 剩余可靠性项优先于 A4），设计约束 android-agent-design.md §7，本轮同步落地摘要 §10'。零 C++/Go 改动（断连缓存自治的 C++ 面自 A1 现成），全部为 Kotlin 壳 + 文档 + CI。
- **开机自启**：`BootCompletedReceiver`（BOOT_COMPLETED + MY_PACKAGE_REPLACED，两者均在系统后台 FGS 启动豁免名单内，exported=false 亦可收到系统保护广播）+ App 内开关（默认开，CheckBox 入主界面）+ 已配置服务器地址才放行；放行判定抽为纯逻辑 `BootStartGate`；Manifest 启用 `RECEIVE_BOOT_COMPLETED`（原注释占位转正）。
- **崩溃自重启（带退避与放弃上限）**：新增 `WingmanApplication` 安装进程级 `CrashRestartHandler`——崩溃时记录状态/摘要到 prefs 并按 `RestartPolicy`（指数退避 1s→60s 封顶；10 分钟窗口内连崩 5 次放弃，防崩溃风暴；窗口外新崩溃重开计数）经 AlarmManager 调度 `CrashAlarmReceiver` 重启前台服务。诚实边界：闹钟腿是 best-effort（API 31+ 后台 FGS 启动限制可能被拒，捕获后让位于 START_STICKY 系统路径）；`coreRunning` prefs 门控保证只在「崩溃前服务确实在跑」时复活（MainActivity 崩溃不会拉起从未启动的 agent）；MediaProjection 授权单会话一次性、崩溃后不可恢复（系统约束，登记）。清零语义：用户/开机/覆盖安装路径清零崩溃串，崩溃闹钟路径保持累积。
- **核心看门狗（断连/进程内自愈的 App 侧补位）**：WingmanService 内 30s 周期检查——服务期望核心在跑而 `nativeStatus` 报 `running!=true`（或状态不可解析）时幂等重拉 `nativeStart`（C++ `AndroidAgent::start` 对运行中 client 短路返回 true，重拉安全）。网络断开不在其列：`running` 反映 client 存活而非 connected，断连重连/outbox 冲刷由 RemoteClient 退避自治（agentcore_test 既有 28 例覆盖，含断连入 outbox、重连冲刷、超容量丢弃恰好 100 条）。判定抽为纯逻辑 `WatchdogPolicy`。
- **机型保活指引**：`docs/guides/android-keep-alive.md`（小米/华为/OPPO/vivo/三星逐机型设置步骤 + 通用项 + 验证方法 + 已知边界；入文档站「进阶指南」导航）+ App 内「机型保活指引」对话框（五厂商要点内联）。
- **测试（Android 工程首批单测）**：纯逻辑对象 `RestartPolicy`/`BootStartGate`/`WatchdogPolicy` 零 Android 依赖，Kotlin JVM 单测 15 例（`app/src/test`，JUnit 4.13.2）：退避进度/窗口内外串归并/放弃与恢复、开关与空白地址矩阵、看门狗期望态×状态解析矩阵（含缺字段/坏 JSON 保守重拉、connectionState 不参与判定）。org.json 以 Maven 真实现入测试类路径——android.jar 对其只部分真实实现（`optBoolean` 即 stub 抛「not mocked」，首跑 5 例实证）。本机 `gradle :app:testDebugUnitTest` 15/15 全绿（连跑两轮稳定；Gradle 8.10.1 + 本机 SDK，首次运行完整编译主源码+测试源码）。
- **CI**：build-package 的 build-android job 在打 APK 前先跑 `gradle :app:testDebugUnitTest`——此前 Kotlin 逻辑无任何 CI 门禁（per-push CI 不编 Kotlin，nightly 只做编译检查）。该 workflow 为 workflow_call 型，随 nightly/release 生效。
- **文档同步**：android-agent-design.md §1.2（A3 行转已落地）/§5.4（壳组件清单）/§6.3（开发机环境口径修正）/§7（生命周期表逐场景实现归属 + 实现要点）/§8（补注：可靠性组件全为本地组件、不改变信任模型——任务指定的口径同步点）/§9（验证矩阵补 JVM 单测与 outbox 行）；apps/android/README.md（标题范围、环境说明改「可本地跑 JVM 单测」、新增 A3 验证步骤节、里程碑对账）；ROADMAP M9 A3 行转 ✅（P2 安全演进另列）+ 行动表销项；todo.md 新增本轮条目 + 状态行。
- **假设与登记**：开机自启默认开（A3 设计目标即无人值守，UI 可关）；本地验证口径为 JVM 单测 + 编译（NDK/vcpkg 全量 APK 组装由 nightly 验证）；`apps/android/.gitignore` 新增（.gradle/ 等本地产物，首次本地构建暴露的遗漏）。

### docs（2026-09-30，Android Agent 现状登记：ROADMAP 补 Milestone 9 移动端里程碑，文档滞后收口）

- **背景**：问询「Android agent 什么时候实现」并指认文档未更新。核查结论：代码侧 A1 链路打通（2026-09-19，ce4a648）、A2 能力闭环（2026-09-21，c1df705/c0a67fe——dispatchGesture 手势注入/MediaProjection 采集/找色找图/`screenshot.capture` 远程截图 + 反向 JNI 桥）、A3-P1 token 认证（2026-09-20，4c9a8f4——`WINGMAN_AGENT_TOKENS` 白名单三端）均已落地，nightly CI 自 2026-09-20 每日打 Android arm64 APK；但 ROADMAP.md 仅有桌面 M1-M8、平台说明未提 Android、todo.md 无移动端条目——**滞后的是路线图对账，不是实现**。
- **ROADMAP.md**：新增 Milestone 9「移动端 Agent（Android）」——A1-A4 四阶段状态表（A1/A2 ✅、A3 🚧 P1 已落地、A4 ⬜ 协议预留）+ 工程基建注记（nightly APK/核心下沉 `libs/agentcore`+`libs/androidagent`）；平台说明补 Android 实验性支持；时间估算表补 M9 未排期行；下一阶段行动表登记 A3 剩余可靠性项（开机自启/崩溃自重启/断连缓存自治/机型保活指引）与 A4 多设备编排（Dashboard 设备视图/批量下发/asset.sync）为待排期项。
- **docs/android-agent-design.md**：§1.2 里程碑表 A3 行补注「P1 token 认证已落地（2026-09-20）」，与 §8 安全节的「A3-P1（已落地）」口径对齐。
- **todo.md**：新增 2026-09-30 Android Agent 现状登记节——已落地三项（附提交时间线）、未完成三项（A3 剩余/A3-P2 per-agent token + Keystore 迁移 + challenge-response 与 TLS/A4 多设备编排）与排期状态，供后续任务派发对账；顶部状态行同步移动端摘要。
- 纯 .md 改动：主 CI 按路径规则跳过、Docs workflow 随推送运行；无代码/测试变更。

### test（2026-09-29，C++（Linux）覆盖率扫描续：StandaloneMode 78% → 92%（12 例），ScriptManager 状态机结构性缺陷登记）

- **缺口定位**：重跑 gcovr 全量报告（xvfb 口径门禁 2474/2474 全绿后出报，TOTAL 14849/16600），排除项复核不变（misc_modules UIA 平台耦合 / lua_engine 死代码 / crypt OpenSSL 失败分支 / x11_recorder 真桌面 / packer 私有死代码 + PE 平台 / main + start_command 入口胶水 / screenshot_handler VISION 变体门 / notify WebhookSender 配置性死代码）。remote_client.cpp 与 standalone_mode.cpp 并列 51 miss；后者为纯编排层（进程级 ScriptManager + EventBuffer，无网络/平台分支、确定性可离线驱动），可达行比例更高，锁定 `apps/runtime/src/standalone_mode.cpp` **78%（51 行未覆盖）**。
- **新增 `apps/runtime/tests/standalone_mode_coverage_test.cpp` 12 例**（runtime_tests 接线 + `registerLuaEngine()` 惰性注册同 rpc_ipc_test 口径；全平台编译，Windows CI 全量参与）：start 二次调用幂等 true；scriptDir 被普通文件占据 → `create_directories` 异常 → false；autoStart 装配（快脚本加载 + 阻塞运行至完成、**输出回调空串跳过**——空 `print()` 的空串经引擎输出回调实证送达、只转发非空输出恰好一条、completed→Unknown 现状钉）；autoStart 缺失脚本跳过；loadScript 登记（**unloaded→Stopped 现状钉**：manager.loadScript 不置 loaded 态，`reloadScript` 后才 Loaded——两处状态映射按现状钉并在余量登记）；失败脚本 error 态 + error 事件经 start() 注册回调推送全链；startScript unknown-id false；manager 失同步（登记 map 命中、manager 条目已被直卸）→ getScript 回落默认 Info / unloadScript false / stop() 降级不崩；批量操作闲置脚本计数 0 契约（pauseAll/resumeAll/stopAll 三态一致）；getScript unknown-id 默认值；getConfig 构造镜像。
- **结构性不可达登记（19 行，不写假用例）——ScriptManager 状态机缺陷发现**：`runScriptInternal` 从不赋值 `ScriptState::running`（全文件唯一赋值点在 resumeScript，而 resume 前置 paused、pause 前置 running——**死锁环**），故 pause/resume 成功腿（238-242 / 257-261）、pauseAll/resumeAll/stopAll 非零计数增量（295 / 314 / 333）、toRuntimeState 的 running/paused 映射（26-29）均结构性不可达——**真机同样不可达**（执行期状态为 starting 非 running；本轮修正 round-4 将其归为「真机观察范畴」的口径）；stopScript 成功腿（276-280）仅 starting 态并发 stop 可达，但该路径并发 `engine->shutdown()`（销毁执行线程正在使用的 lua_State，数据竞争）不触发。修复需补 running 赋值 + 引擎级协作停止设计（指令计数钩子），超出覆盖率轮范畴，建议独立任务。
- **另登记**：167-168（manager 加载失败腿——manager.loadScript 唯一失败条件是文件不存在，而 StandaloneMode 已前置检查，TOCTOU 窗口外不可达，防御性双检）；46（toRuntimeInfo 尾行已被多次驱动仍计 miss，-O2 行归因伪影）。
- **覆盖率**（gcovr 行）：`standalone_mode.cpp` 78%（189/240）→ **92%**（221/240）；TOTAL 14849 → **14878**/16600（89%，+29 行）。
- **验证**：新增 12 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2486/2486**、插桩 build-cov 全量 ctest **2486/2486**（两树注册数一致，含本轮 +12；均 xvfb 口径 0 failed，31 例 xclip/XRecord 条件 skip 与既往一致）；Go/JS 零改动沿用已验结果，CI 随推送全量复跑。

### test（2026-09-28，C++（Linux）覆盖率扫描续：module_helpers 张量转换层 68% → 96%（15 例），dtype 全矩阵直测）

- **缺口定位**：gcovr 全量报告（CI 同款 xvfb 口径，TOTAL 89%）按未覆盖行数重排（marshal 收口后）并逐一验证可达性——`packer.cpp` 62%（63 行缺：`compileToBytecode`/`replaceIcon`/`setVersionInfo` 均为 private 且 `build()` 不调用，Linux 成功路径被 PE 资源写入卡死（非 Windows `updateResource` 恒 false），离线仅 ~10 行可达，暂记）；`notify_module.cpp` 75%（52 行缺几乎全落 WebhookSender：**`setAllowedHosts`/`setWebhooksEnabled` 全仓零调用方**，白名单默认空 → 每次连接都被 SSRF 防护拒绝，URL 解析/worker HTTP 管线/并发上限/shutdown join 在现网行为下均不可达——既有用例已钉住「URL not in whitelist or webhooks disabled」拒绝路径，按配置性死代码登记）；`remote_client.cpp`/`standalone_mode.cpp` 各 51（散布的重连/错误腿，loopback 逐腿搭建性价比低，暂记）；`crypt.cpp` 79（前轮既定口径）。锁定 `lib/wingman/src/script/modules/module_helpers.hpp` **68%（58 行未覆盖）**——缺口几乎全部是张量 dtype 矩阵：11 个枚举值仅 FLOAT32/FLOAT64/INT8/BOOL 被既有 ml/misc 用例顺带踩过，其余 dtype 的 elementSize/类型名解析/appendBytes 实例化/appendElement/elementToScriptValue 分支全零覆盖。
- **新增 `lib/wingman/tests/module_helpers_tensor_test.cpp` 15 例**（core_tests 接线；纯头文件内联函数直测，无 I/O/网络/平台分支，Windows CI 全量参与）：`tensorElementSize` 全 11 dtype + 越界兜底 0；`tensorTypeFromString` 全 11 名解析 + 未知名/大小写敏感拒绝（失败不动 out 哨兵）；**dtype 全矩阵往返**（ScriptValue spec → tensorFromScriptValue → TensorData → modelOutputToScriptValue → ScriptValue，逐 dtype 断言 shape/字节数/值语义，一次驱动全部 `tensorAppendBytes<T>` 实例化与 appendElement/elementToScriptValue 分支）；int 实参经 asFloat 的类型转换路径（int32 混合正负）；bool 0→false；spec 错误矩阵（非对象/缺 data/空 data/非数组 data/未知 dtype 名/**dtype 非字符串**——asString 落默认空串拼进错误信息按现状钉/shape 非数组）；shape 语义 2（缺省一维 = data 长度、显式 shape 逐字保留）；未知 dtype 兜底 3（elementSize==0 → ModelOutput 空数据且 shape 照常、elementToScriptValue 直读 → null、appendElement 直调不追加字节）。
- **余量 7 行登记（不写假用例）**：106-111（6 行）= `tensorTypeFromString` 局部 static map 初始化、213 = `modelOutputToScriptValue` 聚合初始化——两处的全部行为均已被用例真实驱动（全 11 dtype 名解析 + 15+ 次 ModelOutput 输出），-O2 下编译器将初始化代码合并至相邻行、这些源行无独立代码归属，属行归因伪影非测试缺口。
- **覆盖率**（gcovr 行）：`module_helpers.hpp` 68%（128/186）→ **96%**（179/186）；TOTAL 89% → **89%**（14865/16600，+51 行）。
- **验证**：新增 15 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2474/2474**、插桩 build-cov 全量 ctest **2474/2474**（两树注册数一致，含本轮 +15；均 xvfb 口径 0 failed，31 例 xclip 未装/XRecord 扩展缺席条件 skip 与既往口径一致）；Go/JS 零改动沿用今日同一代码态已验结果，CI 随推送全量复跑；CI 结果见本提交对应的 workflow run。

### test（2026-09-28，C++（Linux）覆盖率扫描续：lua_marshal 40% → 97%（28 例），双向转换契约直测）

- **缺口定位与排除**：gcovr 全量报告（CI 同款 xvfb 口径，TOTAL 88%）按未覆盖行数排序后，剔除不可离线测项——`misc_modules.cpp` 359-462（UIA OO 区块：Linux 无 `createUIAManager` 后端，元素句柄闭包仅在 Win/Mac 可达，平台耦合）；`lua_engine.cpp` 80 行 0%（零引用遗留类，全仓仅 `registerLuaEngine()`——后者属 lua_script_engine.cpp）；`screenshot_handler.cpp` 19%（`WINGMAN_ENABLE_VISION` 编译变体门外，本机无 vision 构建）；`crypt.cpp` 75%（余量为 OpenSSL 内部失败分支，前轮既定口径）；x11_recorder/x11_clipboard/clipboard（平台后端）；main.cpp/start_command.cpp（入口胶水）。锁定 `libs/lua/src/lua_marshal.cpp` **40%（62/104 未覆盖）**——ScriptValue↔Lua 双向转换层，仅被 timer_module 间接触发零星分支，转换契约零直测。
- **新增 `lib/wingman/tests/lua_marshal_test.cpp` 28 例**（core_tests 直链 wingman::lua 既有接线；纯嵌入式 sol::state，Windows CI 全量参与）：toLuaObject 10 例（null→nil、bool/int 保持 lua 5.4 integer 性/float/string 含 CJK 转义、数组→序列表、对象→映射表、嵌套容器 Lua 往返、C++ callable 经 variadic_args 实参 marshaling 被 Lua 调用（int/string/bool/nil/table 五类实参逐个钉型）、threadSafe callable 返回字符串往返）；toScriptValue 10 例（invalid 对象→null、bool、**lua_isinteger 整数性分流 42→Int/2.5→Float**、字符串、Lua function→**非线程安全** ScriptValue callable 并实调、数值实参与 Int 返回、error() 函数实证为 protected 调用落 null（与 executeString 直调走 catch 腿路径不同，按现状钉）、userdata→null 兜底）；tableToScriptValue 8 例（序列→数组、字符串键→对象、**空表落对象分支**、混合键丢数字键、非正整数键取消数组资格、稀疏数组保形空洞补 Null、嵌套递归、table 引用直转）。
- **余量 3 行登记（不写假用例）**：48（toLuaObject switch 全 8 枚举 case 均在分支内返回，末尾兜底恒不可达）；57-58（sol::type::nil case——实测真实 nil 经 sol 代理均以 invalid 对象到达、走 52-53 已覆盖腿，valid-nil 引用无自然构造路径，防御分支）。
- **覆盖率**（gcovr 行）：`lua_marshal.cpp` 40%（42/104）→ **97%**（101/104）；TOTAL 88% → **89%**（14814/16600）。
- **验证**：新增 28 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2459/2459**、插桩 build-cov 全量 ctest **2459/2459**（两树注册数一致；本地全量门禁自本轮起沿用 CI 的 xvfb-run 口径，X 相关用例不再因环境缺 DISPLAY 而 skip）；Go `-race -count=1 -timeout 90m` 14 包全绿、dashboard jest 420、GUI vitest 全绿（本轮零 Go/JS 改动，沿用今日早前同一代码态的已验结果；CI 仍随推送全量复跑）；CI 结果见本提交对应的 workflow run。

### test（2026-09-28，C++（Linux）覆盖率收口：Agent 主类 0% → 84%（35 例），两缺陷根治：system.shutdown 死锁 / shutdown 悬空事件 sink）

- **缺口定位**：gcovr 全量报告（TOTAL 行覆盖 87.1%）剔除真机/平台耦合项（x11_recorder/x11_clipboard）、入口胶水（main.cpp）与 OpenSSL 内部失败分支后，行覆盖最低且可离线测的自有模块锁定 `apps/runtime/src/agent.cpp` **297 行 0%**——runtime 编排核心（initialize 能力分支、start 组件装配、applyRemoteConfig 热重建、handleRemoteCommand 全命令面、EventBuffer 远程转发）此前只被间接编译、无任何测试驱动。
- **新增 `apps/runtime/tests/agent_loopback_test.cpp` 35 例 / 2 套件**（tests/CMakeLists 接线；TCP 回环沿用 agentcore 测试 harness 模式，IPC 客户端沿用 rpc_ipc_test 的 TestIpcClient 模式）：
  - **AgentLifecycleTest 17 例**（全平台，无 socket 依赖，Windows CI 全量参与）：能力→组件派生矩阵 4（各能力组合的 RemoteClient/StandaloneMode 派生与 RunMode）；配置文件首跑写默认+读回 2；生命周期契约 4（无组件 start/stop、shutdown 幂等、standalone 启停、**start 失败 running 仍置位的降级契约**）；applyRemoteConfig 矩阵 7（未启用拒绝、内存+落盘持久化、无 configPath 只改内存、写盘失败部分成功串、运行中重建「已写入待重连」与「连接+写盘双重失败」两腿）。
  - **AgentLoopbackTest 18 例**（POSIX，`#ifndef _WIN32`——XDG_RUNTIME_DIR 端点重定向与 UnixSocket 通道为 POSIX 原语，Windows NamedPipe 默认端点语义本机不可验证，登记假设）：每用例把 XDG_RUNTIME_DIR/TMPDIR 重定向到私有目录后走 **Agent::start() 真实装配路径**。本地 IPC 面 8（system.getVersion 全链、system.getStatus providers 反映、config.getRemote 镜像、config.setRemote 校验矩阵 7 项+无远程能力拒绝、setRemote 应用+热重建+落盘全链、不可写配置路径部分成功、EventBuffer 三类事件转发+log.line 过滤、shutdown 摘 sink 回归钉）；远程命令面 10（get_status 含脚本 loaded→stopped 落定与 error 态、list_windows 信封、run_script 四腿、stop_script 三错误腿、unknown、trigger.* Dispatcher Reuse——JSON 串参数解析/非 JSON 字符串回退/handler 错误信封透传、screenshot.capture 信封、system.shutdown）。
- **两缺陷根治（均由本轮用例暴露）**：
  - ① **system.shutdown 远程命令自我死锁（EDEADLK）**：命令回调内联运行在 RemoteClient 消息处理线程上，同步 `stop()` 会回收正在执行回调的线程自身——实测日志 `Resource deadlock avoided`，ack 永远发不出、server 侧超时。改为先回 ack、stop 移交独立线程收尾；回归钉 `SystemShutdownCommandStopsAgent`（轮询组件全停再收尾，规避与后台 stop 线程的停机竞态）。
  - ② **`Agent::shutdown` 不摘除 EventBuffer 远程 sink**：sink lambda 捕获 `this`，而 EventBuffer 是进程级单例——Agent 析构/重建后任何 push 事件都会调用悬空回调（UB）。shutdown 补 `setRemoteSink(nullptr)`；回归钉 `ShutdownClearsRemoteEventSink`。
- **登记假设与余量 46 行（不写假用例）**：stop_script 成功腿与脚本 running 态需长驻脚本协作停止（真机观察范畴，同 rpc_ipc_test 口径）；list_windows 窗口枚举循环体在 Xvfb 下无窗口不执行（真桌面依赖）；scriptStateToString 的 Loaded 为过渡态实测不停驻（加载后异步落定 stopped）、Running/Paused 同前、Unknown 为防御兜底；触发器 onFired→EventBuffer 推送需真实屏幕命中（真机观察）；screenshot/trigger 的 dispatcher 空指针防御与 initRemoteClient/initStandaloneMode 失败腿恒不可达（构造恒成功）；encodeWindowHandle 的 `_WIN32` 分支为非激活编译侧；多行 braced-init 与 spdlog 双行语句的行归因伪影（语句必然整体执行，响应字段断言为证）。
- **覆盖率**（gcovr 行）：`agent.cpp` 0%（0/297）→ **84%**（252/298）；TOTAL 87.1% → **88%**（14740/16600）。
- **验证**：新增 35 例全绿（连跑 3 轮稳定）；build/ 全量 ctest **2431/2431**、插桩 build-cov 全量 ctest **2431/2431**（两树终版注册数一致；早前记录的 2424 为终版用例集落地前的陈旧注册数）；Go `-race -count=1 -timeout 90m` 14 包全绿（integration 包 1868s）；dashboard jest 420、GUI vitest 全绿。CI 注记：前笔 c97a57a 为加固未完成的中间态提交，其 Linux full-tests 两作业失败的 6 例（断言与实测契约错配 4、trigger.update 数值 id 的 JSON 解析歧义 1、system.shutdown 死锁超时 1）即本笔修复对象，最终 CI 结果见本提交对应的 workflow run。

### test（2026-09-27，C++（Linux）覆盖率扫描续：LuaScriptEngine 55% → 86%（31 例），余量全部登记不可归因/防御分支）

- **缺口定位**：上轮扫描的次低自有可测模块 `libs/lua/src/lua_script_engine.cpp` 55%（75 行未覆盖）。既有覆盖仅来自 script_manager/script_module 胶水测试的间接驱动——`executeString`/`callFunction`/`getGlobal`/`setGlobal`/`enableSandbox`/`disableSandbox`/`getLanguageName` 等公开面整段零直测；round-1 修复的 `package.preload` 钩子（非沙箱分支）此前从未被任何测试驱动过。
- **新增 `lib/wingman/tests/lua_script_engine_test.cpp` 31 例**（core_tests 直链 wingman::lua，既有接线；纯引擎 API + std::filesystem，无 POSIX/X11 分支，Windows CI 全量参与）：initialize 6 例（沙箱危险全局清除全集 io/os/debug/package/dofile/loadfile/load/require、非沙箱全库打开、**require("wingman") 非沙箱解析为 wingman 表**、沙箱 require 调用报错、config.env 注入为 `_ENV_*` 全局、二次 initialize 幂等）；executeString/executeFile 4 例（语法/运行时错误 lastError、文件成功+缺失失败、未初始化拒绝执行）；callFunction 6 例（未找到报 "Function not found: N"、参数传递+整数返回、Lua error 进 lastError、nil 返回→Null、混合标量参数转换、Lua 5.4 整数性保持 Int/Float 分流）；registerModule 4 例（注册函数被 Lua 调用+返回、实参值传递、**C++ 异常 → sol::error 传播为 Lua error**、未初始化注册为 no-op）；global 3 例（四标量类型往返、未知名→Null、未初始化 no-op）；沙箱开关 3 例（enableSandbox 事后剥离、**disableSandbox 只重开 io/os/debug——package/require 不恢复（按现状钉）**、未初始化 no-op）；print 捕获 4 例（简单捕获、变参制表符分隔+非 string 走 tostring 匹配 Lua 原生、空 print 空串回调、**initialize 前安装回调不生效（initialized_ 门挡下 set_function，按现状钉死并注明调用方约束）**）；shutdown 1 例（幂等+执行拒绝）。
- **余量 22 行全部登记（不写假用例）**：① 35-37/45-51（9 行）= `open_libraries` 的 sol 变参模板实参行——沙箱/非沙箱两分支均已被用例真实驱动（沙箱另经 StandaloneMode 间接覆盖），gcov 对变参展开不落行归因，属工具盲区非测试缺口；② 80-83（4 行）= initialize 的防御 catch——无故障注入无自然触发路径（登记口径同 crypt.cpp）；③ 108-111/125-128（9 行）= executeFile/executeString 的 `!result.valid()` 腿——**实证不可达**：本轮全部失败用例（语法错误/运行时 error/文件缺失）在该 sol 版本下均走 catch 异常腿，invalid-result 分支是对实际 sol 行为的死防御。
- **覆盖率**（gcovr 行）：`lua_script_engine.cpp` 55%（93/168）→ **86%**（146/168）；TOTAL 86.5%→**87.1%**（14457/16599）。工具注记：本次 gcov 处理触发 gcc bug 68080（smart_trigger.cpp switch 行负值），按 gcovr 文档以 `--gcov-ignore-parse-errors negative_hits.warn` 出报告——smart_trigger.cpp 行数归因由 235 变 197（98%→100%），为跳过记录的伪影、真实覆盖未变（对 TOTAL 影响 ≈0.03pp，方向中性）。
- **验证**：新增 31 例 26ms 全绿；build/ 全量 ctest **2386/2386**（2355+31 精确吻合）；插桩 build-cov 全量 ctest 2386/2386；Go `-race -count=1 -timeout 90m` 全绿；dashboard jest 420、GUI vitest 511 全绿；CI 结果见本提交对应的 workflow run。

### test（2026-09-27，C++（Linux）覆盖率扫描续：AgentConfig 45% → 100%（24 例），连带根治 saveToFile 丢失 [performance] 节）

- **双端扫描定位**：Go 侧 `go test -cover`（total **98.5%**）低于 80% 的仅 7 个函数——`remoteticket.NewManager` 0%（一行包装，测试走 `newManagerWithSweep` 短周期 seam）+ guacamole/recordings handler 6 个 70~75% 分支（需 guacd 基础设施，属环境依赖），不构成实质缺口；C++ 侧 gcovr（TOTAL 86.0%）最低且无平台耦合的自有模块为 `apps/runtime/src/agent_config.cpp` **45%（78 行未覆盖）**——能力/模式派生、loadFromFile、节域解析的 debugger/logging/performance 分支、saveToFile 整段为零覆盖，而它们是 agent.cpp 的真实生产路径（`Agent::initialize` 派生 RunMode、远程配置写回 `saveToFile`）。
- **新增 `apps/runtime/tests/agent_config_test.cpp` 24 例**：能力派生 6 例（默认 Hybrid、仅远端 Remote、仅单机 Standalone、仅 LocalIpc→Unknown、全关 Unknown、Hybrid 优先于 Standalone 的判定序）；loadFromString 13 例（空串全默认、[global] 别名、remote 五整型键+两字符串键、debugger/logging/performance/standalone 全键、引号内 `#` 保留、未配对引号不剥、未知节/键静默忽略、int 键配非数字串走字符串分支被忽略（当前容错契约）、超范围整数 std::stoi 抛出且 `Agent::initialize` 的 catch 兜底、文件不存在抛错带路径、不可写目录 save 返回 false）；文件往返 5 例（全节段 save→load 相等、**[performance] 回归钉**、默认配置首跑写盘可读回、模式一致）。
- **连带根治——saveToFile 漏写 `[performance]`**：loadFromString 支持该节三整型键而 saveToFile 不写回，用户手调的性能配置会在 runtime 首次配置落盘（`Agent::applyRemoteConfig` → `saveToFile`）时被**静默抹掉**。补写 [performance] 节（3 行），与解析端对称；回归钉 `SaveToFilePersistsPerformanceSection` 同时断言文件内容含该节与读回值相等。
- **平台说明**：纯 std::filesystem/fstream，Windows CI 全量参与（无 loopback 跳过、无真机观察项）；解析器断言一律钉「当前实现的真实契约」（容错忽略而非报错），不写理想化用例。
- **覆盖率**（gcovr 行）：`agent_config.cpp` 45%（64/142）→ **100%**（147/147，含修复新增 5 行）；TOTAL 86.0%→**86.5%**（14366/16599）。
- **验证**：新增 24 例 5ms 全绿；build/ 全量 ctest **2355/2355**（2331+24 精确吻合）；插桩 build-cov 全量 ctest 2355/2355；Go `-race -count=1 -timeout 90m` 14 包全绿；dashboard jest 420、GUI vitest 511 全绿。

### test（2026-09-27，C++（Linux）覆盖率缺口收口：IPC/RPC 控制面约 405 行 0% → 52 例单测，六缺陷根治）

- **缺口定位**：gcovr 全量报告（TOTAL 行覆盖 80%）剔除「需真机/人工观察」与「OpenSSL 内部失败分支不可达（crypt.cpp，无故障注入无解）」后，剩余最大缺口是 GUI ⇄ runtime 的控制面集群——`local_ipc_server.cpp` 168 行、`script_handler.cpp` 95 行、`macro_handler.cpp` 44 行、`config_handler.cpp` 15 行、`event_log_sink.hpp` 18 行全部 **0%、零测试**（均已编进 runtime_tests 二进制但从未被驱动），`system_handler.cpp` 余 29 行 55%。
- **新增 `apps/runtime/tests/rpc_ipc_test.cpp` 52 例 / 7 套件**（Linux 下 loopback 真链路；Windows 侧 `#ifndef _WIN32` 跳过 loopback——NamedPipe 语义本机不可验证，属登记假设，仅由 Windows CI 做编译检查）：ScriptHandler 15 例（真 StandaloneMode + 真 Lua 文件：列表映射、start/stop/restart/unload 参数与错误信封、同步执行模型契约、load 后析构释放登记）；EventHandler 2 例（drain 上限与 remaining 计数）；ConfigHandler 5 例（无 access 报错、apply 失败透传、往返取新值）；MacroHandler 13 例（status/save/load 往返、坏文件、空队列播放、speed/repeat 越界钳制、start/stop 信封）；SystemProviders 4 例（注入 provider 反映到 system.getStatus、无脚本时 pause/batch 为 no-op）；EventLogSink 3 例（级别过滤、4096 截断、构造参数抬高过滤下限）；LocalIpcServerLoopback 11 例（起停幂等、system.getVersion 往返、provider 状态、未知方法/坏 JSON/缺 method/Error 型信封、config 往返、events.drain、客户端断开事件与重连、带客户端停机干净 join）。
- **意外收获——六个真实缺陷**（四个生产行为级）：
  ① **StandaloneMode::stop() 提前 return → 进程级全局 ScriptManager 永久泄漏**：仅 LocalIpc 能力的 runtime 不会调 start()，但 GUI 可经 script.* 在实例上加载脚本、登记在全局单例；stop/析构跳过清理，脚本连同 Lua 引擎永久泄漏（去掉早退，无条件清空登记；回归钉 `DestructorReleasesNeverStartedLoadedScripts`）。
  ② **macro.play speed=0 → SIGFPE、负数 → 无符号下溢挂死**：speed 未校验直传各平台 recorder 实现，0 作除数 / 负数经无符号运算下溢成天文数字。在 RPC 边界单点钳制 `speed<1→1、repeat<1→1`，一次收口 X11/Win32/Cocoa 三实现（回归钉 `PlayWithZeroSpeedIsClampedInsteadOfCrashing` / `PlayWithNegativeSpeedIsClampedInsteadOfHanging`，修前一个 SIGFPE 一个挂死，修后各 100ms 干净完成）。
  ③ **config_handler 引用捕获悬垂 → 段错误**：`registerRuntimeConfigHandlers(Access&)` 的 handler 以引用捕获 access，调用方传临时对象即 use-after-free（新测 `GetRemoteWithoutAccessReturnsError` 稳定复现段错误）。改按值捕获（与 system_handler 的 by-value providers 一致）。
  ④ **沙箱脚本 100% 启动失败**：Lua 引擎初始化无条件执行 `package.preload["wingman"]=...`，而沙箱模式 package 从未打开且被 applySandbox 置 nil，prelude 引用 nil 直接抛错——GUI `script.start` 的唯一路径是沙箱（StandaloneMode 强制 sandboxed=true），即 GUI 脚本启动全灭。钩子改为 `!sandboxed` 才安装（ScriptHandler 15 例即依赖此修复才能跑通）。
  ⑤ **LocalIpcServer 停机双 disconnect 竞态**：server 线程与 stop() 都会对同一通道 `disconnect()`，并发进入时两边同时 join 同一 receiveThread（双重 pthread_join = UB，代码审读确认、实测间歇挂死——注：观测到的挂死混有同机并发跑测的干扰，定性为审读确认的硬化收口）。统一在 channelMutex 下串行、锁内复查 stopping。
  ⑥ **EventLogSink 级别过滤方向反了**：写成 `> max_level_`，info 下限时 warn/error 全被滤掉、GUI 日志面板永远收不到告警与错误，反而放行 debug 噪音。改回 `<`（仅下发 ≥ 配置级别），回归钉 `ForwardsInfoWarnErrorAndFiltersDebug`。
- **登记的产品契约与真机观察项**（测试注释内注明，不改变产品语义）：ScriptManager 同步执行模型——`script.start` 阻塞到脚本跑完，顺序 RPC 永远打不进 running 窗口，`script.stop` 对已完成脚本必报「Failed to stop script」→ **GUI 无法停运行中脚本**，pause/resume 成功路径与 GUI 停脚本均属真机观察范畴；wingman::unloaded → runtime Stopped → JSON "stopped" 的状态映射；macro.start 录制平台相关（无头环境只断言信封）。
- **覆盖率**（gcovr，行覆盖）：`local_ipc_server.cpp` 0%→**80%**（余 34 行：server 通道创建失败/connect 失败/停机竞态跳过断连/发送失败等错误分支）、`script_handler.cpp` 0%→**78%**（余量即上述真机观察项）、`macro_handler.cpp` 0%→**89%**、`config_handler.cpp` 0%→**100%**、`event_log_sink.hpp` 0%→**94%**、`system_handler.cpp` 55%→**75%**、`standalone_mode.cpp` **70%**；TOTAL 80%→**86%**（14278/16594）。
- **验证**：新增 52 例 2.1s 全绿；loopback 压力 10/10 轮干净；runtime_tests 全量 128/128（22.7s）；插桩 build-cov 全量 ctest **2331/2331**（上轮 2279 + 本轮 52，数目精确吻合）；Go `-race -timeout 90m` 全仓绿；dashboard jest 420、GUI vitest 511 全绿。

### fix（2026-09-27，TcpClient 重连对 joinable IO 线程赋值 → `std::terminate`：服务端断链后的重连必崩根治）

- **症状**：agentcore 新增的 loopback 重连单测（`DisconnectedEventsQueueAndFlushOnReconnect` 等）以 `terminate called without an active exception` 稳定崩进程，崩点在重连线程 `Connecting to server...` 之后。
- **机制**：`TcpClient::connect()` 把 `socket_` move 进 session、用旧 IO 线程驱动 `ioContext_.run()`；`RemoteClient::reconnectLoop` 的重连路径**直接再调 `connect()`、不经 `disconnect()`**——此时 `ioThread_` 仍 joinable，`ioThread_ = std::thread(...)` 对 joinable 线程对象赋值按标准触发 `std::terminate`（move 进 session 的 socket 被 asio 自动重开，连接反而成功，恰好走到赋值那一行）。即：**服务端一断链，agent 进程在第一次重连尝试上必崩**，属生产行为级缺陷，非测试时序问题。
- **修法**：`connect()` 入口对「上一条连接的 IO 线程/session 仍在」的情况先隐式 `disconnect()`（收口旧线程、restart io_context、重置 socket），再建新连接；既有显式 disconnect→connect 路径行为不变。
- **验证**：agentcore 28 例（含断链重连、outbox 冲刷、超容量丢弃、心跳链路统计）连跑 4 轮全绿；全量 ctest 见本轮 test 条目。

### fix（2026-09-27，agentcore 四缺陷：EventBuffer sink 持锁回调自死锁 / 心跳 join 阻塞一个心跳周期 / stop 与 startHeartbeat 生命周期竞争 / 首连重连计数双计）

- **EventBuffer sink 持锁回调**：`push()` 的注释与头文件契约都写明「sink 在锁外回调」，实现却在 `lock_guard` 作用域内调用——sink 里任何 `size()/drain()` 或重入 `push()` 立即自死锁（新单测 `SinkMayQueryBufferWithoutDeadlock` 挂死暴露）。收口锁作用域：入队/驱逐/取快照持锁，回调挪到锁外，与既有「捕获局部变量再转发」的意图对齐。
- **心跳线程整段睡眠**：`sleep_for(seconds(heartbeatInterval))`（默认 30s）不可打断，而 `stop()` 与重连路径 `startHeartbeat()` 都要 join 它——stop 拖慢进程退出至多一个心跳周期，**每次重连被放大 30s**。改 100ms 粒度分段睡眠（与 `sleepInterruptible` 同款），join 阻塞封顶 ~100ms，发送节奏不变。
- **生命周期竞争 → terminate**：stop() 先 join 心跳、后 join 重连；重连线程在退出前 `connect()` 成功会**重新拉起心跳线程**，时序交错时 `Impl` 析构销毁仍 joinable 的 `thread` 对象 → `std::terminate`。收口：新增 `heartbeatMutex` 串行化「旧线程 join / 新线程 spawn」，stop() 改为先停重连再收心跳，`startHeartbeat()` 在 `shouldStop` 已置位时不再 spawn；`stop()` 去掉 `running_` 前置判断改为无条件幂等收口（start 进行中并发 stop 不再漏收线程）。
- **markConnected 双计**：`start()`/`connect()` 成功路径同步调 `markConnected()`，而 `TcpClient::connect()` 返回前已同步触发 `SessionEvent::Connected`、事件处理器再记一次账——**每次首连 `reconnects=1`**（契约：初始连接不计重连），心跳上报的链路统计从第一条连接起就是错的。删除两处直接调用，统一由事件处理器记账。
- **验证**：`HeartbeatCarriesLinkStats` 断言 `link.reconnects == 0` 修复前红（实测 1）、修复后绿；四缺陷修后 agentcore 28 例 ×4 轮全绿。

### test（2026-09-27，C++（Linux）覆盖率缺口收口：agentcore 0% → 28 例单测，EventBuffer 100% / RemoteClient 86%）

- **缺口定位**：gcovr 全量报告（TOTAL 行覆盖 80%）里行覆盖最低且无 Lua/X11/平台耦合的模块是 `libs/agentcore`——`remote_client.cpp` 390 行 + `event_buffer.cpp` 59 行 **0% 覆盖、零测试**。依赖仅 transport + nlohmann_json + spdlog（约束：不得依赖 lib/wingman 本体，Android 构建不引入），可纯 loopback 单测。
- **新增 `libs/agentcore/tests/agentcore_test.cpp` 28 例**：EventBuffer 11 例（push/drain 保序、drain(max) 留余、drain(0)、clear、容量超限 FIFO 逐出 + `dropped_` 累计增量断言、log.line 优先驱逐、sink 逐条转发、sink 重入不死锁、null sink、`IpcEvent::toJson` 三字段、单例）；RemoteClient 17 例走真 loopback TcpServer（同 libs/transport TransportEnv 模式）：连不上端口后台重连 + 事件回调、stop 幂等、register 携带 identity/metadata/token、ack 成功→connected + `connection.state_changed` 入缓冲、ack 失败→error、ack 坏 JSON 不崩、命令回环（seq 透传 + okData JSON、无回调错误响应、回调 error 透传、data 非 JSON 字符串回退、请求体坏 JSON 错误响应）、1s 心跳携带 link 五元组、已连接直发 agent.event、断线事件入 outbox 重连冲刷、超容量（100 上限）丢弃恰好 100 条、config/stateName。全部等待有界，headless 可跑。
- **接线**：根 CMakeLists 新增 `BUILD_AGENTCORE_TESTS`（镜像 transport：option + `WINGMAN_BUILD_TESTS` 级联 FORCE-ON + enable_testing 条件），`libs/agentcore/CMakeLists.txt` 尾部 option + `add_subdirectory(tests)`，`gtest_discover_tests DISCOVERY_MODE PRE_TEST`。
- **覆盖率**：`event_buffer.cpp` 60/60 行 **100%**、`event_buffer.hpp` 7/7；`remote_client.cpp` 344/396 行 **86%**（余量为 Windows 分支、超时事件等边缘路径）。基线（修前）两文件均 0%。
- **意外收获**：测试揪出五个真实缺陷（两fix条目），其中 TcpClient 重连 terminate 属生产行为级。
- **验证**：28 例连跑 4 轮全绿；全量 ctest 2279 例见提交（含本条与 PathRandomness 收口）。

### test（2026-09-27，`TestRunGracefulShutdownOnSIGINT` 15s 就绪窗口：d22abe3 同族第四处 → `waitTCPUp` 60s 收口）

- `-race` + 共享机高负载下复红一次（15.05s 超时，实测同负载 server 启动 30s 上下）——与 d22abe3 收口的三处同族（共享/耗尽 deadline、短窗口误判就绪失败）形状一致，是该轮漏改的第四处。改走既有 `waitTCPUp(t, name, addr, 60*time.Second)`：独立窗口、到点带名收口，绿路径零成本。

### test（2026-09-27，`HumanMouseTest.PathRandomness` 统计型 flake 根治：截断零桶双倍宽 → 多轮分布断言）

- **机制**：控制点偏移经 `static_cast<int>` 向零截断量化，|offset| < ~1.414 落入零桶（向零截断使零桶双倍宽）；两条独立路径存在非零概率（~0.1%–4% 量级，随控制点数）整体量化后完全一致，单次对比断言 `EXPECT_TRUE(different)` 低频误报。全量 ctest 首次红即此（2247/2279，同用例此前多轮全绿）。
- **修法**：20 轮生成取「至少一对不同」，把运气事件变成分布事件（联合失败概率 ≈ P^20）；顺以 `i + 1 < n` 规避 `size() - 1` 在 size<2 时的无符号下溢。修后单用例连跑 100 次全绿。

### docs（2026-09-27，文档语法错误修复：development-todo.md Phase 7 请求块围栏丢失 + api/core.md 两处标题反引号残缺）

- `docs/development-todo.md`「Phase 7: TCP 协议增强」：请求 JSON 块的 `#### 请求消息结构` 标题与 ` ```json ` 开围栏自 c400bd6f 起丢失（对照原提交补回 `type`/`id` 两字段），孤立的闭合围栏把 `#### 响应消息结构` 标题与其代码块吞成一块无法高亮的区域。
- `docs/api/core.md`：`#### `keyUp(key)**` 与 `#### `keyPress(key, duration)**` 两处标题反引号误写成 `**`，行内代码不闭合（32 处同级标题中仅此 2 处）。

### feat（2026-09-27，`wingman.event` 监听器查询与按事件名清理：`listener` / `listeners` / `clear(type?)`）

- **范围**：`docs/development-todo.md`「事件与状态」仅剩的两项未完成条目一次收口。核心层 `EventHub` 新增 `SubscriptionInfo{id,type,name,once}` 快照与三个查询：`subscription(id)`（按订阅 ID）、`subscriptionByName(name)`（同名订阅取最早注册者，匿名订阅不可按名查询，结果不依赖 unordered_map 遍历序）、`subscriptionsForType(type)`（按订阅 ID 升序，即注册顺序）；新增 `clear(type)` 重载只清理指定事件的全部订阅（事件不存在时为无副作用空操作），无参 `clear()` 全量清理语义不变。
- **脚本层（Lua/Python 同步可用）**：`wingman.event` 新增 `listener(id|int|name|string)`（查询单个监听器，返回 `{id,type,name,once}`，不存在返回 nil）与 `listeners(type)`（列出该事件全部监听器，未注册事件返回空数组）；`clear(type?)` 升级——无参/nil 保持全量清理，传事件名只清理该事件，**其他类型参数返回 false 而不是静默全量清理**（`clear(123)` 误用不会清空全部监听）。缺参/不支持的参数类型返回 false，与 `on`/`off` 既有约定一致。Python 侧 `event.pyi` 同步 `ListenerInfo` TypedDict 与三个签名（camelCase 与 snake_case 别名由引擎统一注册）。
- **测试与验证**：`EventHubTest` +4、`EventModuleTest` +12（38→50）——覆盖按 ID/按名查询、once 与匿名快照字段、同名取最早、查询排序、按名清理只影响目标事件（emit 不再触发、其他事件原样）、无参/nil 全量清理兼容、坏参数 false。全量 ctest 与 `go test ./...` 结果见提交记录。

### fix（2026-09-27，X11 `XOpenDisplay` 瞬态拒绝根治 + `X11WindowCloseCenterAndWaitFamily` 负载 flake 收口）

- **症状与定位（测试进程 + Xvfb 双侧 strace）**：`X11PlatformTest.X11WindowCloseCenterAndWaitFamily` 在 CPU 超售时不稳（load 55~61 下 10 跑 7 红，此前已两轮登记未修）。本轮把红实例的现场抓齐了：每个红都以 `[error] X11Window: failed to open X display` 开场——门面 `XOpenDisplay` 返回 NULL → `initialized_=false` → center/close/isInitialized 成片 false、forceClose 落空、holder 防挂死护栏报警，此前登记的两种失败形状全是这一跳的级联。门面那次连接 connect 成功、Xauthority cookie 也读到了，但 **Xvfb 在 accept 后读完 `SO_PEERCRED` 与 `/proc/<pid>/cmdline`，连客户端的 setup 请求都没读就 `shutdown`**（随后按 Xorg 惯例打开 `/etc/X<disp>.hosts` 与 `protocol.txt` 组织拒绝信息）；同一进程几毫秒后的下一次 open 完全正常。用不带任何 wingman 代码的裸 open→close 循环隔离验证：load≈40 下 11/3000 失败、**11/11 立即重试成功**；自造 load≈67 下 7/800、7/7。定性：X server 对「前一个本地连接刚断开 → 新连接立即到达」存在 accept 阶段的瞬态拒绝（对应 `os/access.c`/`os/client.c` 按 pid 缓存的本地凭据在断连清理窗口的竞态），CPU 超售放大概率——不是调用方的时序错误，但只能由调用方吸收。全 fixture 复跑在 screen/capture/input/window 各类都见过散片同款失败（load 38~91 每轮 13~23 次），六个 Linux 门面同病。
- **修法（生产侧一个收口点，测试侧补齐登记的另两项）**：新增 `src/platform/linux/x11_display.hpp` 的 `openX11Display(name, attempts=6, backoff_us=20000)`——瞬态拒绝重试，最坏 120ms 只在失败路径付出；`x11_window`/`x11_screen`/`x11_capture`/`x11_clipboard`/`xtest_input`/`x11_recorder`（control+data 双连接）的 `XOpenDisplay` 全部改走它，重试耗尽仍按原语义 false/nullptr。测试侧：`TestX11Window` ctor/`setActive`/析构与 fork holder 子进程的 `XFlush` 改 `XSync`——登记项「子进程未 XSync、父侧 XKillClient 打在 server 建窗之前落空」是独立次级隐患，跨连接读回与强杀目标必须以 server 已处理为前提；holder 收割护栏 5s 改为随超售缩放（`clamp(load1/nproc, 1, 8)` × 5s，同 dashboard 二轮口径），报警信息带实际秒数与因子。WM 集成用例的就绪探测（`displayAccepts`/`wmRegistered` 等）保持裸 open——它们本身就在等待循环里，瞬态失败由循环吸收。
- **回归钉**：新增 `X11PlatformTest.WindowInitializeSurvivesConnectionChurn`——25 轮「探测连接 open→close 紧接门面 open」逐轮复刻实证的竞态形状，断言门面每轮必须靠重试站起来、末轮功能真实可用（center/close 生效）；循环里的裸 open 失败不作断言（那是压力本身）。
- **验证**：自造 CPU 超售（load 60~77，高于登记红区间 55~61）：目标用例 + 回归用例连跑 **10/10 绿**；`ctest -R 'X11PlatformTest\.|Recorder|X11'` 66 用例 **×3 轮全绿**；全量 ctest **2235 例 0 红**（含新增回归钉，load 30~60 共享机背景负载下）。修复前同方法复现：单用例 xvfb-run 连跑 4 轮红 1，与本轮红实例签名一致。

### test（2026-09-27，根包 `TestRunHTTPEndpointsAndScriptOutput` 负载 flake 根治：共享 deadline 耗尽 → nil conn SIGSEGV → `waitTCPUp` 收口）

- **症状与定性**：全仓 `-race` 复跑（load 46+）红过一次——15s HTTP 就绪循环超时后，两个 GET 报 connection refused（`t.Errorf` 不终止）；agent 拨号循环因**复用同一个已耗尽的 `deadline`**，循环体一次都不执行，循环外的 `err` 保持 `os.Getwd` 留下的 nil、压掉了 `if err != nil { Fatalf }`，nil `agentConn` 传进 `writeAgentFrame` → SIGSEGV。复跑 `-count=3` 绿、CI 常态绿、与同期 workflow 改动无交集，先登记为独立欠账，本轮收掉。
- **修法（一个收口点，三处同族缺陷一起清）**：新增 `waitTCPUp(t, name, addr, timeout)`——每次调用**独立起算**超时窗口、到点带名字与地址 `Fatalf` 收口；HTTP 就绪等待与 frame listener 拨号都走它，nil conn 结构上不可能再往下漏，SIGSEGV 换成可读的失败信息。同测试里落库轮询的 `time.Now().Before(time.Now().Add(5*time.Second))` 恒真条件（事件不落库时挂到测试全局超时而非干净失败）改为先算好的 `dbDeadline`。
- **压测揪出 SIGSEGV 掩盖的第二层**：`-race` + load 50-70 下 server 启动实测要 30s 上下（失败实例 Fatalf 之后才打印 route 注册 → seed → `Server starting`）——原 15s 窗口本身就低于慢启动机器的真实需要，这是 load 46 那次红的直接触发条件。就绪窗口放宽到 60s、落库轮询 15s：窗口只是失败上界，就绪即返回，绿路径零成本。
- **验证**：空闲单跑 10s 绿；28 个 CPU burner（load 50-70）下 `-race -count=10` **10/10 绿**（263s）；中途 30s 窗口版在压测下 4/10 **干净红**、错误信息直指 server 未就绪（收口的诊断价值同步验证，正是它暴露了 30s 启动事实）；`go vet` 干净；全仓 `go test -race -count=1 ./...` 全绿，工作流取消根治同轮未回退。

### fix（2026-09-27，工作流取消被终态回写覆盖：`TestCancelRunningWorkflow` 的写序竞争根治）

- **定性（先测再判）**：`-race` 干净、无共享变量误用，红的是**状态机真缺陷**而非测试时序。用一个临时探针把取消动作同步到「步骤行已 `running`」之后（此刻 runner 必已越过 `execute` 循环顶部的 ctx 早退检查），修复前 **60/60 全红**：`expected cancelled, got completed`。原用例平时绿只是运气——它只等「工作流进了 running 表」（`Submit` 同步写入，等于不等待），取消多半落在 runner 进循环之前，那条路径直接 `return`、不回写终态，缺陷就没机会露面；CI 上负载把窗口撑开才红。
- **机制（两层，第二层是验收 gate 自己抓出来的）**：取消时步骤以 `cancelled` 结束并让步骤返回 `ctx.Err()`，调度循环按 `failed` 出环；而 `finalStatus` 只看 `anyStepStatus("failed")`，被取消的步骤状态是 `cancelled` 不是 `failed` → **误算成 `completed`**。第一层：两侧无条件 `Updates(status=...)`，谁后落库谁赢，`Cancel` 刚写的 `cancelled` 被覆盖。加条件更新收口后，自建验收 gate 的 `-race -count=50` 又抓到第二层（3/50 红 + handlers 包 `TestWorkflowCancelLifecycle` 1 次）：旧 `Cancel` **先 `exec.cancel()` 再落库**，被 ctx 惊醒的 runner 会带着误算的 `completed` **抢先**落库——守卫把「后到覆盖」变成「先到赢」，但先到者写的仍是错值，`Cancel` 反而向自己引发的错误终态让位、对用户回 400（该用例的工作流是 `wait 30s`，不可能真跑完，红即此机制）。
- **修法（终态收口 = 条件写 + 先落库后惊醒 + 诚实取值，不做互斥体）**：① 两侧终态回写都改**条件更新** `WHERE id = ? AND status = 'running'`——终态只能从 running 迁移，后到让位（`RowsAffected == 0`），任何交错都收敛到同一终态；② `Cancel` 改为**先落库、成功后才 `exec.cancel()`**——runner 被 ctx 惊醒前终态已安装，取消必然生效、必然返回 nil，runner 的回写只会让位；③ runner 的 `finalStatus` 把 `ctx.Err() != nil` 判成 `cancelled`（优先于 failed 判定）——就算未来出现其他 cancel-ctx 路径（引擎停机、超时策略），runner 算出的终态也诚实，不再依赖「Cancel 先落库」的顺序假设。让位方同时不做后续动作：runner 不广播与库不一致的 `status_changed`；`Cancel` 让位时不 cancel ctx、不标 skipped、不广播，按既有契约返回 `workflow not running`（该窗口里工作流确实已跑完）。没加锁、没改 `Execution` 的并发结构——竞争在 SQL 条件与调用顺序层面收口比在内存里补锁更小，也覆盖住「ctx 检查与写库之间」这种 check-then-act 窗口。
- **回归用例四条 + 原用例对齐**：① `TestCancelAfterStepStartedKeepsCancelledStatus`——同步在步骤行 `running` 之后取消（修复前 60/60 红），断言严格：取消必生效（nil）且终态恒为 `cancelled`；② `TestExecuteFinalStatusHonoursCancelledContext`——不经 `Cancel`（绕开「取消先落库」的顺序保护）直接 cancel runner 的 ctx，锁住取值层：runner 终态必须把 ctx 取消算成 `cancelled` 而非误算的 `completed`；③ `TestTerminalWriteYieldsWhenRowNotRunning`——不经调度竞争直接驱动 `execute`，锁住「行已是终态时回写让位、不覆盖」；④ `TestCancelYieldsWhenWorkflowAlreadyTerminal`——对称方向，行已 `completed` 时取消不得改写终态、不得标 skipped、按不在运行返回。原 `TestCancelRunningWorkflow` 没有同步点，`Cancel` 若撞上「工作流真已跑完」的让位分支，改为只要求行处于某个合法终态（`cancelled` 的专断言留给上面第一条用例），避免把合法让位判成失败。
- **验证**：`go test -race -count=50 ./internal/workflow` 全绿（第一轮 gate 抓出第二层缺陷，修后复跑）；`go vet ./...` 0 告警、`gofmt -l` 干净；全仓 `go test -race -count=1 ./...` 全绿（integration 包在共享机负载下需 `-timeout 90m`）。上一轮 CI 根治未回退：`scripts/verify-guacd-e2e.sh` 仍是无 `declare -A` 的平行数组，10 个 shell 契约用例与 Go 包同轮复跑绿。

### fix（2026-09-27，guacd e2e 脚本契约测试在 macOS/Windows 恒红：bash 3.2 关联数组 + 被吞掉的 symlink 错误）

- **症状与范围**：CI 自 `fcf7cf9`（2026-09-25，脚本与契约测试引入的那轮）起 **连续 9 个 push 红**，上一个绿的是 `4f97996`。只红 `Go Server (macos-latest)` 与 `(windows-latest)` 两个 job，失败集合恒定——就是本脚本的 8 个 shell 契约用例，同 job 内其它 Go 包全 `ok`。门禁失效比红本身更贵：任何人此后的真回归都混在这 9 个红里，而「全绿才推送」无从判定。
- **macOS 根因（一条真缺陷）**：`declare -A READY_PORTS` 是 bash 4 特性，macOS 自带 `/bin/bash` 停在 3.2 → `[guacd]=4822` 被解析成给未变量 `$guacd` 赋值，`set -u` 下当场崩；崩点在引擎探测之后、任何 SKIP 分支之前，于是「环境不具备 → SKIP(2)」被吞成 exit 1，正是本脚本自己反对的「两种失败长得一模一样」。**实测复现**：`docker run bash:3.2` 跑改前脚本 → `line 60: guacd: unbound variable`，与 CI 日志逐字相同。
- **修复（改齐仓库惯例）**：两条平行数组 `READY_NAMES`/`READY_PORTS` + `ready_port()` 按下标查表——其余 `verify-*.sh` 一律不用关联数组，本脚本是全仓唯一例外，现已消除；`do_status` 的服务名也改取同一张表，端口映射回到单一来源。bash 3.2 与 bash 5.3 下逐路径对拍一致：`--help`(0)、无引擎 `up/test/run/status`(SKIP 2)、未知子命令与未知 flag(2)、假引擎 `up` 端口未就绪(1，四行端口报告正确)、假引擎 `status`(1，NOT READY×4)。
- **Windows 根因（测试自己的缺陷）**：契约用例靠往临时目录软链 coreutils 拼出「净化 PATH」，而 `os.Symlink` 的错误被 `_ =` 静默吞掉；Windows runner 默认没有建符号链接的权限 → PATH 目录为空 → 脚本里每个外部命令 exit=127，看起来像被测脚本坏了。改为软链失败退化成复制；复制也失败就带原因 SKIP；POSIX bash 脚本的契约测试在 Windows 显式 SKIP（守卫集中在 `linkCoreutils`，所有跑脚本的用例必经此处），宿主缺单个命令（如 macOS 无 `timeout`）仍按原语义继续。
- **本轮为何越界改别人的文件**：诊断已完成且修复局部（一个 shell 查表 + 一个测试守卫），留下则 main 继续红、后续批次全部失去门禁判定；两处都是真缺陷而非口味问题。全量 Go 门照跑（`go vet ./...` + `go test -race` integration 包）。

### feat（2026-09-27，加密资源打包→加载闭环：PBKDF2 口令派生 + PACK_HEADER v2）

- **收掉全仓唯一代码 TODO**：`resource_loader` 里「Encrypted resources are not supported yet」的两侧一致禁用，改为口令派生密钥的真闭环——打包 `--encrypt --password`（或 `WINGMAN_PACK_PASSWORD`）→ 运行期 `WINGMAN_SCRIPT_PASSWORD` 解密加载。加密产物没有口令就是永久打不开的字节流，故 `build()` 在复制 stub **之前**硬拒空口令，而不是产出一个打不开的 exe。
- **两套分叉实现收敛为一份**：`PACK_HEADER` 此前在 packer 与 loader 各抄一份，两侧各带一份 `_WIN32` CryptAPI 加解密；非 Windows 分支只剩「XOR 假哈希 + `verifyHash` 恒返回 true + 一律拒绝加密」——Linux 上那套「完整性校验」从来不是校验。新头 `resource_pack.hpp` 成为格式唯一权威（160 字节布局 static_assert、reserved[] 字段位、le32、参数合法性），密码学统一到 `wingman::crypt`（OpenSSL EVP；新增调用方给密钥的 `aesGcmEncrypt/aesGcmDecrypt` 原始字节对，与既有「口令进 base64 出」的 `encryptAES/decryptAES` 明确分工）。
- **v2 版本语义与兼容性**：PBKDF2-HMAC-SHA256（100000 轮 / 随机 16B salt / 随机 12B IV）→ AES-256-GCM（密文 || tag），salt/IV/迭代次数落 `reserved[0,32)`、尾字节清零。升 v2 不破坏任何存量：v1 加密包用的是打包时随机生成、只留 `sha256(key)` 的一次性密钥，构造上不可恢复，而当年 `build()` 又拒绝 encrypt → 合法产物里不存在 v1 加密包（真遇到就明确判不可恢复，不猜密钥）；未加密包照旧写 v2 且 reserved 全零、无指纹，v1 读侧不解释 version/reserved 故仍可加载；未知版本一律拒绝，迭代次数 0 或 >5e6 拒绝（后者等于让加载方替打包方烧 CPU）。
- **完整性三层各司其职 + 用例锁定**：GCM 标签是权威；`keyHash` 只是解密前的口令指纹，用来把「口令错」与「数据损坏」分成两条可读信息（改 salt → 指纹先拒；改 IV → 标签拒）；`dataHash = sha256(原始明文)` 在解密解压后校验，兜住头部篡改（清掉/谎称 COMPRESSED：密文合法但解出的字节对不上 originalSize/dataHash）。新增用例专门验证「伪造一份自洽 keyHash 绕过预检后仍被 GCM 拒」，即预检可绕、边界不塌。
- **顺手修一个真缺陷（变换顺序）**：旧顺序 encrypt → compress，而该简化 LZ 的匹配模型只认「同一字节连出现 ≥4 次」（`data[i - j]` 左下标不随 count 前进），密文里找不到字节连串 → `--encrypt --compress` 静默退化成「只加密」。改为 compress → encrypt（读侧镜像 decrypt → decompress），加密包从此真能压小；无存量产物受影响。
- **让非 Windows 跑到生产代码**：`Packer::buildResourceBytes()` / `ResourceLoader::loadScriptFromBytes()` 这对平台无关字节级入口是往返与错口令用例的执行体（PE 读写 `BeginUpdateResource*`/`FindResourceA` 仍是 Windows-only，只负责换容器），ELF 侧嵌入未实现即明确失败，不静默写「看着成功其实没嵌脚本」的产物。顺带修 `main.cpp` 上「Version/Size 恒打 0」的存量显示 bug（头部字段要解头后才进 `ResourceInfo`）。
- **测试**：`apps/runtime/tests/resource_pack_test.cpp` 新增 43 例（格式布局/KDF 与指纹/写侧标志与随机性/参数化「明文 vs 加密」往返/负路径：错口令、缺口令、改密文、改 salt、改 IV、改迭代次数、截断、未加密包改字节、头部谎报压缩、隐藏压缩、伪造指纹、坏 magic、短输入、v1 加密拒绝、未知版本、v1 明文仍可加载）；`crypt_test.cpp` +10 例（AES-GCM 原始字节对：空明文/大数据/非标准 IV 长度/错密钥/错 IV/改密文/改标签/非法长度）；`cli_test.cpp` 的 `PackerRejectsEncryptedResourcesUntilLoaderSupportsThem` 语义作废，改写为「无口令拒绝加密 / 有口令放行到平台边界」两条。全量 ctest **2234 例 0 红**（31 例平台性 skip），同轮 Go `go vet` + `go test -race ./...` 全绿。
- **本机门禁的盲区，由 Windows CI 补上（本轮自身缺陷，已修）**：`updateResource()` 里把资源字节接成 `const std::vector<uint8_t>`，而 `UpdateResourceA` 第 5 参是 `LPVOID`（只读，签名不带 const）→ MSVC C2664，两个 Windows job 编译失败。该段在 `#ifdef _WIN32` 内，Linux 全量 ctest 永远看不见它——「本地 2234 例 0 红」为真但不足以覆盖 PE 写入侧。修法：局部变量去 const（无需 const_cast），并用同签名探针 TU 复现与验证（const 版报 `no known conversion from 'const unsigned char *' to 'LPVOID'`、非 const 版干净）。副作用是 Windows 侧那 43 例新用例与「Windows 端到端嵌入成功」的断言此前从未真跑，修完才进 CI。

### test（2026-09-27，高负载 flake 二轮根治：等待窗口随 CPU 超售自适应）

- **上一轮固定 5s/15s 窗口在 load ~89（14 核被 20 会话 + 13 个 jest worker 压到 6~7 倍超售）再度假红**：4 套件 11 用例（loginPage / triggerFormModal / login / RemoteFileBrowser）。分类复现：4 套件 `--runInBand` 单跑 8/8 全绿（负载 82~93），失败形状全部为窗口/预算超时、零断言失败——纯负载时序；红线场景 = 全量 13 worker × 外部负载，4-worker 并行在 load 60 仍绿。
- **核心修复（窗口缩放而非再取大固定值）**：新增单一来源 `src/testSupport/rtlWindow.ts`——窗口 = 5s × 超售系数（1 分钟 loadavg / 核数，向上取整、下限 1、封顶 60s）。`setupRTL.jsx` 全局 `configure` 与各测试文件显式 `WAIT_TIMEOUT` 统一取该值；`jest.config.ts` `testTimeout` = 3 × 窗口（下限 15s、封顶 180s，config 内联同款公式——原生 ESM 加载不能走 moduleNameMapper）。空闲机器与 CI runner 系数=1，行为与固定 5s/15s 逐位一致；绿路径零额外耗时（条件满足即返回，窗口只是失败上界）。
- **真缺陷两处**：① `login.test.tsx` 固定 `sleep 200ms` 后直接断言（不走 waitFor，负载下必假红）→ 改条件化 `waitFor`（同时等 token 写入与跳转），删除死的 `waitTime` helper；② RemoteFileBrowser / RemoteDesktop / RemoteDesktopModal 三测试文件的显式 `WAIT_TIMEOUT = 5000` 绕过全局 configure（自适应永远不生效）→ 改引共享常量，RemoteSessionReportModal 的同名常量为零引用死代码删除。
- **实测**：load 108~138（9~10 倍超售，超过用户红线场景的 89）全量 jest **420/420**、`tsc --noEmit` 0 错、eslint 仅存量 2 警告、prettier 干净（全部同负载下复跑）。残余风险：系数进程启动采样一次，运行中负载再涨数倍仍可能超窗（封顶防病态挂死系有意），重跑即可。

### test（2026-09-26，存量 flaky 测试根治：loginPage/triggerFormModal 的 load 假红收口）

- **根因（纯时序，非逻辑回归）**：14 核共享机 14 个 CPU burner + 外部负载下，两文件重复 10 跑 0/10 通过——10/10 超 jest 默认 5s 单测预算、4/10 另超 RTL 默认 1s `waitFor`/`findBy` 窗口。等待条件本身都指向真实异步条件（antd Form/Modal 校验提交、登录跳转），无固定 sleep 可去除；**否决 fake timers**（antd 内部异步链手工推进会侵入组件行为，不增确定性只增脆弱）。
- **修复（只放宽窗口不放宽断言）**：`jest.config.ts` 加 `testTimeout: 15000`；新增 `tests/setupRTL.jsx`（挂 `setupFilesAfterEnv`）里 `configure({ asyncUtilTimeout: 5000 })` 全局覆盖 `tests/` 与 `src/` 全部 24 处 `waitFor`/`findBy` 站点，免逐文件改写。
- **顺带锁死一个 RTL 装配陷阱**：`configure` 若写在 `setupFiles` 阶段，RTL 首次 import 时 `afterEach` 尚不存在（测试框架未装）→ **自动 cleanup 被永久禁用** → 弹窗跨用例堆积、按钮点到旧用例弹窗（实测 5 用例确定性失败）。`setupRTL.jsx` 头注释与 `jest.config.ts` 内联注释均写明该约束。
- **实测口径**：修复前 0/10（10 次全失败）；修复后同款压力（load 68 > 基线 52）10/10 全绿（每次 21/21）；全量 dashboard：jest 420/420、tsc 0 错、eslint 仅存量 2 警告、prettier 干净。

### ci（2026-09-26，Phase 8 收口：Python 脚本引擎纳入主 CI——本地实测连修五个真缺陷）

- **新 job `cpp-linux-python-tests`**：ubuntu-24.04 + GCC 13，与 `cpp-linux-tests` 同一套全量构建 + ctest，仅叠加 vcpkg manifest `python` feature 与 `-DWINGMAN_ENABLE_PYTHON=ON`。全量跑而非只跑 Python 子集——该开关只追加编译定义与 libs/python 子目录，同一轮同时证明「开 Python 不破坏 Lua/既有路径」。独立 job 而非矩阵维度：既有 Lua 默认路径的 job 原样不动。缓存键独立（`-vcpkg-tests-python-`，归档为基础 job 超集，restore 回退共享前缀防两 job 争同一精确键）。apt 清单补 `autoconf-archive`：vcpkg `python3` → `libb2` 的 configure 需要 aclocal 宏，缺失即 BUILD_FAILED（本地实测抓到，CI 预防）。
- **本地把 Python 维度跑通的过程连修五个真缺陷**——全部是「引擎代码就绪但从未真跑过」的直接后果（默认构建不带 `WINGMAN_ENABLE_PYTHON`，Windows `cpp-python` job 只跑名字转换，沙箱/线程/析构路径零执行）：
  1. **沙箱从未生效**：`applySandbox` 以 `!initialized_` 提前返回，而它只在 `initialize()` 置位 `initialized_` 之前被调用——守卫永远命中，「沙箱」里 `import os`、`open()` 畅通。守卫改为以 `globals_` 就绪为准。
  2. **沙箱白名单构建本身会抛**：`builtins.contains(name)` 走 pybind11 的 `__contains__`，模块没有该属性直接 AttributeError（被缺陷 1 掩盖）——改 `py::hasattr`。
  3. **引擎对象一构造就摸 C-API**：pybind11 的 `py::dict` 默认构造即调 `PyDict_New`，作为成员意味着工厂一创建引擎、在 `Py_Initialize` 之前就 fatal abort——成员改 `py::object`，真正的 dict 在 `initialize()` 里赋值。
  4. **GIL 永不释放，ScriptManager 用法必死锁**：旧 `py::gil_scoped_acquire` 在「进入时已持锁」路径上析构不释放，调用线程从此永久持有 GIL；而 ScriptManager 一律在 detached 工作线程跑脚本——Python 引擎在生产路径上 100% 死锁（30s 超时，gdb 实锤在 `take_gil` futex）。修复：裸 `Py_Initialize()` 后 `PyEval_SaveThread()`，引擎全部 C-API 段改 `gil_scoped_acquire_simple`（`PyGILState_Ensure/Release`，按线程管理 tstate，无 pybind11 全局 tstate 共享隐患）。
  5. **marshal 出的可调用对象析构崩进程**：`toScriptValue` 把 `py::object` 按值捕获进 `ScriptValue` 的 lambda——ScriptValue 在调用方任意线程/任意时刻销毁，pybind11 3.0 的 dec_ref GIL 断言直接 abort（此前被「GIL 恰好被主线程一直持有」掩盖）。改 shared_ptr + 取 GIL 的 deleter，拷贝共享引用。
- **Python 测试从 1 个目标扩到 3 个（19 → 51 用例）**：既有 `python_name_conversion_tests`（camelToSnake 纯函数，不起解释器）之外新增 `python_marshal_tests`（ScriptValue ↔ py::object 全类型往返：bool 先于 int 判定锁序、tuple→array、dict 非字符串键静默丢弃、异常回落 null、C++/Python 可调用互转）与 `python_engine_tests`（真实嵌入式 CPython：工厂注册/执行/函数调用全 marshal 链路/globals 读写/语法错误恢复/沙箱白名单只含白名单键且 import/open 封禁/shutdown 后安全/**工作线程执行**——ScriptManager 的真实线程模型，executeString/executeFile/callFunction 各一例 + 「引擎方法返回后调用线程不持有 GIL」回归锁）。core_tests 里 `PythonStdoutAndStderrRouteToOutputCallback`（stdout/stderr → 输出回调 → script_output 上行）此前从未在任何 CI 执行过，本批随 Linux 全量维度首次真跑通过。
- **评估结论：默认维持关闭**。依据：官方包（release/nightly 默认构建）全 Lua；CPython 依赖面大（vcpkg `python3` 拉起 gettext/libffi/libb2/libuuid 等）且嵌入解释器吃体积；沙箱是白名单而非硬边界（同进程嵌入，属性访问未限制，经典 `__subclasses__` 逃逸路径仍在）；部署侧暂无真实 Python 脚本需求——但本批五个缺陷证明「代码就绪」≠「可运行」，CI 维度从此保证「开关打开即正确」。开启路径已文档化（docs/guide/script-development.md「Python 引擎默认关闭」节），需求出现时重评估。ROADMAP Phase 8 状态据此收口为 ✅。

### feat（2026-09-26，M4 远控 P1：浏览器内录像回放——SessionRecording 本地解析）

- **回放入口**：录像列表新增「回放」（与下载同级，desktop:view 即可回放，审计语义不变——取回本身已走 `desktop.recording_download`）；`RemoteRecordingPlayer` Modal 内本地解析 `.mjs`，SessionRecording 就在既有依赖 guacamole-common-js@1.5.0 里，零新增依赖（设计 §16「回放」落地，§11 远期清单同步销项）。播放/暂停/进度/seek/时长全走 1.5.0 官方 API；不做倍速——1.5.0 无变速 API，不硬造。
- **上游 Blob 直连缺陷绕行**：1.5.0 `new SessionRecording(blob)` 构造即抛 `Cannot read properties of undefined (reading 'size')`——构造器只给 tunnel 分支赋 `recordingBlob`，Blob 分支漏赋值；npm 无 >1.5.0 修复版。绕行走官方「边下边播」同款 tunnel 源：`BlobRecordingTunnel`（`recordingPlayer.ts`）把已取回的 Blob 按裸指令流喂 Parser，并配防退化用例锁「不许改回 Blob 直连」。
- **样例录像**：本机无任何真实 `.mjs`（录像卷/目录皆空），按裁定造最小样例而非硬接空数据——`gen-sample-recording.js` 生成 12 帧/4.4s/1280x720（裸指令流 + sync 分帧，与 guacd 录像格式一致），入库 `recordings/sample-session.mjs`，栈起来后回放面板即有真实条目；首帧时间戳从 1000 起（`isPlaying` 以时间戳真值判定，0 会永久「未在播放」）。guacd 真实录像出现即插即用。
- **顺手修存量缺陷**：`useGuacamoleSession` fit 自适应缩放对 `display.scale` 用赋值写法——1.5.0 里它是方法，赋值只会顶掉方法、缩放永不生效；改为调用并同步修 `.d.ts` 类型标注。该文件为桌面会话共享路径，但改动仅此一处语义修复，桌面主链路用例同步更新且全绿。
- 测试：recordingPlayer 封装 8 项（真实库解析样例录像：帧表/进度事件/播放暂停/seek 钳位/坏载荷报错/Blob 直连防退化）+ 回放面板 9 项（加载/就绪/错误三态、控制条联动、重试、关闭清理、fitScale）+ 服务层 `fetchRecordingBlob` 2 项（blob 直传/非 Blob 包装）；全套 420 项 jest 全绿。

### feat（2026-09-26，M4 远控 P1：阶段二页面层 i18n 接线——录像面板文案 8 语言全量）

- **范围裁定（只接「机制所在层」）**：Agents 页的阶段二文案——连接表单「会话录制」勾选 + 会话录像面板 19 条（标题/关闭/空态/列头/回放/下载/删除确认与提示/错误兜底）——改走 umi intl（`pages.agents.recordings.*`），zh-CN/zh-TW/en-US/ja-JP/pt-BR/bn-BD/fa-IR/id-ID 八份 locale 全量补齐；插值占位符用 `{name}`（react-intl 惯例，同页 `shutdownConfirmContent` 先例）。
- **组件层明确不接（留档）**：`RemoteDesktop/*` 是给未来 cockpit 复用的公共件（「Modal 与未来 cockpit 按同一入口接入」），全目录零 `@umijs/max` 依赖是既成设计；且 umi 的 `useIntl` 在 jest 下不可用（`src/.umi` 生成目录不入库、react-intl 未 hoist 到顶层），组件接 intl 需引入 react-intl 直依赖或文案 props 化并补测试基建——架构代价大于本轮收益。回放面板/文件浏览器/剪贴板等组件文案维持中文硬编码（P0 先例），多语言策略留给消费方决定。
- 纯文案接线：不改业务逻辑、零新增依赖，zh-CN 渲染文本与接线前逐字一致；全套 420 项 jest、tsc、eslint（仅存量 2 警告）、prettier 全绿。

### feat（2026-09-26，M4 远控 P1：文件浏览器第二版——分页/进度/重试/文件操作审计）

- **大目录**：协议一次 `get` 返回整个目录体（**无服务端分页**），分页做在浏览器侧（每页 50 行、小列表自动隐藏翻页，排序维持目录在前名称升序）；目录体聚合加 8 MiB 护栏，超限拒绝并停止 ack——ack 纪律即流控，guacd 自然停发，防异常目录打爆内存。
- **关键协议修正（v1 遗留缺陷）**：1.5.0 `BlobWriter` 对错误 ack **只停发不报错**（`onerror` 仅本地读文件失败触发，dist/esm 实测佐证）——v1 上传失败路径是死代码，失败时 Promise 永不落定。v2 从 `writer.onack`（`status.code !== 0`）判定失败，并叠「无进展看门狗」（默认 60s，可注入，0 关闭）兜底协议静默；下载侧协议无错误 ack 可判，停滞检测同靠看门狗。
- **进度与有限重试**：下载逐块上报已收字节（总量取 `entry.size` 折百分比）、上传走 `BlobWriter.onprogress`（逐 ack 触发，第二参为已发字节）；`withRetry` 默认 2 次尝试按序退避，重试耗尽才报错，实际尝试次数进审计。
- **文件操作审计（服务端）**：新增 `POST /api/remote/file-ops`（desktop:view 路由组；upload 为写动作，handler 内联追加 desktop:control——监看可浏览/下载、上传仅接管的权限矩阵在审计通道同样成立）。**信任模型**：上报是客户端 best-effort，会话/agent/操作者由服务端按票据反解（网关 WS 建连后存「票据→会话快照」，closeSession 清除；请求体同类字段不采信），反解不到（进程重启/会话已关）落 `no_session` 降级行而非拒绝；审计 kind `desktop.file_download`/`desktop.file_upload`；列表高频不审计（与 agents 列表同策略）。
- **协议边界不动**：删除/重命名仍不可行（对象流只有 get/put）、服务端分页不存在，均不硬做（设计 §15.2 明确）。
- 测试：Go 新增 7 个 handler 测试（票据→会话快照全链路生命周期走真 mock guacd WS、载荷校验、权限矩阵、降级行、截断）；Dashboard filesystem/浏览器/面板/Modal/服务层全链路测试（401 项 jest 全绿）。

### feat（2026-09-26，M4 远控 P1 递补：SSH 文件浏览器——onfilesystem SFTP 树）

- **协议边界即能力边界**：Guacamole 1.5.x 对象流只有 `get`/`put`，**没有 delete/rename 指令**——浏览器第一版只有列目录/下载/上传。删除不是被砍掉，是线协议不存在该指令；需求出现走会话内 shell（`rm`）或等上游扩展，不自行造指令。
- **复用公共件，不在消费方另起一套**：`RemoteFileBrowser` + `filesystem.ts`（列目录 JSON 分块聚合、逐 blob ack 流控、BlobWriter 上传完成手动 `sendEnd`）全部落在 `RemoteDesktop/` 公共出口，Modal 与未来 cockpit 按同一入口接入。工具栏入口仅 SSH 渲染、SFTP 对象上报后才可展开；监看可浏览/下载、上传仅接管（收发不对称先例同 §14 剪贴板）。
- **网关零改动**：`filesystem`/`get`/`put`/`body`/`ack` 等对象流指令对网关就是普通数据帧（wsToGuacd/guacdToWS 原样转发）；新增 `guacamole_filesystem_test.go` 透传回归测试，锁「网关不碰对象流」不变式。
- **stage 不变量**：浏览器展开/收起只改布局，像素面 stage 恒挂载——卸载即销毁 display 元素，无法恢复。
- **录像回放可行性复核**（只给结论不实现）：`Guacamole.SessionRecording` 就在既有依赖 guacamole-common-js@1.5.0 的 npm 包内（`_PlaybackTunnel`/逐帧解析/play-pause-seek 实测在），「官方 session-player 非 npm 分发」前提不成立——**可行、零新增依赖**，缺的只是 .mjs 数据源适配与播放器壳；待真实录像数据出现再接（设计 §16「回放」）。
- 新代码四项覆盖率 100%（filesystem.ts / RemoteFileBrowser.tsx / useGuacamoleSession.ts / RemoteDesktopToolbar.tsx / RemoteDesktopPanel.tsx / types.ts）。

### style（2026-09-26，Dashboard 存量 36 文件 prettier 对齐）

- **prettier ^3.8.3 全量检查暴露 36 个存量文件格式漂移**（最老 2026-05-10 的 PageStatePanel/global.less，新到 2026-09-22 的 http.test.ts）：这些文件写入时点早于当前锁定版本的格式化规则（长行折叠、数组逐项换行、JSX 缩进层级），`lint:prettier`（检查+写入）后一次性对齐。**被审 4 个远控提交零命中**（漂移清单与 RemoteDesktop 系文件无交集）；`git diff -w` 抽查为纯重排无语义变更，tsc + jest 351/351 复跑实证。仓库 `lint:prettier` 脚本本就是「检查+写入」自愈口径，此前各轮只跑 `-c` 检查未见漂移是因 prettier 依赖近期才升到 3.8.x。

### test（2026-09-25，M4 远控 P1：三协议 e2e 容器栈补强）

- **镜像全部钉 digest**（guacd 仍锁 1.5.5）：四个目标端点除 guacd 外全用 `latest`，上游任何一次重建都会**静默换掉镜像内容**，e2e 会在无人改代码的情况下变红，而排查方向会先怀疑网关。护栏是 `TestGuacdE2EComposePinsImageDigests`——没有它，改回 `latest` 不会有任何测试变红。
- **一键脚本 `scripts/verify-guacd-e2e.sh`**（up/test/run/down/status）：`compose up -d` 在容器**尚未监听端口**时就返回（xrdp 首启要几十秒），原文档没写「等就绪」，于是「栈没起好」与「代码有 bug」两类失败形状一模一样，只能靠人肉重试区分。脚本把「谁没就绪」显式报出，沿用仓库三态 PASS / FAIL / SKIP(未验证)——绝不把「没跑成」判成通过。引擎探测不只 `command -v`：装了 docker 但没装 compose 插件时报 SKIP 并说清缺什么，不在 compose 那一步炸出无关的 `unknown command`。
- **四服务带 healthcheck**：用 bash `/dev/tcp` 探端口，**不依赖 curl/nc**（目标镜像未必自带）。端口可达只说明「已监听」，不等于「协议握手可用」，后者交给 e2e 用例自己的等待窗口。
- **契约测试**（`guacd_e2e_script_test.go`，10 例）：脚本三态（无引擎 / 引擎缺 compose / 端口未就绪 / 未知子命令 / 未知 flag / `--help`）、compose 四服务钉 digest + healthcheck 探针形状、脚本端口与 e2e 目标端口一致、`WINGMAN_GUACD_E2E` 门控仍在。用假引擎 + 净化 PATH 在 0.5s 内跑完，**不碰真容器**。

### feat（2026-09-25，M4 远控 P1：远程桌面会话审计报表）

- **另立专表而非复用 AuditLog**：`AuditLog` 是事件流水（一条事件一行、meta 是 JSON），适合「谁在何时做了什么」的合规逐条查；报表要的是「这台机器被谁接管了多久、失败几次」，需要**可聚合的结构化列**——在 JSON meta 上聚合既慢又脆（`json_extract` 走不到索引，且 meta 结构会随事件演进漂移）。新增 `models.RemoteSessionAudit`（表 `remote_session_audits`），既有四事件审计**保持不变**：两者受众不同，缺一不可。
- **只写终态**：仅 `closed`/`failed` 落行，进行中不落。否则报表把未结束会话算进时长、进程崩溃还留永远不闭合的脏行（无法与真实零时长会话区分）。代价是「当前几人在看」拿不到——那是实时态，属 registry 职责不属审计。`RecordRemoteSession` 为唯一落库入口（UTC 归一/时长口径/终态枚举是契约，散在调用点迟早分叉），写失败只记日志不阻断会话关闭。
- **录像可关联**：`recording_name` 与录像检索 API 的 name 同值（`{agentID}-{sessionID}.mjs`）；会话 ID 提前到拨号前生成，故**建连失败也有唯一标识**（否则报表只能说「有一次失败」而无法定位）。
- **查询接口** `GET /api/remote/sessions`（desktop:view，不新增 RBAC 码——报表是只读视图，无接管能力）：多维过滤（agent/协议/操作者/状态/监看接管/录制/时间范围）+ `groupBy`（白名单化，杜绝 SQL 注入）+ `bucket`（hour/day/week/month）。**一次请求返回列表 + 汇总 + 维度聚合 + 时间趋势四块视图**——分四个端点迟早出现「列表 12 条、汇总 13 条」的口径分叉；四条查询共用同一过滤器，汇总/分组/分桶基于全量而非当页。
- **Dashboard**：`RemoteSessionReportModal`（Agents 页「会话审计」入口）渲染汇总六项 + 维度分布 + 时间趋势 + 明细表，与录像面板并列——录像面板管**文件**，本面板管**行为**。前端不做二次聚合（后端已保证四块视图同口径）。
- **测试**：Go 侧 12 个用例（落库契约的 UTC/时长/枚举归一、16 组过滤口径、多维度分组、四种分桶、分页口径、五条查询失败分支逐条注入、gorm 回调链探针）；Dashboard 侧面板 14 例 + 服务层 7 例。**新文件 stmt/branch/func/line 四项 100%**；jest 328 → 351。Swagger 已再生成（仅新增 `/remote/sessions`）。

### refactor（2026-09-25，M4 远控 P1：远程桌面前端公共件抽取）

- **动机**：`RemoteDesktopModal` 此前是「连接逻辑 + 阶段二 UI」混在一起的单体，cockpit 要复用连接语义只能复制粘贴——而设计 §7 第 3 条明确禁止（复制品会在协议细节上分叉）。按该条约定抽公共件 `src/components/RemoteDesktop/`，四件边界一一对应：`useGuacamoleSession`（连接生命周期）、`TicketClient` 接口 + `createWingmanTicketClient`（票据客户端）、`RemoteErrorNotice`/`classifyRemoteError`（错误与降级）、`RemoteDesktopToolbar`（监看接管与工具栏）。
- **容器解耦**：`RemoteDesktopPanel` 是不假定容器的合成件（画布 + 工具栏 + 错误条）；`RemoteDesktopModal` 降级为 Modal 容器适配器，cockpit 可直接放进抽屉/全屏页而无需 fork。`TicketClient` 抽成接口正是因为两边 API base 与鉴权方式可能不同。
- **类型单一来源**：`RemoteProtocol`/`RemoteSessionParams` 收敛到公共件 `types.ts`，`services/remote.ts` 不再重复声明——协议枚举分叉正是 §7 要防的那类复制分叉。顺手补齐 `guacamoleWSPath` 的 https→wss 分支可测性（loc 可注入）。
- **错误分类落地**：权限拒绝/能力未配置**不渲染重试按钮**（重试无意义且诱导反复点），只有断链与未知类给重试——票据一次性意味着重试只能是重新申请票据 + 重建会话。
- **死代码清理**：`classifyRemoteError` 三处 `text ||` 兜底为不可达分支（命中条件本身要求原文非空）已删；工具栏 file input 复位改用 `e.target`（查 ref 是不可达分支）。
- **测试**：新增 `RemoteDesktop/index.test.tsx` 60 例（hook 生命周期/取消竞态、协议能力派生、四类错误分类、工具栏监看与 VNC 约束、面板下载与剪贴板），补 `services/remote.test.ts` 5 例（wss 分支、录像兜底、非 Blob 响应）。**新文件与改动文件 stmt/branch/func/line 四项 100%**；Dashboard jest 261 → 328。
- **附带修 load-induced flake**：`RemoteDesktop*` 两个测试文件的 `waitFor` 统一放宽到 5s（只放宽等待窗口不放宽断言）。实测 8 份全量 jest 并发压满 CPU 时，旧的 1s 窗口会假红；放宽后同款压力下新代码 0 失败。**注**：`tests/loginPage.test.tsx` 与 `tests/triggerFormModal.test.tsx` 在同款压力下仍有既有 load flake（本轮未触碰其代码，已在基线提交上复现确认为存量问题）。

### feat（2026-09-25，M4 远控阶段二：Guacamole 剪贴板 / 文件传输 / 会话录制）

- **选型落地**：第三方 VNC/SSH/RDP 远控集成采用 Apache Guacamole（guacd 1.5.5 协议翻译 + Go server 反代 WS 网关 + 浏览器 guacamole-common-js 像素面），runtime 零参与不破架构硬约束；备选方案（myrtille、websockify+novnc、自研三协议客户端）否决理由见 `docs/remote-gateway-guacamole-design.md` §9。P0 网关/票据/RBAC/Dashboard 弹窗已先行合入（20fdb30），本轮补齐阶段二 DG-7/8/9：
- **剪贴板（§14）**：`onclipboard` 接收（逐块 ack）+ `createClipboardStream` 发送（text/plain），监看模式隐藏发送 UI；网关零改动纯透传。
- **文件传输（§15）**：按协议注入 connect 参数——SSH `enable-sftp=true`、RDP `enable-drive`+`drive-path`（默认 `/wingman-drive`）、VNC 无通道（UI 整块隐藏）；上传 `createFileStream`+`BlobWriter` 分块，下载 `onfile` 聚合 Blob 触发浏览器下载。
- **会话录制（§16）**：票据新增 `record` 字段（未配置双路径时 400 拒绝并附指引）→ connect 注入 `recording-path/{agentID}-{sessionID}.mjs`；安全默认 `recording-include-keys=false`（按键内容永不入录像）；新增检索 API：`GET /api/remote/recordings`（desktop:view，mtime 倒序）+ `GET .../:name/download`（desktop:view + 审计）+ `DELETE .../:name`（desktop:control + 审计），名字 basename+.mjs 白名单防穿越，未配置返回结构化 501。
- **配置**：`WINGMAN_GUACD_DRIVE_PATH` / `WINGMAN_GUACD_RECORDING_PATH` / `WINGMAN_RECORDING_DIR` 三环境变量；`deployments/guacd` compose 增加 drive/recordings 共享卷与 README 说明。
- **Dashboard**：类型声明补齐 1.5.0 实测流 API（InputStream/OutputStream/BlobWriter/Status.Code）；remote.ts 增 record 透传与录像三 API；RemoteDesktopModal 增剪贴板面板（Unicode 安全 base64 编解码）、上传入口（SSH/RDP）、下载、录制指示；Agents 页连接表单增录制勾选 + 会话录像管理（列表/下载/删除）；access 增 canDesktopView/canDesktopControl。
- **测试**：Go 侧新增 mock-guacd（讲线协议的假 guacd）三用例验证 connect 按位注入（SSH SFTP+录制、RDP drive、VNC 无文件参数）+ recordings handler 6 用例 + guacamole 参数单元 4 用例；Dashboard jest 235 → 261（modal 流行为/服务层录像 API）；Swagger 注解补齐 remote REST 端点并再生成。验证基线：Go `go vet`+`go test -race` 14 包全过、tsc 0 错、eslint 仅存量 2 警告。

### fix（2026-09-25，Windows CI 假绿揭穿与四用例平台前提修复）

- **Windows CI 的「success」是退出码吞没造成的假绿，4 个失败用例自 batch11 合入起就一直在失败**（对照 70c44b9 与 438e80c 两轮 job 日志实证）：`run-windows-coverage.ps1` 为防 OpenCppCoverage 退出期 ACCESS_VIOLATION 误伤，对非 1 退出码一律放行——昨晚测试红 + 退出期崩溃（exit -1073741819）被吞成 0 判绿，今晨进程正常 exit 1 才首次如实报红。**修复**：bat 内将测试输出重定向至日志文件，脚本改为以 gtest 的 `[  FAILED  ] N tests` 汇总行为失败判据（退出码仅作辅助），失败时输出逐条 FAILED 清单与日志尾部 40 行供排查；PASSED 汇总行同步打印，CI 日志恢复对测试结果的可观测性。
- ① **`MacroModuleFullFamilyViaLazyDefault`**（`batch11_glue_coverage_test.cpp`）：save/load 路径硬编码 `/tmp/...`——Windows 无此目录（saveToJSON 打开文件即失败），改 `temp_directory_path()`；同用例的「不存在文件」负例与 Bitmap 用例的 save 负例路径一并迁移。
- ② **`BitmapBmpParsingAndSaveFailureBranches`**（同文件）：测试目录改 `temp_directory_path()` 下进程唯一子目录；坏 planes / 坏 bitCount 两条断言按平台分支——Windows 的 `Bitmap::fromFile` 走 GDI+ 系统解码器（拒坏签名但对 planes/bitCount 头字段宽容，实测照样解析出 2x2 图），Linux/mac 走自写解析器严格拒绝，断言各平台的真实语义；坏签名 / 截断 / noread 断言两平台一致不动。
- ③ **`UiaModuleGlueTest` 两用例**（`misc_uia_node_coverage_test.cpp`）：用例设计前提是「Linux stub 无后端」，Windows 有真实 UIA Automation 系统服务——`AllFindFunctionsReturnNullWithoutBackend` 在 `_WIN32` 下 `GTEST_SKIP` 明示（team 空态用例同款模式）；`EventListenersAllBranches` 前 6 条防御分支断言 Windows 同样成立不动，仅 threadSafe 两条按平台分支（Linux 无后端 listenerId 恒 0，Windows 真实注册返回正数 id）。
- 本组 4 用例在 Linux（常规 + ASan/UBSan）复验全绿，Windows 行为由 CI 验证。

### fix（2026-09-25，ASan+UBSan 首战四组真实缺陷：悬垂回调 / 析构期日志 / memcpy UB / sqlite 句柄泄漏）

- **ASan+UBSan 全量插桩首跑即抓到 12 个用例失败，归并为四组根因，全部修复并复验**（`-fsanitize=address,undefined` + `UBSAN_OPTIONS=halt_on_error=1`，vcpkg 依赖不插桩；插件构建目录 `build-san` 仅测试用，不进常规 CI）。
- ① **InotifyFileWatcher pollLoop 在锁外执行用户回调**（`inotify_filewatcher.cpp`）：回调先拷贝进局部 `pendingCallbacks` 再释放锁执行——`unwatch()` 在此窗口移除 entry 并释放调用方上下文后，旧拷贝仍被调用（ASan 实测 heap-use-after-free：gtest Test 对象析构后 pollThread 还在跑其 capture `this` 的 lambda）。修复：回调执行挪进锁作用域（单 poll 线程，锁内执行无并发损失），`mutex_` 改 `std::recursive_mutex` 使回调重入 `watch()`/`unwatch()` 不死锁。
- ② **DbConnection::close() 在析构路径打日志**（`db_module.cpp`）：进程退出时静态对象析构顺序不定，spdlog registry 先析构（销毁全部 logger）、存活期更长的 DbConnection 后析构，`close()` 里的 `spdlog::debug` 访问已销毁 logger（ASan 实测 heap-use-after-free，6 个 DB 用例同根因）。修复：`close()` 删日志并注释「析构路径禁止打日志」。
- ③ **Tensor::createFloat32/createInt32 空数据 memcpy(null, null, 0)**（`ml_stub.cpp`）：UBSan 实测 null pointer passed as argument 1——C 标准明文 memcpy 参数不可为 null，与 size 是否为 0 无关。修复：`byteSize > 0` 守卫。
- ④ **KeyValueStore sqlite3_open 失败分支泄漏连接句柄**（`kvstore.cpp`）：sqlite3_open 失败时 handle 仍非空（供取错误信息），原代码直接 `return false` 不关闭（LSan 实测泄漏，save/load 两处）。修复：失败分支补 `sqlite3_close(raw)`。
- **测试侧配套修复**（`filewatcher_test.cpp`）：`UnwatchPath` 故意保留的 dir2 watch（断言意图所在）与 `WatchFileInsteadOfDirectory` 的 filePath watch 均不被 TearDown 的 `unwatchPath(testDir)`（精确路径匹配）清理，回调 capture `this`，测试对象析构后条目残留进程级单例，后续任意事件触发即 UAF（事件到达时机不定，表现为 flaky）——两用例在行为断言完成后显式补清理。修复后 FileWatcher 全套 12 用例 ASan 下 15 轮压竞态复验全绿；**ASan+UBSan 全量复跑 2180 用例零失败，首轮 12 个失败全部清零**。
- **win32 后端同构缺陷预防性修复**（`win32_filewatcher.cpp`/`.hpp`）：`checkIOCompletion` 与 inotify 修复前完全同构——锁内收集 pendingCallbacks、锁外执行，unwatch 窗口同样 UAF。同步修复：回调执行挪进锁块、`watchesMutex_` 改 `std::recursive_mutex`（7 处 `lock_guard` 显式模板参数随之同步，linux 后端用 CTAD 无此问题）。Linux 主机无法编译 _WIN32 分支，语法经锁块配对与引用清点自查，行为由 Windows CI 验证；mac/fsevents 后端已用 `shared_ptr<CallbackContext>` 拷贝持有方案，无此缺陷，不改动。


### test（2026-09-24，C++ 第十一批补测：EventHub 全家族 + task 重试间隙 + inotify 独立实例 + clipboard flaky 收口）

- **新增 27 用例，行覆盖 91.0% → 91.9%（13098 → 13104 行，miss 1185 → 1062），函数 95.2% → 95.6%**（v18 基线，全量 2030 用例：1998 PASSED + 32 环境性 skip；基线为清空 gcda 后全量重采——增量编译后旧 gcda 与新二进制 checksum 不匹配，libgcov 的 overwrite 会混入旧版行布局数据，凡增量重编译后必须先 `find -name '*.gcda' -delete` 再采集）。目标文件（v18 绝对值）：**event.cpp 91.6% → 94.4%**（107 行，函数 100%）、**task_module 95.3% → 97.5%**（321 行，函数 100%）、**inotify_filewatcher 94.0% → 96.2%**（函数 100%）、**inbox_module 95.9% → 97.2%**（函数 100%）、transport_module 89.0%（剩余全为防御与伪影，见论证）、x11_screen 94.8%（函数 100%）。
- ① **EventHub 6 用例**（`batch11_event_coverage_test.cpp` 新文件，此前零覆盖）：空 type 订阅/发射拒绝、type 超长（含边界 256 可接受）、订阅名超长、单事件订阅数上限（1000 满载后第 1001 个被拒 + 清场后可再订阅证明拒绝来自上限而非 type 损坏）、once 订阅 emit 成功后自动移除 + handler 抛异常被 catch 吞噬不阻断同 type 其他订阅者（unordered_map 迭代序不定，用例不依赖顺序）。
- ② **A11 胶水 7 用例**（`batch11_glue_coverage_test.cpp` 新文件）：task async 语义大用例（运行中查询 pending/running→cancel→wait→canceled、timeoutMs<work 时长置 timeout、wait 自身超时置 timeout 并 emit、fast/slow 双任务驱动 cleanupFinishedTasks 两分支、metadata 传 callable 走 toJson default 分支）；**task 重试间隙用例**（`maxRetries=1` + 抛异常 fn + backoff 睡眠期间 cancel/timeout 竞速——循环顶 `isCanceled()`/`status()==timeout` 早退分支，此前 execute 一轮循环永不回头检查）；team 空态查询、inbox report 非 string 结果、BMP 解析与 save 失败分支、macro 全家族（lazy 默认实例 + playback 空事件）、screen bounds 静态查询。
- ③ **platform_x11 8 用例**：屏幕监控查询降级链、DPI 资源管理器链、无 display 静态 API 优雅失败、空区域截图早退、未知鼠标键回退左键、死句柄窗口居中 false、inotify 后端名与普通文件递归早退；**inotify 独立实例用例**（工厂函数直构，避免单例 shutdown 污染后续用例）——工厂二次 `initialize()` 幂等回归（本批 fix，首跑 terminate 实测）、回调先计数再抛异常被 pollThread catch 吞噬且 watcher 存活、`chmod 000` 子目录触发递归遍历逐条跳过（root 下权限检查旁路不断言）、shutdown 对活跃 watch 执行 rm_watch 清理。
- ④ transport_inbox 1 用例：`tcp://host` 无端口变体的 host 解析 else 分支（port 落默认值，连上/失败均合法只防句柄泄漏）+ report 第三参非法 JSON parse 抛出被 catch 回退原始串（解析在 handle 校验之前，无需真实连接）。⑤ trigger_posix：`ScriptActionsRunAndEmptyBranches` 补 `registerLuaEngine()`——lua 引擎靠显式注册（runtime main 同款），不注册时 createEngine 返回空、RunScript 分支静默落 warn 兜底；修复后 engine 创建/`registerAllModules`/`executeString`/shutdown 真实链首次覆盖，另补运行中二次 start 的 CAS 幂等早退。⑥ posix_process 2 用例（waitForever 阻塞至子进程退出、未知 pid 的 name/path 空串防御）。⑦ inbox_downlink 1 用例（consume 对 null/float payload 的容错分支）。
- **顺带发现并修复的两处时序性漏覆盖/挂死**：① `TcpSelfConnectSendAndSessionManagement` 的 server→client 推送后立即 closeSession——v17 基线实测消息回调两行时序性漏计（v16 纯属派发先于清理的巧合命中），sendTo 后补 100ms 收包等待（loopback <1ms）后稳定。② `X11WindowCloseCenterAndWaitFamily` 的 forceClose 外部持窗子进程——v18 首轮采集实测一次整跑挂死：子进程 `XCreateSimpleWindow` 失败不检查（句柄 0 照发，父侧 forceClose(0) 落空）且两侧 `select`/`waitpid` 均无超时，任一环节落空即互等死锁。修复：子进程 w==0 早退 + 限时 select（10s），父侧 waitpid 改 5s 限时轮询、超时 SIGKILL 收尸并如实报失败。
- **clipboard flaky 收口**：xclip 每次 setText/clear fork 一个 daemon **异步**接管 CLIPBOARD selection——写后立即查询命中旧 owner（实测读回上一用例内容），固定 sleep 在高负载下不可靠（xclip 启动实测 >500ms）；新建 `clipboard_poll.hpp` 共享 helper（40×50ms=2s 谓词轮询，命中即真的后端零等待），6 个测试文件 12 处写后断言统一迁移，本批开发中依次冒头的 flaky 五连（SetUnicodeText/SpecialCharacters/HtmlImageAndFiles/ClipboardTextRoundtrip/Clear）全部收口。
- **team 空态用例 SKIP 模式**：TeamManager 进程级单例 client 不可销毁，空态前提（无 teamId）仅在独立进程成立；gtest 单进程按注册顺序运行（A11 注册靠后，此时其他用例已留下 teamId）——用例以 teamId 检测前提、不满足时 GTEST_SKIP 明示，ctest 独立进程模式（gtest_discover_tests）下前提成立完整断言。
- **task 生命周期语义实测记录**：async 任务达终态即被自己 worker 收尾的 `cleanupFinishedTasks` erase（事后 status 恒 "failed"、wait 已 erase 任务恒 false）；同步任务完成后留 map 可 retry 复提交，但任何 async worker 收尾的 cleanup 会把已终态同步任务一并 erase——retry(syncId) 断言块必须先于任何 async 提交。
- **不可覆盖论证（不硬凑）**：transport 270-273/342-354 为 client/server 工厂创建失败（bad_alloc 级）与 server 线程启动失败防御，不可稳定注入；inbox 82-84 为 TCP 握手成功后注册消息立即发送失败（发送缓冲区空时不可注入）、232/287 为模块内部类 InboxClient 的 send/messageCallback_ 胶水面无暴露；task 38 为枚举全 case 后 default 兜底、306 为 TaskManager 析构期 shutdown_ 防御（submit-after-shutdown 仅存在于进程退出瞬间，无观察点）；screen 813 为 BMP 写 header 失败（磁盘满/IO 硬错误）、956/964 仅无 X11 连接环境可达（测试进程预置 DISPLAY）、335 的 seekg 超文件尾在 libstdc++ 不置 failbit（u32 offset 经 streamoff 恒非负）；inotify 26-27 为 inotify_init1 资源耗尽、143-144 为 removeWatch 与内核 IN_IGNORED 事件竞态；posix_trigger 343-344 需真实屏幕像素在两次采样间变化超容差、349-350 为全枚举 switch 兜底、461-462/491-492 为 zenity/aplay fork 子进程侧（exec 替换映像或 `_exit`，gcda 永不写出——gcov 结构性盲区，行为由 PlayAudioValidPathForksPlayer 实证）；x11_screen 65-66 需多显示器 primary 标志（Xvfb 单显示器不置位）、146/172-173 为 DisplayWidthMM/noutput 畸形数据防御、184-185 为显示器热拔竞态；posix_process 191-216 为 fork 子进程侧同上盲区（行为由 WaitForever 等实证）、34/61/99/107/185 为 /proc 未挂载级病态环境与 fork 资源耗尽防御、61/281 换行剥离分支因读取路径产出不含尾换行为防御性不可达；x11_factory 40-70 为无 X/无扩展环境拒绝分支（测试进程预置健康 Xvfb 恒不触发）。
- **gcov 行归属伪影清单（分支实际已覆盖，不硬凑）**：跨行 `spdlog::warn` 调用首行无指令归属（条件行/续行/return 计数齐全）——event 23/29/39/102；跨行 `ScriptValue::fromObject` 块中单行漂移——transport 287/293/362/368/454/469/503、inbox 505；`push_back({...})` 收尾行——transport 320/328/418/443/521、inbox 567/578/589、task 497/503/510/516/522；其余——task 213（wait 超时 emit 参数中间行）、transport 279-281（跨行 debug 调用）、posix_trigger 288/454（跨行构造/lambda 收尾）、posix_process 176（循环闭合）。
- **Windows CI 编译两轮收口**（[de76a93](https://github.com/cuihairu/wingman/commit/de76a93)、[00256c4](https://github.com/cuihairu/wingman/commit/00256c4)）：batch11_glue 先是多余 `unistd.h`（MSVC C1083）、删除后暴露真实 POSIX 调用 `geteuid`/`chmod`（权限块属 POSIX 语义，整块 `#ifndef _WIN32`）——本地 Linux 全绿不代表 Windows 可编，且 MSVC 在 C1083 预处理即断、并发编译取消其余单元，会掩盖后续文件与同文件后续符号错误；教训：跨平台测试文件 push 前两步自查（POSIX-only 头 + POSIX 符号调用），root 权限类语义用 `_WIN32` 条件编译隔离。

### fix（2026-09-24，task async 等待唤醒缺陷 + X11 屏幕/录制健壮性）

- **Task::execute 两处终态置位缺失 notify，`task.wait` 只能睡满 deadline**（`task_module.cpp`）：成功路径置 `succeeded` 与超时监控线程置 `timeout` 都只改状态不 `cond_.notify_all()`——`Task::wait` 阻塞在条件变量上无人唤醒，只能等到自身 deadline 超时才醒来收尾（第十一批补测用例实测：500ms 完成的任务令 `wait(5000)` 阻塞满 5s 才返回 true）。修复：两处置位后补 `notify_all`（`cancel`/`setStatus(failed)` 路径既有 notify 不变）。
- **X11Screen 无宽容 X error handler，死句柄查询杀死整个进程**（`x11_screen.cpp`）：`getMonitorFromWindow(已销毁窗口)` → `XGetWindowAttributes` BadWindow → Xlib 默认 handler 直接 `exit()`（新用例 `ScreenMonitorQueryFallbacks` 实测测试进程消失）。修复：对齐 `x11_capture.cpp`/`x11_window.cpp` 既有模式，`initialize()` 幂等安装宽容 handler，失败请求以降级返回值呈现而非进程死亡。
- **宏录制流动性检查时序缺陷：任何健康 X server 上录制都无法启动**（`x11_recorder.cpp`）：`g_recordDataFlowing` 原在 `XRecordEnableContextAsync` 之后清零——libXtst 的 EnableContextAsync 会在返回前同步投递 StartOfData 到回调，enable 之后再清零会把回调刚置位的标志抹掉，300ms 流动性检查永远超时（此前被误判为「Xvfb 的 RECORD 扩展必然失败」，实为本序缺陷）。修复：清零前移到 enable 之前；注释补记控制/数据双连接请求无全局顺序、CreateContext 后必须对控制连接 XSync。`recorder_x11_e2e` 同步修正：XTest 注入后 `XFlush`→`XSync`（XFlush 只写 socket 不等 server 消费，FakeInput 请求在 Xvfb 上实测滞留 2s 不注入），Xvfb（RECORD 1.13）实证可用，用例从「仅真实桌面」放宽为「无 X 才 skip」。
- **InotifyFileWatcher::initialize 无幂等防护，二次调用 std::terminate**（`inotify_filewatcher.cpp`）：`initialize()` 直接对 `pollThread_` 赋新线程——对象已初始化时该线程 joinable，对 joinable 的 `std::thread` 赋值直接 `terminate`（第十一批补测独立实例用例首跑即崩实测）。产品单例路径（static lambda 只初始化一次）侥幸未触发，但工厂函数与单例初始化路径各自调一次的组合随时可能踩中。修复：开头 `if (initialized_) return true` 幂等防护。
- **EventHub Meyer's 单例在进程退出期被 emit 撞段错误**（`event.cpp`）：命名空间级静态 `g_taskManager`（main 前构造）析构晚于 Meyer's 静态局部 hub（首次 emit 才构造）——退出析构逆序使 hub 先死、TaskManager 析构后死；`shutdown()→join` 等待残留任务线程完成期间，任务线程 `emit("task.canceled")` 访问已析构的 subscriptions_ 直接 SIGSEGV（全量测试连续两次稳定复现，gdb 抓栈定位）。TaskManager 设计上不逐任务 join（用例结束≠任务线程终结），runtime 退出时活跃 task 同样踩中。修复：EventHub 改泄漏式单例（`static EventHub& hub = *new EventHub()`），基础设施退出期必须可 emit，内存交由 OS 回收。

### fix（2026-09-23，IPC 双通道 SIGPIPE 进程杀手 + notify 桥订阅泄漏）

- **UnixSocket/Tcp 通道对端断开后 send 触发 SIGPIPE 终止整个进程**（`unix_socket_channel.cpp`/`tcp_channel.cpp`）：POSIX 默认语义下向已断开对端 send 收到 EPIPE 的同时进程被 SIGPIPE 杀死——GUI/runtime 同样暴露，且 receiveLoop 感知断开与下一次 send 之间存在天然竞态窗口（第十批补测用例首次实证：`SendAfterPeerDisconnectFailsGracefully` 无修复时整个测试进程消失）。修复：Linux 侧 send 加 `MSG_NOSIGNAL`、macOS 侧 socket 挂 `SO_NOSIGPIPE`（Windows NamedPipe 通道无此语义），send 失败统一走既有 Error 状态而非进程死亡。
- **notify bridge 订阅泄漏**（`notify_module.cpp`）：`bridge()` 同 target 重复建桥不摘旧订阅，而 EventHub 对同名订阅不去重——脚本每次重跑都新增一份订阅，同一事件被重复转发 N 次且永不释放（第九批 db 句柄泄漏同型：胶水层只增不减）。修复：建桥前按固定订阅名（`notify.bridge.<target>`）摘旧订阅并清 `bridges_` 陈旧项；回归用例 `BridgeSameTargetReplacesOldSubscription`（修复前同一 emit 投递 2 次，修复后恰 1 次）。
- **死代码删除**：`script_manager.cpp` `checkAllReloads_Locked`（与 `checkAllReloads` 内联逻辑完全重复，全库零调用方）、`x11_clipboard.cpp` `escapeShell`（private static，零调用方）、`notify_module.cpp` `clearBridges`（ModuleDescriptor 无生命周期钩子，该清理函数全库无执行路径）。

### test（2026-09-23，C++ 第十批补测：IPC 错误路径与原始帧注入 + X11 wait/close 家族 + script_manager 配置重载）

- **新增 10 用例，行覆盖 90.7% → 91.0%（13090 → 13098 行，miss 1217 → 1185），函数 95.2%（1766/1856）**（v15 基线，全量 2003 用例：1963 PASSED + 40 环境性 skip，skip 口径与 v14 同；基线为重启后自起 Xvfb :98 全量重采，lcov 1.16 按生产 TU/libs/tests 三子树并行 capture 后合并，与单次全量 capture 逐 SF 等价）。目标文件（v15 绝对值）：**x11_window 98.6%**（272/276——wait 家族/close/forceClose/center 批前全部零覆盖）、**unix_socket_channel 91.6%**（229/250）、**script_manager 91.0%**（463/509）、**notify_module 76.1%**（162/213，剩余全为白名单断链，见不可覆盖论证）。
- ① unix_socket_channel 5 用例（含 RawSocketClient 裸帧注入器，绕过 IpcMessage 封装直接构造协议层畸形输入）：`setErrorCallback` 首个真实触发用例（连接失败置 Error 回调携带路径——该回调此前全库零调用方）；空 payload 序列化为 object 分支；裸帧坏 JSON → deserializeMessage 异常分支 → Error 消息进回调；0 长度帧 → receiveLoop 长度校验 break → 通道转 Disconnected；对端断开后 send 优雅失败（SIGPIPE 修复回归）。② x11_window 1 用例（`X11WindowCloseCenterAndWaitFamily`）+ 剪贴板断言补全：center 居中、waitFor 250ms 轮询超时 false、waitClose 存在/不存在两分支、waitForForeground 死句柄 false 与真前台 true、getBackendInfo、close（WM_DELETE_WINDOW ClientMessage 无 WM 下断言请求本身）、forceClose——**XKillClient 语义为终结拥有目标资源的客户端连接**，打在本进程自建窗口上等于切断自己的 X 连接（首版踩坑：后续任何 Xlib 调用触发 fatal IO error 整进程退出），用例改为 fork 子进程扮演外部客户端持窗、管道传句柄、EOF 退出；`ClipboardFullSurfaceMethods` 补 getBackendInfo/空格式表断言。③ script_manager 3 用例（`script_manager_config_reload_test.cpp`）：loadJsonConfig 非 object 根拒绝（数组/字符串/数字/坏 JSON 四态，对照组合法对象可载）、autoReload 关闭时 checkReload 早退（重写文件使 mtime 前移 + 打开全局开关反向证明早退门控真实生效，随后 reload 返回 true 且 lastModified 刷新）、脚本文件删除后 stat 失败返回 0 不误触发 reload（注册表不受影响）。④ notify_module 1 用例（订阅去重回归，见上条 fix）。
- **通道并发语义实测记录**：对端存活时裸调 `stopReceiving()` 永久挂起——接收线程阻塞在无超时 `recv()` 上，`join()` 永等；`disconnect()` 内部先 `shutdown` 再 stop 才是唯一安全序（既有 `StopReceivingWithoutStartIsSafe` 注释同理）。初版两个用例据此挂死，均改为不触碰接收线程的确定性方案。
- **不可覆盖论证（不硬凑）**：clipboard 剩余 26 行全为 NullClipboard 兜底类（仅 XInitThreads 失败时使用）与工厂恒 `make_unique` 不返回空的分支；x11_clipboard 剩余 ~30 行为 fork+execvp 子进程侧代码——子进程被 exec 替换或 `_exit`，gcda 永不写出，属 gcov 结构性盲区；notify 剩余 ~47 行为 webhook 白名单**装配断链**：`setWebhooksEnabled`/`setAllowedHosts` 全库零调用方 → `allowedHosts_` 恒空 → `isUrlAllowed` 恒 false → 所有 webhook 永远被拒，属功能级缺陷（修复需新增配置入口的产品决策，另行跟踪）；script_manager 剩余 running 态分支为同步执行模型遗留（`runScript` 同步等待完成即 completed，无任何入口将脚本置为 running，`callFunction`/`pauseScript`/`resumeScript` 的 running 路径不可达）；unix_socket 剩余 14 行为 `socket()`/`listen()`/`accept()` 资源耗尽级失败（fd 打满）不可注入。

### feat（2026-09-23，Guacamole 远程桌面像素面网关 P0：票据门禁 + 三协议 e2e + Dashboard 组件）

- **架构落位**（`docs/remote-gateway-guacamole-design.md` 方案B）：浏览器 ⇄ Go server（`GET /api/remote/guacamole` WebSocket 承载 Guacamole 指令流）⇄ guacd（纯 TCP，loopback/内网）⇄ agent 桌面（RDP/VNC/SSH）。**runtime 零参与**像素面——不改 C++、不加监听，已同步 `architecture-decisions.md`（Allowed WebSocket Usage 补像素面网关段 / Forbidden Changes 补「浏览器直连 guacd」禁项）。
- **一次性票据门禁**：`POST /api/remote/tickets`（`desktop:view`/`desktop:control` 任一可申请；接管模式 handler 内额外要求 `desktop:control`），新增 `desktop:view`/`desktop:control` 两个内置权限并配入 Viewer/Operator 角色。票据短时效单次有效，经 URL query `?ticket=`（首选）+ `Sec-WebSocket-Protocol[0]`（兼容位）双通道传递；WS 端点不挂 AuthRequired（浏览器 WS 无法自定义 header，票据即凭证，与 `/ws` 先例一致）。配置新增 `WINGMAN_GUACD_ADDR`（默认 `127.0.0.1:4822`）。
- **协议实现要点**（单测回归护栏固化）：guacd 对 select 回的 args 名单做**参数个数硬校验**——connect 必须与名单按位等长（含首段版本名，`VERSION_x_y_z` 原样回显），短一位即静默断连（浏览器侧只见会话无响应）；rdp 显式 `disable-audio`（guacd 镜像 libguac 未链音频编码器，RDPSND 协商即 NULL 崩溃）与 `disable-gfx`（xrdp 类 VNC 后端不支持 rdpgfx 通道，FreeRDP 等 GFX 帧无限挂起；仅 1.6.0 认识该参数，1.5.5 自动忽略）。
- **guacd 版本锁定 1.5.x**：1.6.0 实测双崩溃（gdb 符号栈定位：`guac_audio_assign_encoder` NULL 与 display 重构后 `guac_user_supports_webp` NULL 竞态，后者无参数可规避），e2e compose、`deployments/guacd/README.md`、设计文档 §13.2 三处固化；另记录 cockpit 侧 `api_guacamole.go` connect 以 name=value 发参的协议缺陷待反哺（§13.3）。
- **三协议 e2e**（`integration/guacd_e2e_test.go`，`WINGMAN_GUACD_E2E=1` 门控；podman 起 guacd/sshd/xrdp/VNC 容器）：SSH/VNC/RDP 各一条完整链路（select→握手→connect→指令流像素帧到达），隧道读帧按 gorilla「读失败即毒化」语义单次总 deadline 实现。
- **Dashboard 前端**：`services/remote.ts`（票据申请 + WS 路径拼接）、`RemoteDesktopModal` 组件（fitScale 自适应缩放、监看/接管分态挂载键鼠、关闭清理会话）、Agents 页「远程桌面」入口（协议/凭据/监看-接管表单）；guacamole-common-js 最小类型声明；新增 8 用例（服务层 4 + 组件 4）。

### test（2026-09-23，C++ 第九批补测：trigger 序列化全枚举/timer+system+human 胶水收口）

- **新增 12 用例，行覆盖 89.9% → 90.4%（13090 行，miss 1324 → 1252），函数 94.4% → 94.7%**（v13 基线，全量 1993 用例：1953 PASSED + 40 环境性 skip）。目标文件：**trigger_handler 86.89% → 98.36%**（rpc/handlers）、**timer_module 84.31% → 93.46%**、**system_module 84.48% → 92.24%**、**posix_system 89.03% → 98.39%**（platform 层）、human_module 86.96% → 88.41%（剩余全为签名常量尾行 gcov 伪影）。
- ① trigger RPC list 序列化 3 用例（`trigger_list_serialization_test.cpp`——既有 ListTriggers 只 list 过 ColorFound+Click 单一组合，两个序列化函数的其余枚举 case 全部零覆盖）：11 种 condition 类型 list 全枚举断言 type 字符串、10 种 action 类型全枚举断言、非法枚举值 999 经 static_cast 入库后 list 走 switch 兜底回退（"ColorFound"/"Log"，宽容不崩溃——同时覆盖两函数 switch 后的兜底 return）。② timer 胶水 4 用例：setTimeout 真实触发回调、参数防御、setInterval 周期触发 + cancel 正路径停转、clearTimeout/clearInterval 别名函数与缺参/未知 id 防御（既有测试只覆盖 after/every 老入口）。③ system 胶水 4 用例：getCpuUsage 首调基线 0 + 二调 [0,100] 值域、getDiskInfo 带路径参数返回单对象（既有只测无参数组版）、**fake-xrandr PATH 注入驱动 getDisplayInfo 解析链**——popen("xrandr …") 继承 PATH，临时目录注入 fake 脚本输出 connected+primary+disconnected 三行，真实驱动 platform 层 fgets 循环/换行剥除/connected 匹配/分辨率 stoi/isPrimary 判定/disconnected 跳过（宿主机无 xrandr，该解析链自项目创建以来零覆盖）、getNetworkAdapters 全枚举（ifa_addr 空项 continue 分支）。④ human setConfig 通用入口 1 用例：move_speed/typing_variance 两 key 写读往返与后端映射（既有测试只走 setMoveSpeed/setTypingVariance 专用函数，通用入口分支零覆盖）。
- **顺带修复在案 flaky**（v13 全量实测暴露）：`TriggerPosixCoverageTest.InputActionsDriveMockInput`——pump(2) 固定 sleep 120ms 等待 watchLoop 触发，全量 1993 用例高负载下调度延迟超窗口致 0 轮触发断言全挂；改为条件轮询等待（上限 2s），同文件另外 2 处同型断言点一并加固。
- **不可覆盖论证（不硬凑）**：kv_module 剩余 21 行全为函数签名常量多行表达式 gcov 归属伪影（与 ini 470-488 同现象，函数体均已覆盖）；human/timer/system 剩余 miss 同为尾行伪影 + 少量跨行表达式归属伪影；posix_system 剩余 5 行为 popen/getifaddrs 系统调用失败与 macOS 兜底分支；trigger_handler 剩余 3 行为 region Rect 构造跨行伪影与 list 空循环闭合伪影。

### fix（2026-09-23，db_module 句柄注册表双泄漏 + 裸指针 UAF 防护缺失 + 死代码清理）

- **g_queries/g_tables 注册表只增不删**（`db_module.cpp`）：`db.table()`/`db.table_where()` 每次调用 `storeTable`/`storeQuery` 新增注册项且全库无任何删除路径——脚本循环中反复建表查询，`shared_ptr` 永不释放（第八批补测 8 轮 create-close 循环用例实测暴露）。修复：新增 `table_close`/`query_close` 胶水函数显式释放，close 后旧句柄经注册表校验被拒绝，不会悬空。
- **extractTable 裸 `reinterpret_cast` 无注册表校验**（同文件）：`extractConnection`/`extractQuery` 均验证句柄在注册表中命中才返回裸指针，唯 `extractTable` 把整型句柄直接 cast 成指针——句柄伪造或 close 后复用即 use-after-free（此前被泄漏掩盖：对象永不销毁故 UAF 不触发）。修复：对齐三处统一的注册表校验模式。
- **死代码删除 88 行**（`db_module.cpp`/`db_connection.hpp`）：Stmt 移动构造/赋值（`prepare()` 返回纯右值，C++17 强制省略拷贝保证移动构造不可达）、executeUnlocked/queryUnlocked/scalarUnlocked 三件套（事务回调内直接用加锁版——`m_mutex` 为 recursive_mutex 可重入不死锁，三函数全库零调用方）、closeAllConnections（零调用方）。

### test（2026-09-23，C++ 第八批补测：db/ini/tcp_channel 三模块深度收口）

- **新增 48 用例，行覆盖 88.4% → 89.9%（13090 行，miss 1515 → 1324），函数 94.1% → 94.4%**（v12 基线，全量 1981 用例：1941 PASSED + 40 环境性 skip；总行数 -15 系死代码删除 88 行与新增 close 胶水的净效果）。目标文件：**db_module 81.97% → 94.81%**（867 行）、**ini_module 83.96% → 95.15%**（268 行）、**tcp_channel 77.19% → 95.06%**（263 行）。
- ① db_module 深度补测 23 用例（`db_deep_coverage_test.cpp`）：目录路径陷阱触发 open CANTOPEN（POSIX open 目录返回 EISDIR）、closed 连接全操作拒绝、坏 SQL 全入口、嵌套事务防御（内层回调不执行）、maxRows 截断、表/字段/类型注入防护矩阵、query builder 非法操作符与排序方向、伪造句柄七函数拒绝（connection/table/query 三注册表校验）、table_close/query_close 生命周期与旧句柄失效、8 轮 create-close 循环无泄漏。② ini 分支补测 13 用例（`ini_branch_coverage_test.cpp`）：畸形行/畸形 section 容错跳过、非法转义保留反斜杠、非法 key/section 名警告降级、`\n \r \t \\` 转义矩阵编码解码往返、get/set/delete/has_*/sections/keys 全防御分支与副本语义、merge 类型覆盖分支。③ tcp_channel 真实 socket 错误路径 12 用例（`tcp_channel_e2e_coverage_test.cpp`，POSIX gate）：server 非法地址/端口占用/accept 阻塞中断解除、client 非法主机回退后重试耗尽（实测 ≥5s）、原始帧攻击注入（0 长度帧/超长 11MB 声明/合法长度头后对端消失——SO_LINGER 0 触发 RST）、坏 JSON 后连接存活、缺字段消息全默认值、payload 字符串/对象二态、空 payload 序列化为空对象、对端 RST 后 send 失败转 Error（SIGPIPE 屏蔽）。
- **不可覆盖论证（不硬凑）**：db 剩余 45 行全为 sqlite3 内部失败防御（bind/begin/commit 失败需磁盘满或库损坏级注入）与 getScriptDataDir 环境回退；tcp_channel 剩余 13 行为 socket()/listen() 资源耗尽防御与 send 长度头首发失败（需内核发送缓冲满时序）；ini 剩余 13 行为逻辑不可达防御（刚插入的 section 必然 find 命中）与函数签名常量多行表达式 gcov 伪影（470-488，v11 既有现象）。

### fix（2026-09-23，inbox 下行链两处真实缺陷：timestamp 恒 0 与整型 payload 语义丢失）

- **下行消息 timestamp 恒 0**（`inbox_module.cpp`）：`data.value("timestamp", uint64_t(0))`——nlohmann `value()` 严格匹配数值类型，JSON 整数字面量存为 signed int64，与 `uint64_t` 默认值类型不合时**静默回落默认值**，服务端下发的整数时间戳全部丢失为 0（第七批补测用例实测暴露：断言 `timestamp==1234` 得 0）。修复：`contains()` + `is_number()` + `get<int64_t>()` 中转。
- **整型 payload 经 consume 后 `asInt()` 恒 0**（同文件）：胶水层 payload 转换缺 `is_number_integer` 分支，正整数走 `fromFloat`——而 `ScriptValue::asInt()` 对 `Type::Float` **恒返回默认值 0**（无转换，见 `iscript_engine.hpp` 数值语义），脚本侧拿到的整数语义尽失。修复：补 `is_number_integer` 分支走 `fromInt` 保真。

### test（2026-09-23，C++ 第七批补测：inbox 下行链全驱动 + crypt KDF 失败分支）

- **新增 4 用例，行覆盖 87.9% → 88.4%（13105 行），函数 93.8% → 94.1%**（v11 基线，全量 1933 用例：1893 PASSED + 40 环境性 skip）。目标文件：**inbox_module 72.1% → 95.0%**（35 函数 100%）、**crypt 70.5% → 72.0%**。
- ① inbox 下行链 2 用例（`inbox_downlink_coverage_test.cpp`，此前仅上行生命周期覆盖，server→client 方向为零覆盖）：transport 胶水 `tcpListen` 起 server、`tcpSendTo` 向已注册 session 推 JSON 帧驱动 client IO 线程，端到端触达——`handleMessage` 三类型分发（register_ack/heartbeat_ack/未知 type）+ 坏 JSON 异常分支；`handleInboxMessage` 入队/pending 上限拒绝（maxPending=1，push 后 sleep 300ms 定格——loopback 往返 <5ms，两个数量级冗余窗口，避免与随后的 report 竞态）/空 msgId 防御丢弃；consume 出队与 payload 五类型转换（object 的 kv 值为 dump 字符串/string/integer/bool/array）；ack 标记 pending + report 释放恢复接收；connect 二次调用复用同 handle（clientId 遍历命中 + "Already connected" 短路）。② crypt KDF 失败分支 1 用例：`deriveKey(pw, salt, iter=0)` 使 PBKDF2 的 `EVP_KDF_derive` 返回 ≤0，触达错误清理分支（ctx/kdf 释放 + 空串）——该文件其余 miss 全为 RAND 失败/EVP ctx OOM/合法输入下中途失败等 OpenSSL 内部防御，不可确定性触发，本用例是唯一可稳定触达的错误分支。
- **不可覆盖论证（不硬凑）**：inbox `setMessageCallback` 与回调调用点无胶水入口（脚本域不暴露回调注册）；`sendRegister` 失败回滚（connect 中 disconnect+复位）需 TCP 握手成功后首包 send 失败——RST 时序竞态不可控。
- **环境事故记录**：首测因宿主机重启后 Xvfb `:98` 未恢复，44 个 X11/剪贴板/Recorder 用例被环境性 skip（40→84），覆盖率假低至 82.9%；恢复 Xvfb 后同口径全量重测，以 88.4% 为准。教训：**覆盖率基线采集前必须先验证 X 环境存活**（PASSED 计数与上批基线差 >5 即应怀疑环境）。

### fix（2026-09-23，ctype 函数收负值致 MSVC Debug 断言对话框挂死 Windows CI）

- **`GameProfileModuleGlue.CreateTemplateAndDelete` 在 Windows CI 100% 确定性挂死**：第五批、第六批两次 Windows job 均在该用例 `[ RUN ]` 后无输出静止 58 分钟，直到步骤 60 分钟超时强杀（`Terminate batch job`，第五批曾被同 job 的 Clipboard 断言失败掩盖）。根因：用例传中文 gameName `"胶水游戏"`，`GameProfileManager::createTemplate`（`game_profile.cpp`）的 `std::transform(..., ::tolower)` 与 `std::replace_if(..., ::isspace)` 把 signed char 直接传给 ctype 函数——UTF-8 字节（如 `0xE8`）为负值，MSVC UCRT 对负值（除 EOF）触发 `_CrtDbgReport` **模态断言对话框**，headless runner 无人点击即进程永久阻塞；Linux glibc/macOS 对负值查表偏移安全故全绿。
- **全库排查并修复 9 处同类隐患**（每处改为 `static_cast<unsigned char>` 转换后再调 ctype）：`game_profile.cpp`（tolower/isspace，本次根因）、`smart_trigger.cpp`（OCR_EQUALS 归一化两处 isspace）、`security.cpp`（VM 进程名 tolower）、`script_manager.cpp`（配置扩展名 tolower 两处）、`db_module.cpp`（SQL 操作符 tolower）、`human.cpp`（randomCase isalpha——同函数旁 toupper/tolower 原已转换，此处系漏网）、`verification.cpp`（Base32 toupper）、`posix_system.cpp`（/proc 目录名 isdigit）。
- **测试兜底防线**：core_tests 以 namespace 级静态对象先于 main 执行 `_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE)` + `_CRTDBG_FILE_STDERR`（仅 `_WIN32 && _DEBUG`）——未来任何漏网断言在 headless 环境输出到 stderr 而非弹窗挂死，行为与 Linux 对齐。

### fix（2026-09-22，filewatcher 胶水缺参越界）

- **filewatcher 胶水三函数参数裸下标越界**（`filewatcher_module.cpp`）：`watch`/`unwatch`/`isWatching` 的参数校验直接 `args[0]`/`args[1]` 下标访问而无长度检查——脚本侧少传参调用（如 `filewatcher.watch("/tmp/x")`）即触发 `std::vector` 越界断言 abort 整个 runtime 进程（第六批补测用例实测复现）。修复：三处统一补 `args.size()` 前置检查（不足即返回 false/null 防御值），测试补缺参回归守卫断言。

### test（2026-09-22，C++ 第六批补测：smart_trigger 触发链/misc uia+bt/notify bridge 全驱动）

- **新增 21 用例，行覆盖 86.8% → 87.9%（13092 行），函数 93.4% → 93.8%**（v10 基线，全量 1930 用例：1890 PASSED + 40 skip）。① smart_trigger 真实触发链 12 用例（`smart_trigger_triggerpath_coverage_test.cpp`——此前 executeActions 七种动作、watchLoop 触发段、六种条件 case 体为纯零覆盖，现有用例全是"条件不满足 + start 即 stop"穿行）：恒真条件驱动完整触发——IMAGE_NOT_FOUND（坏模板恒找不到）触发至 maxTriggers 自停、TEXT_NOT_FOUND（OCR 无数据 !success 恒真）即触、COLOR_NOT_FOUND（空屏无目标色）即触；全动作类型单轮走完（CLICK/KEY_PRESS 经 `platform::mock::MockInput` 第二构造注入断言副作用、WAIT/LUA_SCRIPT/CUSTOM_CALLBACK 计数、LOG、STOP 收尾）+ watchLoop fast-exit；STOP 动作自停路径、maxTriggers=1 精确计数、start 二次调用已运行返回 false、默认 input 注入分支（第一构造委托第二构造预注入 `defaultSharedInput()` 使 input_ 恒非空，显式传 nullptr 才走 watchLoop 注入分支，仅 LOG 动作无真实点击副作用）；恒假条件（TEXT_FOUND/OCR_CONTAINS/OCR_EQUALS 不可达文本、EDGE_DETECTED 空 region、COLOR_CHANGED 静态画面自比较、IMAGE_FOUND 坏模板）循环执行 case 体且保持不触发。② misc uia/node/bt 缺口 6 用例（`misc_uia_node_coverage_test.cpp`）：uia 全查找函数 Linux stub 路径（11 函数 null/空数组 + makeUiaElementObject 无元素分支）、事件监听全防御分支（缺参/非 callable/非线程安全 callable 拒绝/线程安全经门面到无后端返回 0、remove_event_listener）、bt action callable 的 RUNNING/FAILURE 字符串映射 tick（此前仅 SUCCESS 路径）、tick 未知树 default、bt.wait 构造、smarttrigger.setCheckInterval 存在路径、node.sendHeartbeat、ocr.recognize text 字段。③ notify bridge 真驱动 2 用例：EventHub 订阅捕获证实 bridge 订阅回调此前从未被 emit 触发——event:// 目标转发（transform 合并 + original 载荷送达）、http:// 目标经 g_webhookSender 直连走白名单拒绝路径（blocked 事件断言）。④ x11 集成 1 用例（platform_x11_test）：node.getWindows 胶水窗口枚举循环体（TestX11Window 模拟 _NET_CLIENT_LIST，无 X 时该循环恒空转零覆盖）。⑤ filewatcher 胶水修正：第五批"ScriptValue 无公开 Callable 构造器"论证有误（`fromCallable` 公开存在），`ScriptValue::fromCallable` 构造回调后 watch 成功行直接可达已覆盖。
- **目标文件**：smart_trigger.cpp 52.3% → **100% 行**（v9 全库最低之一）、misc_modules.cpp 68.8% → 77.0%、notify_module.cpp 71.6% → 75.5%、filewatcher 胶水 → 94.1%。
- **顺带修复在案 flaky**：`ClipboardTest.HasFiles`——x11 后端 selection 所有权转移异步生效，clear 后立即探测可能命中前一用例残留的 FILE_LIST target（第六批全量实测偶发失败），改为轮询等待（100×10ms 上限）替代定值断言。
- **顺带修复第五批 Windows CI 失败**：`ClipboardModuleGlue.HtmlImageAndFilesBehavior`（`module_glue_gaps_coverage_test.cpp`）——用例按 X11 后端在案行为（setImage 恒 false → getImage null → hasImage false）写死断言，但 Windows CF_DIB 后端可真实写入 image（"ab" 4 字节恰为 2×2 像素 BGRA 合法尺寸，返回 true/非空/true），第五批推送后 Windows job 即失败。修正为平台无关自洽断言：`imageSet ↔ hasImage ↔ getImage 非空` 三态一致，写入失败则三者反向一致（X11 实测走反向分支 PASSED）。
- **不可覆盖论证（逐条记录，不硬凑）**：① notify WebhookSender 异步 worker/HTTP 段与 isUrlAllowed URL 解析分支——白名单与开关无任何胶水配置入口（`NotifyManager::webhook` 忽略 options、bridge 直连 sender），默认空白名单在 send 入口同步拒绝、worker 线程永不启动，可达性需产品补胶水层白名单配置函数（潜在缺口已记录）；② WebhookSender 析构 join 与 shutdown 入口分支——进程退出路径，静态析构顺序在 gcov flush 之后；③ misc UIElement 12 方法闭包体与 uiaElementRegistry/uiaStoreElement——需平台 UIA 后端产生真实 IUIAElement 实例，Linux `ui_automation.cpp` 无后端实现（#else 恒 warn+return false）且 registry 无注入点永空；④ filewatcher 胶水 2 行 gcov 行归属伪影（lambda 尾聚合行，函数体已全覆盖）。

### fix（2026-09-22，超时脚本执行线程悬空引用 + Xvfb 刷新率 NaN）

- **`ScriptManager::runScriptInternal` 超时 detach 后悬空引用 UAF**（`script_manager.cpp`）：执行线程按引用捕获局部栈对象（`infoPtr`/`scriptDone`/`scriptSuccess`），超时路径 detach 后主线程返回、栈帧销毁，线程仍在写这些悬空引用——补测超时用例实测段错误（exit 139）。修复：全部改为按值捕获（`shared_ptr<atomic>` + 路径拷贝）；`ScriptInfo::engine` 改 `shared_ptr` 由执行线程与 manager 共享持有——超时时 manager 侧 `engine.reset()`，detached 线程持引用跑完脚本后自动销毁，杜绝超时后 unload/重跑与旧引擎并发复用。
- **Xvfb/虚拟 GPU 下 `getSupportedDisplayModes` 返回 `INT_MIN` 垃圾刷新率**（`x11_screen.cpp`）：模式 `dotClock` 可为 0，`0/0 → NaN → (int)NaN` 为 UB（实测得 `INT_MIN` 流向调用方）。修复：dotClock/hTotal/vTotal 任一非正时刷新率取 0，不再做除法。
- **死代码清理**（`script_manager.cpp/.hpp`）：删除 `checkTimeLimit`/`triggerEvent`/`triggerEventUnlocked`——全库 grep 零调用方（事件分发实际走 `setEventCallback` 存储的回调直调）。

### test（2026-09-22，C++ 第五批补测：script/filewatcher/clipboard/verification/ml/game_profile 胶水缺口）

- **新增 17 用例，行覆盖 85.4% → 86.8%（13092 行）**（v9 终版基线，全量 1909 用例）。① script_module 胶水全链 7 用例（`script_module_glue_coverage_test.cpp`，此前 15.5% 全库最低——12 个 `wingman.script.*` 函数零覆盖）：真实 ScriptManager 经 `setScriptManager` RAII 注入（wingman::lua 引擎），load（含 config 对象三字段 autoReload/sandboxed/timeoutMs 下发与 manager 内部状态对照）/list（infoToValue 全字段 + 语法错误后 lastError 键出现）/getState/isRunning/has/run/stop/reload 生命周期（实测状态机：load→unloaded、run→completed、reload→loaded、completed 态 stop→false）、setEnv/getEnv/setConfig/getConfig 往返、setHotReload 双向；null 注入 14 函数默认值 + 参数不足 12 防御分支全触达。② filewatcher stub 胶水 1 用例：5 函数参数校验与返回契约全覆盖。③ clipboard_module 胶水 2 用例（ClipboardLockGuard 串行化 + 探测 skip）：文本往返/clear/isEmpty、HTML 行为自洽、setImage 类型防御三分支与 X11 后端无 image 能力的恒 false/null 在案行为、setFiles 混合参数（字符串/数组内字符串/非字符串忽略）、getFiles/hasFiles。④ verification TOTP 3 用例：totp 的 Base32 校验链（空参/非 string/过短/含 0·1·小写非法字符→空串）、合法 32 字符 secret 产 6/8 位纯数字、verify 自产自销（totp 生成即 verify 通过 window=1）与五类非法分支、remaining 周期边界。⑤ ml stub 错误分支 1 用例：loadModel stub 恒 nil、unload/isLoaded 未知 id 与空参、inputs/outputs 空数组、run 的 usage/缺 name/model not found 三类 fail 契约（success=false + outputs 空 + timeMs 0）。⑥ game_profile 胶水 3 用例（GameProfileManager 单例 + 临时目录真实落盘）：目录设置/扫描、createTemplate 落盘后 scan 仍可 get、空名 null、delete 幂等三分支、exportJson/importJson 往返（override id/非法 JSON/不存在 id 空串）、exportPackage 不存在 false。
- 结构性不可覆盖分支（不硬凑，论证记录）：filewatcher.watch 的成功返回行（ScriptValue 无公开 Callable 构造器——Callable 实例仅由脚本引擎构造，`args[1].isCallable()` 恒 false，校验 if 恒走 false 分支）；ml 真实模型路径（load 成功/ioInfoToArray/run 成功输出，依赖 WINGMAN_ENABLE_ML 与真实 onnx 模型，CI 为 ml_stub）。
- 补测过程中修掉新用例自身两处缺陷：`ScriptValue::asString()` 按值返回导致 `all_of(asString().begin(), asString().end(), …)` 两个临时 string 迭代器不配对的 UB（打印正常断言失败的诡异症状）；测试环境 TempDir() 指向 systemd-private 服务目录（含不可读 profile.json），不可传给 `setProfilesDirectory`，改为记录原目录恢复。

### test（2026-09-22，前端覆盖率收口：GUI vitest 行/语句/函数 100%，Dashboard 99.71%）

- **GUI vitest（67 文件 511 用例全绿）：语句 100%（3830/3830）、函数 100%（719/719）、行 100%（2403/2403），分支 99.37%（1420/1429）**。本批新增 7 用例：ScreenPickerModal footer 两用例（取消按钮回调 `onclose` 且不触发 `onconfirm`；「重新截图」按钮 refresh 全链——重置选择态→重新捕获→就绪恢复）；scripts 页 previewLoading 占位（pending Promise 锁定「正在读取文件…」渲染分支）；scripts 页脚本名空回退用例编写中发现 `scripts` store 归一化（`name = String(input?.name || input?.path || '')`）已在 store 层保证 name 非空且该归一化已有专测——页面层 `script.name || fileName(script.path)` 回退分支不可达，用例删除改为论证记录；screen 页 monitor.name 空回退（`显示器 N` 兜底 + 主标记）；settings 页读取远程配置的 runtime 错误分支（invoke 抛错 → `读取失败: Error: …` 面板——字符串拼接 Error 对象产生双重前缀，与字符串错误路径不同）。
- **Dashboard jest（20 套件 236 用例全绿）：行覆盖 99.71%**。新增 fetchJSON 存储异常降级用例（`localStorage.getItem` 抛异常时请求照常发出且不带 `Authorization` 头）。
- 剩余 9 个未覆盖分支逐条论证（不硬凑）：triggers 页 5 个为 Svelte 编译器为 `{#each}`/`{:else if}` 链生成的空迭代与不可达组合分支（语义上无法与已覆盖路径区分）；scripts 页 640 行为 `previewLoading` 与 `previewError` 的编译器全组合分支——状态机前置清空（进入 loading 前置 `previewError = ''`）保证二者不同时为真；scripts 页 709 行见上（store 归一化保证 name 非空）；settings 页 401 行为 Svelte 事件处理编译产物防御分支（DOM `Event.target` 规范恒非空）；screen 页 461 行为监视器列表为空的 each 分支（真实设备恒有 ≥1 显示器）。Dashboard 唯一未覆盖行 `services/core/http.ts:21` 为 ts-jest sourcemap 映射伪影——该行 `localStorage.getItem('token')` 实际执行已由请求头断言证明。

### test（2026-09-22，C++ 第三批补测：x11 平台/进程/HTTP/录制器/脚本执行）

- **新增 33 用例，行覆盖 81.2% → 85.4%（13092 行）**（v8 终版基线，全量 1892 用例）。① x11 平台补测 7 用例（`platform_x11_test.cpp`）：screen 显示器信息全扫描（越界索引回退/DPI/坐标映射往返/displayModes/虚拟屏 bounds/monitorFromPoint·Window）、input 全扫描（鼠标五键 down/up/pressed、69 键 keysym 映射与组合键、textInput、配置与后端元数据）；② x11 截图补测 4 用例：真窗口内容指纹截图（绘制白块后逐像素验证）、无效/已销毁句柄安全 nullptr（宽容 error handler 吞 BadWindow）、窗口区域截图与越界失败分支、capture 侧显示器元数据与空区域回退全链；③ clipboard 全接口面 1 用例：图像 stub 通道、文件列表换行拼接走文本通道回读拆行、格式枚举、clear 清空（xclip 空输入 = 清空 selection，实测验证）；④ posix_process 全链路 6 用例（`posix_process_coverage_test.cpp`）：真实 fork/exec 生命周期（comm 轮询等待 exec 完成）、SIGTERM/SIGKILL 终止与超时分支、非法 pid 防御（waitpid ECHILD/kill ESRCH）、exec 失败立即退出、`/proc` 全量 enumerate 与 findAll/find、waitFor/waitExit 名字轮询；⑤ HTTP 本地真 server 3 用例（此前全部只打连接拒绝，成功路径零覆盖）：手写 POSIX 最小 HTTP server（随机端口、shutdown 唤醒 accept）——GET 往返含响应头解析（HeaderCallback 的 CRLF 剥离/冒号切分）、POST/PUT/DELETE 方法分支与 body 真实送达、postForm 表单编码与默认请求头下发；⑥ MacroRecorder 离线状态机 6 用例（`recorder_offline_test.cpp`，recordEvent 手动注入不依赖 XRecord）：连续 MouseMove 去重、saveToLua 六事件类型全分支、saveToJSON/loadFromJSON 往返保真与错误三分支（文件不存在/非法 JSON/缺 events 数组）、pause/resume、playback 经 XTest 真实注入并回读终点坐标、Xvfb 下 start 失败优雅回退（真桌面自动转 skip 由 e2e 用例覆盖）；⑦ script_manager 真实执行路径 6 用例（`script_manager_exec_coverage_test.cpp`，链接 wingman::lua 显式注册引擎）：加载/执行/输出路由/env 注入/语法错误捕获/超时标记（有限循环防永久忙等——本用例即 UAF 缺陷复现器）/事件回调/reload 重跑。
- 结构性不可覆盖分支（不硬凑，论证记录）：x11_clipboard 的 fork 子进程行（execvp 成功替换映像 / `_exit` 退出均不触发 gcov flush）与 pipe/fork 失败分支（测试进程无法注入系统调用失败）；http 的 HEAD 分支（`perform` 私有、公共接口无 HEAD 入口）；posix_process 的 fork 失败分支（同前）；X11Capture/X11Screen 的 `!initialized_` 分支（类私有于 .cpp，工厂 new 后立即 initialize）；`ClipboardTest.Clear` 在极高系统负载（load>25）下偶发失败——xclip selection 接管异步窗口被拉长，低负载复测 5/5 通过，列为环境型在案 flaky。

### fix（2026-09-22，传输/胶水层三处补测中复现的真实缺陷）

- **`TcpServer::start()` 在 `listen()` 前调用时 server 永不接受连接**（`transport_server.hpp`）：`start()` 启动 IO 线程跑 `ioContext_.run()`，此刻无任何异步工作，`run()` 立即返回、IO 线程空转退出；而 `async_accept` 是之后的 `listen()` 里才注册的——已无人执行。胶水层 `tcpListen` 的固定顺序恰为 start→listen，故 server 从不 accept 会话（sessions 恒空）。修复：`executor_work_guard` 保活（start 重建 / stop 释放），start/listen 顺序无关；此版 asio 的 guard 不可赋值，以 `std::optional` 持有。
- **`inbox.connect()` 自死锁**（`inbox_module.cpp`）：`connect()` 持 `clientMutex_` 调用 `sendRegister()`→`sendNotify()`，后者再次对同一把不可重入锁加锁——脚本线程 connect 成功后永久卡死（strace 实证：connect 返回后主线程 futex 无限等待）。修复：`sendNotify` 去锁（`connected_` 原子守卫 + `TcpClient::send` 的 session/connected 检查兜底），并把 `connected_` 置位挪到注册发送前（注册是 fire-and-forget，发送失败即回滚为连接失败）。
- **`team.joinTeam` 每次调用新建客户端**（`team_module.cpp`）：胶水层每次 `createClient` 新建实例（句柄随调用次数泄漏），其乐观加入状态对脚本查询不可见——`leaveTeam`/`isJoined`/`getTeamStatus` 固定读 handle 1，首次 joinTeam 之后 `isJoined()` 恒 false。修复：复用 handle 1（与其它函数语义一致）。

### fix（2026-09-22，TriggerActionData 未初始化成员）

- `TriggerActionData` 的 `int x, y, delay` 补默认初始化器（`trigger.hpp`）。此前为裸 POD 成员：构造方若未显式赋值（如触发器动作缺省 `delay` 字段），读到的是栈垃圾；`posix_trigger.cpp` 的 `executeActions` 会对 `delay > 0` 的动作 `sleep(delay ms)`——垃圾值恰好为正时 watchLoop 一次可睡数天（覆盖率补测中实测复现 24 天，进程假死）。Windows/mac 平台实现共用同一结构体，一并受益。

### test（2026-09-22，C++ 第二批补测：db/transport/inbox/team 胶水端到端）

- **新增 28 用例，行覆盖 74.7% → 81.2%（13106 行）中间态**。① `db_module_glue_coverage_test.cpp`（12 用例）：open/execute/query/scalar/transaction（含回调异常回滚）/table ORM 全生命周期（insert/get/where/limit/order_by/update/delete）与全部句柄防护分支，连接一律 `:memory:` 不落盘；② `transport_inbox_coverage_test.cpp`（8 用例，仅非 Windows 编译）：127.0.0.1 真实 TCP/UDP 端到端——自连自收 + session 管理、拒绝/端口冲突/非法句柄分支、UDP bind→sendTo→recvFrom 往返（先发后收规避 `receive_from` 无超时阻塞）、inbox 对真实 listener 的 connect/heartbeat/consume/ack/report 生命周期与错误分支（sendRegister 为 fire-and-forget，对任意可连 listener 即成功）；③ `team_module_glue_coverage_test.cpp`（8 用例）：join 生命周期（重复 join 拒绝/重复 leave 拒绝）、投票/广播/状态上报的 JSON 解析三分支（合法/回退/object）与 joined 守卫、事件订阅含回调异常吞噬。补测中复现并修复上节三处真实缺陷。④ 修复在案低频 flaky `X11WindowPlatformFeatures`：`findByClassName` 在窗口刚映射后即时枚举，X server 侧 WM_CLASS 属性同步偶发滞后，改为轮询等待（至多 ~2s），全量套件首次零失败。
- 已知胶水不可达分支（不硬凑）：`getVoteResult` 命中 `votes_` 的分支仅由 `TeamClient::handleServerMessage` 填充，胶水层无入口；`UdpSocket::recvFrom` 的 timeoutMs 参数未实现（asio `receive_from` 同步阻塞），超时分支不存在。

### test（2026-09-22，Go 覆盖率 100% 收口 + C++ 基线与第一批补测）

- **Go 侧 statements 100.0%**（全 internal 包 + 根包，`go test -coverprofile`）。第五轮补测收尾：batch stop/trigger 校验失败与离线/传输错误分支、`commandErrorText` 五分支优先级（err > error 字段 > message 字段 > 兜底）、`runBatch` 空目标短路、脚本 ReadInline 缺文件 500 与超限 400、`RegisterRoutes` 以真实依赖完整装配并断言 23 条域路由（补 routes.go 装配路径 0% 缺口）、tagstore nil-db 分支。两处不可达防御分支不做无效测试、以行为等价重构收口：`listener.go` tokenValid 的空白名单分支（唯一调用点已被 `authEnabled()` 守卫）、`tagstore.go` SaveTags 的 marshal 死分支（`[]string` 的 MarshalJSON 恒成功）。
- **C++ 行覆盖率基线确立：71.5%（13101 行）/ 分支 79.4%**。口径修正先行：仅 `CODE_COVERAGE` 选项只给 core_tests 插桩（库对象无 gcda），数字虚高无意义；正确口径为全局 `CMAKE_CXX_FLAGS="--coverage -O0"` + lcov extract `lib/wingman/*`（跨目录通配）+ remove `*/tests/*`，固化为 `scripts/cxx-coverage-baseline.sh`。缺口大头：脚本模块 sol2 胶水层（db 881 / misc 430 / inbox 315 / transport 308 行）与 Linux X11 平台实现，补测清单见 [development-todo](docs/development-todo.md)。
- **C++ 第一批补测：71.5%/79.4% → 74.7% 行（13106 行）/82.3% 分支**。① 解锁 `smart_trigger_test.cpp`——774 行/49 用例被历史遗留的 `if(WIN32)` 误 gate（用例全走跨平台抽象，无 Windows API），Linux 从未编译，此为 smart_trigger.cpp 覆盖率 20% 的直接原因，解 gate 后升至 52.3% 行/95% 分支；② 新增 `misc_modules_coverage_test.cpp`（smarttrigger 全条件/动作类型映射、bt 行为树胶水含 parallel 四 policy 与无效句柄防御、node 心跳形状、ocr 字段形状，10 用例）；③ 新增 `trigger_posix_coverage_test.cpp`（TriggerManager 注入 MockInput，checkTrigger 十条件 × executeActions 九动作逐分支触达，14 用例；弹窗/音频等阻塞分支以 `access()` 环境守卫，规避多线程 + fork 的 `std::system` 死锁）；④ 新增 `small_modules_coverage_test.cpp`（crypto 纯计算往返含 AES salt 16 字节约束、clipboard X11 往返、macro 录制状态机与 save/load 校验分支，12 用例）。补测过程中实测复现 `TriggerActionData` 未初始化缺陷（见上条 fix）并一并收口。全量套件唯一失败 `X11PlatformTest.X11WindowPlatformFeatures` 为在案既有低频 flaky（单跑必过），与补测无关。

### test（2026-09-22，真机验证自动化）

- **XRecord 正向录制端到端用例**：新增 `RecorderX11E2E.*`（仅 Linux 编译）——XTest 注入按键（优先 F13、键码缺失回退 'a'）→ 轮询捕获计数 → `saveToJSON` 内容精确断言（`"type": 5` 即 KeyDown、`"keyCode": <注入键码>` 带字段名匹配防 timestamp 误命中）。Xvfb 下 EnableContext 必然失败（XRecordBadContext），用例自动 GTEST_SKIP；无 DISPLAY 与 Xvfb 双场景实测 skip 正确、不误报失败，防无头环境放绿。
- **真机验证一键脚本**：`scripts/verify-xrecord-desktop.sh`（Linux + DISPLAY 守卫，用例全 skip 判「未验证」exit 2 而非通过）与 `scripts/verify-macos-runtime.sh`（darwin + VCPKG_ROOT 守卫、缺失即报错不回退系统库，triplet 按 arch 自动选，跑 Clipboard/FileWatcher/Screen/Input/UnixSocketChannel 五套件并提示 CGEvent 授权等人工观察项）。macOS / XRecord 两项真机验证遗留自此降为「真机各跑一条命令」。

### 清理（2026-09-22）

- **M4/M5 收尾校准**：三套 UI 测试基线全绿（Dashboard jest 235、GUI vitest 506、Rust cargo 14）；M4 三层契约审计无缺口（server 7 类下发命令 runtime 全支持、3 类上行事件全转发、9 类广播 Dashboard 全消费，log.line 有意不转发）；ROADMAP M4/M5 状态 🚧 → ✅，清理过时的 PermissionRequired「未接线」注记。
- **文档站构建验证与死链修复**：`docs:build` 成功；`ignoreDeadLinks: true` 不拦死链，全量扫描 206 条站内链接发现并修复 4 条真死链（getting-started 架构决策链接层级错、guides 两篇引用不存在的 `api/storage.md`/`api/serialization.md`）。
- **handlers 评估收口（⑤ 关闭）**：经实测评估不拆 Go 子包——46 文件一域一文件、测试全为黑盒 HTTP 测试（经路由层，零直接符号引用），拆包纯成本无收益；跨文件共享的辅助函数（`parsePositiveInt`、`actorName`、`isUniqueConstraint`）归位新建 `helpers.go`。
- **文档去重与会话产物清理**：删除 15 个文件——根目录 `macOS_SESSION_SUMMARY`/`macOS_VERIFICATION_REPORT` 会话产物、`docs/superpowers/` 会话计划、`pending-changes`/`project-improvements`×2/`architecture-improvement-plan` 时令文档、重复的根级 `getting-started.md`（保留 `docs/guide/getting-started.md`）、`docs/installation.md` 与 `docs/setup.md`（收口至 [BUILD.md](BUILD.md)，独有故障排除已并入）、大全式 `user-guide.md`（内容由站点专篇覆盖）；`guides/` 下 configuration/database/triggers 三篇教程挂上文档站「进阶指南」导航；修复 docs/README 索引中 3 个死链（storage/serialization/debugging → kv/db、serialize/json/ini、debugger）。
- 移除从未接入链路的死代码层：`protobuf/` 协议定义、`libs/proto`（构建脚本指向不存在的路径，protoc 从未生成代码）、`libs/debug` EmmyLua C++ 适配器（无任何调用方）、clasp 命令行库（submodule + vcpkg overlay port 双落位、零引用）。Agent 传输协议以 **16 字节头 + JSON 体** 为准（见 [protocols.md](docs/protocols.md)）。
- 同步清理：CMake 选项 `WINGMAN_BUILD_PROTO`/`WINGMAN_BUILD_DEBUG`/`WINGMAN_ENABLE_EMMY`/`WINGMAN_BUILD_DEBUGGER` 及相关测试开关、vcpkg.json 与安装脚本中的 protobuf、CI compat 构建目标、平台边界 allowlist 中的 `libs/debug` 条目。

### refactor（2026-09-22，平行实现合并）

- **删除 lib/wingman 的 system RPC stub 版**：`wingman/rpc/system_handler`（`system.getStatus` 返回硬编码假数据）在 LocalIpcServer 中注册后即被 runtime 版静默覆盖，属假数据陷阱；`system.getVersion` 并入 runtime 版统一提供，协议方法不减。
- **移除 XOR 混淆遗留**（**breaking**）：`SecurityManager::encryptString/decryptString` 与 Lua `security.encryptString/decryptString` 删除（XOR 非真实加密，此前已标 deprecated 并运行时告警）；加密请改用 `crypto.encryptAES`/`crypto.decryptAES`（AES-256-GCM）。
- **手写 SHA-256 收口**：`security.cpp` 内 60 行手写实现删除，`SecurityManager::hashString` 改调 `wingman::crypt::sha256`（OpenSSL EVP），输出格式不变（64 字符 hex）。

### refactor（2026-09-22，Go 包收敛）

- **`pkg/agent` 并入 `internal/agent`**：同名 `agent` 包不再分居 pkg/internal 两处；FrameListener / TeamManager / Registry / 线协议类型单包收口，消费方 import 与别名全量清理。
- **路由装配收口**：main.go 的中间件与全部路由注册（约 230 行）抽至 `internal/handlers/routes.go` 的 `RegisterRoutes`，main.go 415 → 178 行，行为等价。

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

- 平台支持与 API 适用性文档 `docs/platforms.md`：平台信息唯一对账表（README 承诺 / CI 验证范围 / 代码条件编译三线对齐）——支持矩阵（桌面三平台已支持、Android 实验性、iOS 规划中）、36 个 Lua 模块逐平台 API 适用性标注（Android 端侧仅 A2 三子表，每条不可用写明原因）、实际踩坑环境差异、CI 覆盖缺口如实标注（Linux/macOS C++ 核心无 CI 测试、macOS continue-on-error、Android 仅打包）、Linux 本机验证命令与收敛路线（[0e016f2](https://github.com/cuihairu/wingman/commit/0e016f2)）
- 补全所有 handler 端点 Swagger 注解（[5c6db74](https://github.com/cuihairu/wingman/commit/5c6db74)）
- API 文档对齐实际 33 个模块（[59e8831](https://github.com/cuihairu/wingman/commit/59e8831)）及 screen/input/vision/ocr/fsm/event/kv/perf/util/config/http 全量对齐（[a32e7cc](https://github.com/cuihairu/wingman/commit/a32e7cc)、[3e112d3](https://github.com/cuihairu/wingman/commit/3e112d3)、[47fb06b](https://github.com/cuihairu/wingman/commit/47fb06b)）
- Dashboard 与 Runtime GUI 使用教程（[8074bfa](https://github.com/cuihairu/wingman/commit/8074bfa)）；架构文档记录 display selection 设计（[201375a](https://github.com/cuihairu/wingman/commit/201375a)）
- 行为树已实现节点与组装 API 文档（[145b556](https://github.com/cuihairu/wingman/commit/145b556)）；ONNX API 对齐实现（[ccc17d0](https://github.com/cuihairu/wingman/commit/ccc17d0)）

### ci / chore 工程与维护

- **新增 `C++ Linux (full tests)` job：Linux 核心测试首次获得 CI 覆盖**（[402d2ec](https://github.com/cuihairu/wingman/commit/402d2ec)）。此前 Linux 在 CI 上仅 compat 编译 `wingman_transport` 单 target，全量核心测试只有 Windows job 与开发者本地执行（[platforms.md](docs/platforms.md) CI 对账节标注的最大验证缺口）。要点：ubuntu-24.04（GCC 13，C++23 全量编译；compat 矩阵的 ubuntu-22.04/GCC 11 全库编译仍未验证，文档如实标注）完整构建 + 全量 core_tests；`xvfb-run`（显式 1280x800x24，与 WmEnvironment 自起参数一致）提供虚拟显示使 XTest 注入/窗口枚举/剪贴板用例真实执行而非 skip；openbox/xclip 随 apt 安装——WM 集成用例（WmEnvironment 自起 Xvfb+openbox）与 xclip fork 链路用例由环境性 skip 变为真跑；vcpkg 缓存独立 key 并以 restore-keys 兜底复用 compat 缓存。
- **CI job 采串行 ctest（每用例独立进程），并行模式实测 4 处用例不安全**：① storage_test 目录名用裸 `std::rand()`（未播种各进程序列相同）必然碰撞，TearDown `remove_all` 撞上并行进程写入报 "Directory not empty"；② trigger_engine_test 完全固定路径 `wingman_trigger_test` 被并行进程抢先删除；③ team 用例对 TeamManager 单例前置状态的依赖——ctest 独立进程下胶水 `getClient(1)` 判空提前返回 null，触达不了 "Vote not found" 分支（单进程全量靠前序用例铺垫 client 才通过，该用例漏了文件头"每用例自带 ensureJoined/ensureLeft"的约定，已修复）；④ X11 同标题窗口跨进程被并行 forceClose 串扰 + WM 焦点兑现超时。前两条与第 4 条属测试基建并行安全债，作为独立后续工作；job 内既有 flock 守卫（X11 根属性/焦点锁、剪贴板 selection 锁、WM 搭建锁）仍为 ctest 进程间并发的正确性兜底。
- **GCC 13 传递包含差异首轮即抓到真实编译缺陷**：`posix_system.cpp` 用 `std::all_of` 未显式 `#include <algorithm>`，GCC 15 的 libstdc++ 靠传递包含侥幸编译通过，GCC 13（ubuntu-24.04 runner）直接报错——新增 Linux job 的第一轮红就补上了这条可移植性证据，与 platforms.md "工具链差异须 CI 对账" 的论点互证。同步静态扫描全库（237 文件）补齐同嫌疑 4 处：kvstore（`remove_if`）、db_module（`transform`）、team_module（`find`）、resource_loader（`any_of`）。
- **FdExhaustionGuard 按个数压限额在 fd 稀疏布局下失效（CI 第二轮红抓到的真 bug）**：Linux `RLIMIT_NOFILE` 按"新分配 fd 的编号 ≥ soft"判 EMFILE，不是按"已打开个数"——ctest 独立进程 / CI runner 环境下 fd 编号稀疏（存在高于"打开个数"的已占编号），把 soft 压到"打开个数"拦不住低位的空闲编号，`socket()` 照样成功（`StreamChannelTest.ListenFailsWhenDescriptorsExhausted` 在 CI listen 返回 true，本机 fd 紧凑布局纯属侥幸）。修复：soft 压到"最低空闲编号"，任何新分配的首选编号必 ≥ soft，与布局无关严格生效；本地以继承高编号 fd（900）模拟 CI 稀疏布局验证通过。
- **测试并行安全债清偿（`ctest --parallel 4` 本地两轮 2202/2202 全绿）**：① storage_test 目录名由裸 `std::rand()`（未播种各进程序列相同必碰撞）改为 `std::random_device`（进程唯一，跨平台无平台头）；② trigger_engine_test 固定路径 `wingman_trigger_test` 改为进程唯一目录（`random_device` 后缀 + 进程内 static 恒定），TearDown 顺带 `error_code` 容错；③ X11WindowCloseCenterAndWaitFamily 窗口标题进程唯一化（X server 枚举全局可见，`waitFor`/`waitClose` 标题匹配在并行进程间互相串扰）——加上此前已修的 team 用例前置状态，四处并行不安全全部收口。CI Linux job 暂保持串行（已验证稳定、与 Windows job 精神一致），并行提速留作后续观察项。
- **并行安全模式全量扫描补漏 3 处（`-j4` 全量两轮 2202/2202 复验全绿）**：① StorageFactoryTest.CreateLocal 残留的裸 `std::rand()` 目录名改 `std::random_device`（与同文件 SetUp 纪律对齐）；② game_profile_test 与 game_profile_io_test 两个用例共用固定目录 `wingman_scan_test` 且尾部都 `remove_all`（并行互删对方正用目录的真实碰撞对），前者改名独占；③ GameProfileModuleGlue fixture 3 用例共用固定目录、SetUp 互相 `remove_all`，改 `random_device` 进程唯一。其余固定路径临时目录逐一核对：或已带用例名/时间戳后缀、或单用例独占、或只写不删文件名互异，判定安全不动。
- **GCC 11 全库编译本地实测（可行性调研闭环，未改产品代码）**：`g++-11` 完整构建（vcpkg 依赖按新编译器指纹全重编 + 211 个构建目标）验证静态扫描结论——语言特性面全部兼容（`<format>`/`ranges::to`/C++23 views 等高风险特性零使用）；唯一硬缺口为 `ScriptValue::objectVal` 类内递归 `std::unordered_map<std::string, ScriptValue>`（标准仅豁免 vector/list/forward_list 的不完整类型，GCC 15 libstdc++ 容忍、GCC 11 在 pair 实例化时拒绝），55 处错误同根因、连锁失败脚本子系统 53 个目标，其余目标全绿。修复需 278 处调用面的破坏性重构（`fromObject` 243 + `objectVal` 35），判定不值得——GCC 11 支持维持在 transport 层（compat job）承诺。platforms.md 对账结论同步写实。
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
