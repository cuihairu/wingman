# Wingman 开发待办事项

> Windows 平台游戏自动化工具开发计划

## Chimpeon 核心功能对比

| 功能模块 | Chimpeon | Wingman (当前) | 状态 |
|---------|----------|---------------|------|
| 像素检测 | ✅ 超快速检测 | ✅ 已实现 | ✅ 完成 |
| 颜色匹配 | ✅ 多点检测 | ✅ 已实现 | ✅ 完成 |
| 图像识别 | ❌ | ✅ OpenCV 支持 | ✅ 完成 |
| 触发器系统 | ✅ 多种触发条件 | ✅ 已实现 | ✅ 完成 |
| 宏录制 | ✅ 录制-回放 | ✅ 已实现 | ✅ 完成 |
| 按键模拟 | ✅ | ✅ 已实现 | ✅ 完成 |
| 鼠标模拟 | ✅ | ✅ 已实现 | ✅ 完成 |
| 远程控制 | ✅ 流式支持 | ✅ TCP Server | ✅ 完成 |
| Lua 脚本 | ❌ | ✅ 核心特性 | ✅ 完成 |
| VS Code 调试 | ❌ | ✅ 已规划 | 🚧 进行中 |
| 防检测 | ✅ | ✅ 人性化模拟 | ✅ 完成 |
| UI Automation | ❌ | ✅ 已实现 | ✅ 完成 |
| WebSocket | ❌ | ✅ 已实现 | ✅ 完成 |

---

## 当前缺口清单

> **🔄 核对更新（2026-09-18）**：对照 `lib/wingman/src/script/modules/`（45 个模块源文件）、
> `libs/python/typing/wingman/`（37 个 .pyi）与 200+ 条模块测试逐项核对。P0/P1 的
> event/fsm/task/notify 均已落地并有完整测试（各 50/44/47/46 条用例）与 typing 文件，
> 此前清单未同步勾选。orchestration 已有基础工作流 API。timer 已于 2026-09-19 落地
> （timer_module.cpp + timer.pyi + 12 条测试）；事件按名清理与监听器查询已于
> 2026-09-27 落地；task pause/resume 已于 2026-10-01 落地；hotkey 模块、文件 IO
> 工具、notify tray 已于 2026-10-04 落地（见 todo.md「按新架构决策重排的任务队列」
> P1 第 4-6 条）

### 脚本与运行时
- [x] 统一 Lua / Python API 形状与文档（2026-10-12 落地：`scripts/audit_pyi_drift.py` 以 C++ 描述符为唯一事实源比对 pyi 桩（模块面/函数面 camelToSnake 同形归一/`__init__` 导出面），漂移即非零退出；清偿全部存量漂移——补 macro.pyi/script.pyi 新文件、删除 qrcode.pyi 孤儿（模块 535fbfe 已移除）与其 types.pyi 的 QRLoginResult、bt/db/gameprofile/human/team/verification/security 七桩对齐真实注册面、__init__ 补导出 macro/script/systemwatch/timer；fsm.pyi 保留字键改函数式 TypedDict、ini.pyi 修 docstring 嵌套三引号语法错——两文件此前不能过 Python ast 解析；`scripts/gen_lua_hints.py` 同源重生成 vscode-wingman-lua/wingman.d.lua（42 模块 412 函数，luac -p 过），CI 增 api-typing-audit job（audit 门 + Lua 提示重生成 diff 门））
- [x] Python typing 基础包已提供
- [x] 补齐事件/状态机/任务/通知模块的 typing 文件（event/fsm/task/notify/orchestration 等 37 个 .pyi 齐全）
- [x] 统一模块命名和函数命名风格（2026-10-12 落地：口径写进 docs/guide/script-development.md「函数命名约定」——描述符 camelCase 原名两种语言通用，Python 引擎同源 camelToSnake 双暴露，pyi 桩 snake 为主形 docstring 标注 camel 别名；存量 snake 直登记模块（db/human 等）与 pythonic 手写别名（timer.after）为兼容保留不再新增；audit 门把漂移锁死）
- [x] 所有 Python 公开 API 统一到 `wingman.*`（typing 命名空间 + module_registry 统一注册）

### 事件与状态
- [x] `wingman.event`（event_module.cpp + event.pyi + 50 条模块测试）
  - [x] `on(name, handler)` 注册持久监听
  - [x] `once(name, handler)` 注册一次性监听
  - [x] `off(id | name)` 按订阅 ID 或名称取消
  - [x] `emit(name, payload?, meta?)` 触发事件
  - [x] `listener(id | name)` / `listeners(type)` 查询监听器（返回 `{id,type,name,once}`；同名取最早注册者，listeners 按订阅 ID 升序，未注册事件返回空数组）
  - [x] `clear(type?)` 清理全部或指定事件（无参/nil 保持全量清理语义；传事件名只清理该事件）
  - [x] 事件对象统一字段：`name/type/source/correlationId/priority/timestamp/payload`（EventMessage TypedDict）
  - [x] 预留桥接：脚本事件 -> 任务事件 -> 通知事件（notify 模块 bridge/transform 机制）
- [x] `wingman.fsm`（fsm_module.cpp + fsm.pyi + 44 条测试）
  - [x] `create(name, initialState, options?)`
  - [x] `addState(name, spec)`（`state(machine_id, name, on_enter, on_exit)`）
  - [x] `transition(from, to, guard?, action?)`
  - [x] `onEnter(state, handler)` / `onExit(state, handler)`（并入 state() 回调参数）
  - [x] `onEvent(eventName, handler)` 驱动状态转移（transition 的 `on:` 参数）
  - [x] `dispatch(eventName, payload?)`
  - [x] `getState()` / `setState()`（`current()` 读取 + `reset()`；状态由转移驱动，无直接 setState）
  - [x] 状态变更自动发出 `fsm.changed`
- [x] `wingman.task`（task_module.cpp + task.pyi + 47 条测试）
  - [x] `submit(fn | workflow, options?)`
  - [x] `cancel(taskId)`
  - [x] `status(taskId)` / `wait(taskId, timeout?)`
  - [x] `retry(taskId, options?)`（含 backoffMs/backoffFactor/maxRetries）
  - [x] `pause(taskId)` / `resume(taskId)`（2026-10-01 落地：协作式暂停——开工前驻留、work 完成后扣住结果不落账、重试间隙停试，超时时钟暂停期间停走；仅 pending/running 可暂停、仅 paused 可恢复）
  - [x] `result(taskId)` / `error(taskId)`
  - [x] 任务生命周期事件：`task.submitted/started/paused/resumed/succeeded/failed/canceled/timeout`（pending/running/paused/succeeded/failed/canceled 状态流转）
- [x] `wingman.notify`（notify_module.cpp + notify.pyi + 46 条测试）
  - [x] `info/warn/error/debug`
  - [x] `toast(title, message, level?)`
  - [x] `log(channel, message, meta?)`（notify.log + 事件化）
  - [x] `webhook(url, payload, options?)`（含 pending/success/failed/blocked 全生命周期事件）
  - [x] `trayShow()/trayHide()/traySetBadge(text?)/traySetTooltip(text?)`（2026-10-04 落地：向 EventHub 发 `notify.tray.*` 意图事件，runtime notify_bridge 转投 EventBuffer，GUI 经 tauri `tray_control` 驱动系统托盘；badge 因 tauri v2 TrayIcon 无角标 API 暂不支持——GUI 端返回错误走前端日志（2026-10-05 nightly 实证修正），待上游支持或自绘方案再接）
  - [ ] 订阅 `event.*` 与 `task.*` 的通知桥接（2026-10-11 部分落地：`task.*` 九类与 `orchestration.*` 四类生命周期事件并入 `installSystemEventBridge` 统一转投 EventBuffer（GUI events.drain 可见），`hotkey.error` 补齐文档已列而桥接缺失的漂移；**待拍板**——脚本自定义事件（`wingman.event.emit` 任意类型名）的通配桥接需 EventHub tap/前缀订阅机制，通知侧自动规则（哪些事件自动生成哪级通知）属产品语义）

### 编排与恢复
- [x] `wingman.orchestration`（基础版：orchestration_module.cpp + orchestration.pyi——submit/get/get_all/cancel_workflow）
  - [x] 工作流定义
  - [x] 依赖关系（2026-10-09 落地：runtime 本地执行——`dependsOn` 前置任务集 + DFS 环检测提交即拒（自环/多节点环/未知引用/重复 id）；前置全 succeeded 才调度，单工作流拓扑序串行（并发控制留后续批次）；前置 failed/canceled/timeout 沿依赖图传播 skipped（传递），取消走用户语义置 canceled；任务执行复用抽出的 task_core（timeoutMs/maxRetries/backoff 与 task 模块同词汇表同默认值，emitEvents 门控关 task.* 事件）；拒绝路径发 `orchestration.error` 事件；快照反映 blocked/pending 阻塞态。Lua 可调用体提交即拒（调度线程执行，同 task async 门控）；17 例测试（依赖图/环检测/传播矩阵/取消/快照）+ task 模块回归全绿）
  - [x] 条件分支（2026-10-09 落地：任务项可选 `when` 线程安全可调用体（与 run 同门控，提交即拒非可调用体/非线程安全），前置满足后的调度点锁外求值一次（谓词可重入查询/取消本工作流）；不成立落条件链 skipped——分支过滤是正常控制流，不判工作流失败，沿依赖图传递、混合前置时失败链优先；谓词求值异常沿用 task 失败语义 Task::fail 落 failed 携带错误信息；真值口径：Bool 按值、Int/Float 非 0、String/Array/Object 非空、Null 假；9 例测试（真假/二选一/传递/失败链优先/异常/真值矩阵/拒绝/求值一次））
  - [x] 并发控制（2026-10-09 落地：工作流级 `maxParallel` 同时执行中任务数上限，默认 1 串行、<1 提交即拒；单调度线程决策 + 每任务独立 worker 线程，执行额度满则调度线程驻留 cond 等 worker 收尾；依赖边仍优先于并发额度，取消/停机唤醒调度定稿、收集-取消-join 全在锁外；快照对已派发未收账任务以 Task 内核状态为准（协作取消即时反映）；6 例测试（并行/额度/默认串行/校验/并发下依赖序/取消））
  - [x] 子任务聚合（2026-10-11 落地：快照聚合任务产出——落账成功任务 `result` 字段携带 `run()` 返回值原样聚合（null 省略字段与未产出同形；失败/超时/取消/跳过不携带）；下游任务经 `get_workflow` 读上游 `result` 组合数据流，嵌套工作流可借 `run` 提交子工作流等终局后返回其结果逐层聚合（组合即嵌套，v1 不做图内子工作流节点）；2 例测试（多型产出聚合/null 与失败不携带/下游读上游））
  - [x] 流程级状态事件（2026-10-11 落地：`orchestration.*` 三类——started{f,name} 调度线程首发、task_finished{f,task,status,error?} 单任务落账终态、finished{f,name,status,tasks{succeeded/failed/skipped/canceled/other/total}} 定稿；调度线程单点全序广播、锁外 emit（防订阅方重入自死锁）；传播 skipped 与取消窗口未收账任务不单发 task_finished，计数落 finished；`task.*` 模块级事件已由 emitEvents 门控关闭不重复；4 例测试全序/失败计数/条件链/取消 other 窗口；文档见 api/orchestration.md 流程级事件节）
