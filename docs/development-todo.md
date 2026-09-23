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
> event/fsm/task/notify 均已落地并有完整测试（各 38/44/42/39 条用例）与 typing 文件，
> 此前清单未同步勾选。orchestration 已有基础工作流 API。timer 已于 2026-09-19 落地
> （timer_module.cpp + timer.pyi + 12 条测试）。仍缺：hotkey 模块、文件 IO 工具、
> notify tray、事件按名清理与监听器查询、task pause/resume。

### 脚本与运行时
- [ ] 统一 Lua / Python API 形状与文档
- [x] Python typing 基础包已提供
- [x] 补齐事件/状态机/任务/通知模块的 typing 文件（event/fsm/task/notify/orchestration 等 37 个 .pyi 齐全）
- [ ] 统一模块命名和函数命名风格
- [x] 所有 Python 公开 API 统一到 `wingman.*`（typing 命名空间 + module_registry 统一注册）

### 事件与状态
- [x] `wingman.event`（event_module.cpp + event.pyi + 38 条测试）
  - [x] `on(name, handler)` 注册持久监听
  - [x] `once(name, handler)` 注册一次性监听
  - [x] `off(id | name)` 按订阅 ID 或名称取消
  - [x] `emit(name, payload?, meta?)` 触发事件
  - [ ] `listener(name)` / `listeners(name)` 查询监听器（未实现）
  - [ ] `clear(name?)` 清理全部或指定事件（现状仅 `clear()` 全量清理，无按名清理）
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
- [x] `wingman.task`（task_module.cpp + task.pyi + 42 条测试）
  - [x] `submit(fn | workflow, options?)`
  - [x] `cancel(taskId)`
  - [x] `status(taskId)` / `wait(taskId, timeout?)`
  - [x] `retry(taskId, options?)`（含 backoffMs/backoffFactor/maxRetries）
  - [ ] `pause(taskId)` / `resume(taskId)`（未实现）
  - [x] `result(taskId)` / `error(taskId)`
  - [x] 任务生命周期事件：`task.submitted/started/succeeded/failed/canceled/timeout`（pending/running/succeeded/failed/canceled 状态流转）
- [x] `wingman.notify`（notify_module.cpp + notify.pyi + 39 条测试）
  - [x] `info/warn/error/debug`
  - [x] `toast(title, message, level?)`
  - [x] `log(channel, message, meta?)`（notify.log + 事件化）
  - [x] `webhook(url, payload, options?)`（含 pending/success/failed/blocked 全生命周期事件）
  - [ ] `tray.show()/hide()/setBadge()`（未实现）
  - [ ] 订阅 `event.*` 与 `task.*` 的通知桥接（bridge 机制已有，自动桥接规则未完整接线）

### 编排与恢复
- [x] `wingman.orchestration`（基础版：orchestration_module.cpp + orchestration.pyi——submit/get/get_all/cancel_workflow）
  - [x] 工作流定义
  - [ ] 依赖关系
  - [ ] 并发控制
  - [ ] 条件分支
  - [ ] 子任务聚合
  - [ ] 流程级状态事件
- [x] 任务重试、超时、退避封装（task 模块 backoffMs/backoffFactor/maxRetries/timeoutMs）
- [ ] 任务状态持久化与恢复
- [ ] 事件订阅持久化与断线重连
- [ ] 统一回调/通知策略，避免 Lua 和 Python 语义分裂

### 常用工具补齐
- [x] 剪贴板模块（clipboard_module.cpp + clipboard.pyi）
- [ ] 文件系统模块（filewatcher 已提供文件变化监控；文件 IO 工具未实现）
- [ ] 热键监听模块（未实现）
- [x] 定时器 / 计划任务模块（timer_module.cpp + timer.pyi + 12 条测试；after/every/取消/查询/sleep）
- [ ] 更完整的 UI 控件树遍历与等待（uia 模块已有树遍历基础，等待类 API 待补）
- [ ] 图像模板批量管理与识别
- [x] 录制 / 回放闭环（macro_module.cpp）
- [ ] UIA 事件统一抽象
- [ ] 进程/窗口/文件变化统一事件源（文件变化已有 filewatcher；进程/窗口事件源未实现）

### 优先级建议
- [x] P0: `event`、`task`、`fsm`
- [x] P1: `notify`、`orchestration`
- [ ] P2: `clipboard` ✅、`file`（监控 ✅ / IO ❌）、`hotkey` ❌、`timer` ✅
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

