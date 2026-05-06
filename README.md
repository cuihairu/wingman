# Wingman

<div align="center">

<img src="docs/public/logo.svg" alt="Wingman" width="100" />

**游戏自动化可编程控制引擎**

C++ + Lua 的高性能游戏自动化框架

[![CI](https://github.com/cuihairu/wingman/workflows/CI/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cuihairu/wingman/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/wingman)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Lua](https://img.shields.io/badge/Lua-5.4-blue.svg)](https://www.lua.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[文档](https://cuihairu.github.io/wingman/) | [快速开始](#快速开始) | [API](https://cuihairu.github.io/wingman/api/) | [示例](https://cuihairu.github.io/wingman/examples/)

</div>

## 简介

**Wingman** 是一个游戏自动化可编程控制引擎。

- 基于 **C++** 开发核心引擎，提供高性能的屏幕操作和输入模拟能力
- 使用 **Lua** 作为脚本引擎，灵活可扩展
- 纯**用户态**运行，使用合法 Windows API，安全可靠
- 支持**远程控制**，TCP Server/Client 模式，暴露 API 供外部调用

## 特性

- 🚀 **高性能** - C++ 核心引擎，Lua 脚本执行，毫秒级响应
- 🔒 **安全可靠** - 纯用户态运行，使用合法 Windows API，不读写游戏内存
- 🎮 **可编程** - Lua 脚本控制，无限扩展可能，支持复杂业务逻辑
- 🌐 **远程控制** - 支持 TCP Server/Client 模式，可远程统一调度
- 🐛 **强大的调试** - VS Code 插件支持，断点调试、变量查看、性能分析
- 🤖 **人性化模拟** - 贝塞尔曲线鼠标移动、随机延迟、自然操作模式

## 核心功能

| 模块 | 功能 |
|-----|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 (OpenCV) |
| **UI Automation** | Windows UIA 自动化，直接操作 UI 控件（按钮/编辑框/列表等） |
| **Vision 视觉** | 颜色检测、图像匹配、边缘检测、轮廓检测、形状识别 |
| **OCR 识别** | Tesseract 文字识别，支持多语言 |
| **ML/AI 推理** | ONNX Runtime 模型推理，支持图像分类、目标检测、分割 |
| **智能触发器** | SmartTrigger 系统，支持颜色/图像/文字/OCR 条件触发 |
| **行为树引擎** | BehaviorTree，支持 Sequence/Selector/Parallel/Wait/Retry 节点 |
| **输入模拟** | 鼠标点击/移动、按键发送、文本输入 |
| **人性化模拟** | 贝塞尔曲线鼠标移动、随机延迟、自然操作 |
| **窗口管理** | 查找窗口、激活窗口、获取位置 |
| **进程管理** | 启动/等待/终止进程 |
| **HTTP 客户端** | GET/POST/PUT/DELETE 请求，表单提交 (libcurl) |
| **JSON 封装** | JSON 解析和序列化 (nlohmann/json) |
| **存储系统** | 四层存储架构：SessionStorage/LocalStorage/TeamStorage/ServerStorage |
| **组队编排** | 多 Client 协同组队、投票协调 |
| **宏录制** | 录制鼠标键盘操作，保存为 Lua/JSON 回放 |
| **触发器系统** | 颜色/图像/窗口/进程触发，自动执行动作 |
| **Web Dashboard** | React + UmiJS + Ant Design 管理界面 |
| **VSCode 插件** | 语法高亮、自动完成、调试支持 |

## 快速开始

### 环境要求

- Windows 10/11
- Visual Studio 2022
- CMake 3.20+
- vcpkg

### 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### 编译

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman

# 安装依赖
vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio curl

# 配置项目
cmake -B build -S . -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build build --config Release
```

### 运行

```bash
.\build\Release\wingman.exe scripts\examples\hello.lua
```

## 开发

### C++ 单元测试

```bash
# 构建测试
cmake -B build -DWINGMAN_BUILD_TESTS=ON
cmake --build build --config Debug

# 运行测试
.\build\tests\cpp\Debug\wingman_tests.exe
```

### Lua 开发工具 (可选)

Wingman 提供可选的 Lua 开发工具：

```bash
# 安装 LuaRocks 包管理器
scripts\install-luarocks.cmd

# 安装 Busted 测试框架
scripts\install-busted.cmd

# 运行 Lua 测试
scripts\run-lua-tests.cmd
```

详细开发指南请参考 [DEVELOPMENT.md](docs/DEVELOPMENT.md)。

## 文档

详细文档请访问 [https://cuihairu.github.io/wingman/](https://cuihairu.github.io/wingman/)

- [指南](https://cuihairu.github.io/wingman/guide/)
- [API 参考](https://cuihairu.github.io/wingman/api/)
- [示例](https://cuihairu.github.io/wingman/examples/)

## Lua API 示例

### UI Automation 自动化

```lua
-- 从前台窗口获取 UIA 根元素
local root = uia.fromForeground()

-- 查找并点击按钮
local btn = uia.findButton("确定")
if btn then
    btn:click()
end

-- 查找编辑框并输入文本
local edit = uia.findEdit("")
if edit then
    edit:setValue("Hello, UI Automation!")
end

-- 等待窗口出现
local dialog = uia.waitForName("对话框", 3000)
if dialog then
    local okBtn = dialog:findFirstChild("Button", "确定")
    if okBtn then
        okBtn:click()
    end
end

-- 操作下拉框
local comboBox = uia.findByName("下拉选择")
if comboBox then
    comboBox:expand()
    comboBox:selectItem("选项2")
end
```

### Vision 视觉系统

```lua
-- 查找颜色（带容差）
local pos = vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    input.click(pos.x, pos.y)
end

-- 查找所有匹配颜色
local positions = vision.findAllColors({r=0, g=255, b=0}, 5)

-- 图像匹配
local result = vision.findImage("target.png", 0.8)
if result.found then
    input.click(result.position.x, result.position.y)
end

-- 边缘检测
local edges = vision.detectEdges({x=0, y=0, width=800, height=600})
```

### OCR 文字识别

```lua
-- 识别屏幕区域文字
local result = ocr.recognize({x=100, y=100, width=200, height=50})
if result.success then
    print("识别到文字: " .. result.text)
end

-- 简化版本
local text = ocr.recognizeText({x=0, y=0, width=300, height=100})
```

### SmartTrigger 智能触发器

```lua
-- 创建触发器：血量低自动喝药
smarttrigger.create("auto_heal")

-- 添加条件：检测红色血条
smarttrigger.addCondition("auto_heal", "COLOR_FOUND", {r=255, g=0, b=0}, 10, {x=100, y=100, width=50, height=10})

-- 添加动作：按 1 键喝药
smarttrigger.addAction("auto_heal", "KEY_PRESS", 49)

-- 启动触发器
smarttrigger.start("auto_heal")

-- 等待 10 秒后停止
util.sleep(10000)
smarttrigger.stop("auto_heal")
```

### BehaviorTree 行为树

```lua
-- 创建行为树
bt.create("combat_tree")

-- 设置根节点：选择执行任务或攻击
local selector = bt.selector("main_selector")
local sequence = bt.sequence("attack_sequence")

-- tick 行为树
local status = bt.tick("combat_tree")
print("树状态: " .. status)  -- SUCCESS/FAILURE/RUNNING
```

### 组合使用

```lua
-- 智能战斗脚本
while true do
    -- 检测敌人
    local enemy = vision.findImage("enemy.png", 0.7)
    if enemy.found then
        -- 点击攻击
        input.click(enemy.position.x, enemy.position.y)
        util.sleep(500)

        -- 检测血量
        local hp = ocr.recognizeText({x=200, y=600, width=100, height=20})
        if string.find(hp, "低") then
            input.key(49)  -- 按 1 喝药
        end
    end

    util.sleep(100)
end
```

## C++ API 示例

### 智能触发器系统

```cpp
// 创建智能触发器
auto trigger = SmartTriggerManager::instance().createTrigger("auto_heal");

// 添加颜色检测条件
TriggerCondition condition;
condition.type = TriggerConditionType::COLOR_FOUND;
condition.targetColor = Color(255, 0, 0);
condition.tolerance = 10;
condition.searchRegion = Rect(100, 100, 50, 50);
trigger->addCondition(condition);

// 添加按键动作
TriggerAction action;
action.type = TriggerActionType::KEY_PRESS;
action.keyCode = 49;  // 1 键
trigger->addAction(action);

// 启动触发器
trigger->start();
```

### 行为树引擎

```cpp
// 创建行为树
auto tree = BehaviorTreeManager::instance().createTree("combat");

// 构建节点：序列执行
auto sequence = BehaviorTree::sequence("attack");
sequence->addChild(BehaviorTree::action("find_enemy", []() {
    // 查找敌人逻辑
    return NodeStatus::SUCCESS;
}));

tree->setRoot(sequence);

// 执行行为树
NodeStatus status = tree->tick();
```

### Web Dashboard

```bash
cd dashboard
npm install
npm run dev
```

- 脚本管理
- 窗口监控
- 系统状态
- 远程控制

### VSCode 插件

```bash
cd vscode-extension
npm install
npm run compile
```

- Lua 语法高亮
- API 自动完成
- 断点调试

## 许可证

[Apache-2.0](LICENSE)
