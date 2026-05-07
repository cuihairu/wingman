# Wingman 开发路线图

> 类似 Chimpeeon 的游戏自动化工具完整开发计划

> **🚧 架构重构中 (2025)** - 正在进行多模块架构重构，详见下方 "重构计划"

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

## 🏗️ 架构重构计划 (2025)

### 重构目标

1. **多模块架构** - 分离核心库、Client、Server，便于维护和扩展
2. **双模运行** - Client 支持主动连接和被动监听同时开启（双保险）
3. **EmmyLua集成** - 使用成熟的 Lua 调试器而非自研
4. **Go控制端** - Server 改用 Go 实现，利用其并发优势

### 新架构设计

```
wingman/
├── proto/                    # Protobuf 协议定义
│   ├── agent_api.proto       # Client API
│   ├── common.proto          # 公共类型
│   └── debug.proto           # 调试协议
│
├── libs/                     # 共享库
│   ├── proto/        # Protobuf C++ 封装
│   ├── transport/    # 传输层 (TCP/WebSocket)
│   ├── core/         # 核心功能 (Screen/Input/Trigger等)
│   ├── lua/          # Lua 引擎 + EmmyLua 集成
│   └── debug/        # 调试器客户端
│
├── client/                   # C++ Client (被动执行)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── active_mode.cpp   # 主动连接 Server
│   │   ├── passive_mode.cpp  # 被动监听端口
│   │   └── script_engine.cpp # Lua 脚本执行
│   ├── CMakeLists.txt
│   └── client.toml           # Client 配置
│
├── server/                   # Go Server (主动控制)
│   ├── cmd/
│   │   └── wingmand/
│   ├── internal/
│   │   ├── api/              # HTTP API
│   │   ├── client/           # Client 管理
│   │   └── workflow/         # 工作流编排
│   ├── proto/                # Go Protobuf 生成代码
│   ├── web/                  # 前端资源
│   └── go.mod
│
└── debugger/                 # EmmyLua 调试器集成
    ├── emmy_core/            # EmmyLua 核心库
    └── adapter/              # 适配层
```

### Client 配置 (client.toml)

```toml
# 运行模式配置
[mode]
enable_active = true           # 启用主动连接
enable_passive = true          # 启用被动监听
connection_strategy = "fallback"  # fallback/parallel/primary

# 主动模式配置
[active]
server_ip = "192.168.1.100"
server_port = 9527
reconnect_interval = 5         # 重连间隔(秒)
heartbeat_interval = 30        # 心跳间隔(秒)

# 被动模式配置
[passive]
listen_ip = "0.0.0.0"
listen_port = 9528

# 单机模式配置
[standalone]
script_dir = "./scripts"
auto_start = []

# 调试器配置
[debugger]
enable = true
listen_port = 9966
```

### Protobuf 协议设计

```protobuf
// common.proto
syntax = "proto3";
package wingman;

message JsonData {
    string data = 1;          // JSON 字符串包裹，方便脚本扩展
}

message Empty {}

// agent_api.proto
syntax = "proto3";
package wingman;

service AgentService {
    // 屏幕操作
    rpc CaptureScreen(CaptureRequest) returns (CaptureResponse);
    rpc FindColor(FindColorRequest) returns (FindColorResponse);

    // 输入模拟
    rpc Click(ClickRequest) returns (Empty);
    rpc Move(MoveRequest) returns (Empty);
    rpc Key(KeyRequest) returns (Empty);

    // 脚本控制
    rpc RunScript(ScriptRequest) returns (ScriptResponse);
    rpc StopScript(StopRequest) returns (Empty);
    rpc ListScripts(Empty) returns (ScriptList);

    // 触发器管理
    rpc AddTrigger(TriggerConfig) returns (Empty);
    rpc RemoveTrigger(RemoveTriggerRequest) returns (Empty);
    rpc ListTriggers(Empty) returns (TriggerList);
}

// JSON 包裹示例
message ScriptRequest {
    string script_path = 1;
    JsonData params = 2;      // JSON 参数，灵活性高
}
```

### EmmyLua 集成

```cpp
// lua/src/debug_adapter.cpp
#include "emmy_core.h"

namespace wingman::lua {

class DebugAdapter {
public:
    void enable(int port = 9966) {
        // 加载 EmmyLua 模块
        lua_getglobal(L_, "require");
        lua_pushstring(L_, "emmy_core");
        lua_call(L_, 1, 1);

        // 启动 TCP 监听
        lua_getfield(L_, -1, "tcpListen");
        lua_pushstring(L_, "0.0.0.0");
        lua_pushinteger(L_, port);
        lua_call(L_, 2, 0);
    }

    void wait() {
        // 等待 IDE 连接
        lua_getglobal(L_, "require");
        lua_pushstring(L_, "emmy_core");
        lua_call(L_, 1, 1);

        lua_getfield(L_, -1, "waitIDE");
        lua_call(L_, 0, 0);
    }
};

} // namespace wingman::lua
```

### 重构里程碑

