# API 参考

Wingman 提供以下 API 模块，同时支持 **Python** 和 **Lua** 两种脚本语言。

## 核心模块

基础操作模块，提供屏幕、输入、窗口、进程等核心功能。

- [screen](/api/screen) - 屏幕截图、像素查找、图像匹配
- [input](/api/input) - 鼠标点击、移动、键盘输入
- [window](/api/window) - 窗口查找、激活、获取信息
- [process](/api/process) - 进程启动、查找、终止
- [uia](/api/uia) - UI Automation 控件交互

## 视觉与 AI

计算机视觉与 AI 相关功能。

- [vision](/api/vision) - 颜色检测、边缘检测、轮廓识别
- [ocr](/api/ocr) - 文字识别（OCR）

## 自动化系统

高级自动化编排功能。

- [event](/api/event) - 事件订阅与发布系统
- [fsm](/api/fsm) - 有限状态机（FSM）
- [task](/api/task) - 任务编排与执行
- [notify](/api/notify) - 通知系统（日志、Toast、Webhook）
- [smart-trigger](/api/smart-trigger) - 智能触发器
- [behavior-tree](/api/behavior-tree) - 行为树引擎

## 网络与数据

网络请求与数据处理。

- [http](/api/http) - HTTP 客户端（GET、POST、下载）
- [json](/api/json) - JSON 解析与序列化
- [kv](/api/kv) - 持久化键值存储

## 辅助功能

辅助自动化脚本的工具模块。

- [human](/api/human) - 人性化模拟（随机延迟、不规律输入）
- [config](/api/config) - 配置管理
- [verification](/api/verification) - 验证码识别、TOTP
- [gameprofile](/api/gameprofile) - 游戏配置档案
- [debugger](/api/debugger) - 调试器
- [util](/api/util) - 工具函数（sleep、随机数等）
- [tray](/api/tray) - 系统托盘图标

## 高级功能

高级功能模块。

- [remote](/api/remote) - 远程控制协议
- [team](/api/team) - 组队编排

---

## 语言差异

### 命名风格

- **Python**: 使用 snake_case 命名（如 `get_pixel`、`find_color`）
- **Lua**: 使用 camelCase 命名（如 `getPixel`、`findColor`）

### 导入方式

**Python:**
```python:line-numbers
from wingman import screen, input, event
```

**Lua:**
```lua:line-numbers
local screen = require("wingman.screen")
local input = require("wingman.input")
local event = require("wingman.event")
```

### 回调函数

**Python:**
```python:line-numbers
event.on("my_event", lambda e: print(f"Received: {e}"))
```

**Lua:**
```lua:line-numbers
event.on("my_event", function(e)
    print("Received: " .. e["payload"])
end)
```
