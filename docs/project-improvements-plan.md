# Wingman 项目改进实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 提升 Wingman 项目的形象和开发者体验，包括移动 todo.md、添加免责声明、改进 README、创建安装脚本和添加示例。

**Architecture:** 分三个阶段执行，每阶段独立可验证：
- 阶段 1：快速形象改进（5 分钟）
- 阶段 2：开发者体验（30 分钟）
- 阶段 3：核心功能展示（可选，需要运行环境）

**Tech Stack:** Markdown, PowerShell, Lua

---

## 阶段 1：快速形象改进

### Task 1.1: 移动 todo.md 到 docs 目录

**Files:**
- Move: `todo.md` → `docs/development-todo.md`

- [ ] **Step 1: 使用 git mv 移动文件**

```bash
git mv todo.md docs/development-todo.md
```

- [ ] **Step 2: 验证移动成功**

```bash
git status
```

Expected: `renamed: todo.md -> docs/development-todo.md`

- [ ] **Step 3: 提交**

```bash
git add -A
git commit -m "chore: move todo.md to docs/development-todo.md"
```

---

### Task 1.2: 添加免责声明到 README

**Files:**
- Modify: `README.md:21` (在简介之前添加)

- [ ] **Step 1: 在 README.md 第 21 行（简介之前）添加免责声明**

在第 21 行 `## 简介` 之前添加以下内容：

```markdown
> ⚠️ **免责声明**
>
> 本工具仅供合法场景使用，包括但不限于：自动化测试、可单机游戏辅助、无障碍辅助等。
> 使用本工具违反任何游戏或软件的用户协议所导致的后果，由使用者自行承担。
> 作者不对因使用本工具而产生的任何法律责任负责。
>
```

修改后 README.md 的第 21-30 行应该如下：

```markdown
## 简介

**Wingman** 是一个游戏自动化可编程控制引擎。

...
```

注意：在 `## 简介` 和 `**Wingman**` 之间需要添加免责声明块。

- [ ] **Step 2: 验证修改**

```bash
head -n 35 README.md
```

Expected: 能看到免责声明块

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: add legal disclaimer to README"
```

---

### Task 1.3: 改进 README 结构 - 添加 Windows Only 标识

**Files:**
- Modify: `README.md:1` (标题行)

- [ ] **Step 1: 修改标题行添加 Windows Only**

将第 1 行从：
```markdown
# Wingman
```

改为：
```markdown
# Wingman [![Windows](https://img.shields.io/badge/OS-Windows-blue.svg)](https://github.com/cuihairu/wingman)
```

- [ ] **Step 2: 验证修改**

```bash
head -n 5 README.md
```

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: add Windows-only badge to README title"
```

---

### Task 1.4: 改进 README - 突出 ONNX 特性

**Files:**
- Modify: `README.md:43-48` (核心功能表格)

- [ ] **Step 1: 将 ML/AI 推理行移到表格顶部**

在核心功能表格中，将 `**ML/AI 推理**` 行从第 8 行移到第 3 行（屏幕操作之后）。

修改后的核心功能表格前几行应该是：

```markdown
| 模块 | 功能 |
|-----|------|
| **屏幕操作** | 截图、像素检测、颜色匹配、图像查找 (OpenCV) |
| **ML/AI 推理** | ONNX Runtime 模型推理，支持图像分类、目标检测、分割 |
| **UI Automation** | Windows UIA 自动化，直接操作 UI 控件（按钮/编辑框/列表等） |
```

- [ ] **Step 2: 验证修改**

```bash
sed -n '39,48p' README.md
```

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: promote ML/AI feature in core capabilities table"
```

---

## 阶段 2：开发者体验

### Task 2.1: 创建 bootstrap.ps1 安装脚本

**Files:**
- Create: `bootstrap.ps1`

- [ ] **Step 1: 创建 bootstrap.ps1 文件**

创建以下内容：

```powershell
#!/usr/bin/env pwsh
# Wingman Bootstrap Script
# 自动安装 vcpkg 和项目依赖

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Wingman 依赖安装脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否在项目根目录
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "错误: 请在项目根目录运行此脚本" -ForegroundColor Red
    exit 1
}

