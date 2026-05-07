---
title: Wingman 用户手册
---

# Wingman 用户手册

Wingman 是一个 Windows 游戏自动化平台，支持 Lua 脚本编写、图像识别、远程控制等功能。

## 目录

- [安装](#安装)
- [快速入门](#快速入门)
- [主界面](#主界面)
- [配置](#配置)
- [API 参考](#api-参考)
- [常见问题](#常见问题)

## 安装

### 安装版

1. 下载 `wingman-setup-0.1.0.exe`
2. 双击运行安装程序
3. 选择安装目录（默认：`C:\Program Files\Wingman`）
4. 选择是否创建桌面快捷方式
5. 点击"安装"完成

### 便携版

1. 下载 `wingman-portable-0.1.0.zip`
2. 解压到任意目录
3. 运行 `wingman.exe`

## 快速入门

### Hello World

创建文件 `test.lua`：

```lua
print("Hello, Wingman!")
```

运行：

```bash
wingman.exe test.lua
```

### 截图并保存

```lua
local screen = require("wingman.screen")

-- 截取全屏
local img = screen.capture()

-- 保存到文件
img:save("screenshot.png")
```

### 查找颜色并点击

```lua
local screen = require("wingman.screen")
local input = require("wingman.input")

-- 查找红色 (0xFF0000)
local points = screen.findColor(0xFF0000, {
    region = {0, 0, 1920, 1080},
    tolerance = 10
})

if points and #points > 0 then
    -- 点击第一个匹配点
    input.click(points[1].x, points[1].y)
    print("Clicked at:", points[1].x, points[1].y)
else
    print("Color not found")
end
```

## 主界面

Wingman 启动后显示 Web 控制面板：

```
┌─────────────────────────────────────────────────────┐
│ Wingman                          [●] [⏸] [⚙]        │
├──────────┬──────────────────────┬───────────────────┤
│          │                      │                   │
│ 触发器   │    预览             │    日志           │
│          │                      │                   │
│ □ 技能CD │   [Screenshot]      │ [Log] 信息...     │
│ □ 血量   │                      │                   │
│ □ 自动战 │   [区域选择]         │                   │
│          │                      │                   │
│ [+] 添加 │                      │                   │
└──────────┴──────────────────────┴───────────────────┘
```

### 功能说明

| 面板 | 功能 |
|------|------|
| **触发器** | 管理像素/图像触发器，自动响应游戏事件 |
| **预览** | 实时屏幕预览，选择检测区域 |
| **脚本** | 加载、运行、停止 Lua 脚本 |
| **日志** | 查看运行日志和调试信息 |

## 配置

配置文件位置：`%USERPROFILE%\.wingman\config.json`

### 默认配置

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 9527
  },
  "script": {
    "autoLoad": true,
    "directory": "scripts"
  },
  "gui": {
    "showPreview": true,
    "refreshInterval": 500
  },
  "logging": {
    "level": "info",
    "maxFiles": 10,
    "maxSize": "10M"
  }
}
```

### 触发器配置

在 GUI 中创建触发器，或编辑配置文件：

```json
{
  "name": "技能冷却",
  "enabled": true,
  "condition": {
    "type": "pixel",
    "color": "0x00FF00",
    "region": [100, 100, 50, 50],
    "tolerance": 10
  },
  "action": {
    "type": "key",
    "key": "1"
  },
  "cooldown": 500
}
```

## API 参考

### 屏幕 (screen)

```lua
local screen = require("wingman.screen")

-- 获取屏幕尺寸
local width, height = screen.getSize()

-- 截图
local img = screen.capture()
local img = screen.capture(x, y, width, height)

-- 获取像素
local color = screen.getPixel(x, y)

-- 查找颜色
local points = screen.findColor(color, options)
```

### 输入 (input)

```lua
local input = require("wingman.input")

-- 鼠标点击
input.click(x, y)
input.rightClick(x, y)

-- 鼠标移动
input.move(x, y)
input.move(x, y, duration)  -- 带动画

-- 键盘
input.press("A")  -- 按键
input.type("Hello")  -- 输入文本
```

### 人性化 (human)

```lua
local human = require("human")

-- 贝塞尔曲线鼠标移动
human.mouse.move(x, y)
human.mouse.click(x, y)

-- 随机延迟键盘
human.keyboard.type("Hello, World!")
```

### 窗口 (window)

```lua
local window = require("wingman.window")

-- 查找窗口
local win = window.find("Notepad")

-- 激活窗口
window.activate(win)

-- 获取窗口标题
local title = window.getTitle(win)
```

## 调试器

Wingman 内置 Lua 调试器，支持断点、单步执行、变量查看。

### 启动调试

在脚本中添加：

```lua
local debugger = require("debugger")

-- 启动调试器
debugger.start()

-- 设置断点
debugger.breakpoint("scripts/test.lua", 42)

-- 调试断点
debugger.breakHere()
```

在 VS Code 中按 F5 开始调试。

## 常见问题

### Q: 找不到 MSVCR*.dll？

A: 安装 [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Q: 杀毒软件误报？

A: 将 Wingman 安装目录添加到杀毒软件白名单

### Q: 脚本运行但没反应？

A: 检查：
1. 管理员权限运行
2. 游戏窗口在前台
3. 脚本中的坐标/颜色是否正确

### Q: 如何获取像素颜色？

A: 使用预览面板的区域选择工具，或使用截图工具取色

## 高级用法

### 远程控制

```bash
# 启动服务器
wingman.exe --server --port 9999

# 在另一台机器上连接
wingman.exe --connect 192.168.1.100:9999 script.lua
```

### 多脚本协作

```lua
local team = require("wingman.team")

-- 加入房间
team.join("farming_room")

-- 发送消息
team.send({
    action = "found_target",
    data = {x = 100, y = 200}
})
```

### HTTP API

Wingman 提供 RESTful API 用于外部控制：

```
POST /api/v1/scripts/run
{
  "path": "scripts/auto_farm.lua"
}
```

---

更多信息请访问：[GitHub](https://github.com/cuihairu/wingman)