- [x] 任务重试、超时、退避封装（task 模块 backoffMs/backoffFactor/maxRetries/timeoutMs）
- [ ] 任务状态持久化与恢复（2026-10-13 评估挂起：v1 任务体是线程安全可调用体（Lua/Python 闭包），不可序列化——重启后「恢复」语义需要产品拍板（仅声明式定义可重放？未终局工作流标 failed 落账供查询？持久化介质 KV vs 文件？）。在语义定案前实现一半（只落账不恢复）价值存疑，挂起待拍板）
- [ ] 事件订阅持久化与断线重连（2026-10-13 评估挂起：transport 层重连已存在（RemoteClient 指数退避重连循环 + 重连后 outbox 冲刷）；缺的是「订阅」面——Go server 目前没有事件订阅注册表（无 SSE/WS 订阅端点，事件面只在 runtime 内部 EventHub/EventBuffer），无存量订阅可持久化。做这件事=先设计 server 侧订阅/投递架构（订阅关系存谁、断线窗口事件是否补投），属产品/架构决策，挂起待拍板）
- [ ] 统一回调/通知策略，避免 Lua 和 Python 语义分裂（2026-10-13 评估挂起：现状已收敛大半——事件回调统一 callableThreadSafe 门控（hotkey/systemwatch/uia.error 同口径），命名约定已由描述符审计门钉死（camelCase 原名 + Python snake 别名）。剩余分裂点（回调线程模型是否对齐 Lua 的协程式排队、错误上报事件名是否统一）属 API 语义设计，改动即破坏存量脚本，需产品拍板后单独立项）

### 常用工具补齐
- [x] 剪贴板模块（clipboard_module.cpp + clipboard.pyi）
- [x] 文件 IO 模块（`wingman.file`：read/write/append/exists/isFile/isDir/size/move/copy/remove/removeAll/mkdir/listDir 13 函数，move 跨文件系统 copy+remove 回退，2026-10-04 落地；文件变化监控由 filewatcher 提供）
- [x] 热键监听模块（`wingman.hotkey`：轮询式全局键态 + 组合键上升沿回调，register/unregister，Windows/Linux/macOS，2026-10-04 落地；v1 限制：轮询间隔内点按可能漏检、macOS 辅助功能权限待真机）
- [x] 定时器 / 计划任务模块（timer_module.cpp + timer.pyi + 12 条测试；after/every/取消/查询/sleep）
- [x] 更完整的 UI 控件树遍历与等待（uia 模块：树遍历基础 + `get_parent` 父节点接口（2026-10-09：win RawViewWalker.GetParentElement / mac kAXParentAttribute）+ 等待族补全——`wait_for_name`/`wait_for_id`/`wait_for_role`/`wait_for(selector)`，选择器 {name?,id?,className?,role?,text?}，name/text 子串、其余全等；默认超时 3000ms，超时返回 null）
- [x] AI 视觉识别（2026-10-07 启动并**完成**，MVP 单场景「识别屏幕按钮并点击」全原子项验收——含 OpenAI 兼容 provider 加密凭据面、locate/elements 双协议、桌面+Android 三面同形、本地 ONNX provider 分流、模拟器真机走查 PASS 留证；后续增量见「图像模板批量管理与识别」条）：
  - [x] provider 配置面：`aiSetup`（OpenAI 兼容端点 baseUrl/model/凭据，配置驻内存；`apiKeyEnc` 走 crypto AES-256-GCM 密文 + 口令解密驻内存——保险箱同型加密面，明文与密钥不落盘不落日志）+ 状态查询
  - [x] locate 协议：请求构造（截屏 JPEG→base64→chat.completions）与响应解析（模型返回 `found/label/bbox_2d/confidence`，坐标 0–1000 归一，按帧尺寸换算像素）——纯函数可单测
  - [x] 桌面 `wingman.vision.aiLocate(desc[, region])` 闭环：Screen::capture 截帧 → provider → 结构化 bbox → 脚本 `input.click` 动作映射
  - [x] Lua/Python 绑定 + docs/api/vision.md AI 节（凭据加密面写法与 provider 约定）
  - [x] Android 接入：androidagent 挂同形 `aiLocate`（captureFrame 位图经 `VisionAi::setFrameProvider` 注入复用同 provider 通道；http.cpp/crypt.cpp cherry-pick，清单补 curl/openssl）
  - [x] 真机走查截图证据（模拟器：识别指定按钮并点击成功，B 批同口径）——2026-10-07 PASS：模拟器 agent 连 Go server，`/api/scripts/run` 下发脚本 aiSetup(mock provider)→aiLocate("目标按钮") found=true box=(471,1457,135,133) conf=0.93→input.click(538,1523) 注入成功，Settings 主页跳 Notifications 子页（前后截图佐证）；provider 收 163KB 截帧请求，script_output 落库至 finished（结论记 CHANGELOG 同日条目）
  - [x] 扩展：`aiElements()` 批量元素识别（2026-10-07 落地：`VisionAi::elements` 批量协议 `{"found", "elements":[{label,bbox_2d,confidence}]}`，desc 空=全部可交互元素；Lua/Python/Android 三面同形绑定，单项坏跳过不废整批，失败与空批靠 error 区分；两构建档 2633+2580 全绿）
  - [x] 扩展：本地 ONNX 检测模型 provider（2026-10-07 落地：`VisionAi::setupLocal`/`isLocalMode` 与 HTTP setup 互斥、后调用者生效、`reset` 双清，未构建 ML 恒 false 不静默降级；`aiLocate`/`aiElements` 入口分流返回同形，locate 取置信最高、labels 双向子串匹配；ml.cpp `detectObjects` 真实推理链 NCHW→简单 resize→0-1 归一→run→decode→比例还原→NMS，`decodeYoloOutput` 支持 YOLOv5/v8 双布局形状判别；decode/nms 抽 `ml_postprocess.cpp` 两档共编单源；ORT 1.23 API 现代化并修输出拷贝宽写死 4 字节；ml feature 平台扩 `windows | linux`——隔离试装 baseline 本地装通 1.23.2，CI 不激活 ml feature 零影响）
- [x] 图像模板批量管理与识别（2026-10-11 落地：`TemplateRegistry` 具名登记（add 期校验路径存在、重名拒绝、显式 remove 后可再登记）+ `templateAdd/templateRemove/templateHas/templateList/templateFindAll` 五入口；`templateFindAll` 名序确定输出、names 过滤跳过未知名、匹配逐个锁外执行、单模板失败按未命中落账不废整批；`findImage` 返回形状不变（name 键仅批量面携带）；新增 vision_module_test 3 例，pyi 与 api/vision.md 同步）
- [x] 录制 / 回放闭环（macro_module.cpp）
- [x] UIA 事件监听绑定（`onPropertyChanged` / `onStructureChanged` / `removeEventListener`，Windows UIA 专属，回调线程安全门控走 `uia.error`；跨平台统一事件源见下条「进程/窗口/文件变化统一事件源」）
- [x] 进程/窗口/文件变化统一事件源（2026-10-09 落地：进程/窗口腿 `wingman.systemwatch`——SystemWatcher 轮询 diff 快照（500ms，首拍基线）+ callableThreadSafe 门控直调 + `systemwatch.*` 事件面，Python 回调限定同 hotkey；文件腿 `wingman.filewatcher` 脚本桥接接真——原生 inotify/FSEvents/Win32 后端 + 路径簿记（isWatching/getWatchedPaths）+ `filewatcher.changed`/`filewatcher.error` 事件面，watch 返回注册 ID）

### 优先级建议
- [x] P0: `event`、`task`、`fsm`
- [x] P1: `notify`、`orchestration`
- [x] P2: `clipboard` ✅、`file` ✅（监控+IO）、`hotkey` ✅、`timer` ✅
- [ ] P3: UI 树、模板管理、录制回放增强（录制回放主体 ✅）

### 建议的落地顺序
1. `event` 先补齐事件对象、订阅管理和一次性监听
2. `fsm` 直接建立在 `event` 之上，统一状态迁移事件
3. `task` 引入任务生命周期和重试/超时封装
4. `notify` 消费 `event` / `task`，统一输出到日志、托盘和 webhook
5. `orchestration` 复用 `task` + `fsm` 做工作流编排

---

## 移动端支持（规划，2026-09-19 立项）

> 可行性分析与路线图详见 [mobile-support-feasibility.md](./mobile-support-feasibility.md)。
> 核心形态：**Android 端侧 Agent + TCP 长链接直连 Go Server（云控模式）**，控制面在
> Server、执行面在端侧；iOS 端侧不可行（沙箱无合法通道），iOS 主机控（WDA）为远期可选。

### A1 PoC：链路打通 [2026-09-22 校准，代码已落地]
- [x] Kotlin 壳：ForegroundService + 长链接（`WingmanService.kt`：前台服务 + START_STICKY + dataSync|mediaProjection 类型；复用 libs/transport 编译到 Android）
- [x] wingman 核心 NDK 编译通过（Lua + transport + 核心库；vcpkg 扩展 arm64-android triplet；nightly Android arm64 job 全绿，2026-09-22）
- [x] Go Server 下发脚本 → 端侧执行（`android_agent.cpp` 支持三命令 run_script/stop_script/screenshot.capture，`system.shutdown` 显式拒绝——不响应远端关停；日志经 agent.event 上行回传 Dashboard）

