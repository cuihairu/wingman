# Wingman 开发待办事项

> 对比 Chimpeeon 功能规划 Server/Client 架构

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
- [x] `window.setBounds(hwnd, x, y, w, h)` - 设置窗口位置
- [x] `window.isForeground(hwnd)` - 判断是否前台
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

---

## Phase 2: 触发器系统 ✅

### 2.1 触发器类型 ✅
- [x] 像素触发器 - 检测到指定颜色/图像
- [x] 定时触发器 - 间隔执行
- [x] 时间触发器 - 指定时间执行
- [x] 窗口触发器 - 窗口出现/消失
- [x] 进程触发器 - 进程启动/停止
- [x] 像素变化触发器

### 2.2 触发器动作 ✅
- [x] 发送按键
- [x] 鼠标操作
- [x] 显示消息
- [x] 播放声音
- [ ] 执行 Lua 函数 (待集成)
- [ ] 日志输出 (待实现)

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

---

## Phase 4: Server 模块 (远程控制) ✅

### 4.1 TCP Server ✅
- [x] 监听端口启动
- [x] 客户端连接管理
- [x] 消息协议定义
- [x] 心跳保活

### 4.2 远程 API ✅
- [x] 执行 Lua 脚本
- [x] 获取屏幕截图
- [x] 像素检测请求
- [x] 输入模拟请求
- [x] 状态查询

### 4.3 组队编排引擎 ✅
- [x] Client 注册和心跳
- [x] 队伍自动编排
- [x] 投票协调
- [x] KV 状态存储

---

## Phase 5: Client 模块

### 5.1 Python Client ✅
- [x] TCP 连接管理
- [x] API 封装
- [x] 异步请求支持
- [x] 类型提示
- [x] 事件处理
- [x] 完整文档和示例

### 5.2 Node.js Client ✅
- [x] TCP 连接管理
- [x] API 封装
- [x] TypeScript 类型定义
- [x] 事件处理
- [x] 自动重连
- [x] 完整文档和示例

### 5.3 Web Dashboard ✅
- [x] 实时监控界面
- [x] 脚本管理
- [x] 日志查看
- [x] 配置编辑器

---

## Phase 6: 调试工具 🚧

### 6.1 VS Code 扩展
- [x] 项目结构
- [x] Lua 语法高亮
- [x] API 自动完成
- [x] 悬停提示
- [x] 签名帮助
- [x] 诊断支持
- [x] 断点设置
- [x] 变量查看
- [x] 调试协议 (DAP) 实现

### 6.2 日志系统
- [x] 分级日志 (DEBUG/INFO/WARN/ERROR) - 使用 spdlog
- [x] 文件输出
- [x] 控制台输出
- [ ] 性能统计

---

## Phase 7: 高级功能 ✅

### 7.1 脚本管理 ✅
- [x] 脚本热加载
- [x] 配置文件解析
- [x] 环境变量支持
- [x] 脚本沙箱

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

## Phase 8: 文档和示例 🚧

### 8.1 文档
- [x] API 参考文档
- [x] 快速入门指南
- [ ] 示例脚本集合
- [ ] 视频教程

### 8.2 示例脚本
- [x] Hello World
- [x] 像素检测示例
- [x] 图像匹配示例
- [x] 自动循环示例
- [x] MMORPG 游戏自动化
- [x] 窗口操作示例
- [x] 进程管理示例
- [x] HTTP API 示例
- [x] KV 存储示例
- [x] 脚本管理示例
- [x] 安全模块示例
- [x] 游戏配置示例

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
8. ✅ Server TCP 模块
9. ✅ HTTP/JSON/KV 模块

### P2 - 增强功能 ✅
10. ✅ 人性化模拟
11. ✅ 调试工具
12. ✅ Python Client
13. ✅ Node.js Client
14. ✅ 性能优化
15. ✅ 脚本管理

### P3 - 高级功能 ✅
16. ✅ Web Dashboard
17. ✅ VS Code 扩展
18. ✅ 脚本管理
19. ✅ 安全特性
20. ✅ 多游戏配置

---

## 当前状态