| 阶段 | 内容 | 状态 |
|------|------|------|
| Phase 1 | Protobuf 协议定义 + 多模块 CMake | ✅ 已完成 |
| Phase 2 | wingman-* 共享库迁移 | ✅ 已完成 |
| Phase 3 | Client 重构 (主动/被动/单机) | ✅ 已完成 |
| Phase 4 | EmmyLua 集成 | ✅ 已完成 |
| Phase 5 | Go Server 实现 | ✅ 已完成 |
| Phase 6 | 测试与迁移 | 🚧 进行中 |

---

## 原架构 (待废弃)

```
┌─────────────────────────────────────────────────────────────┐
│                        Wingman (旧)                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │   Client    │  │   Web UI    │  │   VS Code Debug     │ │
│  │ (Python/JS) │  │ (Dashboard) │  │      Extension      │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
│         │                │                    │             │
│         └────────────────┴────────────────────┘             │
│                          │                                 │
│         ┌────────────────▼────────────────┐                │
│         │       TCP / WebSocket           │                │
│         └────────────────┬────────────────┘                │
│                          │                                 │
│  ┌───────────────────────▼──────────────────────────────┐  │
│  │                   Core Engine                        │  │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐         │  │
│  │  │ Screen │ │ Input  │ │Window  │ │Process │         │  │
│  │  └────────┘ └────────┘ └────────┘ └────────┘         │  │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐         │  │
│  │  │ Trigger│ │  Macro │ │ Human  │ │  Log   │         │  │
│  │  └────────┘ └────────┘ └────────┘ └────────┘         │  │
│  │  ┌─────────────────────────────────────────┐          │  │
│  │  │           Lua Runtime                  │          │  │
│  │  └─────────────────────────────────────────┘          │  │
│  └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Milestone 1: MVP (最小可行产品)

**目标**: 实现基本的像素检测和输入模拟

### 1.1 核心模块 (2周)
```cpp
// screen.cpp
class Screen {
    HBITMAP capture(int x, int y, int w, int h);
    COLOR getPixel(int x, int y);
    Point* findColor(COLOR color, Rect region, int tolerance);
};

// input.cpp
class Input {
    void click(int x, int y, int button);
    void move(int x, int y, int duration);
    void keyDown(int vk);
    void keyUp(int vk);
};
```

### 1.2 Lua 基础绑定 (1周)
```lua
-- test.lua
local wingman = require("wingman")

-- 截图
local screenshot = screen.capture(0, 0, 1920, 1080)

-- 查找红色
local points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        input.click(p.x, p.y)
    end
end
```

### 1.3 命令行工具 (1周)
```bash
wingman.exe script.lua              # 运行脚本
wingman.exe --record macro.lua      # 录制宏
wingman.exe --test pixel.lua        # 测试模式
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
- **Qt6** - C++ 原生，跨平台
- **Dear ImGui** - 轻量级，游戏内调试
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

## 下一阶段行动 (重构优先)

### 🔥 当前重点 - 多模块架构重构

| 优先级 | 任务 | 预计时间 | 状态 |
|--------|------|----------|------|
| P0 | 定义 Protobuf 协议 (agent_api.proto, common.proto) | 1天 | ✅ |
| P0 | 创建多模块 CMake 结构 (libs/, client/) | 1天 | ✅ |
| P1 | 迁移核心代码到 core | 2天 | ✅ |
| P1 | 实现 Client 主动/被动模式 | 2天 | ✅ |
| P1 | 集成 EmmyLuaDebugger | 1天 | ✅ |
| P2 | Go Server 基础框架 | 3天 | ✅ |
| P2 | Protobuf Go 代码生成 | 1天 | ✅ |

### 📋 重构检查清单

#### Phase 1: 协议与结构
- [x] 创建 proto/ 目录
- [x] 定义 common.proto (JsonData 包装)
- [x] 定义 agent_api.proto (完整 API)
- [x] 定义 debug.proto (调试协议)
- [x] 创建 libs/ 子目录结构
- [x] 更新根 CMakeLists.txt 支持多模块

#### Phase 2: 核心库迁移
- [x] 创建 core (Screen/Input/Trigger/Window...)
- [x] 创建 lua (Lua 引擎 + 绑定)
- [x] 创建 proto (Protobuf C++ 封装)
- [x] 创建 transport (TCP/WebSocket)

#### Phase 3: Client 重构
- [x] 实现 ActiveMode (主动连接 Server)
- [x] 实现 PassiveMode (被动监听)
- [x] 实现 StandaloneMode (单机脚本)
- [x] client.toml 配置解析

#### Phase 4: EmmyLua 集成
- [x] 添加 EmmyLuaDebugger 为依赖
- [x] 创建 debug 适配层
- [x] 测试 VS Code 断点调试

#### Phase 5: Go Server
- [x] 初始化 Go 项目结构
- [x] 生成 Protobuf Go 代码
- [x] 实现 Client 管理器
- [x] 实现 HTTP API

#### Phase 6: 测试与迁移 (进行中)
- [ ] 跨平台构建测试 (Windows/macOS/Linux)
- [ ] 集成测试覆盖
- [ ] 性能基准测试
- [ ] 文档完善

---

需要我开始实现哪个任务？
