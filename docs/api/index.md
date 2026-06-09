# API 参考

Wingman 提供 Python 和 Lua 两种脚本语言的 SDK，支持完整的游戏自动化功能。

## 为什么支持两种语言

Wingman 选择同时支持 Python 和 Lua，是为了满足不同用户的需求：

### Python

**适合用户**：
- 有编程经验的开发者
- 需要处理复杂逻辑的自动化任务
- 需要调用第三方库（网络请求、数据处理等）
- 重视开发效率和代码可维护性

**优势**：
- 生态丰富：可使用 pip 安装海量第三方库
- 类型友好：支持类型提示，IDE 自动补全完善
- 调试方便：成熟的调试工具和错误追踪
- 代码组织：支持模块、类、函数等完整面向对象特性

**典型场景**：
- 复杂的游戏辅助工具（需要网络同步、数据库存储）
- 需要调用 AI 模型（OCR、YOLO 目标检测）
- 团队协作开发的大型自动化项目

### Lua

**适合用户**：
- 游戏玩家或轻度编程用户
- 需要快速编写简单脚本的场景
- 资源受限环境（嵌入式、低配置设备）
- 重视启动速度和内存占用

**优势**：
- 轻量级：解释器体积小（< 1MB），启动迅速
- 简单易学：语法简洁，上手快
- 部署方便：单文件脚本，无需依赖管理
- 游戏行业：许多游戏的宏系统使用 Lua，用户可能已熟悉

**典型场景**：
- 简单的挂机脚本
- 游戏内宏录制后的小幅修改
- 需要快速测试自动化想法

---

## SDK 设计哲学

### 命名规范

两种语言遵循各自社区的最佳实践：

**Python（snake_case）**：
```python
screen.get_pixel(x, y)
screen.find_image(path, x, y, w, h)
uia.find_button("确定")
http.post(url, data)
```

**Lua（camelCase）**：
```lua
screen.getPixel(x, y)
screen.findImage(path, x, y, w, h)
uia.findButton("确定")
http.post(url, data)
```

### 模块化设计

所有功能按模块划分，清晰明确：

| 模块 | 功能 | 典型用途 |
|-----|------|---------|
| `screen` | 屏幕操作 | 截图、找图、取色 |
| `input` | 输入模拟 | 鼠标点击、键盘输入 |
| `window` | 窗口管理 | 查找窗口、激活窗口 |
| `process` | 进程管理 | 启动游戏、检查进程 |
| `uia` | UI Automation | 稳定的控件操作 |
| `http` | 网络请求 | API 调用、数据同步 |
| `kv` | 数据存储 | 保存脚本状态 |

### 一致性原则

两种语言的 API 在功能上完全对等，只是语法适应语言习惯：

**相同功能，不同语法**：

```python
# Python
from wingman import screen

img = screen.capture(0, 0, 400, 300)
result = screen.find_image("button.png", 0, 0, 1920, 1080)
```

```lua
-- Lua
local screen = require("wingman.screen")

local img = screen.capture(0, 0, 400, 300)
local result = screen.findImage("button.png", 0, 0, 1920, 1080)
```

---

## 设计取舍

### 为什么不是 JavaScript / TypeScript

- **执行效率**：V8 引擎体积大、启动慢
- **部署复杂**：需要 node.js 环境，对普通用户门槛高
- **游戏冲突**：许多游戏浏览器组件嵌入 JS 引擎，可能冲突

### 为什么不是 C# / .NET

- **运行时依赖**：需要 .NET Runtime，部署不友好
- **编译步骤**：用户需要编译，不符合脚本快速迭代的特性
- **学习曲线**：对轻度用户过重

### Python vs Lua 的性能

两种语言在实际使用中的特点：

**Python 优势**：
- 丰富的第三方库支持（numpy、pandas、requests 等）
- 成熟的开发工具和调试器
- 类型提示和 IDE 自动补全
- 适合处理复杂逻辑和大规模项目

**Lua 优势**：
- 解释器体积小，启动快
- 内存占用相对较低
- 语法简洁，脚本文件小
- 部署方便，无需额外依赖

**建议**：
- 简单脚本、快速原型 → Lua
- 复杂项目、需要第三方库 → Python

---

## 快速开始

### Python

```python
from wingman import screen, input

# 截图并找图
img = screen.capture(0, 0, 400, 300)
result = screen.find_image("button.png", 0, 0, 1920, 1080)

if result:
    x, y, confidence = result
    input.click(x, y)
```

### Lua

```lua
local screen = require("wingman.screen")
local input = require("wingman.input")

-- 截图并找图
local img = screen.capture(0, 0, 400, 300)
local result = screen.findImage("button.png", 0, 0, 1920, 1080)

if result then
    input.click(result.x, result.y)
end
```

---

## 数据类型

查看 [数据类型参考](./types.md) 了解 API 中使用的各种对象和数据结构。

---

## API 模块

### 核心模块

基础操作模块，提供屏幕、输入、窗口、进程等核心功能。

- [screen](./screen.md) - 屏幕截图、像素查找、图像匹配
- [input](./input.md) - 鼠标点击、移动、键盘输入
- [window](./window.md) - 窗口查找、激活、获取信息
- [process](./process.md) - 进程启动、查找、终止
- [system](./system.md) - 系统信息获取
- [uia](./uia/) - UI Automation 控件交互

### 视觉与 AI

计算机视觉与 AI 相关功能。

- [vision](./vision.md) - 颜色检测、边缘检测、轮廓识别
- [ocr](./ocr.md) - 文字识别（OCR，可选依赖）

### 自动化系统

高级自动化编排功能。

- [event](./event.md) - 事件订阅与发布系统
- [fsm](./fsm.md) - 有限状态机（FSM）
- [task](./task.md) - 任务编排与执行
- [notify](./notify.md) - 通知系统（日志、Toast、Webhook）
- [smart-trigger](./smart-trigger.md) - 智能触发器
- [behavior-tree](./behavior-tree.md) - 行为树引擎

### 网络

网络请求与通信。

- [http](./http.md) - HTTP 客户端（GET、POST、下载）
- [transport](./transport.md) - TCP 客户端、服务器与 UDP Socket

### 序列化

数据序列化与格式转换。

- [serialize](./serialize.md) - 序列化格式总览（json、yaml、ini 选择指南）
- [json](./json.md) - JSON 解析与序列化
- [ini](./ini.md) - INI 配置文件解析与编码

### 数据存储

本地数据持久化与缓存。

- [data](./data.md) - 数据存储总览（kv 与 db 选择指南）
- [kv](./kv.md) - 持久化键值存储（Redis-like API）
- [db](./db.md) - 本地 SQLite 数据库（支持 SQL 和 ORM）

### 辅助功能

辅助自动化脚本的工具模块。

- [human](./human.md) - 人性化模拟（随机延迟、不规律输入）
- [perf](./perf.md) - 性能监控
- [config](./config.md) - 配置管理
- [verification](./verification.md) - 验证码识别、TOTP
- [gameprofile](./gameprofile.md) - 游戏配置档案
- [debugger](./debugger.md) - 调试器
- [util](./util.md) - 工具函数（sleep、随机数等）

### 安全与加密

安全和加密相关功能。

- [security](./security.md) - 安全模块
- [crypto](./security.md) - 加密函数（与 security 共用文档）

### 编排与协作

多实例协作和编排功能。

- [orchestration](./orchestration.md) - 编排控制
- [inbox](./inbox.md) - 消息收件箱
- [team](./team.md) - 组队编排
- [node](./orchestration.md) - 节点管理（与 orchestration 共用文档）
