# Wingman 开发路线图

> 类似 Chimpeeon 的游戏自动化工具完整开发计划

> **✅ 架构重构已完成 (2025)** - 采用 apps + lib 架构，详见 docs/architecture.md

> **📌 平台说明：第一版本仅支持 Windows，跨平台支持不在计划内**

## 参考项目分析

| 项目 | 语言 | 特点 | 参考价值 |
|------|------|------|----------|
| [PyAutoGUI](https://github.com/asweigart/pyautogui) | Python | 跨平台GUI自动化 | 屏幕操作API设计 |
| [AutoHotkey](https://github.com/AutoHotkey/AutoHotkey) | C++ | 热键/宏录制 | 触发器系统 |
| [OpenCV](https://opencv.org) | C++ | 图像识别 | 像素/图像检测 |
| [MicroMacro](https://github.com/Fallas/micromacro) | C++/Lua | 游戏自动化框架 | Lua绑定设计 |
| [Osenpa Auto Clicker](https://sourceforge.net/projects/osenpa-autoclicker) | C# | 图像/颜色检测 | UI设计 |
| [EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger) | C++ | Lua调试器 | 调试协议实现 |
| [Drogon](https://github.com/drogonframework/drogon) | C++ | Web框架 | C++ HTTP服务 |

---

## 🏗️ 新架构 (2025)

### 架构设计

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── client/                   ← 主应用
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── cli/              ← CLI 命令（一个文件一个命令）
│   │   │   │   ├── start.cpp
│   │   │   │   ├── stop.cpp
│   │   │   │   └── status.cpp
│   │   │   ├── modes/            ← 运行模式
│   │   │   │   ├── active_mode.cpp      ← 使用 TcpClient
│   │   │   │   ├── passive_mode.cpp     ← 使用 TcpServer
│   │   │   │   └── standalone_mode.cpp
│   │   │   └── gui/              ← GUI
│   │   │       ├── app.cpp
│   │   │       ├── panels/
│   │   │       └── widgets/
│   │   ├── include/wingman/client/
│   │   ├── tests/
│   │   └── CMakeLists.txt
│   │
│   └── inspector/                ← 检查工具
│       ├── src/
│       ├── include/
│       └── CMakeLists.txt
│
├── lib/wingman/                  ← 核心库
│   ├── include/wingman/
│   │   ├── screen.hpp            ← 屏幕捕获
│   │   ├── input.hpp             ← 输入模拟
│   │   ├── trigger.hpp           ← 触发器
│   │   ├── vision.hpp            ← 视觉识别
│   │   ├── behavior_tree.hpp     ← 行为树
│   │   ├── ocr.hpp               ← OCR
│   │   └── ...
│   ├── src/
│   ├── tests/
│   └── CMakeLists.txt
│
├── libs/                         ← 内部辅助库
│   ├── transport/                ← 网络传输（TCP/WebSocket）
│   │   ├── include/wingman/transport/
│   │   │   ├── transport.hpp     ← 传输抽象
│   │   │   ├── transport_client.hpp
│   │   │   ├── transport_server.hpp
│   │   │   ├── session/          ← 会话层
│   │   │   └── channel/          ← 消息通道
│   │   └── src/
│   │
│   ├── lua/                      ← Lua 绑定（桥接 Lua → 核心库）
│   │   ├── include/wingman/lua/
│   │   └── src/
│   │
│   └── proto/                    ← Protobuf
│
├── examples/                     ← 示例代码
├── scripts/                      ← 脚本
├── docs/                         ← 文档
└── CMakeLists.txt                ← 根 CMake（聚合构建）
```

### 架构分层

```
┌─────────────────────────────────────────────────────────┐
│                    apps/                                 │
│  ┌──────────────┐           ┌──────────────┐           │
│  │   client     │           │  inspector   │           │
│  │  (应用入口)   │           │  (检查工具)   │           │
│  └──────┬───────┘           └──────────────┘           │
└─────────┼───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│              lib/wingman/ (核心库)                        │
│  屏幕捕获、输入模拟、触发器、视觉识别、行为树、OCR...     │
└─────────┬───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│              libs/ (辅助库)                              │
│  ┌──────────┐  ┌──────┐  ┌──────┐                      │
│  │transport │  │ lua  │  │ proto│                      │
│  └──────────┘  └──────┘  └──────┘                      │
└─────────────────────────────────────────────────────────┘
```

### 调用链

```
Lua 脚本
    ↓
libs/lua/ (Lua 绑定)
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/client/ (应用：CLI/GUI + 运行模式)
```

### 运行模式

| 模式 | 说明 | Transport |
|------|------|-----------|
| ActiveMode | 主动连接到服务器 | TcpClient |
| PassiveMode | 被动监听，等待连接 | TcpServer |
| StandaloneMode | 单机模式，无网络 | - |

### 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`

### 重构里程碑

| 阶段 | 内容 | 状态 |
|------|------|------|
| Phase 1 | Protobuf 协议定义 + 多模块 CMake | ✅ 已完成 |
| Phase 2 | 目录结构重组 (apps + lib) | ✅ 已完成 |
| Phase 3 | 核心库迁移 (libs/core → lib/wingman) | ✅ 已完成 |
| Phase 4 | Client 重构 (主动/被动/单机) | ✅ 已完成 |
| Phase 5 | GUI 迁移到 apps/client | ✅ 已完成 |
| Phase 6 | EmmyLua 集成 | ✅ 已完成 |
| Phase 7 | 测试与文档 | ✅ 已完成 |
| Milestone 1 | MVP (最小可行产品) | ✅ 已完成 |

---

## Milestone 1: MVP (最小可行产品) ✅

**目标**: 实现基本的像素检测和输入模拟

**状态**: 已完成

### 1.1 核心模块 ✅
```cpp
// screen.cpp - 已实现
class Screen {
    HBITMAP capture(int x, int y, int w, int h);
    COLOR getPixel(int x, int y);
    Point* findColor(COLOR color, Rect region, int tolerance);
};

// input.cpp - 已实现
class Input {
    void click(int x, int y, int button);
    void move(int x, int y, int duration);
    void keyDown(int vk);
    void keyUp(int vk);
};
```

### 1.2 Lua 基础绑定 ✅
```lua
-- test.lua
local wingman = require("wingman")

-- 截图
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)

-- 查找红色
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        wingman.input.click(p.x, p.y)
    end
end
```

### 1.3 命令行工具 ✅
```bash
wingman-cli.exe start              # 启动服务
wingman-cli.exe stop               # 停止服务
wingman-cli.exe status             # 查看状态
wingman-cli.exe script.lua         # 运行脚本
```

**交付物**: 可运行的 CLI 版本

---

## Milestone 2: 触发器系统

**目标**: 实现 Chimpeeon 风格的条件触发机制

### 2.1 触发器定义 (YAML/Lua)
```yaml
# config/triggers.yaml
triggers:
  - name: "技能冷却检测"
    enabled: true
    condition:
      type: pixel
      color: 0x00FF00
      region: [100, 100, 50, 50]
      tolerance: 10
    action:
      type: key
      key: "1"
    cooldown: 500

  - name: "血量监控"
    enabled: true
    condition:
      type: pixel
      color: 0xFF0000
      region: [50, 50, 20, 20]
    action:
      type: macro
      name: "使用药水"
```

### 2.2 触发器引擎
```cpp
class TriggerEngine {
    void loadConfig(string path);
    void registerTrigger(Trigger* trigger);
    void update();
    void enable(string name);
    void disable(string name);
};
```

**交付物**: 配置驱动的自动化系统

---

## Milestone 3: 宏系统

**目标**: 录制和回放用户操作

### 3.1 录制格式 (JSON)
```json
{
  "name": "采集路径",
  "version": "1.0",
  "actions": [
    {"type": "move", "x": 100, "y": 200, "duration": 500},
    {"type": "click", "x": 100, "y": 200, "button": "left"},
    {"type": "delay", "ms": 1000},
    {"type": "key", "key": "F1"}
  ]
}
```

### 3.2 宏编辑器 (可选)
- 可视化编辑器
- 时间轴视图
- 动作拖拽排序

**交付物**: 宏录制/回放功能

---

## Milestone 4: 远程控制

**目标**: 通过网络远程控制游戏

### 4.1 TCP 协议
```protobuf
message Request {
    uint32 id = 1;
    string action = 2;
    bytes params = 3;
}

message Response {
    uint32 id = 1;
    bool success = 2;
    bytes data = 3;
    string error = 4;
}
```

### 4.2 Python Client
```python
from wingman import Client

client = Client("localhost", 9999)

# 截图
screenshot = client.screen.capture()

# 查找并点击
points = client.screen.find_color(0xFF0000)
if points:
    client.input.click(points[0].x, points[0].y)

# 运行脚本
result = client.script.run_file("auto_farm.lua")
```

**交付物**: Python SDK + 协议文档

---

## Milestone 5: GUI 界面

**目标**: 类似 Chimpeeon 的可视化配置界面

### 5.1 主界面布局
```
┌─────────────────────────────────────────────────────────┐
│ Wingman                          [●] [⏸] [⏹] [⚙]        │
├──────────┬──────────────────────────────┬──────────────┤
│          │                              │              │
│ Triggers │        Preview               │   Actions    │
│          │                              │              │
│ □ 技能CD  │   ┌────────────────────┐    │  □ 点击      │
│ □ 血量    │   │                    │    │  □ 按键      │
│ □ 蓝量    │   │    [Screenshot]    │    │  □ 宏        │
│ □ 自动战斗│   │                    │    │              │
│          │   └────────────────────┘    │              │
│ [+] 添加  │                              │              │
│          │   [🔍 区域选择]              │              │
├──────────┴──────────────────────────────┴──────────────┤
│ [Log] 检测到绿色 (100, 200) → 按键 1                      │
│ [Log] 触发器 "技能CD" 执行，冷却 500ms                   │
└─────────────────────────────────────────────────────────┘
```

### 5.2 技术选型
- **Dear ImGui** - 轻量级，游戏内调试
- **Qt6** - C++ 原生，跨平台 (后期)
- **Electron + Web** - 现代化 UI (后期)

**交付物**: 图形化配置工具

---

## Milestone 6: 人性化模拟

**目标**: 降低被检测风险

### 6.1 贝塞尔曲线鼠标移动
```cpp
class HumanMouse {
    void moveTo(int x, int y, int duration);
    void click(int x, int y, ClickType type);

private:
    vector<Point> generateBezier(Point start, Point end, int ctrlPoints);
    int addRandomness(int base, int variance);
};
```

### 6.2 随机延迟系统
```lua
human.config = {
    minDelay = 50,
    maxDelay = 150,
    clickVariance = 5,
    pathVariance = 10
}
```

**交付物**: 防检测模拟系统

---

## Milestone 7: EmmyLua 调试器集成

**目标**: 集成成熟的 EmmyLua 调试器，提供完整 Lua 开发体验

### 7.1 EmmyLuaDebugger

[EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger) 是成熟的跨平台 Lua 调试器：

**特性：**
- ✅ 完整调试功能：断点、单步、变量监视、调用栈
- ✅ 跨平台支持：Windows、macOS、Linux
- ✅ 多 Lua 版本：Lua 5.1-5.5、LuaJIT
- ✅ 高性能 TCP 通信

### 7.2 集成方式

**CMake 配置:**
```cmake
# Fetch EmmyLuaDebugger as dependency
FetchContent_Declare(
    emmylua
    GIT_REPOSITORY https://github.com/EmmyLua/EmmyLuaDebugger.git
    GIT_TAG        master
)

# 构建对应 Lua 版本
set(EMMY_LUA_VERSION 54)  # Lua 5.4
FetchContent_MakeAvailable(emmylua)

target_link_libraries(lua PRIVATE emmy_core)
```

**Agent 端集成:**
```cpp
// lua/src/debug_adapter.cpp
#include "emmy_core.h"

namespace wingman::lua {

class EmmyAdapter {
public:
    // 启动调试监听
    void enable(int port = 9966) {
        luaGetGlobal(L, "require");
        luaPushString(L, "emmy_core");
        luaCall(L, 1);

        luaGetField(L, -1, "tcpListen");
        luaPushString(L, "0.0.0.0");
        luaPushInteger(L, port);
        luaCall(L, 2, 0);
    }

    // 等待 IDE 连接
    void wait() {
        luaGetGlobal(L, "require");
        luaPushString(L, "emmy_core");
        luaCall(L, 1);

        luaGetField(L, -1, "waitIDE");
        luaCall(L, 0, 0);
    }
};

} // namespace wingman::lua
```

**Lua 脚本端:**
```lua
-- 调试模式启动
local emmy = require('emmy_core')
emmy.tcpListen('localhost', 9966)
emmy.waitIDE()

-- 你的脚本代码
local wingman = require('wingman')
-- ...
```

### 7.3 IDE 支持

**VS Code:**
- 安装 [EmmyLua 扩展](https://github.com/emmylua/vscode-emmylua)
- 配置 launch.json:
```json
{
  "type": "emmylua",
  "request": "attach",
  "name": "Attach to Wingman",
  "host": "localhost",
  "port": 9966,
  "ext": [
    ".lua",
    ".lua.txt"
  ]
}
```

**IntelliJ IDEA:**
- 安装 EmmyLua 插件
- 配置 TCP Attach 连接

**交付物**: EmmyLua 集成完成，IDE 调试可用

---

## Milestone 8: 发布准备

**目标**: 开源项目发布

### 8.1 分发
- [x] 安装程序 (InnoSetup)
- [x] 便携版 ZIP
- [x] 自签名证书生成脚本
- [ ] 自动更新 (可选)

### 8.2 文档
- [x] 用户手册
- [x] API 参考
- [ ] 视频教程
- [x] 示例脚本库

---

## 时间估算

| Milestone | 工作量 | 依赖 |
|-----------|--------|------|
| M1: MVP | 4 周 | - |
| M2: 触发器 | 3 周 | M1 |
| M3: 宏系统 | 2 周 | M1 |
| M4: 远程控制 | 2 周 | M1 |
| M5: GUI 界面 | 4 周 | M2 |
| M6: 人性化 | 1 周 | M1 |
| M7: 调试器 | 3 周 | M1 |
| M8: 发布准备 | 2 周 | 全部 |

**总计**: ~21 周 (约 5 个月)

---

## 下一阶段行动

### 🔥 当前重点

| 优先级 | 任务 | 预计时间 | 状态 |
|--------|------|----------|------|
| P0 | Milestone 2: 触发器系统 | 3周 | 🚧 进行中 |
| P1 | Milestone 3: 宏系统 | 2周 | ⏳ 待开始 |
| P1 | Milestone 4: 远程控制 | 2周 | ⏳ 待开始 |

### 📋 检查清单

#### Phase 7: 测试与文档 (已完成)
- [x] Windows 构建 - MSVC + x64-windows-static
- [x] 集成测试覆盖 - 57 个测试通过
- [x] 性能基准测试 - KVStore 性能指标
- [x] 文档完善 - BUILD.md + 更新 README

#### 测试覆盖详情
| 模块 | 测试数 | 状态 |
|------|--------|------|
| core | 58/58 | ✅ KVStore + Trigger + Recorder |
| transport | 7/7 | ✅ Message 序列化 |
| proto | 7/7 | ✅ JSON 封装 |
| debug | 4/4 | ✅ 状态枚举 |
| **总计** | **76/76** | ✅ **全部通过** |

#### Milestone 2: 触发器系统 (已完成)
- [x] TriggerManager - 完整的触发器生命周期管理
- [x] TriggerEngine - Lua配置加载支持
- [x] 触发器测试 - 16个测试用例
- [x] 示例配置 - config/triggers.lua
- [x] 集成Lua库 - trigger_engine.cpp链接

#### Milestone 3: 宏系统 (已完成)
- [x] MacroRecorder - 鼠标/键盘录制
- [x] Hook支持 - 低级键盘鼠标钩子
- [x] 保存为Lua脚本 - saveToLua()
- [x] 保存/加载JSON - saveToJSON/loadFromJSON
- [x] 回放功能 - playback(speed, repeat)
- [x] 宏系统测试 - 17个测试用例

#### 性能指标
| 操作 | 性能 |
|------|------|
| SET | 0.04 μs/op |
| GET | 0.03 μs/op |
| DELETE | 0.21 μs/op |
| INCR | 0.07 μs/op |
| HSET | 0.04 μs/op |
| HGET | 0.04 μs/op |
| LPUSH | 0.12 μs/op |
| LPOP | 0.20 μs/op |
| WRITE 吞吐量 | 10M ops/sec |

---

参考项目：[moderncpp-project-template](https://github.com/madduci/moderncpp-project-template)
