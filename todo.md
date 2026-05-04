# Wingman 开发待办事项

> 对比 Chimpeeon 功能规划 Server/Client 架构

## Chimpeon 核心功能对比

| 功能模块 | Chimpeon | Wingman (当前) | 状态 |
|---------|----------|---------------|------|
| 像素检测 | ✅ 超快速检测 | ✅ 已规划 | 待实现 |
| 颜色匹配 | ✅ 多点检测 | ✅ 已规划 | 待实现 |
| 图像识别 | ❌ | ✅ OpenCV 支持 | 待实现 |
| 触发器系统 | ✅ 多种触发条件 | ✅ 已规划 | 待实现 |
| 宏录制 | ✅ 录制-回放 | ✅ 已规划 | 待实现 |
| 按键模拟 | ✅ | ✅ 已规划 | 待实现 |
| 鼠标模拟 | ✅ | ✅ 已规划 | 待实现 |
| 远程控制 | ✅ 流式支持 | ✅ TCP Server | 架构完成 |
| Lua 脚本 | ❌ | ✅ 核心特性 | 待实现 |
| VS Code 调试 | ❌ | ✅ 已规划 | 待实现 |
| 防检测 | ✅ | ✅ 人性化模拟 | 待实现 |

---

## Phase 1: 核心引擎 (C++)

### 1.1 屏幕操作模块
- [ ] `screen.capture()` - 截取屏幕/窗口
- [ ] `screen.getPixel(x, y)` - 获取单点像素
- [ ] `screen.findColor(color, x1, y1, x2, y2, tolerance)` - 单点颜色查找
- [ ] `screen.findColors(color, x1, y1, x2, y2, tolerance, count)` - 多点颜色查找
- [ ] `screen.findImage(imagePath, x1, y1, x2, y2, threshold)` - 图像匹配
- [ ] `screen.getWindowTitle(hwnd)` - 获取窗口标题
- [ ] `screen.getWindowBounds(hwnd)` - 获取窗口位置

### 1.2 输入模拟模块
- [ ] `input.click(x, y, button)` - 鼠标点击
- [ ] `input.move(x, y, duration)` - 鼠标移动（贝塞尔曲线）
- [ ] `input.scroll(x, y, delta)` - 鼠标滚轮
- [ ] `input.keyDown(key)` - 按键按下
- [ ] `input.keyUp(key)` - 按键释放
- [ ] `input.type(text, delay)` - 文本输入

### 1.3 窗口管理模块
- [ ] `window.find(title)` - 查找窗口
- [ ] `window.activate(hwnd)` - 激活窗口
- [ ] `window.getBounds(hwnd)` - 获取窗口位置
- [ ] `window.setBounds(hwnd, x, y, w, h)` - 设置窗口位置
- [ ] `window.isForeground(hwnd)` - 判断是否前台

### 1.4 进程管理模块
- [ ] `process.find(name)` - 查找进程
- [ ] `process.start(path, args)` - 启动进程
- [ ] `process.wait(pid)` - 等待进程
- [ ] `process.terminate(pid)` - 终止进程

### 1.5 人性化模拟模块
- [ ] `human.moveTo(x, y)` - 贝塞尔曲线移动
- [ ] `human.click(x, y)` - 带随机延迟的点击
- [ ] `human.randomDelay(min, max)` - 随机延迟
- [ ] `human.jitter(amount)` - 位置抖动

### 1.6 Lua 绑定
- [ ] 暴露所有 C++ API 到 Lua
- [ ] LuaJIT 集成
- [ ] 错误处理和异常转换

---

## Phase 2: 触发器系统

### 2.1 触发器类型
- [ ] 像素触发器 - 检测到指定颜色/图像
- [ ] 定时触发器 - 间隔执行
- [ ] 时间触发器 - 指定时间执行
- [ ] 计数触发器 - 执行N次后触发
- [ ] 组合触发器 - 多条件AND/OR

### 2.2 触发器动作
- [ ] 执行 Lua 函数
- [ ] 发送按键
- [ ] 鼠标操作
- [ ] 播放宏
- [ ] 日志输出