### A2 能力闭环（2026-09-22 校准：IInput/ICapture/脚本 API 已落地，余触发器与真机验证）
- [x] `platform/android/` IInput 后端（AccessibilityService dispatchGesture，经 JNI；`WingmanAccessibilityService.kt` 主线程 post + seq/promise 回执，超时兜底在 C++）
- [x] `platform/android/` ICapture 后端（MediaProjection 主通道；`ScreenCaptureManager.kt` + AndroidHostBridge::captureFrame。⚠️ takeScreenshot API 30+ 兜底未做）
- [x] 找色/找图/像素检测对手机截帧可用；screen/input 脚本 API 全通（input click/tap/longPress/swipe/delay + screen getScreenWidth/Height/capture/getPixel/findColor/findColors/findImage + vision 五函数；桌面同源编译验证：android_api_test 6 + script_runner_test 8 用例。⚠️ 真机触摸/截帧行为待真机验证）
- [x] 触发器系统端侧跑通（定时/像素触发）（2026-10-13：AndroidTriggerManager（单监视线程，timer 边沿/color_found/color_not_found/pixel_changed 条件 + tap/long_press/swipe/stop 动作，checkInterval 轮询 + maxTriggers 自停，对齐桌面 SmartTrigger 语义）+ `wingman.trigger` Lua 绑定 + 桌面同源单测 9 用例（libs/lua/tests，OpenCV/无 OpenCV 两档树全绿）。⚠️ 真机触摸/截帧行为与宿主拆桥前 stopAll 时序待真机验证）

### A3 可靠性与部署体验
- [x] 开机自启、崩溃自重启、断连自治（缓存脚本继续执行、重连后汇报）（2026-09-30，6cecfad：BootCompletedReceiver/退避闹钟/核心看门狗 + RemoteClient 既有 outbox）
- [x] Android 13+ 受限设置引导（手动允许 / adb 预授权 / Device Owner 批量部署）（2026-09-30：`scripts/android-restricted-settings.sh`（幂等，Go 契约测试护栏）+ App 内引导（RestrictedSettingsPolicy 自动弹一次 + 常驻按钮）+ `docs/guides/android-restricted-settings.md`）
- [x] 模板图片 asset.sync 下发、脚本版本管理（2026-10-13 全量落地：asset.sync 前半——服务端资产存储（internal/assets，sha256 sidecar，1MB 上限）+ `POST /api/assets*` 五端点（权限 assets:manage）+ agent 通道 `asset.sync` 命令（内容内联 base64 + sha256 强校验，runtime 落盘 `<scriptDir>/assets/<name>`，Go/C++ 两端测试绿）；脚本版本管理后半——内容即版本：Script 行 sha256 创建/保存时维护、列表对存量空行懒回填，run_script payload 携带 contentSha256（单发/批量/workflow 三处同口径），Execution 落 scriptSha256，runtime 忽略未知字段向后兼容；Android 端侧落盘已于 2026-10-15 接入——RpcDispatcher + registerAssetHandlers 进 androidagent cherry-pick 清单（依赖仅 filesystem+crypt，零新增重量依赖），AndroidAgent 命令面新增 `asset.*` 分支落 `<filesDir>/assets/`（脚本以 `assets/<name>` 相对引用与桌面同形，findImage 相对路径按 filesDir 解析），桌面同源守卫测试 android_asset_test 只链 androidagent 目标防清单回退；⚠️ 端侧行为经桌面同源测试 + NDK 层宿主编译核验，真机走查待真机）

### A4 多设备编排
- [x] Team/inbox 模块接入端侧 Agent（2026-10-13 两半补齐——①服务端读面：`GET /api/teams`（全量列表，teamId 升序）、`GET /api/teams/:teamId`（详情含 memberAgents 成员→设备映射）、`POST /api/teams/:teamId/messages`（团队范围收件箱推送，未绑定设备的成员计 skipped 不向空 agentID 入队），需 agents:manage 权限并落审计（team.create / team.message_push）；②端侧桥：桌面 runtime 此前脚本 `wingman.team.*` 只做本地乐观更新（team.outbound 事件零消费者、handleServerMessage 零调用方、inbox.message 被 agent 通道静默丢弃）——现 RemoteClient 新增 sendNotify/NotifyHandler，apps/agent 装配双向桥（team.outbound → 同名 Notify 下行；inbox.message → team.inbound 事件 → team 模块 handleServerMessage 状态机，处理即 inbox.ack），并补齐 team.md 声明的 joined/left/member_joined/member_left 四个本地事件 emit；⚠️ 团队与收件箱均为内存态，服务重启丢失；Dashboard Teams 页面已做（列表/详情含成员→设备映射/团队收件箱推送，canAgentManage 守卫）；Android runtime 仍不消费 inbox.message（platforms.md 既定口径））
- [x] Dashboard 设备视图（分组、批量下发、状态大盘）（2026-10-13：Agents 页面增量——按标签分组视图（平铺/分组切换，每组一表、勾选跨组共享）、按标签批量下发选择器（agentIds ∪ tags 与勾选取并集，服务端既有契约直接复用）、状态大盘平台维度（desktop/android/ios 计数，缺失 platform 按桌面口径）；批量操作按钮禁用口径随之改为选择器非空；8 语言 i18n key 补齐，纯函数口径抽 utils/agents.ts 单测钉住）

### 远期可选
- [ ] I1: iOS 主机控——PC 端 ICapture/IInput 的 WDA 后端（usbmuxd/libimobiledevice，依赖 Mac 签名链）
- [ ] 端侧 Python 引擎评估（Lua-first，Python+NDK 体积/维护成本高，暂缓）

---

## Phase 1: 核心引擎 (C++)

### 1.1 屏幕操作模块
- [x] `screen.capture()` - 截取屏幕/窗口
- [x] `screen.getPixel(x, y)` - 获取单点像素
- [x] `screen.findColor(color, region, tolerance)` - 单点颜色查找（region:{x,y,width,height}，返回 `[point, found]` 数组；签名按现役注册修正）
- [x] `screen.findColors(color, region, tolerance, maxCount)` - 多点颜色查找（返回点数组；签名按现役注册修正）
- [x] `screen.findImage(imagePath, region, threshold)` - 图像匹配（region 可省略默认全屏，返回 `[point, found]`；签名按现役注册修正）
- [x] 窗口标题/位置查询 - 实注册为 window.getTitle/getBounds（screen 模块无 getWindowTitle/getWindowBounds，2026-10-08 审计修正归属）

### 1.2 输入模拟模块
- [x] `input.click(x, y, button)` - 鼠标点击
- [x] `input.move(x, y, duration)` - 鼠标移动（贝塞尔曲线）
- [x] `input.scroll(x, y, delta)` - 鼠标滚轮
- [x] `input.keyDown(key)` - 按键按下
- [x] `input.keyUp(key)` - 按键释放
- [x] `input.type(text, delay)` - 文本输入

### 1.3 窗口管理模块
- [x] `window.find(title)` - 查找窗口
- [x] `window.activate(hwnd)` - 激活窗口
- [x] `window.getBounds(hwnd)` - 获取窗口位置
- [x] `window.getTitle(hwnd)` - 获取窗口标题
- [x] `window.getForeground()` - 获取前台窗口
- [x] `window.waitFor(title)` - 等待窗口出现

### 1.4 进程管理模块
- [x] `process.find(name)` - 查找进程
- [x] `process.start(path, args)` - 启动进程
- [x] `process.wait(pid)` - 等待进程
- [x] `process.terminate(pid)` - 终止进程
- [x] `process.exists(pid)` - 检查进程存在
- [x] `process.waitFor(name)` - 等待进程启动

### 1.5 人性化模拟模块
- [x] `input.move(x, y, duration)` - 贝塞尔曲线移动
- [x] `input.randomDelay(min, max)` - 随机延迟
- [x] 集成在 input 模块中

### 1.6 Lua 绑定
- [x] 暴露所有 C++ API 到 Lua
- [x] Lua 集成
- [x] 错误处理和异常转换
- [x] HTTP 客户端绑定
- [x] JSON 封装绑定
- [x] KV 存储绑定
- [x] UI Automation 绑定

---

## Phase 2: 触发器系统

### 2.1 触发器类型
- [x] 像素触发器 - 检测到指定颜色/图像
- [x] 定时触发器 - 间隔执行
- [x] 时间触发器 - 指定时间执行
- [x] 窗口触发器 - 窗口出现/消失
- [x] 进程触发器 - 进程启动/停止
- [x] 像素变化触发器
- [x] 触发器统一接入 `wingman.event`（2026-10-09：TriggerManager 命中时同步 emit `trigger.fired`（source `trigger`，载荷 id/name/type/triggered/lastTriggerTime），与 runtime EventBuffer 的 trigger.fired 缓冲推送同源；posix/win 双 fire 点同改；内置事件源已在 docs/api/event.md 列表化）

### 2.2 触发器动作
- [x] 发送按键
- [x] 鼠标操作
- [x] 显示消息
- [x] 播放声音
- [x] 执行 Lua 函数 (RunScript 动作)
- [x] 日志输出 (Log 动作)
- [x] 触发器动作事件通知（2026-10-09：动作执行完 emit `trigger.action`（source `trigger`，载荷 id/name/actionCount），与 `trigger.fired` 区分——fired=条件命中、action=动作执行完成；posix/win 双 fire 点同改）

---

## Phase 3: 宏系统

### 3.1 录制功能
- [x] `macro.startRecording()` - 开始录制
- [x] `macro.stopRecording()` - 停止录制
- [x] 记录鼠标移动/点击
- [x] 记录键盘输入
- [x] 时间戳记录

### 3.2 回放功能
- [x] `macro.play(name)` - 播放宏
- [x] `macro.save(name, path)` - 保存宏
- [x] 回放速度控制
- [x] 循环播放
- [x] 宏录制事件流导出（2026-10-09：recordEvent 每条落库事件同步 emit `macro.recorded`（source `macro`，载荷 type/x/y/keyCode/timestamp）；三平台 recorder 实现同改，去重后的 MouseMove 不发同型抖动）
- [x] 宏回放状态事件（2026-10-09：start/stop/pause/resume/playback 发 `macro.state`（source `macro`，载荷 state ∈ recording/paused/playing/stopped）；三平台 recorder 实现同改）

---

## Phase 4: 调试工具

### 6.1 VS Code 开发环境
- [x] 使用 EmmyLua 插件提供语法高亮、自动完成、悬停提示
- [x] 使用 EmmyLuaDebugger 提供断点调试
- [x] 项目配置 (.vscode/settings.json, launch.json)
- [x] Lua 库路径配置 (Lua.library)