### A1 PoC：链路打通 ✅（2026-09-22 校准，代码已落地）
- [x] Kotlin 壳：ForegroundService + 长链接（`WingmanService.kt`：前台服务 + START_STICKY + dataSync|mediaProjection 类型；复用 libs/transport 编译到 Android）
- [x] wingman 核心 NDK 编译通过（Lua + transport + 核心库；vcpkg 扩展 arm64-android triplet；nightly Android arm64 job 全绿，2026-09-22）
- [x] Go Server 下发脚本 → 端侧执行（`android_agent.cpp` 支持 run_script/stop_script/screenshot.capture/system.shutdown 四命令；日志经 agent.event 上行回传 Dashboard）

### A2 能力闭环（2026-09-22 校准：IInput/ICapture/脚本 API 已落地，余触发器与真机验证）
- [x] `platform/android/` IInput 后端（AccessibilityService dispatchGesture，经 JNI；`WingmanAccessibilityService.kt` 主线程 post + seq/promise 回执，超时兜底在 C++）
- [x] `platform/android/` ICapture 后端（MediaProjection 主通道；`ScreenCaptureManager.kt` + AndroidHostBridge::captureFrame。⚠️ takeScreenshot API 30+ 兜底未做）
- [x] 找色/找图/像素检测对手机截帧可用；screen/input 脚本 API 全通（input click/tap/longPress/swipe/delay + screen getScreenWidth/Height/capture/getPixel/findColor/findColors/findImage + vision 五函数；桌面同源编译验证：android_api_test 6 + script_runner_test 8 用例。⚠️ 真机触摸/截帧行为待真机验证）
- [ ] 触发器系统端侧跑通（定时/像素触发）

### A3 可靠性与部署体验
- [ ] 开机自启、崩溃自重启、断连自治（缓存脚本继续执行、重连后汇报）
- [ ] Android 13+ 受限设置引导（手动允许 / adb 预授权 / Device Owner 批量部署）
- [ ] 模板图片 asset.sync 下发、脚本版本管理

### A4 多设备编排
- [ ] Team/inbox 模块接入端侧 Agent
- [ ] Dashboard 设备视图（分组、批量下发、状态大盘）

### 远期可选
- [ ] I1: iOS 主机控——PC 端 ICapture/IInput 的 WDA 后端（usbmuxd/libimobiledevice，依赖 Mac 签名链）
- [ ] 端侧 Python 引擎评估（Lua-first，Python+NDK 体积/维护成本高，暂缓）

---

## Phase 1: 核心引擎 (C++) ✅

### 1.1 屏幕操作模块 ✅
- [x] `screen.capture()` - 截取屏幕/窗口
- [x] `screen.getPixel(x, y)` - 获取单点像素
- [x] `screen.findColor(color, x1, y1, x2, y2, tolerance)` - 单点颜色查找
- [x] `screen.findColors(color, x1, y1, x2, y2, tolerance, count)` - 多点颜色查找
- [x] `screen.findImage(imagePath, x1, y1, x2, y2, threshold)` - 图像匹配
- [x] `screen.getWindowTitle(hwnd)` - 获取窗口标题
- [x] `screen.getWindowBounds(hwnd)` - 获取窗口位置

### 1.2 输入模拟模块 ✅
- [x] `input.click(x, y, button)` - 鼠标点击
- [x] `input.move(x, y, duration)` - 鼠标移动（贝塞尔曲线）
- [x] `input.scroll(x, y, delta)` - 鼠标滚轮
- [x] `input.keyDown(key)` - 按键按下
- [x] `input.keyUp(key)` - 按键释放
- [x] `input.type(text, delay)` - 文本输入

### 1.3 窗口管理模块 ✅
- [x] `window.find(title)` - 查找窗口
- [x] `window.activate(hwnd)` - 激活窗口
- [x] `window.getBounds(hwnd)` - 获取窗口位置
- [x] `window.getTitle(hwnd)` - 获取窗口标题
- [x] `window.getForeground()` - 获取前台窗口
- [x] `window.waitFor(title)` - 等待窗口出现

### 1.4 进程管理模块 ✅
- [x] `process.find(name)` - 查找进程
- [x] `process.start(path, args)` - 启动进程
- [x] `process.wait(pid)` - 等待进程
- [x] `process.terminate(pid)` - 终止进程
- [x] `process.exists(pid)` - 检查进程存在
- [x] `process.waitFor(name)` - 等待进程启动