---

## Phase 3: 宏系统

### 3.1 录制功能
- [ ] `macro.startRecording()` - 开始录制
- [ ] `macro.stopRecording()` - 停止录制
- [ ] 记录鼠标移动/点击
- [ ] 记录键盘输入
- [ ] 时间戳记录

### 3.2 回放功能
- [ ] `macro.play(name)` - 播放宏
- [ ] `macro.loop(name, times)` - 循环播放
- [ ] `macro.save(name, path)` - 保存宏
- [ ] `macro.load(path)` - 加载宏
- [ ] 回放速度控制

---

## Phase 4: Server 模块 (远程控制)

### 4.1 TCP Server
- [ ] 监听端口启动
- [ ] 客户端连接管理
- [ ] 消息协议定义
- [ ] 心跳保活

### 4.2 远程 API
- [ ] 执行 Lua 脚本
- [ ] 获取屏幕截图
- [ ] 像素检测请求
- [ ] 输入模拟请求
- [ ] 状态查询

### 4.3 协议定义
```json
{
  "type": "request",
  "id": 1,
  "action": "screen.capture",
  "params": {"x": 0, "y": 0, "w": 1920, "h": 1080}
}
```

---

## Phase 5: Client 模块

### 5.1 Python Client
- [ ] TCP 连接管理
- [ ] API 封装
- [ ] 异步请求支持
- [ ] 类型提示

```python
class WingmanClient:
    def connect(self, host, port)
    def screen_capture(self, x, y, w, h) -> bytes
    def find_color(self, color, region) -> List[Point]
    def click(self, x, y, button)
```

### 5.2 Node.js Client
- [ ] TCP 连接管理
- [ ] API 封装
- [ ] TypeScript 类型定义

### 5.3 Web Dashboard (可选)
- [ ] 实时监控界面
- [ ] 脚本管理
- [ ] 日志查看
- [ ] 配置编辑器

---

## Phase 6: 调试工具

### 6.1 VS Code 扩展
- [ ] Lua 语法高亮
- [ ] 断点设置
- [ ] 变量查看
- [ ] 调试协议 (DAP) 实现
- [ ] 调用栈查看

### 6.2 日志系统
- [ ] 分级日志 (DEBUG/INFO/WARN/ERROR)
- [ ] 文件输出
- [ ] 控制台输出
- [ ] 性能统计

---

## Phase 7: 高级功能

### 7.1 脚本管理
- [ ] 脚本热加载
- [ ] 配置文件解析
- [ ] 环境变量支持
- [ ] 脚本沙箱

### 7.2 性能优化
- [ ] 像素检测加速
- [ ] 图像匹配缓存
- [ ] 多线程处理
- [ ] 内存优化

### 7.3 安全特性
- [ ] 代码签名
- [ ] 进程保护
- [ ] 反检测机制
- [ ] 混淆支持

---

## Phase 8: 文档和示例

### 8.1 文档
- [ ] API 参考文档
- [ ] 快速入门指南
- [ ] 示例脚本集合
- [ ] 视频教程

### 8.2 示例脚本
- [ ] Hello World
- [ ] 像素检测
- [ ] 图像匹配
- [ ] 自动循环
- [ ] 多游戏示例 (MMORPG/FPS)

---

## 优先级排序

### P0 - 核心基础 (必须)
1. 屏幕操作模块
2. 输入模拟模块
3. Lua 绑定
4. 主程序入口

### P1 - 基本功能 (重要)
5. 窗口管理模块
6. 触发器系统基础
7. 宏录制回放
8. Server TCP 模块

### P2 - 增强功能 (可选)
9. 人性化模拟
10. 调试工具
11. Python Client
12. 性能优化

### P3 - 高级功能 (未来)
13. Web Dashboard
14. VS Code 扩展
15. 安全特性
16. 多游戏配置

---

## 当前状态

- [x] 项目初始化
- [x] CMake 构建系统
- [x] 基础目录结构
- [x] 文档网站框架
- [x] CI/CD 配置
- [ ] 核心功能实现 (0%)