- [x] 项目初始化
- [x] CMake 构建系统
- [x] 基础目录结构
- [x] 文档网站框架
- [x] CI/CD 配置
- [x] 核心功能实现 (100%)
- [x] Lua 测试框架 (busted)
- [x] Codecov 配置
- [x] 示例脚本 (100% - 15个示例)
- [x] VS Code 扩展 (90% - 调试器、语言服务器)
- [x] Python Client
- [x] Node.js Client
- [x] 性能优化模块
- [x] 脚本管理模块
- [x] 安全模块
- [x] 游戏配置管理
- [x] Web Dashboard

## 下一步计划

1. 文档完善 (示例脚本集合)
2. 发布准备

---

## Phase 9: 多账号协作编排 🆕

### 9.1 任务编排引擎
- [ ] Orchestrator 核心类 - 工作流调度
- [ ] TaskStep 定义 - 步骤、依赖关系、完成检测
- [ ] Worker 协议 - 客户端与服务端通信规范
- [ ] 屏障同步 (Barrier Synchronization) - 等待所有账号完成当前步骤
- [ ] 进度存储 - 服务端状态持久化

### 9.2 进度追踪机制
- [ ] WorkerStatus 状态定义 (pending/running/completed/failed)
- [ ] 进度报告 API - `reportProgress(workerId, stepId, progress)`
- [ ] 下一步请求 API - `getNextStep(workerId)`
- [ ] 工作流状态查询 API - `getStatus(workflowId)`

### 9.3 Lua 绑定
- [ ] `orchestration.get_next_step(agentId)` - 请求下一步任务
- [ ] `orchestration.report_progress(agentId, stepId, progress)` - 报告进度
- [ ] `orchestration.get_workflow_status(workflowId)` - 查询状态

---

## Phase 10: TCP Server/Client 增强 🆕

### 10.1 协议定义

#### 请求消息结构
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

#### 响应消息结构
```json
{
  "request_id": "req-002",       // 对应的请求 ID
  "code": 0,                     // 错误码（数字）
  "timestamp": 1714928901,       // 响应时间戳
  "message": "success",          // 可读描述（可选）
  "data": {...}                  // 业务数据（成功时）
}
```

#### 错误码定义（0-1023 系统保留，1024+ 用户自定义）
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

#### 消息类型
- [ ] `kRegister` - 客户端注册消息
- [ ] `kHeartbeat` - 心跳消息
- [ ] `kGetAgents` - 获取所有在线客户端列表
- [ ] `kSyncTask` - 同步任务状态
- [ ] `kShutdown` - 关闭客户端

### 10.2 服务端会话管理
- [ ] `AgentInfo` 结构 - 客户端信息 (agentId, hostname, ip, status, lastSeen)
- [ ] `getOnlineAgents()` - 获取所有在线客户端
- [ ] `sendToAgent(agentId, response)` - 向指定客户端发送消息
- [ ] `disconnectAgent(agentId)` - 断开指定客户端
- [ ] 心跳超时检测 - 定时检查超时客户端并清理

### 10.3 客户端增强
- [ ] `setAgentId(id)` - 设置客户端 ID
- [ ] `enableAutoReconnect(enable, interval)` - 启用自动重连
- [ ] `setStateCallback(callback)` - 连接状态事件回调
- [ ] `startHeartbeat(interval)` - 启动自动心跳
- [ ] 连接后自动注册 - 发送 register 消息

### 10.4 事件系统
- [ ] 服务端：`onConnect(agentId)` - 客户端上线事件
- [ ] 服务端：`onDisconnect(agentId)` - 客户端下线事件
- [ ] 客户端：`onConnectionStateChanged(connected)` - 连接状态变化事件

---

## 架构设计原则 🆕

### 核心能力 vs 业务逻辑
- **核心层** (保留): 验证码生成/验证 (TOTP/SteamGuard)、键值存储、任务队列、脚本执行引擎
- **扩展层** (可选): AccountManager 插件、进度存储接口、调度器接口
- **用户层** (用户定义): 账号概念、游戏逻辑、脚本进度、调度策略

### 模块位置调整建议
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
- ❌ 不使用 protobuf (不必要复杂度)
