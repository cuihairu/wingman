# Wingman 开发路线图

> 类似 Chimpeeon 的游戏自动化工具完整开发计划

> **架构重构已完成 (2025)** - 采用 apps + lib 架构，详见 docs/architecture.md

> **平台说明：支持 Windows（主要）、macOS、Linux；Android 端侧 Agent 实验性支持（A1 链路/A2 脚本能力/A3（token 认证 + 可靠性自愈）已落地，见 Milestone 9 与 docs/android-agent-design.md）。跨平台通过平台抽象层实现，详见 docs/architecture.md 与 docs/platforms.md**

> **脚本层多语言抽象已完成 (2026-05)** - 支持 Lua (sol2) 和 Python (pybind11)，详见下文 "脚本引擎抽象"

## 参考项目分析

| 项目 | 语言 | 特点 | 参考价值 |
|------|------|------|----------|
| [AutoHotkey](https://github.com/AutoHotkey/AutoHotkey) | C++ | 热键/宏录制 | 触发器系统 |
| [OpenCV](https://opencv.org) | C++ | 图像识别 | 像素/图像检测 |
| [MicroMacro](https://github.com/Fallas/micromacro) | C++/Lua | 游戏自动化框架 | Lua绑定设计 |
| [Osenpa Auto Clicker](https://sourceforge.net/projects/osenpa-autoclicker) | C# | 图像/颜色检测 | UI设计 |
| [EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger) | C++ | Lua调试器 | 调试协议实现 |

---

## 新架构 (2025)

### 架构设计

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── runtime/                  ← 主运行时（outbound agent + 本地 IPC 控制面）
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── agent.cpp         ← 运行时编排入口
│   │   │   ├── local_ipc_server.cpp ← 本地 IPC 控制面
│   │   │   └── standalone_mode.cpp  ← 本地脚本/触发器执行能力
│   │   ├── include/wingman/runtime/
│   │   ├── tests/
│   │   └── CMakeLists.txt
│   │
│   └── gui/                      ← 本地 Tauri GUI（通过 IPC 控制 runtime）
│       ├── src-tauri/
│       ├── src/
│       └── package.json
│
├── lib/wingman/                  ← 核心库
│   ├── include/wingman/
│   │   ├── screen.hpp            ← 屏幕捕获
│   │   ├── platform/iinput.hpp   ← 输入模拟接口
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
│   ├── transport/                ← 网络传输：TCP/UDP（自定义帧）
│   │   ├── include/wingman/transport/
│   │   │   ├── transport.hpp     ← 传输抽象
│   │   │   ├── transport_client.hpp
│   │   │   ├── transport_server.hpp
│   │   │   ├── session/          ← 会话层
│   │   │   └── channel/          ← 消息通道
│   │   └── src/
│   │
│   ├── lua/                      ← Lua 引擎（sol2 绑定，实现 IScriptEngine）
│   │   ├── include/wingman/lua/
│   │   └── src/
│   │
│   ├── python/                   ← Python 引擎（pybind11 + CPython，实现 IScriptEngine）[可选]
│   │   ├── include/wingman/python/
│   │   └── src/
│   │
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
│  │   runtime    │           │     gui      │           │
│  │ (主动 Agent) │           │ (本地 IPC UI)│           │
│  └──────┬───────┘           └──────────────┘           │
└─────────┼───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│           lib/wingman/ (核心库 + 脚本抽象)               │
│  ScriptManager (语言无关) + IScriptEngine 接口           │
│  41 ModuleDescriptor (screen/input/window/...)           │
│  屏幕捕获、输入模拟、触发器、视觉识别、行为树、OCR...     │
└─────────┬───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│         libs/ (引擎与链路实现)                            │
│  ┌──────────┐ ┌──────┐ ┌──────┐ ┌─────────┐ ┌────────┐ │
│  │transport │ │ lua  │ │python│ │agentcore│ │android │ │
│  │          │ │      │ │      │ │         │ │agent   │ │
│  └──────────┘ └──────┘ └──────┘ └─────────┘ └────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 调用链

```
Lua/Python 脚本 (.lua / .py)
    ↓
ScriptManager (自动检测语言)
    ↓
ScriptEngineFactory → LuaScriptEngine / PythonScriptEngine
    ↓
ModuleDescriptor (41 语言无关模块) → C++ 核心 API
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/agent/ (应用：CLI + outbound agent + local IPC)
```

### 脚本引擎抽象

```
                        apps/agent
                             │
                    ┌────────▼────────┐
                    │  ScriptManager  │ (语言无关)
                    │  owns IScriptEngine │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  IScriptEngine  │ (纯虚接口)
                    └───┬────────┬───┘
                        │        │
          ┌─────────────▼──┐  ┌──▼──────────────┐
          │ LuaScriptEngine│  │PythonScriptEngine│
          │ (sol2)         │  │ (pybind11+CPython)│
          └────────────────┘  └─────────────────┘
                    │        │
                    └───┬────┘
                ┌────────▼────────┐
                │ ModuleDescriptor │ (语言无关的模块定义)
                │ screen/input/... │ (41 模块)
                └─────────────────┘
```

**启用 Python 支持：**
```bash
cmake -B build -DWINGMAN_ENABLE_PYTHON=ON ...
vcpkg install pybind11
```

### Runtime 控制面能力

Runtime 不再按互斥“运行模式”建模。远程编排和本地 UI 控制是两条可以共存的控制面：

| 能力 | 说明 | 默认关系 |
|------|------|----------|
| Remote Agent | runtime 主动 outbound 连接 Go orchestrator，由 Go server 做中控编排 | 可与本地 IPC 同时启用 |
| Local IPC Control | Tauri UI -> Rust backend -> local IPC -> runtime，用于本机控制 | 可与 Remote Agent 同时启用 |
| Standalone Execution | 本地脚本、触发器和自动化执行能力 | 被 Local IPC 和 CLI 复用 |

`wingman-agent start --standalone` 只是便捷启动参数：它强制关闭远程连接，只启动本地执行能力和 local IPC。它不是说本地 UI 与远程 agent 在架构上互斥。

禁止把本地 UI 控制实现成 runtime HTTP/WebSocket server。Local TCP 只允许显式 debug fallback，默认关闭。

### 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`
5. **不涉及账号管理** - 不做账号 CRUD、登录轮询、用户认证等业务功能；工具类函数（如 TOTP 验证码生成）保留为无状态纯函数，持久化需求由 kv 模块承担

### 重构里程碑

| 阶段 | 内容 | 状态 |
|------|------|------|
| Phase 1 | Protobuf 协议定义 + 多模块 CMake（协议已演进为 16B 头 + JSON 帧，落地于 libs/transport） | ✅ 已完成 |
| Phase 2 | 目录结构重组 (apps + lib) | ✅ 已完成 |
| Phase 3 | 核心库迁移 (libs/core → lib/wingman) | ✅ 已完成 |
| Phase 4 | Runtime outbound agent + local execution 能力整理 | ✅ 已完成 |
| Phase 5 | Tauri GUI 通过 local IPC 控制 runtime | ✅ 已完成（54 个 Tauri 命令覆盖脚本/触发器/宏/系统/截屏/配置/档案/事件/托盘，三平台 CI 构建） |
| Phase 6 | EmmyLua 集成 | ✅ 已完成 |
| Phase 7 | 测试与文档 | ✅ 已完成 |
| Phase 8 | 脚本层抽象 (IScriptEngine) + Lua 引擎；Python 为可选后端（默认关闭，按需启用；引擎已纳入主 CI——Linux 全量测试维度 + Windows 链接链路 job，默认启用经评估维持关闭，2026-09-26） | ✅ |
| Phase 9 | 移除账号/二维码/认证模块，TOTP 重构为纯函数 | ✅ 已完成 |
| Milestone 1 | MVP (最小可行产品) | ✅ 已完成 |

---

## Milestone 1: MVP (最小可行产品)

**目标**: 实现基本的像素检测和输入模拟

**状态**: 已完成

### 1.1 核心模块
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

### 1.2 Lua 基础绑定
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

### 1.3 命令行工具
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

## Milestone 4: 远程编排

**目标**: runtime 作为 agent 主动连接 Go orchestrator，由 Go server 做远程中控编排

**状态**: 完成 (2026-09-22 校准) — 交付物全部落地；三层契约审计干净（server 下发 7 类命令 runtime 全支持；runtime 上行 trigger_fired/script_state/script_output 三类事件 server 全部转发，log.line 有意不转发防高频淹没上行；server 广播的 agent/workflow/script/screenshot 事件 Dashboard wsService 全部消费）；Go server 14 包测试 stmt/branch/func 100% 覆盖，三套 UI 基线全绿（Dashboard jest 235、GUI vitest 506、Rust cargo 14）

**交付物**:
- [Runtime outbound client（`libs/agentcore/src/remote_client.cpp`：心跳 30s + 重连 + 指数退避 + 注册 + 有界 outbox）]
- [Go orchestrator 作为远程中控边界（`orchestrator/server/`）]
- [Agent 注册、心跳、命令下发、结果回传（FrameListener TCP 协议 + 30s/90s 超时）]
- [工作流引擎（`internal/workflow/engine.go`：DAG 调度 + 环检测 + 持久化 + 取消/超时 + 指数退避重试 + 负载均衡 + wait 步骤 + 模板库）]
- [Team/投票/Inbox 多 agent 协同（`internal/agent/team.go`）]
- [JWT auth + bcrypt + 限流 + 审计日志]
- [Dashboard 仅连接 Go server（`orchestrator/dashboard/` wsService），不直接连接 runtime]
- [RBAC 权限系统（`internal/rbac/` + Role/Permission 模型 + `PermissionRequired` 中间件 + 用户/角色管理 API + Dashboard Admin 页面；`PermissionRequired` 已接线 8 权限码，历史「已实现未接线」缺陷已修复）]
- [Debugger 端点（`/api/debugger/info` 直连模式契约：返回各 agent host:9966 + launch.json，非中转）]
- [Runtime IPC 事件推送（EventBuffer + `events.drain` pull 模型 + log/trigger/script/connection 事件 + 公平性优先丢 log.line + dropped 计数）]
- [Go orchestrator 测试覆盖（600+ 个测试函数：rbac/workflow/handlers/agent/ws/middleware/debugger，`go test ./...` 全绿）]
- [跨语言 frame/protocol 集成测试（`integration/protocol_test.go`：按 C++ `MessageHeader` 16 字节小端布局的字节级裸 TCP 读写，覆盖 agent.register/agent.heartbeat/command dispatch+result/event report 四类帧 + 边界（超大 payload>16MiB 断连、非法 JSON/未知类型/截断 header 容错、断线重连恢复），`go test ./...` 全绿）]

**目标命令流**:

```text
runtime -> Go server: agent.register / agent.heartbeat / event.*
Go server -> runtime: command.*
runtime -> Go server: command.result
dashboard/browser -> Go server: HTTP/WebSocket
```

Runtime 不作为远程控制 server 被 Go server 反向拨入。

---

## Milestone 5: GUI 界面

**目标**: 类似 Chimpeeon 的可视化配置界面

**状态**: 完成 (2026-09-22 校准) — 本地 Tauri GUI 六页面 + 远程 Dashboard 九页面全部可用；GUI↔IPC↔Runtime 跨语言集成测试（Rust spawn 真 runtime 子进程）通过；macOS 真机运行时验证为独立遗留项（见 todo.md 跨平台验证）

**已完成**:
- [Tauri 2.0 框架集成]
- [Rust backend 通过 local IPC 连接 runtime]
- [IPC 方法路由 (script.start, script.stop, script.list, system.*)]
- [前端界面原型]
- [IPC 连接状态管理（状态机 + 指数退避重连 + 连接诊断，2026-09）]
- [触发器可视化配置（截图拾取 + 全类型 + 值转换层，2026-09）]
- [屏幕预览面板（拖拽选区 + 真实取色 + 匹配高亮，2026-09）]
- [日志实时显示（来源标记 + 丢弃提示 + 智能滚动，2026-09）]
- [脚本管理增强（2026-09）：runtime RPC 新增 script.pause/resume/restart/unload；script.list 返回 state/error/loadedAt；GUI 脚本页五态生命周期管理（按状态操作矩阵、批量暂停/恢复/停止、运行时长、错误展示）；script.state_changed 事件实时联动列表状态]

**已完成**:
- [日志实时显示（2026-09）：日志来源标记（GUI/Runtime 徽标 + 按来源筛选）；runtime EventBuffer 溢出丢弃提示（Rust 端透传 events.drain 的 dropped/remaining，累计丢弃时显示横幅）；settings.logLevel 联动入口过滤（debug 级别映射）；智能自动滚动（上滚暂停跟随 + 「N 条新日志」跳转按钮）；单条点击复制；导出含来源列]
- [屏幕预览面板（2026-09）：预览图直接交互——拖拽框选区域（同步 RegionPicker）+ 单击拾取真实像素颜色（离屏 canvas 读色，替代原假取色）+ 悬停跟随读数（屏幕绝对坐标 + hex + 放大镜）+ 坐标/颜色/区域一键复制 + 颜色匹配高亮（与 runtime Color::matches 同公式：欧氏距离 ≤ tolerance，品红叠加层 + 匹配计数）+「设为查找颜色」联动；修复副显示器时 overlay 错位（region 与捕获起点换算）]
- [触发器可视化配置（2026-09）：ScreenPickerModal 截图拾取组件（拖拽选区 + 放大镜取色，坐标换算到屏幕绝对坐标）；ColorPicker/RegionPicker 接通屏幕取色/框选；条件类型补全至 runtime 全部 11 种（窗口/进程/像素变化，分组展示）；动作类型补全至 10 种并按类型结构化编辑（坐标拾取/delay/路径等）+ 动作排序；GUI⇄runtime 值格式转换层（#rrggbb⇄0xRRGGBB、键名⇄VK 码、像素坐标⇄region）；修复 get_triggers 丢条件/动作、颜色值 runtime 无法解析两个缺陷]
- [IPC 连接状态、断线重连、错误提示完善（2026-09）：GUI 端 IPC 链路状态机（disconnected/connecting/connected/reconnecting/error）+ 指数退避自动重连（1s→30s，可设置开关）+ 手动断开抑制自动重连 + TopBar 五态连接指示（tooltip 显示端点/错误/重试次数，可点击立即重试）+ Settings 连接诊断（实际端点/错误信息/重试按钮）+ Rust `get_ipc_state` 诊断命令]

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
- **Tauri 2.0 + Web UI** - 本地桌面控制台
- **Tauri invoke + Rust IPC client** - GUI 前端不直接连接 runtime；Rust backend 通过本地 IPC 控制 runtime
- **Windows Named Pipe / macOS/Linux Unix Domain Socket** - 默认 local IPC；Local TCP 仅显式 debug fallback

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

[EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger) 是成熟的 Lua 调试器：

**特性：**
- [完整调试功能：断点、单步、变量监视、调用栈]
- [多 Lua 版本：Lua 5.1-5.5、LuaJIT]
- [高性能 TCP 通信]

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

**交付物**: EmmyLua 调试链路依赖外部 emmy_core 组件，仓库不分发（构建期无开关，缺少即调试链路不可用）

> 注（2026-09-22）：实际调试链路是脚本运行时 `require('emmy_core')`（依赖外部 emmy_core 组件，仓库不分发；构建期无开关，缺少即调试链路不可用）。C++ 侧的 `libs/debug` EmmyAdapter 从未被任何代码调用，已作为死代码移除。

---

## Milestone 8: 发布准备

**目标**: 开源项目发布

### 8.1 分发
- [x] 安装程序 (InnoSetup)
- [x] 便携版 ZIP
- [x] 自签名证书生成脚本
- [x] 自动更新 (可选)——Go server 发布/manifest/下载 API（admin 发布、
  公开 latest/download、sha256/size 服务端实测）+ lib 客户端核心
  （manifest 解析、semver 比较、流式 sha256 校验、file:// 可测传输层）；
  runtime 轮询触发与 GUI 安装编排未接（后续按需补）

### 8.2 文档
- [x] 用户手册
- [x] API 参考
- [x] 远程控制协议文档
- [ ] 视频教程——六集系列分镜脚本已备（docs/tutorials/video-storyboard.md，
      含逐镜旁白与录制规格），实拍录制需真实 Windows 环境人工完成
- [x] 示例脚本库

---

## Milestone 9: 移动端 Agent（Android）

**目标**: Android 设备作为 agent 接入 Go Server 中控（架构硬约束不变：agent 主动 outbound TCP、无本地 UI、零监听端口），远程执行 Lua 脚本并具备手势注入/屏幕采集/找色找图/远程截图能力

**状态**: 部分完成（2026-09-30 校准）— A1/A2 已实施、A3-P1 已落地，A3 剩余可靠性项与 A4 未排期。设计基线 [docs/android-agent-design.md](docs/android-agent-design.md)，平台与 API 适用性 [docs/platforms.md](docs/platforms.md)

| 阶段 | 内容 | 状态 |
|------|------|------|
| A1 链路打通 | Dashboard→Go Server `run_script{content}`→设备端 NDK C++ 核心执行 Lua→`agent.event` 日志实时回传；Kotlin 壳（WingmanService 前台服务/MainActivity/JNI 窄接口）；复用 16B 帧头 + JSON 协议，零新消息类型 | ✅ 已完成（2026-09-19 设计并落地，ce4a648；2026-09-20 起 nightly 打 arm64 APK） |
| A2 脚本能力 | `platform/android` 宿主桥：dispatchGesture 手势注入、MediaProjection 采集、找色找图、`screenshot.capture` 远程截图；反向 JNI 桥 | ✅ 已完成（2026-09-21，c1df705/c0a67fe） |
| A3 可靠性 | A3-P1 register token 白名单（`WINGMAN_AGENT_TOKENS`，server+桌面+Android 三端，默认关闭、向后兼容）✅ 已落地（2026-09-20，4c9a8f4，见 [docs/agent-token-auth-design.md](docs/agent-token-auth-design.md)）；A3 剩余可靠性项（开机自启、崩溃自重启、断连缓存自治、机型保活指引）✅ 已落地（2026-09-30，见设计文档 §7/§10'：BootCompletedReceiver + 退避闹钟崩溃重启 + 核心看门狗 + [docs/guides/android-keep-alive.md](docs/guides/android-keep-alive.md)，Kotlin JVM 单测 15 例入 build-android CI job）；Android 13+ 受限设置引导 ✅ 已落地（2026-09-30，设计文档 §10''：[docs/guides/android-restricted-settings.md](docs/guides/android-restricted-settings.md) + scripts/android-restricted-settings.sh 幂等预授权（Go 契约测试）+ App 内 RestrictedSettingsPolicy 引导）；剩余 A3-P2 安全演进 | ✅ 可靠性与受限设置引导已落地；P2 安全演进部分落地（2026-10-10：per-agent token 入库 + Dashboard 管理面/审计 + listener env∪DB 双源校验，agent 侧零改动；Keystore 迁移已落地（EncryptedSharedPreferences + 明文一次性迁移）/challenge-response/TLS 未实施，见 docs/agent-token-auth-design.md §6） |
| A4 多设备编排 | Dashboard 设备视图、批量下发、asset.sync 模板分发 | ⬜ 协议预留，未实施 |

工程基建：nightly CI 打 Android arm64 APK（vcpkg manifest 依赖、NDK 27；自 2026-09-30 起先跑 Kotlin JVM 单测再打 APK）；共用 agent 核心已下沉 `libs/agentcore`（RemoteClient/EventBuffer）与 `libs/androidagent`（ScriptRunner/脚本能力 API），与桌面同源编译、单测在桌面端跑（`libs/lua/tests`）。

**交付物**: 远程可控的 Android 自动化 agent（链路、脚本能力与可靠性自愈可用；安全演进与多设备编排待 A3-P2/A4）

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
| M9: Android Agent（A3-P2 + A4） | 未排期 | A1/A2/A3 已完成 |

**总计**: ~21 周 (约 5 个月)；M9 为 2026-09 新增的移动端里程碑（A1/A2/A3-P1 已落地），未计入原估算

---

## 下一阶段行动

### 当前重点（2026-09-30 校准）

| 优先级 | 任务 | 预计时间 | 状态 |
|--------|------|----------|------|
| P2 | Android A3 可靠性剩余项（开机自启/崩溃自重启/断连缓存自治/机型保活指引） | — | ✅ 已完成（2026-09-30，android-agent-design.md §7/§10'） |
| P2 | Android A3 受限设置引导（Android 13+ 侧载开箱：手动允许/adb 预授权/Device Owner） | — | ✅ 已完成（2026-09-30，android-agent-design.md §10''：手册 + scripts/android-restricted-settings.sh + App 内引导） |
| P3 | Android A4 多设备编排（Dashboard 设备视图/批量下发/asset.sync 模板分发） | 未排期 | ⬜ 协议预留 |
| P1 | Dashboard Monitor triggers 接真实 API（需扩 agent 协议） | 1周 | ✅ list/toggle + add/update/remove CRUD 全链路（runtime Dispatcher Reuse，agents:manage） |
| P2 | 跨平台运行时验证（macOS/Linux，需真机） | — | ⬜ 待验证 |
| P2 | Swagger/OpenAPI 文档 | — | ✅ 全端点注解补全 + typed schema（workflows/scripts/debugger/triggers）+ 统一 ErrorResponse（401/403/404/500）+ 双注册路由标注，swag init 生成 docs，`/swagger/` UI 可用 |
| P2 | 自动发布（tag→release）+ 打包分发 | — | ✅ `v*` tag 推送自动发布：CHANGELOG 提取 notes → draft → 三平台构建上传 → SHA256SUMS → publish（release.yml） |
| ✅ | 代码缺陷修复（10 项，见 todo.md「✅ 代码缺陷」） | - | ✅ 已完成 |
| ✅ | 远程连接状态回调通知 GUI + EventBuffer 公平性 + 全局快捷键 | - | ✅ 已完成 |
| ✅ | RBAC 权限系统（Go orchestrator，PermissionRequired 已接线 8 权限码） | - | ✅ 已完成 |
| ✅ | Runtime IPC 事件推送（log/trigger/script/connection，pull 模型） | - | ✅ 已完成 |
| ✅ | Debugger 端点（降级为直连模式） | - | ✅ 已完成 |
| ✅ | Go orchestrator 测试覆盖（600+ 函数） | - | ✅ 已完成 |
| ✅ | 工作流引擎增强（重试/负载均衡/模板/wait） | - | ✅ 已完成 |
| ✅ | Milestone 6: 人性化模拟 | - | ✅ 已完成 |
| ✅ | 代码覆盖率 90% (C++ runtime) | - | ✅ 已完成 |
| ✅ | 脚本层抽象：Lua 生产就绪（默认构建+CI）；Python 引擎纳入主 CI（Linux 全量测试维度 + Windows 链接链路 job，2026-09-26），默认构建仍关闭 | - | ✅ |
| ✅ | 移除账号/二维码/认证模块 | - | ✅ 已完成 |
| ✅ | 工作流引擎 + Agent 心跳 + 审计 | - | ✅ 已完成 |

### 检查清单

#### Phase 7: 测试与文档 (已完成)
- [x] Windows 构建 - MSVC + x64-windows-static
- [x] 集成测试覆盖 - 1584 个测试用例
- [x] 代码覆盖率 - 90.04% (6652/7388 行)
- [x] 性能基准测试 - KVStore 性能指标
- [x] 文档完善 - BUILD.md + 更新 README

#### 测试覆盖详情
| 模块 | 测试数 | 状态 |
|------|--------|------|
| screen / vision | 屏幕捕获、颜色查找、图像匹配、Bitmap | ✅ |
| input / human | 输入模拟、人性化鼠标/键盘 | ✅ |
| behavior_tree | 行为树节点、策略、重试/重复 | ✅ |
| config / storage | 配置管理、本地/会话存储 | ✅ |
| ipc / tcp_channel | IPC 工厂、TCP 通道 | ✅ |
| script modules | screen/input/window/event/kv/config/json/verification/db/ini/transport | ✅ |
| db (SQLite) | 连接管理、CRUD、事务、ORM、QueryBuilder | ✅ |
| ini | 解析、编码、读写、合并、转义序列 | ✅ |
| transport | TCP 客户端/服务器、UDP socket | ✅ |
| kvstore | KV + Hash + List 操作 | ✅ |
| security / window / json / ... | 其他核心模块 | ✅ |
| **总计** | **1584 tests / 143 suites** | ✅ **90.04% 行覆盖率** |

#### Milestone 2: 触发器系统 (已完成)
- [x] TriggerManager - 完整的触发器生命周期管理
- [x] TriggerEngine - Lua配置加载支持
- [x] 触发器测试 - 16个测试用例
- [x] 示例配置 - examples/configs/triggers.lua
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