### 6.2 日志系统
- [x] 分级日志 (DEBUG/INFO/WARN/ERROR) - 使用 spdlog
- [x] 文件输出
- [x] 控制台输出
- [x] 性能统计（2026-10-13 落地为**脚本执行统计**：`ScriptInfo` 增 `ScriptStats`（runCount/successCount/failCount/lastDurationMs/totalDurationMs/lastRunAt，进程内累计不持久化）——runScriptInternal 启动即计 runCount+lastRunAt，成败终局计 success/fail 与耗时（被超越运行不计，用户主动 stop 不计成败不计耗时，超时计失败）；`wingman.script.getStats(name)` 三语言出口（pyi 补 ScriptStats TypedDict + get_stats，Lua 提示重生成 413 函数）。测试：ScriptManagerExecTest.Stats* 2 例（累计/失败计 stop 不计）+ ScriptModuleGlueTest.GetStatsAfterRealRun（胶水真实跑+防御分支））
- [x] 事件通知桥接（2026-10-09：新增 `installSystemEventBridge`——把 EventHub 统一系统事件源（systemwatch.*/filewatcher.*/trigger.fired/trigger.action/macro.state/macro.recorded）原样转投 EventBuffer，method 与事件名一一对应；GUI 经 `events.drain` 拉取；与托盘桥接互补，幂等安装）

---

## Phase 5: 高级功能

### 7.1 脚本管理
- [x] 脚本热加载
- [x] 配置文件解析
- [x] 环境变量支持
- [x] 脚本沙箱
- [x] 任务状态机（task_module：pending/running/succeeded/failed/canceled 状态流转，2026-09-18 核对）
- [x] 工作流编排（orchestration_module：submit/get/get_all/cancel_workflow，2026-09-18 核对）

### 7.2 性能优化
- [x] 像素检测加速 - 使用 OpenCV
- [x] 图像匹配缓存 - LRU 缓存机制
- [x] 多线程处理 - OpenCV 并行
- [x] 图像金字塔加速
- [x] 内存优化 (智能缓存管理)

### 7.3 安全特性
- [x] 完整性自检 (verifyIntegrity；代码签名验证未实现)
- [x] 进程保护 (反调试、反VM)
- [x] 反检测机制 (随机延迟、点击抖动)
- [x] 哈希 (hashString；字符串加密未实现)

---

## Phase 6: 文档和示例

### 8.1 文档
- [x] API 参考文档
- [x] 快速入门指南
- [x] UI Automation API 文档
- [x] Window API 文档
- [x] Process API 文档
- [ ] 视频教程

### 8.2 示例脚本
- [x] Hello World
- [x] 像素检测示例
- [x] 图像匹配示例
- [x] 自动循环示例
- [x] UI Automation 示例
- [x] MMORPG 游戏自动化
- [x] 窗口操作示例
- [x] 进程管理示例
- [x] HTTP API 示例
- [x] KV 存储示例
- [x] 脚本管理示例
- [x] 安全模块示例
- [x] 游戏配置示例

---

## Phase 9: UI Automation (新增)

### 9.1 核心功能
- [x] UI Automation COM 接口集成
- [x] 元素查找 (byName, byId, byControlType)
- [x] 元素操作 (click, setValue, getValue)
- [x] 元素遍历 (getChildren, getParent)
- [x] 元素等待 (waitFor)

### 9.2 Lua 绑定
- [x] `uia.from_foreground()` - 获取前台窗口根元素
- [x] `uia.from_point(x, y)` - 从坐标获取元素
- [x] `uia.from_window(hwnd)` - 从句柄获取元素
- [x] `uia.find_button(name)` - 查找按钮
- [x] `uia.find_edit(name)` - 查找编辑框
- [x] `uia.find_text(name)` - 查找文本
- [x] `uia.find_by_name(name)` - 按名称查找
- [x] `uia.find_by_id(id)` - 按 ID 查找
- [x] `uia.wait_for_name(name, timeout)` - 等待元素
- [x] `uia.find_check_box` / `find_radio_button` / `find_combo_box` / `find_list` / `find_list_item` / `find_tree` / `find_tree_item` / `find_menu_item` / `find_hyperlink` / `find_image` / `find_slider` / `find_spinner` / `find_progress_bar` / `find_tab` / `find_tab_item` - 控件专用查找（2026-10-09 批落地）
- [x] `uia.wait_for_id` / `wait_for_role` / `wait_for(selector)` - 等待族（2026-10-09 落地）

### 9.3 UIElement 方法
- [x] `:click()` - 点击
- [x] `:right_click()` - 右键点击（2026-10-09：win 落元素中心 SendInput 右键；mac AXShowMenu 弹上下文菜单）
- [x] `:double_click()` - 双击
- [x] `:focus()` - 设置焦点
- [x] `:get_value()` - 获取值
- [x] `:set_value(value)` - 设置值
- [x] `get_info().name` - 获取名称（经 get_info 返回的 info.name 取得，无独立 getName 方法）
- [x] `:get_info()` - 获取完整信息
- [x] `:get_children()` - 获取子元素
- [x] `:get_parent()` - 获取父元素（2026-10-09：win RawViewWalker.GetParentElement / mac kAXParentAttribute）
- [x] `:select()` - 编程选中（2026-10-09：win SelectionItem.Select / mac AXSelected=true）
- [x] `:get_selection()` - 容器首个选中子项（2026-10-09：win Selection.GetCurrentSelection / mac AXSelectedRows/Children）
- [x] `:expand()` / `:collapse()` / `:is_expanded()` / `:is_visible()` / `:is_enabled()` - 状态与展开（随 UIElement 12 方法批落地）
- [x] `:is_checked()` / `:set_checked(bool)` - 复选/单选状态读取与设置（2026-10-09：win Toggle 模式 / mac AXChecked）

---

## Phase 7: TCP 协议增强 (可选)

#### 请求消息结构 ✅
```json
{
  "type": "heartbeat",           // 消息类型
  "id": "req-002",               // 请求 ID
  "timestamp": 1714928900,       // 发送时间戳（通用字段）
  "agent_id": "vm-wow-1",        // 发送者 ID（已注册客户端）
  "priority": 0,                 // 优先级（可选）
  "data": {                      // 业务数据
    "status": "busy",
    "current_task": {...}
  }
}
```

#### 响应消息结构 ✅
```json
{
  "request_id": "req-002",       // 对应的请求 ID
  "code": 0,                     // 错误码（数字）
  "timestamp": 1714928901,       // 响应时间戳
  "message": "success",          // 可读描述（可选）
  "data": {...}                  // 业务数据（成功时）
}
```

#### 错误码定义 ✅
| Code | 名称 | 说明 |
|------|------|------|
| 0 | OK | 成功 |
| 1 | UNKNOWN | 未知错误 |
| 2 | INVALID_REQUEST | 请求格式错误或参数无效 |
| 3 | NOT_FOUND | 资源未找到 |
| 4 | TIMEOUT | 操作超时 |
| 5 | BUSY | 服务忙碌 |
| 6 | NOT_AUTHORIZED | 未授权 |
| 7 | ALREADY_EXISTS | 资源已存在 |
| 8 | FAILED | 操作失败 |
| 9 | DISCONNECTED | 连接断开 |
| 10 | RATE_LIMITED | 请求频率限制 |
| 1024+ | 用户自定义 | 业务错误码（>= 1024） |

#### 消息类型 ✅
- [x] `kRegister` - 客户端注册消息
- [x] `kHeartbeat` - 心跳消息
- [x] `kGetAgents` - 获取所有在线客户端列表
- [x] `kSyncTask` - 同步任务状态
- [x] `kShutdown` - 关闭客户端

### 12.2 服务端会话管理 ✅
- [x] `AgentInfo` 结构 - 客户端信息 (agentId, hostname, ip, status, lastSeen)
- [x] `getOnlineAgents()` - 获取所有在线客户端
- [x] `sendToAgent(agentId, response)` - 向指定客户端发送消息
- [x] `disconnectAgent(agentId)` - 断开指定客户端
- [x] 心跳超时检测 - 定时检查超时客户端并清理

### 12.3 客户端增强 ✅
- [x] `setAgentId(id)` - 设置客户端 ID
- [x] `enableAutoReconnect(enable, interval)` - 启用自动重连
- [x] `setStateCallback(callback)` - 连接状态事件回调
- [x] `startHeartbeat(interval)` - 启动自动心跳
- [x] 连接后自动注册 - 发送 register 消息

### 12.4 事件系统 ✅
- [x] 服务端：`onConnect(agentId)` - 客户端上线事件
- [x] 服务端：`onDisconnect(agentId)` - 客户端下线事件
- [x] 客户端：`onConnectionStateChanged(connected)` - 连接状态变化事件

---

## 架构设计原则

### 核心能力 vs 业务逻辑
- **核心层** (保留): 验证码生成/验证 (TOTP/SteamGuard)、键值存储、任务队列、脚本执行引擎
- **扩展层** (可选): AccountManager 插件、进度存储接口、调度器接口
- **用户层** (用户定义): 账号概念、游戏逻辑、脚本进度、调度策略

### 模块位置调整
| 模块 | 当前位置 | 建议位置 | 理由 |
|------|----------|----------|------|
| TOTP/SteamGuard 算法 | 核心保留 | ✅ 核心能力层 | 通用能力 |
| VerificationManager 类 | 核心封装 | ⚠️ 移到 examples/ | 过度封装，用户自己定义数据结构 |
| QRLoginManager 类 | 核心封装 | ⚠️ 移到 examples/ | 过度封装 |
| 账号概念 | - | 用户层定义 | 框架不预判业务 |

### 通信协议
- ✅ 使用 TCP 长连接 (已有 asio 实现)
- ✅ 使用 JSON 序列化 (可读、易调试)
- ✅ 使用自定义信封协议 (已实现: `length\njson\n`)
- ✅ 使用 Protobuf (严格的消息定义)

---

## 优先级排序

### P0 - 核心基础 ✅
1. ✅ 屏幕操作模块
2. ✅ 输入模拟模块
3. ✅ Lua 绑定
4. ✅ 主程序入口

### P1 - 基本功能 ✅
5. ✅ 窗口管理模块
6. ✅ 触发器系统基础
7. ✅ 宏录制回放

### P2 - 增强功能 ✅
10. ✅ 人性化模拟
11. ✅ 调试工具
14. ✅ 性能优化
15. ✅ 脚本管理

### P3 - 高级功能 ✅
16. ✅ UI Automation
17. ✅ VS Code 开发环境配置
18. ✅ 脚本管理
19. ✅ 安全特性
20. ✅ 多游戏配置

