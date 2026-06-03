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

### 脚本与运行时
- [ ] 统一 Lua / Python API 形状与文档
- [x] Python typing 基础包已提供
- [ ] 补齐事件/状态机/任务/通知模块的 typing 文件
- [ ] 统一模块命名和函数命名风格
- [ ] 所有 Python 公开 API 统一到 `wingman.*`

### 事件与状态
- [ ] `wingman.event`
  - [ ] `on(name, handler)` 注册持久监听
  - [ ] `once(name, handler)` 注册一次性监听
  - [ ] `off(id | name)` 按订阅 ID 或名称取消
  - [ ] `emit(name, payload?, meta?)` 触发事件
  - [ ] `listener(name)` / `listeners(name)` 查询监听器
  - [ ] `clear(name?)` 清理全部或指定事件
  - [ ] 事件对象统一字段：`name/type/source/correlationId/priority/timestamp/payload`
  - [ ] 预留桥接：脚本事件 -> 任务事件 -> 通知事件
- [ ] `wingman.fsm`
  - [ ] `create(name, initialState, options?)`
  - [ ] `addState(name, spec)`
  - [ ] `transition(from, to, guard?, action?)`
  - [ ] `onEnter(state, handler)` / `onExit(state, handler)`
  - [ ] `onEvent(eventName, handler)` 驱动状态转移
  - [ ] `dispatch(eventName, payload?)`
  - [ ] `getState()` / `setState()`
  - [ ] 状态变更自动发出 `fsm.changed`
- [ ] `wingman.task`
  - [ ] `submit(fn | workflow, options?)`
  - [ ] `cancel(taskId)`
  - [ ] `status(taskId)` / `wait(taskId, timeout?)`
  - [ ] `retry(taskId, options?)`
  - [ ] `pause(taskId)` / `resume(taskId)`
  - [ ] `result(taskId)` / `error(taskId)`
  - [ ] 任务生命周期事件：`task.submitted/started/succeeded/failed/canceled/timeout`
- [ ] `wingman.notify`
  - [ ] `info/warn/error/debug`
  - [ ] `toast(title, message, level?)`
  - [ ] `log(channel, message, meta?)`
  - [ ] `webhook(url, payload, options?)`
  - [ ] `tray.show()/hide()/setBadge()`
  - [ ] 订阅 `event.*` 与 `task.*` 的通知桥接

### 编排与恢复
- [ ] `wingman.orchestration`
  - [ ] 工作流定义
  - [ ] 依赖关系
  - [ ] 并发控制
  - [ ] 条件分支
  - [ ] 子任务聚合
  - [ ] 流程级状态事件
- [ ] 任务重试、超时、退避封装
- [ ] 任务状态持久化与恢复
- [ ] 事件订阅持久化与断线重连
- [ ] 统一回调/通知策略，避免 Lua 和 Python 语义分裂

### 常用工具补齐
- [ ] 剪贴板模块
- [ ] 文件系统模块
- [ ] 热键监听模块
- [ ] 定时器 / 计划任务模块
- [ ] 更完整的 UI 控件树遍历与等待
- [ ] 图像模板批量管理与识别
- [ ] 录制 / 回放闭环
- [ ] UIA 事件统一抽象
- [ ] 进程/窗口/文件变化统一事件源

### 优先级建议
- [ ] P0: `event`、`task`、`fsm`
- [ ] P1: `notify`、`orchestration`
- [ ] P2: `clipboard`、`file`、`hotkey`、`timer`
- [ ] P3: UI 树、模板管理、录制回放增强

### 建议的落地顺序
1. `event` 先补齐事件对象、订阅管理和一次性监听
2. `fsm` 直接建立在 `event` 之上，统一状态迁移事件
3. `task` 引入任务生命周期和重试/超时封装
4. `notify` 消费 `event` / `task`，统一输出到日志、托盘和 webhook
5. `orchestration` 复用 `task` + `fsm` 做工作流编排

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
- [ ] 任务状态机
- [ ] 工作流编排

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