### 1.5 人性化模拟模块 ✅
- [x] `input.move(x, y, duration)` - 贝塞尔曲线移动
- [x] `input.randomDelay(min, max)` - 随机延迟
- [x] 集成在 input 模块中

### 1.6 Lua 绑定 ✅
- [x] 暴露所有 C++ API 到 Lua
- [x] Lua 集成
- [x] 错误处理和异常转换
- [x] HTTP 客户端绑定
- [x] JSON 封装绑定
- [x] KV 存储绑定
- [x] UI Automation 绑定

---

## Phase 2: 触发器系统 ✅

### 2.1 触发器类型 ✅
- [x] 像素触发器 - 检测到指定颜色/图像
- [x] 定时触发器 - 间隔执行
- [x] 时间触发器 - 指定时间执行
- [x] 窗口触发器 - 窗口出现/消失
- [x] 进程触发器 - 进程启动/停止
- [x] 像素变化触发器
- [ ] 触发器统一接入 `wingman.event`

### 2.2 触发器动作 ✅
- [x] 发送按键
- [x] 鼠标操作
- [x] 显示消息
- [x] 播放声音
- [x] 执行 Lua 函数 (RunScript 动作)
- [x] 日志输出 (Log 动作)
- [ ] 触发器动作事件通知

---

## Phase 3: 宏系统 ✅

### 3.1 录制功能 ✅
- [x] `macro.startRecording()` - 开始录制
- [x] `macro.stopRecording()` - 停止录制
- [x] 记录鼠标移动/点击
- [x] 记录键盘输入
- [x] 时间戳记录

### 3.2 回放功能 ✅
- [x] `macro.play(name)` - 播放宏
- [x] `macro.save(name, path)` - 保存宏
- [x] 回放速度控制
- [x] 循环播放
- [ ] 宏录制事件流导出
- [ ] 宏回放状态事件

---

## Phase 4: 调试工具 🚧

### 6.1 VS Code 开发环境
- [x] 使用 EmmyLua 插件提供语法高亮、自动完成、悬停提示
- [x] 使用 EmmyLuaDebugger 提供断点调试
- [x] 项目配置 (.vscode/settings.json, launch.json)
- [x] Lua 库路径配置 (Lua.library)

### 6.2 日志系统
- [x] 分级日志 (DEBUG/INFO/WARN/ERROR) - 使用 spdlog
- [x] 文件输出
- [x] 控制台输出
- [ ] 性能统计
- [ ] 事件通知桥接

---

## Phase 5: 高级功能 ✅

### 7.1 脚本管理 ✅
- [x] 脚本热加载
- [x] 配置文件解析
- [x] 环境变量支持
- [x] 脚本沙箱
- [x] 任务状态机（task_module：pending/running/succeeded/failed/canceled 状态流转，2026-09-18 核对）
- [x] 工作流编排（orchestration_module：submit/get/get_all/cancel_workflow，2026-09-18 核对）

### 7.2 性能优化 ✅
- [x] 像素检测加速 - 使用 OpenCV
- [x] 图像匹配缓存 - LRU 缓存机制
- [x] 多线程处理 - OpenCV 并行
- [x] 图像金字塔加速
- [x] 内存优化 (智能缓存管理)

### 7.3 安全特性 ✅
- [x] 代码签名 (验证支持)
- [x] 进程保护 (反调试、反VM)
- [x] 反检测机制 (随机延迟、点击抖动)
- [x] 混淆支持 (字符串加密、哈希)

---

## Phase 6: 文档和示例 ✅

### 8.1 文档 ✅
- [x] API 参考文档
- [x] 快速入门指南
- [x] UI Automation API 文档
- [x] Window API 文档
- [x] Process API 文档
- [ ] 视频教程

### 8.2 示例脚本 ✅
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

## Phase 9: UI Automation (新增) ✅

### 9.1 核心功能 ✅
- [x] UI Automation COM 接口集成
- [x] 元素查找 (byName, byId, byControlType)
- [x] 元素操作 (click, setValue, getValue)
- [x] 元素遍历 (getChildren, getParent)
- [x] 元素等待 (waitFor)