---

## 当前状态

### 已完成 ✅
- [x] 项目初始化
- [x] CMake 构建系统
- [x] 基础目录结构
- [x] 文档网站框架
- [x] CI/CD 配置
- [x] 核心功能实现 (100%)
- [x] Lua 测试框架 (busted)
- [x] Codecov 配置
- [x] 示例脚本 (13个示例)
- [x] VS Code 开发环境配置 (EmmyLua + EmmyLuaDebugger)
- [x] 性能优化模块
- [x] 脚本管理模块
- [x] 安全模块
- [x] 游戏配置管理
- [x] UI Automation 模块

### 进行中 🚧
- [x] UIA 事件监听器 Lua 绑定
- [x] 触发器系统增强（Lua 函数执行、日志输出）
- [x] 完善 UIA 文档和示例
- [x] 添加更多 UIA 控件类型支持

所有短期任务已完成！

### 待规划 📋
- [ ] 视频教程
- [x] 更多示例脚本（2026-10-13 首批：Python 示例 0→4（hello 入门/template_batch 模板批量/workflow_pipeline 工作流编排——run/when 需线程安全可调用体故仅 Python/events_fsm 事件+状态机），Lua 补 template_batch.lua；语法门 luac -p + py_compile；workspace 工程补 python_scripts 与 Python 桩目录，docs/examples/index.md 补索引。示例是滚动项，新模块落地时按需续补）
- [x] 性能基准测试（2026-10-13：`lib/wingman/tests/benchmarks/` 四基准二进制 + `bench.hpp` 中位数计时助手（warmup + 7 轮取中位 ns/op）+ `scripts/run-benchmarks.sh` 一键构建运行（自动重配 BENCHMARKS=ON、按 OpenCV 档裁剪、打印机器上下文）+ `docs/benchmarks.md` 基线与新增指引。首基线（i9-10880H/load 26-42/Xvfb 1280x800）：kvstore 0.02~0.19µs/操作、event_hub 同步分发 288~542ns/op、lua 引擎创建 119µs、脚本往返 2.5µs、模块胶水端到端 ~5.5µs、vision 全屏截屏 ~130ms/找色 ~92ms/找图 ~350ms（宿主负载 wall 偏移大，仅看量级）。基准不进 CI 门禁；优化半场归入下方中期「性能基准测试和优化」）
- [ ] 用户反馈收集

