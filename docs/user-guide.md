---
title: Wingman 用户手册
---

# Wingman 用户手册

Wingman 是一个跨平台的游戏自动化工具，支持 Lua 和 Python 脚本编写、图像识别、远程控制等功能。

## 系统要求

### Windows
- Windows 10/11 (x64)
- Visual Studio 2022

### macOS
- macOS 12+ (Monterey 或更高)
- Xcode 14+ 或 Clang

### Linux
- Ubuntu 22.04+ 或等效发行版
- GCC 11+ 或 Clang 14+

### 通用
- CMake 3.20+
- vcpkg

## 目录

- [安装](#安装)
- [快速入门](#快速入门)
- [主界面](#主界面)
- [配置](#配置)
- [API 参考](#api-参考)
- [常见问题](#常见问题)

## 安装

### Windows

#### 安装版

1. 下载 `wingman-setup-0.1.0.exe`
2. 双击运行安装程序
3. 选择安装目录（默认：`C:\Program Files\Wingman`）
4. 选择是否创建桌面快捷方式
5. 点击"安装"完成

#### 便携版

1. 下载 `wingman-portable-0.1.0.zip`
2. 解压到任意目录
3. 运行 `wingman-runtime.exe`

### macOS/Linux

```bash
# 克隆仓库
git clone https://github.com/cuihairu/wingman.git
cd wingman

# 配置和编译
cmake -B build -S . -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux  # 或 x64-osx for macOS
cmake --build build --config Release
```

### Windows

```cmd
build-scripts\build-runtime-msvc-ninja.bat
```

## 快速入门

### Hello World

创建文件 `test.lua`：

```lua
print("Hello, Wingman!")
```

运行：

```bash
# Windows
wingman-runtime.exe script test.lua

# macOS/Linux
./wingman-runtime script test.lua
```

### 截图并保存

```lua
local wingman = require("wingman")

-- 截取全屏
local img = wingman.screen.capture(0, 0, 1920, 1080)

-- 保存到文件
img:save("screenshot.png")
```

### 查找颜色并点击

```lua
local wingman = require("wingman")

-- 查找红色 (0xFF0000)
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)

if points and #points > 0 then
    -- 点击第一个匹配点
    wingman.input.click(points[1].x, points[1].y)
    print("Clicked at:", points[1].x, points[1].y)
else
    print("Color not found")
end
```

## 主界面

Wingman GUI 启动后显示控制面板：

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
| **脚本** | 加载、运行、停止 Lua/Python 脚本 |
| **日志** | 查看运行日志和调试信息 |

## 配置

配置文件位置：
- Windows: `%USERPROFILE%\.wingman\config.json`
- macOS/Linux: `~/.wingman/config.json`

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

> 详细 API 文档请参考 [API 参考](./api/index.md)

### 屏幕 (screen)

```lua
local wingman = require("wingman")

-- 获取屏幕尺寸
local width, height = wingman.screen.getDimensions()

-- 截图
local img = wingman.screen.capture()
local img = wingman.screen.capture(x, y, width, height)

-- 获取像素
local color = wingman.screen.getPixel(x, y)

-- 查找颜色（Lua 使用 camelCase）
local points = wingman.screen.findColor(color, x, y, w, h, tolerance)
```

### 输入 (input)

```lua
local wingman = require("wingman")

-- 鼠标点击
wingman.input.click(x, y)
wingman.input.rightClick(x, y)

-- 鼠标移动
wingman.input.move(x, y)
wingman.input.move(x, y, duration)  -- 带动画

-- 键盘
wingman.input.sendKeys("A")  -- 按键
wingman.input.typeText("Hello")  -- 输入文本
```

### 人性化 (human)

```lua
local wingman = require("wingman")

-- 贝塞尔曲线鼠标移动
wingman.human.mouse.move(x, y)
wingman.human.mouse.click(x, y)

-- 随机延迟键盘
wingman.human.keyboard.type("Hello, World!")
```

### 窗口 (window)

```lua
local wingman = require("wingman")

-- 查找窗口
local win = wingman.window.find("Notepad")

-- 激活窗口
wingman.window.activate(win)

-- 获取窗口标题
local title = wingman.window.getTitle(win)
```

## 调试器

Wingman 内置 EmmyLua 调试器，支持断点、单步执行、变量查看。

### 启动调试

在脚本中添加：

```lua
local debugger = require("emmy_core")

-- 启动调试器
debugger.tcpListen("localhost", 9966)
debugger.waitIDE()

-- 你的脚本代码
local wingman = require("wingman")
-- ...
```

在 VS Code 中按 F5 开始调试（需要安装 EmmyLua 扩展）。

## 常见问题

### Q: 找不到 MSVCR*.dll？（Windows）

A: 安装 [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Q: Windows SmartScreen 警告"无法验证发布者"？

A: Wingman 使用自签名证书（开源项目标准做法），首次运行会看到警告：

1. 点击"更多信息"
2. 点击"仍要运行"

**永久解决方案：** 信任 Wingman 证书

1. 双击 `setup\wingman-cert.cer`
2. 点击"安装证书"
3. 选择"本地计算机"→"将所有证书放入下列存储"→"受信任的根证书颁发机构"
4. 完成后不再出现警告

### Q: 杀毒软件误报？

A: 将 Wingman 安装目录添加到杀毒软件白名单。杀毒软件对自动化工具误报是常见现象。

### Q: 脚本运行但没反应？

A: 检查：
1. 管理员权限运行（Windows）
2. 游戏窗口在前台
3. 脚本中的坐标/颜色是否正确

### Q: 如何获取像素颜色？

A: 使用预览面板的区域选择工具，或使用截图工具取色

### Q: macOS 上如何获取屏幕权限？

A: 首次运行需要授予屏幕录制权限：
1. 系统偏好设置 → 安全性与隐私 → 隐私
2. 选择"屏幕录制"
3. 勾选 Wingman 并重启应用

### Q: Linux 上需要安装什么依赖？

A: Ubuntu/Debian 系统需要：
```bash
sudo apt-get install libx11-dev libxrandr-dev libxext-dev libxtst-dev
```

## 高级用法

### 远程控制

```bash
# 启动服务器
wingman-runtime start --port 9999

# 在另一台机器上连接
wingman-runtime connect 192.168.1.100:9999 script.lua
```

### 多脚本协作

```lua
local wingman = require("wingman")

-- 加入房间
wingman.team.join("farming_room")

-- 发送消息
wingman.team.send({
    action = "found_target",
    data = {x = 100, y = 200}
})
```

### HTTP API

Wingman 提供 RESTful API 用于外部控制：

```
POST /api/scripts/run
{
  "path": "scripts/auto_farm.lua"
}
```

详细 API 文档请参考 [API 文档](./API.md)。

---

更多信息请访问：[GitHub](https://github.com/cuihairu/wingman)