### 9.2 Lua 绑定 ✅
- [x] `uia.fromForeground()` - 获取前台窗口根元素
- [x] `uia.fromPoint(x, y)` - 从坐标获取元素
- [x] `uia.fromWindow(hwnd)` - 从句柄获取元素
- [x] `uia.findButton(name)` - 查找按钮
- [x] `uia.findEdit(name)` - 查找编辑框
- [x] `uia.findText(name)` - 查找文本
- [x] `uia.findByName(name)` - 按名称查找
- [x] `uia.findById(id)` - 按 ID 查找
- [x] `uia.waitForName(name, timeout)` - 等待元素

### 9.3 UIElement 方法 ✅
- [x] `:click()` - 点击
- [x] `:rightClick()` - 右键点击
- [x] `:doubleClick()` - 双击
- [x] `:focus()` - 设置焦点
- [x] `:getValue()` - 获取值
- [x] `:setValue(value)` - 设置值
- [x] `:getName()` - 获取名称
- [x] `:getInfo()` - 获取完整信息
- [x] `:getChildren()` - 获取子元素

---

## Phase 7: TCP 协议增强 (可选)
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
- [ ] 更多示例脚本
- [ ] 性能基准测试
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
- [ ] C++ 生产代码继续补测（剩余缺口见 v13 基线明细）：x11_recorder 67.3%（91 行，XRecord 正向链路需真实桌面，已由 verify-xrecord-desktop.sh 真机脚本覆盖）、clipboard.cpp 58.1%（26 行）、ml_module 67.3%（34 行）、x11_clipboard 71.8%（37 行）、x11_window 45 miss、unix_socket_channel 36 miss、transport_module 34 miss（多为 client/server 创建失败注入难）、notify 51 miss、script_manager 53 miss、misc_modules 99 miss（UIA 段已论证排除）、debugger/orchestration/security 胶水零星段、crypt 剩余 73 行（OpenSSL 内部失败防御，不可确定性触发，论证在案）、Windows/macOS 平台实现（真机专属，逐条论证不硬凑）。

---

## 下一步计划

### 短期 (1-2周)
- [x] 修复 CI 构建错误（Protobuf 枚举冲突、条件编译）
- [x] 完善 UIA 文档和示例
- [x] 实现 UIA 事件监听器
- [x] 添加更多 UIA 控件类型支持

### 中期 (1-2月)
- [ ] 性能基准测试和优化
- [ ] 用户文档完善

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
- [x] 实现选择项操作 (selectItem/getSelection)
- [x] 实现高级查找方法 (find/findAll)
- [x] 添加 UIA 事件监听支持 (PropertyChangedEventHandler)
- [x] Lua 绑定更新 (getParent, expand, collapse, isExpanded, selectItem, getSelection)
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
- [x] 实现 `uia.onPropertyChanged(name, callback)` - 属性变更事件
- [x] 实现 `uia.onStructureChanged(name, callback)` - 结构变更事件
- [x] 实现 `uia.removeEventListener(listenerId)` - 移除监听器
- [x] 添加全局监听器注册表和清理机制
- [x] 修复 SmartTrigger 日志语法错误
- [x] 更新 todo.md 确认触发器系统 Lua 函数执行和日志输出已完成

### Phase 18: UIA 文档更新 ✅
- [x] 更新 `docs/api/uia.md` 添加事件监听器 API 文档
- [x] 更新 `docs/examples/ui-automation.md` 添加事件监听器示例
- [x] 添加监听对话框自动响应示例
- [x] 添加监听内容变化示例

### Phase 19: UIA 控件类型支持扩展 ✅
- [x] 添加 `findCheckBox(name)` - 查找复选框
- [x] 添加 `findRadioButton(name)` - 查找单选按钮
- [x] 添加 `findComboBox(name)` - 查找下拉框
- [x] 添加 `findList(name)` - 查找列表
- [x] 添加 `findListItem(name)` - 查找列表项
- [x] 添加 `findTab(name)` / `findTabItem(name)` - 查找标签页
- [x] 添加 `findTree(name)` / `findTreeItem(name)` - 查找树形控件
- [x] 添加 `findMenuItem(name)` - 查找菜单项
- [x] 添加 `findHyperlink(name)` - 查找超链接
- [x] 添加 `findImage(name)` - 查找图像
- [x] 添加 `findSlider(name)` - 查找滑块
- [x] 添加 `findSpinner(name)` - 查找微调器
- [x] 添加 `findProgressBar(name)` - 查找进度条