### 测试覆盖率（2026-09-22 启动，目标 100%）
- [x] Go 侧 100%（`go test -coverprofile` 全 internal+根包 statements 100.0%；五轮补测：batch/stop/trigger 校验与失败分支、commandErrorText 优先级链、ReadInline 400/500、routes.go 装配全量断言、tagstore nil-db；两处不可达防御分支以行为等价重构收口——listener.go 空白名单 tokenValid、tagstore.go marshal 恒成功）
- [x] C++ 基线采集口径确立：`scripts/cxx-coverage-baseline.sh`（全局 `--coverage -O0` 插桩 + lcov extract `lib/wingman/*` + remove `*/tests/*`；仅 CODE_COVERAGE 选项只插桩测试自身，数字虚高不可用）
- [x] C++ 第一批补测（2026-09-22）：① **解锁 smart_trigger_test.cpp**——774 行/49 用例被历史遗留的 `if(WIN32)` 误 gate，Linux 从未编译（smart_trigger.cpp 20% 覆盖的直接原因），解 gate 后 20% → 52.3% 行/95% 分支；② misc_modules 胶水补测（smarttrigger 全类型映射/bt 行为树胶水/node/ocr，10 用例）；③ posix TriggerManager 补测（checkTrigger 十条件 × executeActions 九动作，14 用例，MockInput 断言副作用）；④ crypto/clipboard/macro 胶水补测（12 用例，SHA/编解码/AES 往返）。**基线 71.5%/79.4% → 74.7% 行（13106 行）/82.3% 分支**（v5，36 个新用例 + 解 gate 55 用例全量跑出）。补测中发现并修复 `TriggerActionData` 未初始化成员缺陷（delay 栈垃圾使 watchLoop 睡 24 天，见 CHANGELOG fix 条目）；全量套件唯一失败为 `X11PlatformTest.X11WindowPlatformFeatures`（todo 在案的既有低频 flaky，单跑必过，与补测无关）
- [x] C++ 第二批补测（2026-09-22）：① db 胶水 12 用例（23 个导出函数与连接注册表句柄防护全触达，`:memory:` 不落盘）；② transport/inbox 胶水 8 用例（127.0.0.1 真实 TCP/UDP 端到端 + inbox 对真实 listener 的完整生命周期）；③ team 胶水 8 用例（join 生命周期/投票/广播/状态上报 JSON 三分支/事件订阅）。补测中复现并修复三处真实缺陷：TcpServer start→listen 顺序下 IO 线程空转退出致 server 永不 accept（work_guard 保活）、inbox.connect 自死锁（sendNotify 重入不可重入锁）、team.joinTeam 每次新建客户端致状态查询错位与句柄泄漏；另修复在案 flaky X11WindowPlatformFeatures（WM_CLASS 属性同步滞后改轮询等待）。**74.7% → 81.2% 行（13106 行）中间态**；transport 89%/分支 100%、db 82%、inbox 72%。
- [x] C++ 第三批补测（2026-09-22）：script_manager 真实执行路径 6 用例（wingman::lua 引擎真实跑脚本：执行/输出路由/env/语法错误/超时/事件/reload），超时用例实测复现 detach 线程悬空引用 UAF（exit 139）并修复（按值捕获 + engine shared_ptr 共享，见 CHANGELOG）；x11 screen/input 补测 7 用例（显示器全扫描/DPI/坐标往返/displayModes——复现 dotClock=0 NaN→INT_MIN 缺陷并修复、XTest 鼠标/69 键映射/组合键）。**81.2% → 82.8% 行（13090 行）中间态**；script_manager 74.3% → 89.6%。死代码清理：checkTimeLimit/triggerEvent/triggerEventUnlocked（全库零调用方）。
- [x] C++ 第四批补测（2026-09-22）：x11 截图 4 用例（真窗口内容指纹/无效+已销毁句柄/窗口区域越界/monitor 元数据）、clipboard 全接口面 1 用例（图像 stub/文件换行拼接/格式枚举/clear）、posix_process 全链路 6 用例（fork/exec 生命周期/SIGTERM+SIGKILL/非法 pid/exec 失败//proc 遍历/名字轮询）、HTTP 本地真 server 3 用例（GET 往返头解析/POST/PUT/DELETE 方法与 body 送达/postForm+默认头——此前成功路径零覆盖）、MacroRecorder 离线状态机 6 用例（Move 去重/saveToLua 六类型/JSON 往返与错误三分支/playback XTest 真实注入回读/Xvfb start 失败优雅）。**82.8% → 85.4% 行（13092 行，v8 终版基线，全量 1892 用例）**。结构性不可覆盖分支论证记录（fork 子进程 execvp/_exit 不触发 gcov flush、系统调用失败不可注入、HEAD 无公共入口），不硬凑。
- [x] 前端覆盖率收口（2026-09-22）：GUI vitest **67 文件 511 用例全绿，语句/函数/行 100%（3830/719/2403），分支 99.37%**——本批 7 用例（PickerModal footer 取消/重新截图、scripts previewLoading 占位、screen monitor.name 空回退、settings 读取远程配置 runtime 错误分支）；scripts 脚本名空回退用例编写中证实 store 归一化使页面回退分支不可达，改论证记录。Dashboard jest **20 套件 236 用例全绿，行 99.71%**——新增 fetchJSON 存储异常降级用例。剩余 9 分支 + 1 行逐条论证（Svelte 编译器 each/if 链产物、loading/error 状态机互斥、DOM Event.target 恒非空、监视器恒 ≥1、http.ts:21 ts-jest sourcemap 伪影），不硬凑。
- [x] C++ 第五批补测（2026-09-22）：script_module 胶水全链 7 用例（真实 ScriptManager 注入，12 函数此前零覆盖——15.5% 全库最低，含 null 注入与参数不足防御全触达；实测状态机 load→unloaded/run→completed/reload→loaded）+ filewatcher stub 1 + clipboard 胶水 2（LockGuard 串行化）+ verification TOTP 3（Base32 校验链/自产自销 verify）+ ml stub 错误分支 1 + game_profile 3（临时目录真实落盘：模板/JSON 往返/删除幂等）。**85.4% → 86.8% 行（13092 行，v9 终版基线，全量 1909 用例）**。不可覆盖论证：ml 真实模型路径（CI 为 ml_stub）；第五批原记"filewatcher.watch 成功行不可达"论证有误（ScriptValue::fromCallable 公开存在，第六批已修正并覆盖）。
- [x] C++ 第六批补测（2026-09-22）：smart_trigger 真实触发链 12 用例（恒真/恒假条件双路驱动：executeActions 七动作——CLICK/KEY_PRESS 经 MockInput 注入断言副作用、maxTriggers 自停、STOP fast-exit、start 已运行 false、默认 input 注入分支——第一构造预注入 defaultSharedInput 使 input_ 恒非空，显式传 nullptr 才走 watchLoop 注入分支；六种零覆盖条件 case 体——此前现有用例全是"条件不满足 + start 即 stop"穿行）+ misc uia/node/bt 6 用例（uia 全查找 stub 路径与事件监听全防御分支、bt action callable RUNNING/FAILURE 映射 tick、tick 未知树、bt.wait、setCheckInterval 存在路径、sendHeartbeat、ocr text 字段）+ notify bridge 真驱动 2 用例（EventHub 订阅捕获：event:// 转发 + http:// 白名单拒绝 blocked 事件——bridge 回调此前从未被 emit 触发）+ x11 getWindows 胶水循环体 1 用例（TestX11Window 模拟 _NET_CLIENT_LIST）。补测复现并修复 filewatcher 胶水三函数（watch/unwatch/isWatching）参数裸下标越界（缺参即 abort 进程，见 CHANGELOG fix）；更正第五批 filewatcher 论证错误；顺带修复在案 flaky ClipboardTest.HasFiles（x11 selection 异步生效，clear 后改轮询等待）；修复第五批 Windows CI 失败 ClipboardModuleGlue.HtmlImageAndFilesBehavior（原按 X11 行为写死断言，Windows CF_DIB 可真实写入 image，改为平台无关自洽断言 imageSet↔hasImage↔getImage 三态一致）。**86.8% → 87.9% 行（13092 行，v10 基线，全量 1930 用例：1890 PASSED + 40 skip）；smart_trigger.cpp 52.3% → 100% 行**。不可覆盖论证：notify WebhookSender worker/URL 解析段（白名单无胶水配置入口，默认空白名单入口即拒、worker 永不启动——需产品补胶水层配置函数）、WebhookSender 析构 join（进程退出路径）、misc UIElement 12 方法闭包与 uia 注册表（需平台 UIA 后端真实元素，Linux 无实现且 registry 无注入点）、filewatcher 胶水 2 行 gcov 行归属伪影。
- [x] 修复 Windows CI ctype 断言挂死（2026-09-23）：第六批 CI 8/9 绿，唯 C++ Windows 在 `GameProfileModuleGlue.CreateTemplateAndDelete` `[ RUN ]` 后静止 58 分钟被步骤 60 分钟超时强杀——第五批同位置已挂（被同 job Clipboard 断言失败掩盖），100% 确定性复现。根因：中文 gameName 的 UTF-8 字节经 signed char 为负值传给 `::tolower`/`::isspace`，MSVC UCRT Debug 断言弹模态对话框，headless runner 无人点击即永久阻塞（Linux/macOS 查表偏移安全故全绿）。修复：全库 9 处 ctype 裸 char 传参统一转 `static_cast<unsigned char>`（game_profile/smart_trigger×2/security/script_manager×2/db_module/human/verification/posix_system）+ core_tests 静态对象把 `_CRT_ASSERT` 重定向 stderr 兜底（仅 `_WIN32 && _DEBUG`）。上一批的 ClipboardModuleGlue.HtmlImageAndFilesBehavior 平台无关断言修复已在 Windows 实测通过（23 ms OK）。
- [x] C++ 第七批补测（2026-09-23）：inbox 下行链全驱动 2 用例（`inbox_downlink_coverage_test.cpp`——transport 胶水 tcpListen + tcpSendTo 推帧驱动 client IO 线程：handleMessage 三类型分发与坏 JSON 异常、handleInboxMessage 入队/上限拒绝（300ms 定格防竞态）/空 msgId 防御、consume 五类型转换、ack/report 生命周期、connect 复用同 handle）+ crypt KDF 失败分支 1 用例（iter=0 使 EVP_KDF_derive ≤0，唯一可确定性触达的错误分支）。补测实测暴露并修复两处真实缺陷（inbox 下行 timestamp 恒 0——nlohmann value() 类型严格匹配静默回落默认值；整型 payload 走 fromFloat 后 asInt() 恒 0——ScriptValue 数值语义，见 CHANGELOG fix）。**87.9% → 88.4% 行（13105 行，v11 基线，全量 1933 用例：1893 PASSED + 40 环境性 skip）；inbox_module 72.1% → 95.0%（35 函数 100%）、crypt 70.5% → 72.0%**。不可覆盖论证：setMessageCallback 无胶水入口、sendRegister 失败回滚为 RST 时序竞态不可控、crypt 其余 miss 全为 OpenSSL 内部失败防御。环境事故：首测宿主重启后 Xvfb :98 未恢复致 44 用例环境性 skip、假低 82.9%，恢复重测为准——**基线采集前必须验证 X 环境存活**。
- [x] C++ 第八批补测（2026-09-23）：db_module 深度 23 用例（`db_deep_coverage_test.cpp`——目录陷阱触发 open CANTOPEN、closed 连接全拒绝、坏 SQL 全入口、嵌套事务防御、注入防护矩阵、query builder 非法参数、connection/table/query 伪造句柄七函数拒绝、table_close/query_close 生命周期、8 轮 create-close 无泄漏）+ ini 分支 13 用例（`ini_branch_coverage_test.cpp`——畸形行容错、非法转义、非法 key/section 名警告、转义矩阵往返、get/set/delete/has_*/sections/keys/merge 全防御分支）+ tcp_channel 真实 socket 错误路径 12 用例（`tcp_channel_e2e_coverage_test.cpp`，POSIX gate——server 非法地址/端口占用/accept 中断、client 重试耗尽 ≥5s、0 长度/超长 11MB/对端消失帧攻击、坏 JSON 后存活、payload 二态、RST 后 send 失败）。补测实测暴露并修复 db_module 两处注册表泄漏（g_queries/g_tables 只增不删，新增 table_close/query_close 胶水）+ extractTable 裸 cast UAF 防护缺失（对齐三处注册表校验）+ 死代码删除 88 行（Stmt 移动语义/Unlocked 三件套/closeAllConnections，见 CHANGELOG fix）。**88.4% → 89.9% 行（13090 行，miss 1515 → 1324，v12 基线，全量 1981 用例：1941 PASSED + 40 环境性 skip）；db_module 81.97% → 94.81%、ini_module 83.96% → 95.15%、tcp_channel 77.19% → 95.06%**。不可覆盖论证：db 剩余 45 行为 sqlite3 内部失败防御与环境回退、tcp 剩余 13 行为 socket()/listen() 资源耗尽与发送缓冲满时序、ini 剩余为逻辑不可达 + 签名常量 gcov 伪影（v11 既有）。
- [x] C++ 第九批补测（2026-09-23）：trigger RPC list 序列化 3 用例（`trigger_list_serialization_test.cpp`——既有 list 只见过 ColorFound+Click 单一组合，11 种 condition/10 种 action 全枚举 + 非法枚举 999 入库走 switch 兜底回退）+ timer 胶水 4 用例（setTimeout/setInterval/cancel/clearTimeout/clearInterval——既有只测 after/every 老入口）+ system 胶水 4 用例（getCpuUsage 值域、getDiskInfo 带参、**fake-xrandr PATH 注入驱动 getDisplayInfo 解析链**——宿主无 xrandr 该链自项目创建零覆盖、getNetworkAdapters）+ human setConfig 通用入口 1 用例（move_speed/typing_variance——既有只走专用函数）。顺带修复在案 flaky：TriggerPosixCoverageTest.InputActionsDriveMockInput 固定 sleep 高负载击穿（v13 全量实测 0 触发），改条件轮询并加固同文件另 2 处同型断言（见 CHANGELOG）。**89.9% → 90.4% 行（miss 1324 → 1252，v13 基线，全量 1993 用例：1953 PASSED + 40 环境性 skip）、函数 94.4% → 94.7%；trigger_handler 86.89% → 98.36%、timer_module 84.31% → 93.46%、system_module 84.48% → 92.24%、posix_system 89.03% → 98.39%、human_module 88.41%（剩余全为签名尾行伪影）**。不可覆盖论证：kv_module 剩余 21 行全为签名常量 gcov 归属伪影（与 ini 同现象）、posix_system 剩余 5 行为 popen/getifaddrs 失败与 macOS 兜底、trigger_handler 剩余 3 行为跨行表达式与循环闭合伪影。
- [x] C++ v13 基线收口（2026-09-30，扫描收官）：剩余缺口三分类（可测 now / 结构性不可达逐行论证 / 行归因伪影）清账，第十批（Clipboard 门面/X11 后端故障注入 3 例）与收官批（13 例 + 1 断言）落地后 **TOTAL 90%（15290/16931），可测缺口归零**。逐文件终态：x11_recorder 73%（73 行 miss——XRecord 正向链路需真实桌面，verify-xrecord-desktop.sh 真机脚本覆盖）；clipboard.cpp 58%（26 行 = NullClipboard 类体 + 工厂 null 兜底，恒不可达）；ml_module 67%（34 行——onnxruntime 为 vcpkg Windows 平台专属依赖，Linux gate 恒跑 ml_stub）；x11_clipboard 84%（20 行 = fork 子进程行 gcov 永不落盘 + 收尾行伪影）；x11_window 99%（1 行收尾伪影）；unix_socket_channel 97%（6 行 listen() 失败防御 + 1 行收尾伪影）；transport_module 90%（17 行结构性——阻塞 receive_from catch / 工厂永不失败防御 / start() 恒 true + 13 行 braced-init 伪影，其中 4 行错误文案被既有用例直接断言，重跑用例 miss 不变证伪「陈旧 gcda」）；notify 75%（52 行 WebhookSender 配置性死代码，v10 论证在案）；script_manager 91%（46 行既有登记）；misc_modules 77%（99 行 UIA 段已论证排除）；crypt 75%（73 行 OpenSSL 内部失败防御，不可确定性触发）；debugger/orchestration/security 胶水体已全驱动（余量 15 行全为 `}, "sig"});` 收尾行伪影）；Windows/macOS 平台实现真机专属逐条论证。**C++ 覆盖率扫描任务至此收官**（全量 2509 注册 = 2478 passed + 31 环境 skip + 0 failed）。

---

## 下一步计划

### 短期 (1-2周)
- [x] 修复 CI 构建错误（Protobuf 枚举冲突、条件编译）
- [x] 完善 UIA 文档和示例
- [x] 实现 UIA 事件监听器
- [x] 添加更多 UIA 控件类型支持

### 中期 (1-2月)
- [ ] 性能基准测试和优化
    - [x] 基准测试面已落地（2026-10-13，见上「待规划」条目与 docs/benchmarks.md）；优化半场待按基线数据驱动
    - [x] 优化首增量（2026-10-15）：10-14 复测揭示 findImage 双路径——桌面走 OpenCV TM_CCOEFF_NORMED，而 Android 端 screen/vision.findImage 与 trigger 图像条件走 `PatternMatcher::templateMatch` 朴素四重循环（全屏 1600x1000 vs 32x32 达 18.1 s/op，触发器 500ms 轮询下失能）。常数因子优化（模板范数外提 + 亮度面预计算 + 行指针访问，累加顺序逐项保持）→ 全屏 2.49 s/op（7.3x）、400x300 区域 162 ms/op（9.1x），输出位级不变（黄金值回归钉 TemplateMatchGoldenBits/FindImageGoldenBits 前后同值）；新增 matcher_bench（合成帧无显示面依赖）。剩余提速（SIMD/金字塔/端侧 OpenCV 匹配器）均改数值或命中语义，属设计取舍待拍板，见 docs/benchmarks.md 优化记录
    - [x] 优化第二增量（2026-10-15）：`templateMatch` 按行分块并行——每个搜索位置的 sum/sumTarget 相互独立，块内保持原扫描序、块间按块序以严格 `>` 归约，故输出与串行 IEEE-754 逐位一致且「首个最大位置优先」的平局次序不变（位级合法的第 5 类优化，绕开首增量登记的所有改语义门槛）。自动档阈值=扫描行 ≥64 且位置 ≥16384、线程数取 `hardware_concurrency()`；`WINGMAN_MATCHER_THREADS` 强制覆盖（0/1=串行、N≥2 绕过阈值）。14 逻辑核本机全屏 2,440–2,488 → 464–590 ms/op（4.8x）、400x300 区域 155–161 → 34–37 ms/op（4.4x）；单核自动档退化串行不适用。回归钉新增 TemplateMatchParallelBitExact（强制 2/3/5/8 + 自动档 × 全屏/区域两形态与串行基准 EXPECT_EQ 位级）；环境变量表按对账补 `WINGMAN_MATCHER_THREADS` 一行。受影响路径 28 套件 251 例（246 passed / 5 skip / 0 failed）
