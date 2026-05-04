# Wingman 开发路线图

> 类似 Chimpeeon 的游戏自动化工具完整开发计划

## 参考项目分析

| 项目 | 语言 | 特点 | 参考价值 |
|------|------|------|----------|
| [PyAutoGUI](https://github.com/asweigart/pyautogui) | Python | 跨平台GUI自动化 | 屏幕操作API设计 |
| [AutoHotkey](https://github.com/AutoHotkey/AutoHotkey) | C++ | 热键/宏录制 | 触发器系统 |
| [OpenCV](https://opencv.org) | C++ | 图像识别 | 像素/图像检测 |
| [MicroMacro](https://github.com/Fallas/micromacro) | C++/Lua | 游戏自动化框架 | Lua绑定设计 |
| [Osenpa Auto Clicker](https://sourceforge.net/projects/osenpa-autoclicker) | C# | 图像/颜色检测 | UI设计 |

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                        Wingman                              │
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
│  │  │           LuaJIT Runtime               │          │  │
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

## Milestone 7: VS Code 调试器

**目标**: 完整的 Lua 开发体验

### 7.1 DAP (Debug Adapter Protocol)
```json
{
  "type": "wingman-lua",
  "request": "launch",
  "name": "Debug Wingman Script",
  "program": "${workspaceFolder}/scripts/main.lua",
  "wingmanPath": "C:/Wingman/bin/wingman.exe"
}
```

### 7.2 调试功能
- 断点设置
- 单步执行
- 变量查看
- 调用栈
- 表达式求值

**交付物**: VS Code 扩展

---

## Milestone 8: 发布准备

**目标**: 生产级质量

### 8.1 安全
- [ ] 代码签名证书
- [ ] 杀毒软件白名单
- [ ] 混淆保护

### 8.2 分发
- [ ] 安装程序 (InnoSetup)
- [ ] 便携版 ZIP
- [ ] 自动更新

### 8.3 文档
- [ ] 用户手册
- [ ] API 参考
- [ ] 视频教程
- [ ] 示例脚本库

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

1. **本周**: 实现 MVP - 屏幕捕获 + 像素检测
2. **下周**: 输入模拟 + Lua 绑定
3. **月底**: 第一个可运行版本

需要我开始实现哪个模块？
