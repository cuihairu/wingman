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
- [x] LuaJIT 集成
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

### 5.1 Python Client 🚧
- [ ] TCP 连接管理
- [ ] API 封装
- [ ] 异步请求支持
- [ ] 类型提示

### 5.2 Node.js Client 🚧
- [ ] TCP 连接管理
- [ ] API 封装
- [ ] TypeScript 类型定义

### 5.3 Web Dashboard ✅
- [x] 实时监控界面
- [x] 脚本管理
- [x] 日志查看
- [x] 配置编辑器

---

## Phase 6: 调试工具 🚧

### 6.1 VS Code 扩展
- [x] 项目结构
- [ ] Lua 语法高亮
- [ ] 断点设置
- [ ] 变量查看
- [ ] 调试协议 (DAP) 实现

### 6.2 日志系统
- [x] 分级日志 (DEBUG/INFO/WARN/ERROR) - 使用 spdlog
- [x] 文件输出
- [x] 控制台输出
- [ ] 性能统计

---

## Phase 7: 高级功能

### 7.1 脚本管理
- [ ] 脚本热加载
- [ ] 配置文件解析
- [ ] 环境变量支持
- [ ] 脚本沙箱

### 7.2 性能优化
- [x] 像素检测加速 - 使用 OpenCV
- [ ] 图像匹配缓存
- [ ] 多线程处理
- [ ] 内存优化

### 7.3 安全特性
- [ ] 代码签名
- [ ] 进程保护
- [ ] 反检测机制
- [ ] 混淆支持

---

## Phase 8: 文档和示例 🚧

### 8.1 文档
- [x] API 参考文档
- [x] 快速入门指南
- [ ] 示例脚本集合
- [ ] 视频教程

### 8.2 示例脚本
- [ ] Hello World
- [ ] 像素检测示例
- [ ] 图像匹配示例
- [ ] 自动循环示例
- [ ] 多游戏示例 (MMORPG/FPS)

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

### P2 - 增强功能 🚧
10. ✅ 人性化模拟
11. 🚧 调试工具
12. [ ] Python Client
13. [ ] 性能优化

### P3 - 高级功能
14. ✅ Web Dashboard
15. 🚧 VS Code 扩展
16. [ ] 安全特性
17. [ ] 多游戏配置

---

## 当前状态

- [x] 项目初始化
- [x] CMake 构建系统
- [x] 基础目录结构
- [x] 文档网站框架
- [x] CI/CD 配置
- [x] 核心功能实现 (90%)
- [ ] 示例脚本 (10%)
- [ ] VS Code 扩展完善 (20%)

## 下一步计划

1. 完善示例脚本
2. 完善 VS Code 扩展
3. 添加 Python Client
4. 性能优化和测试