# 检查 vcpkg
Write-Host "[1/4] 检查 vcpkg..." -ForegroundColor Yellow
if (-not (Test-Path "vcpkg")) {
    Write-Host "vcpkg 未安装，开始克隆..." -ForegroundColor Cyan
    git clone https://github.com/Microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) {
        Write-Host "错误: 克隆 vcpkg 失败" -ForegroundColor Red
        exit 1
    }
    Push-Location vcpkg
    .\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) {
        Write-Host "错误: vcpkg bootstrap 失败" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
    Write-Host "vcpkg 安装完成" -ForegroundColor Green
} else {
    Write-Host "vcpkg 已安装" -ForegroundColor Green
}

# 设置环境变量
$VCPKG_ROOT = Resolve-Path "vcpkg"
$env:VCPKG_ROOT = $VCPKG_ROOT
Write-Host "VCPKG_ROOT = $VCPKG_ROOT" -ForegroundColor Cyan

# 检查 CMake
Write-Host ""
Write-Host "[2/4] 检查 CMake..." -ForegroundColor Yellow
$cmakeVersion = cmake --version 2>$null
if (-not $cmakeVersion) {
    Write-Host "错误: 未找到 CMake，请先安装 CMake 3.20+" -ForegroundColor Red
    Write-Host "下载: https://cmake.org/download/" -ForegroundColor Cyan
    exit 1
}
Write-Host $cmakeVersion.Split('\n')[0] -ForegroundColor Green

# 检查 Visual Studio
Write-Host ""
Write-Host "[3/4] 检查 Visual Studio..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    Write-Host "错误: 未找到 Visual Studio 2022" -ForegroundColor Red
    exit 1
}
$vsPath = & $vsWhere -latest -property installationPath
if (-not $vsPath) {
    Write-Host "错误: 未找到 Visual Studio 2022" -ForegroundColor Red
    exit 1
}
Write-Host "Visual Studio 2022 已安装" -ForegroundColor Green

# 安装依赖
Write-Host ""
Write-Host "[4/4] 安装 vcpkg 依赖..." -ForegroundColor Yellow
Write-Host "这可能需要 10-30 分钟，请耐心等待..." -ForegroundColor Cyan

Push-Location vcpkg
.\vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio curl tesseract onnxruntime
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 依赖安装失败" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  依赖安装完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "接下来运行:" -ForegroundColor Cyan
Write-Host "  cmake -B build -S . -G `"Visual Studio 17 2022`" -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -ForegroundColor White
Write-Host "  cmake --build build --config Release" -ForegroundColor White
Write-Host ""
```

- [ ] **Step 2: 设置执行权限**

```bash
chmod +x bootstrap.ps1 2>/dev/null || true
```

- [ ] **Step 3: 验证文件创建**

```bash
head -n 20 bootstrap.ps1
```

- [ ] **Step 4: 提交**

```bash
git add bootstrap.ps1
git commit -m "feat: add bootstrap.ps1 for automated dependency installation"
```

---

### Task 2.2: 添加 ONNX 示例脚本

**Files:**
- Create: `scripts/examples/onnx_object_detection.lua`

- [ ] **Step 1: 创建 ONNX 对象检测示例**

创建以下内容：

```lua
-- ONNX 对象检测示例
-- 使用 YOLOv8 模型检测游戏中的目标并自动点击
--
-- 准备工作:
-- 1. 下载 YOLOv8n 模型并转换为 ONNX 格式
-- 2. 将模型文件放在 scripts/models/ 目录下
-- 3. 运行此脚本

local ml = require("wingman.ml")
local screen = require("wingman.screen")
local input = require("wingman.input")
local vision = require("wingman.vision")

print("=== Wingman ONNX 目标检测示例 ===")

-- 配置
local MODEL_PATH = "scripts/models/yolov8n.onnx"
local CONFIDENCE_THRESHOLD = 0.5
local TARGET_CLASS = "enemy"  -- 根据你的模型类别调整

-- 检查模型文件
local function checkModelExists()
    local f = io.open(MODEL_PATH, "r")
    if f then
        f:close()
        return true
    end
    return false
end

-- 主检测循环
local function main()
    -- 检查模型
    if not checkModelExists() then
        print("错误: 找不到模型文件: " .. MODEL_PATH)
        print("请先下载 YOLOv8n ONNX 模型并放在正确位置")
        print("转换命令: ultralytics yolo export model=yolov8n.pt format=onnx")
        return
    end

    -- 加载模型
    print("正在加载模型: " .. MODEL_PATH)
    local model, err = ml.loadModel(MODEL_PATH)
    if not model then
        print("错误: 加载模型失败 - " .. tostring(err))
        return
    end
    print("模型加载成功")

    -- 获取前台窗口
    local hwnd = window.getForeground()
    if not hwnd then
        print("错误: 无法获取前台窗口")
        return
    end
    print("前台窗口: " .. window.getTitle(hwnd))

    print("\n开始检测循环 (按 Ctrl+C 退出)...")
    print("置信度阈值: " .. CONFIDENCE_THRESHOLD)
    print("目标类别: " .. TARGET_CLASS)

    local count = 0
    while count < 10 do  -- 限制循环次数用于测试
        -- 截图
        local screenshot = screen.captureWindow(hwnd)

        -- 运行推理
        local results = model.detect(screenshot, CONFIDENCE_THRESHOLD)

        -- 处理结果
        for _, obj in ipairs(results) do
            if obj.class == TARGET_CLASS or obj.label == TARGET_CLASS then
                print(string.format("检测到 %s: 置信度 %.2f, 位置 (%d, %d)",
                    obj.class or obj.label, obj.confidence, obj.center.x, obj.center.y))

                -- 点击目标中心
                input.click(obj.center.x, obj.center.y, "left")
            end
        end

        count = count + 1
        util.sleep(500)  -- 休眠 500ms
    end

    print("\n检测完成")
end

-- 运行
main()
```