- [ ] 用户文档完善
    - [x] 脚本 API 面对账三批（core.md 全页对账、benchmark 基线同步、API 名/签名/返回值口径清理）+ 签名级对账零漂移（六模块 lambda 体验证，见上）
    - [x] README/结构文档对账（2026-10-15）：脚本模块计数四份口径互不一致（README/README.zh 各两处 40 与 41、docs/project-structure.md 41 与「41 个 ModuleDescriptor」），真值 42——注册表恰 42 个 `modules.push_back` / 42 个 create*Module() / 42 个唯一 `mod.name`，自 2026-10-13 未再变动；10-08 审计表 A-D05 记「实际 41、统一改 41」系时点快照（`132eb09` 加 systemwatch 后 41→42），此后无一处按真值更新。已全仓统一 42。附带纠偏：project-structure.md 示例模块名 `trigger` 查无此模块（脚本面触发器为 `wingman.smarttrigger`，module_registry.cpp 无 trigger 描述符）；`本地测试口径` 段原被 `f9f962e` 误插进 README 安装参数 bash 代码栏（GitHub 渲染为 bash 注释、中文段混入英文 README），已迁回 CONTRIBUTING.md「构建与验证」节（该文件原有按路径检查表、无三路分流约定）
    - [x] 目录改名残留+示例路径对账（2026-10-15 第二批）：`43bc328` 把 apps/runtime 改名 apps/agent 后，六份活文档目录树仍留旧名共 7 处（project-structure.md 树图+`#### runtime/` 小节头、architecture.md、guide/architecture.md、guide/structure.md、DEVELOPMENT.md、ROADMAP.md），README 双语 Quick Start 示例路径 `examples\hello.lua` 实不存在（真路径 examples/lua_scripts/hello.lua，getting-started/index.md 均已用对）——已全改（commit 5434055，docs:build 绿 113.83s）；CHANGELOG/审计台账里的 apps/runtime 字样系历史证据按约定不重写。同批兜底：审计-文档一致性.md 243 行仅 2 行带销行标记（整改未回填、属静态发现清单），按页归组抽 27 行复核（P0/P1/P2 点名 15 行 + api 页分层 12 行）全部为现行态；初判仍漂移的 API.md「26 个方法」细看已修——:97 注记明确 trigger.* 不在本地 IPC 面，26=apps/agent 注册面、protocols.md「31 个唯一方法」=跨两注册面合计，两口径并行不矛盾
    - [x] 实现面路由核验（2026-10-15 第四批，由优化第二增量顺带核出）：`docs/android-agent.md` A2 摘要原写「找色找图（ImageAnalyzer + OpenCV 进 NDK）」——源码核验证伪：`ImageAnalyzer::findImage` 直连 `PatternMatcher::templateMatch`（`lib/wingman/src/vision/image_analyzer.cpp`，纯 C++ 不经 OpenCV），Android 脚本面 `screen.findImage`/`vision.findImage` 经 `libs/androidagent/src/android_script_api.cpp:230,363` 走 `ImageAnalyzer`、`android_trigger.cpp:211` 触发器图像条件同走 `ImageAnalyzer`——NDK 的 OpenCV 仅在 `apps/android/cpp/agent/android_screenshot.cpp` 用于截图 JPEG 编码（`cv::cvtColor` + `cv::imencode`），与找色找图无关。桌面同名 API 走 `Vision::findImage`/`Screen::findImage` 的 `cv::matchTemplate`（OpenCV），两端分数口径不同——已改 android-agent.md 并注明口径差异（commit 8368b59，docs:build 绿 109.30s）。附带核出 `run-benchmarks.sh` 机器上下文未记 matcher 并行度（读数随 `WINGMAN_MATCHER_THREADS` 变，无数值可复现性），补记一行（commit 2d084ec）
    - [x] 示例配置 schema 对账（2026-10-15 第九批，修三处真漂移）：examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响（commit d522dd3）docs 根 24 页 + guide 页全部反引号路径以多根解析（repo 根/orchestrator{,server,dashboard,dashboard/src}/lib/wingman/apps{agent,gui,android,android/cpp}/libs/examples/docs）核验。①`android-agent.md` 环境表「工具链 `scripts/buildsystems/vcpkg.cmake`」为仓内不存在的假路径（scripts/ 下无任何 .cmake）——真值：NDK 构建经仓内 `apps/android/cpp/vcpkg-android.cmake` 包装，gradle 传 `-DWINGMAN_VCPKG_ROOT=<vcpkg 根>`，`scripts/buildsystems/vcpkg.cmake` 属 vcpkg 自带路径（`apps/android/cpp/CMakeLists.txt:4`、`build.gradle.kts:37-38`、`vcpkg-android.cmake:27`）——已改并注明归属。②`dependencies.md` 「`cpp/vcpkg-android.cmake`」缺 apps/android 前缀（仓根无 cpp/）——补全为 `apps/android/cpp/vcpkg-android.cmake`。③非缺陷逐条核销：DEVELOPER_TOOLS.md `luarocks/` = install 脚本安装目标（`install-luarocks.cmd` 实存）、DEVELOPMENT.md `build-tests/` = 示例命令建议目录名、architecture-decisions.md `apps/runtime` = 2026-10-09 改名历史记录、design 文档 `internal/…`/`platform/<os>/`/`services/…` = 模块根相对设计正文、architecture.md `/stop-script` = IPC 方法名、project-structure.md `xxx.hpp` = 占位。**连带登记**：`build-scripts/` 24 文件正被并行席位在途删除（HEAD 仍跟踪），届时 development-environment.md 2 处、guide/structure.md 3 处、guide/runtime-gui.md 3 处引用同批成漂，待其落地后一起改，本席不抢examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响
    - [x] 顶层文档路径/工具链声明对账（2026-10-15 第十批，修两处真漂移）：docs 根 24 页 + guide 页全部反引号路径以多根解析（repo 根/orchestrator{,server,dashboard,dashboard/src}/lib/wingman/apps{agent,gui,android,android/cpp}/libs/examples/docs）核验。①`android-agent.md` 环境表「工具链 `scripts/buildsystems/vcpkg.cmake`」为仓内不存在的假路径（scripts/ 下无任何 .cmake）——真值：NDK 构建经仓内 `apps/android/cpp/vcpkg-android.cmake` 包装，gradle 传 `-DWINGMAN_VCPKG_ROOT=<vcpkg 根>`，`scripts/buildsystems/vcpkg.cmake` 属 vcpkg 自带路径（`apps/android/cpp/CMakeLists.txt:4`、`build.gradle.kts:37-38`、`vcpkg-android.cmake:27`）——已改并注明归属。②`dependencies.md`「`cpp/vcpkg-android.cmake`」缺 apps/android 前缀（仓根无 cpp/）——补全为 `apps/android/cpp/vcpkg-android.cmake`。③非缺陷逐条核销：DEVELOPER_TOOLS.md `luarocks/` = install 脚本安装目标（`install-luarocks.cmd` 实存）、DEVELOPMENT.md `build-tests/` = 示例命令建议目录名、architecture-decisions.md `apps/runtime` = 2026-10-09 改名历史记录、design 文档 `internal/…`/`platform/<os>/`/`services/…` = 模块根相对设计正文、architecture.md `/stop-script` = IPC 方法名、project-structure.md `xxx.hpp` = 占位。**连带登记**：`build-scripts/` 24 文件正被并行席位在途删除（HEAD 仍跟踪），届时 development-environment.md 2 处、guide/structure.md 3 处、guide/runtime-gui.md 3 处引用同批成漂，待其落地后一起改，本席不抢
    - [x] 示例与文档代码块 API 面对账（2026-10-15 第八批）：examples/ 全部 33 个脚本（29 lua + 其余）+ docs 全部 82 个含 ```lua/```python 栅栏的文件，抽 `wingman.mod.func(` 直接调用与 `local X = wingman.mod` 局部别名解析后的 `X.func(` 调用，对**双端注册面**核验——桌面面 = `lib/wingman/src/script/modules/*.cpp` 的 `functions.push_back` + Python camelToSnake 别名，Android 面 = `libs/androidagent/src/android_script_api.cpp` 的 `set_function`（input.{click,tap,longPress,swipe,delay}、screen.*、vision.*、trigger.{add,start,stop,remove,startAll,stopAll,isRunning,getTriggerCount,count}）。**零死调用**。首轮 8 个候选死 API（android-agent-design.md:285 `wingman.input.swipe`、343/358-63 `wingman.trigger.*` 六个）经核全部为 Android 面真实注册（swipe 在 android_script_api.cpp:103、trigger 族 :509-637）——首轮误报系比对面只含桌面注册、漏 Android 面所致，比对面补齐后全清。AUDIT-REPORT.md 等台账历史文本不计入。本批零修复，仅记台账；附带 examples 全部 29 个 lua 脚本 `luac -p`（5.4.8）语法核验 0 失败examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——示例与文档互相矛盾且都非真值，删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响examples/ 全部 33 个脚本（29 lua + 其余）+ docs 全部 82 个含 ```lua/```python 栅栏的文件，抽 `wingman.mod.func(` 直接调用与 `local X = wingman.mod` 局部别名解析后的 `X.func(` 调用，对**双端注册面**核验——桌面面 = `lib/wingman/src/script/modules/*.cpp` 的 `functions.push_back` + Python camelToSnake 别名，Android 面 = `libs/androidagent/src/android_script_api.cpp` 的 `set_function`（input.{click,tap,longPress,swipe,delay}、screen.*、vision.*、trigger.{add,start,stop,remove,startAll,stopAll,isRunning,getTriggerCount,count}）。**零死调用**。首轮 8 个候选死 API（android-agent-design.md:285 `wingman.input.swipe`、343/358-363 `wingman.trigger.*` 六个）经核全部为 Android 面真实注册（swipe 在 android_script_api.cpp:103、trigger 族 :509-637）——首轮误报系比对面只含桌面注册、漏 Android 面所致，比对面补齐后全清。AUDIT-REPORT.md 等台账历史文本不计入。本批零修复，仅记台账；附带 examples 全部 29 个 lua 脚本 `luac -p`（5.4.8）语法核验 0 失败
    - [x] guide 面对账（2026-10-15 第七批）：docs/guide/ 9 页可对齐声明（环境变量/路径/命令）对源码核验。①`config.md` 环境变量节「当前真实生效的环境变量」未限定范围——真值面共 20 个：客户端/运行时侧恰为表列之 4（CONFIG_DIR/PACK_PASSWORD/SCRIPT_PASSWORD/MATCHER_THREADS，`lib/wingman/src` 仅两处 getenv + `apps/agent/src` 三处），Go server 部署面 16 个集中读自 `orchestrator/server/internal/config/config.go`（JWT_SECRET/CORS_ORIGINS 在 middleware 亦按请求读取），全文档站无一处汇总清单（README/design 文档仅按主题零散）——已把声明限定为客户端/运行时侧并补 server 部署面指向注记（读取点/数量/主题文档）。②路径面 34 条反引号路径逐条存在性核对：30 条实存或为已锚定简写（architecture.md:255 表头明示「文件（`lib/wingman/src/`）」、structure.md:115 锚在 `### lib/wingman` 节、dashboard.md 锚在 `cd orchestrator/dashboard` 上下文）；runtime-gui.md:168 `images/target.png` 为触发器条件示例值非仓内路径。③**登记不抢修**：runtime-gui.md:26/33/36 三处 `build-msvc-ninja-vcpkg/apps/agent/wingman-agent.exe` 引用旧构建脚本目录名——build-scripts/ 24 文件正被并行席位在途删除（工作区未提交态），新构建目录约定未定，待其落地后同批对齐，不抢改
    - [x] 全页面签名级对账（2026-10-15 第六批）：46 个 api 页 × 42 模块 × 413 注册函数全量比对，**零漂移**。方法：注册面 = 39 个模块源文件 `functions.push_back`（ModuleDescriptor 仅 name+functions 两字段，无其他注册形状）；文档面 = `###` 函数标题 + 表格行反引号内联函数名（timer/db/uia/types 的可用接口表是第二文档面，只扫标题会误报漏档）。首轮 64 个候选假 API 逐类核销：① Python 引擎 camelToSnake 别名层（`libs/python/src/python_script_engine.cpp:386`，模块函数双注册 camelCase + snake_case）——system/security/perf/ocr/ml/kv/screen/process/filewatcher/human 等 10 页 43 个 snake 形名全部合法；② 模块限定名 `crypto.xxx`/`ini.xxx`/`script.xxx` 标题与表格 = 真名展示形；③ `db.md` ORM/QueryBuilder 标题为方法视图（`table.create(schema)`），示例与可用接口表均教真实扁平调用 `db.table_create(users, …)`；④ uia 元素方法（click/double_click/get_info/expand/is_enabled 等 18 个）= `makeUiaElementObject` 闭包（`misc_modules.cpp:379`），types.md 的 UIElement 方法表与之一致；⑤ timer.md 可用接口表 12 项含 setTimeout/setInterval/cancel 等 JS 风格别名与源码注释逐一对应；⑥ AUDIT-REPORT.md 命中项为台账自身历史文本非 API 面。无假 API、无漏档，本批零代码零文档修复，仅记台账
    - [x] CHANGELOG 补账（2026-10-15 第五批）：Unreleased 段停在 10-13，10-14/15 全部工作未记录——补四条目：perf（模板匹配两轮 18.1s→0.5s：常数因子 fd455bb 位级不变 7.3x/9.1x + 按行分块并行 1a472e9 位级不变 4.8x/4.4x + `WINGMAN_MATCHER_THREADS` 与回归钉，对账面 config.md/benchmarks.md/run-benchmarks.sh 一并列明）、feat（Android 端 asset.sync 接面 006aa09——兑现 10-13 条目自注「Android 端侧接面为后续增量」，Dispatcher Reuse 远程通道与桌面 agent.cpp 同形，3 例通道测试）、docs（本批三批对账：9bb54b1 计数 42 / 5434055 改名残留+坏路径 / 8368b59 找图路径归属证伪）、docs（10-14/15 五批：838c2a0 core.md 全页对账、c134b1a+27b2ce7 API 示例同步、9cbb8d6 基线复测与 10-13 结论修正、0e49af5 语言切换器）。条目事实逐条回源码核验（两路径事实归属按 `git log -S` 实证归 fd455bb 非 9cbb8d6，撰写中纠回一处误归因）（commit ca4caa0+9f923b2，docs:build 绿 54.31s/32.47s）

