# 变更日志 (Changelog)

本项目所有显著变更记录于此文件。

- 格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)；
- 提交信息遵循 [Conventional Commits](https://www.conventionalcommits.org/zh-hans/)（feat / fix / test / docs / chore / refactor / perf / ci）；
- 条目按时间倒序排列，commit 哈希链接至 GitHub；
- `M1`–`M8` 前缀标注对应的 [ROADMAP](ROADMAP.md) 里程碑：M1 MVP / M2 触发器 / M3 宏系统 / M4 远程编排 / M5 GUI / M6 人性化模拟 / M7 调试器 / M8 发布准备。

## [Unreleased]

自 v0.1.1 以来共 354 个提交（feat 64 / fix 129 / docs 59 / test 37 / ci 17 / refactor 8 / chore 16）。

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