- [ ] **Step 2: 创建 models 目录说明文件**

创建 `scripts/models/README.md`:

```markdown
# 模型文件目录

此目录用于存放 ONNX 格式的机器学习模型。

## 获取 YOLOv8n 模型

```bash
# 安装 ultralytics
pip install ultralytics

# 导出 ONNX 模型
ultralytics yolo export model=yolov8n.pt format=onnx
```

将导出的 `yolov8n.onnx` 文件放在此目录下。

## 其他支持

Wingman ONNX Runtime 支持任何 ONNX 格式的模型：
- 图像分类 (ResNet, MobileNet, etc.)
- 目标检测 (YOLO, SSD, etc.)
- 语义分割
- 姿态估计
```

- [ ] **Step 3: 验证文件创建**

```bash
cat scripts/examples/onnx_object_detection.lua | head -n 30
```

- [ ] **Step 4: 提交**

```bash
git add scripts/examples/onnx_object_detection.lua scripts/models/README.md
git commit -m "feat: add ONNX object detection example script"
```

---

### Task 2.3: 更新 README 添加 ONNX 示例引用

**Files:**
- Modify: `README.md:270` (在 Lua API 示例部分末尾添加)

- [ ] **Step 1: 在 Lua API 示例部分添加 ONNX 示例**

在 `### 组合使用` 部分（约第 249 行）之后，添加新的 ONNX 示例章节：

```markdown
### ML/AI 推理

```lua
-- 加载 ONNX 模型
local model = ml.loadModel("scripts/models/yolov8n.onnx")

-- 对屏幕截图进行推理
local screenshot = screen.capture()
local results = model.detect(screenshot, 0.5)

-- 处理检测结果
for _, obj in ipairs(results) do
    if obj.class == "enemy" then
        print(string.format("发现敌人: 置信度 %.2f, 位置 (%d, %d)",
            obj.confidence, obj.center.x, obj.center.y))

        -- 自动点击目标
        input.click(obj.center.x, obj.center.y)
    end
end
```

完整示例请参考: [scripts/examples/onnx_object_detection.lua](scripts/examples/onnx_object_detection.lua)
```

插入位置在第 270 行 `### C++ API 示例` 之前。

- [ ] **Step 2: 验证修改**

```bash
sed -n '268,285p' README.md
```

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: add ML/AI inference example to README"
```

---

## 阶段 3：核心功能展示（可选）

### Task 3.1: 录制功能演示 GIF（需要 Windows 环境）

**注意:** 此任务需要在 Windows 环境下运行 Wingman，当前在 macOS 环境下无法完成。跳过此任务。

---

### Task 3.2: 完成 Lua Orchestration 绑定

**注意:** 此任务需要 C++ 开发，工作量较大（2-3小时），建议作为独立功能开发。本次改进暂不包含。

---

## 完成检查清单

- [ ] todo.md 已移到 docs/ 目录
- [ ] README 添加了免责声明
- [ ] README 添加了 Windows Only 标识
- [ ] README 突出了 ML/AI 特性
- [ ] 创建了 bootstrap.ps1 安装脚本
- [ ] 添加了 ONNX 示例脚本
- [ ] README 添加了 ML/AI 示例引用

---

## 推送更改

所有任务完成后，推送更改到远程仓库：

```bash
git push origin main
```