### 长期
- [ ] 视频教程制作
- [ ] 社区建设
- [ ] 发布 1.0 正式版

---

## 最近完成 (2024-05)

### Phase 13: 编译问题修复 ✅
- [x] 修复 Protobuf 枚举值冲突
  - `UNKNOWN` → `REQ_UNKNOWN` (RequestType)
  - `BUSY` → `ERR_BUSY` (ErrorCode)
- [x] 修复 lua_extensions.cpp 条件编译
  - 清理已废弃的旧 server 条件编译残留
  - 未构建 server 模块时使用存根类型

### Phase 14: UIA 功能增强 ✅
- [x] 实现 UIACondition 查找条件
- [x] 实现元素展开/折叠 (expand/collapse)
- [x] 实现选择项操作（2026-10-09：UIElement `select`（win SelectionItem.Select / mac AXSelected=true）+ `get_selection`（win Selection.GetCurrentSelection 首项 / mac AXSelectedRows/AXSelectedChildren 首项））
- [x] 实现高级查找方法（C++ 层 `find(selector)`/`findAllByRole`，脚本层暴露 find_by_name/find_by_id/find_all_by_control_type）
- [x] 添加 UIA 事件监听支持 (PropertyChangedEventHandler)
- [x] Lua 绑定更新（expand, collapse, isExpanded 及 get_parent/select/get_selection 已随 UIElement 对象方法落地）
- [x] 更新 UIA 文档

### Phase 15: WebSocket 和 Dashboard ✅
- [x] WebSocket 服务端实现
- [x] WebSocket 客户端服务
- [x] Agents 页面实时更新
- [x] Workflows 页面实时更新
- [x] Dashboard Croupier 内容清理
- [x] Welcome 页面更新为 Wingman 产品介绍

### Phase 16: Dashboard 构建修复 ✅
- [x] 创建缺失的 API 服务存根 (audit, messages, permissions, support, storage)
- [x] 修复 Welcome.tsx 语法错误 (level={5})
- [x] 修复 websocket.ts 重复导出 (WSMessageType)
- [x] 创建 PageStatePanel 组件
- [x] pnpm install 成功通过

### Phase 17: UIA 事件监听器 Lua 绑定 ✅
- [x] 实现 `uia.on_property_changed(name, callback)` - 属性变更事件
- [x] 实现 `uia.on_structure_changed(name, callback)` - 结构变更事件
- [x] 实现 `uia.remove_event_listener(listenerId)` - 移除监听器
- [x] 添加全局监听器注册表和清理机制
- [x] 修复 SmartTrigger 日志语法错误
- [x] 更新 todo.md 确认触发器系统 Lua 函数执行和日志输出已完成

### Phase 18: UIA 文档更新 ✅
- [x] 更新 `docs/api/uia/index.md` 添加事件监听器 API 文档
- [x] 更新 `docs/examples/ui-automation.md` 添加事件监听器示例
- [x] 添加监听对话框自动响应示例
- [x] 添加监听内容变化示例

### Phase 19: UIA 控件类型支持扩展

> 2026-10-09 全量落地：脚本层 find* 共 21 个函数 + 等待族 3 个（wait_for_id/wait_for_role/wait_for selector）；
> UIARole 枚举扩至 21 项（14-20 追加 Hyperlink/Image/Slider/Spinner/ProgressBar/Tab/TabItem），
> Windows 后端 controlTypeToRole/findAllByRole 双向映射补全（此前 ListBox/ListItem/Menu/MenuItem/Table/
> Tree/TreeItem 读回恒 Unknown，find_list/find_tree 等在 Windows 上匹配不到——已修），
> macOS 后端 AX 角色串映射补全（AXList/AXMenu/AXMenuItem/AXTable/AXOutline/AXImage/AXSlider/
> AXProgressIndicator/AXTabGroup + 字面量 AXLink/AXIncrementor/AXRow）。

- [x] 添加 `findCheckBox(name)` - 查找复选框（2026-10-09：脚本层 `find_check_box`，CheckBox 角色过滤）
- [x] 添加 `findRadioButton(name)` - 查找单选按钮（2026-10-09：`find_radio_button`，RadioButton 角色）
- [x] 添加 `findComboBox(name)` - 查找下拉框（2026-10-09：`find_combo_box`，ComboBox 角色）
- [x] 添加 `findList(name)` - 查找列表（2026-10-09：`find_list`，ListBox 角色）
- [x] 添加 `findListItem(name)` - 查找列表项（2026-10-09：`find_list_item`，ListItem 角色）
- [x] 添加 `findTab(name)` / `findTabItem(name)` - 查找标签页（2026-10-09：`find_tab`/`find_tab_item`，Tab/TabItem 角色新增）
- [x] 添加 `findTree(name)` / `findTreeItem(name)` - 查找树形控件（2026-10-09：`find_tree`/`find_tree_item`，Tree/TreeItem 角色）
- [x] 添加 `findMenuItem(name)` - 查找菜单项（2026-10-09：`find_menu_item`，MenuItem 角色）
- [x] 添加 `findHyperlink(name)` - 查找超链接（2026-10-09：`find_hyperlink`，Hyperlink 角色新增）
- [x] 添加 `findImage(name)` - 查找图像（2026-10-09：`find_image`，Image 角色新增）
- [x] 添加 `findSlider(name)` - 查找滑块（2026-10-09：`find_slider`，Slider 角色新增）
- [x] 添加 `findSpinner(name)` - 查找微调器（2026-10-09：`find_spinner`，Spinner 角色新增）
- [x] 添加 `findProgressBar(name)` - 查找进度条（2026-10-09：`find_progress_bar`，ProgressBar 角色新增）
