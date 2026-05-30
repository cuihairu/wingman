# ML API

`wingman.ml` 提供机器学习模型推理功能，支持 ONNX 格式模型（如 YOLO 目标检测）。

::: warning 注意
此模块需要编译时启用 `WINGMAN_ENABLE_ML` 选项。未启用时返回存根实现。
:::

## 加载模型

:::tabs

== Python

```python
from wingman import ml

# 加载 ONNX 模型
model = ml.load_model("models/yolov8n.onnx")
if model:
    print("模型加载成功")
```

== Lua

```lua
local ml = require("wingman.ml")

-- 加载 ONNX 模型
local model, err = ml.loadModel("models/yolov8n.onnx")
if model then
    print("模型加载成功")
else
    print("加载失败: " .. tostring(err))
end
```

:::

## 指定执行提供器

:::tabs

== Python

```python
from wingman import ml

# 使用 CUDA 执行提供器（需要 GPU）
model = ml.load_model("models/yolov8n.onnx", "cuda")

# 使用 DirectML 执行提供器（Windows GPU）
model = ml.load_model("models/yolov8n.onnx", "dml")

# 使用 CPU 执行提供器（默认）
model = ml.load_model("models/yolov8n.onnx", "cpu")
```

== Lua

```lua
local ml = require("wingman.ml")

-- 使用 CUDA 执行提供器（需要 GPU）
local model = ml.loadModel("models/yolov8n.onnx", "cuda")

-- 使用 DirectML 执行提供器（Windows GPU）
local model = ml.loadModel("models/yolov8n.onnx", "dml")

-- 使用 CPU 执行提供器（默认）
local model = ml.loadModel("models/yolov8n.onnx", "cpu")
```

:::

## 目标检测

:::tabs

== Python

```python
from wingman import ml, screen

# 加载 YOLO 模型
model = ml.load_model("models/yolov8n.onnx")

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 运行目标检测
results = model.detect(img, 0.5)

# 处理检测结果
for obj in results:
    print(f"类别: {obj['class']}")
    print(f"置信度: {obj['confidence']}")
    print(f"位置: ({obj['center']['x']}, {obj['center']['y']})")
    print(f"边界框: {obj['bbox']}")
```

== Lua

```lua
local ml = require("wingman.ml")
local screen = require("wingman.screen")

-- 加载 YOLO 模型
local model = ml.loadModel("models/yolov8n.onnx")

-- 截图
local img = screen.capture(0, 0, 1920, 1080)

-- 运行目标检测
local results = model.detect(img, 0.5)

-- 处理检测结果
for i, obj in ipairs(results) do
    print("类别: " .. (obj.class or obj.label))
    print("置信度: " .. obj.confidence)
    print("位置: (" .. obj.center.x .. ", " .. obj.center.y .. ")")
    print("边界框: " .. tostring(obj.bbox))
end
```

:::

## 图像分类

:::tabs

== Python

```python
from wingman import ml, screen

# 加载分类模型
model = ml.load_model("models/resnet.onnx")

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 运行分类
label, confidence = model.classify(img)

print(f"类别: {label}, 置信度: {confidence}")
```

== Lua

```lua
local ml = require("wingman.ml")
local screen = require("wingman.screen")

-- 加载分类模型
local model = ml.loadModel("models/resnet.onnx")

-- 截图
local img = screen.capture(0, 0, 1920, 1080)

-- 运行分类
local label, confidence = model.classify(img)

print("类别: " .. label .. ", 置信度: " .. confidence)
```

:::

## 获取模型信息

:::tabs

== Python

```python
from wingman import ml

model = ml.load_model("models/yolov8n.onnx")

# 获取输入信息
inputs = model.get_input_info()
for name, shape in inputs.items():
    print(f"输入: {name}, 形状: {shape}")

# 获取输出信息
outputs = model.get_output_info()
for name, shape in outputs.items():
    print(f"输出: {name}, 形状: {shape}")

# 获取可用的执行提供器
providers = ml.get_available_execution_providers()
print(f"可用的执行提供器: {providers}")
```

== Lua

```lua
local ml = require("wingman.ml")

local model = ml.loadModel("models/yolov8n.onnx")

-- 获取输入信息
local inputs = model.getInputInfo()
for i, info in ipairs(inputs) do
    print("输入: " .. info.name .. ", 形状: " .. tostring(info.shape))
end

-- 获取输出信息
local outputs = model.getOutputInfo()
for i, info in ipairs(outputs) do
    print("输出: " .. info.name .. ", 形状: " .. tostring(info.shape))
end

-- 获取可用的执行提供器
local providers = ml.getAvailableExecutionProviders()
print("可用的执行提供器: " .. table.concat(providers, ", "))
```

:::

## 卸载模型

:::tabs

== Python

```python
from wingman import ml

model = ml.load_model("models/yolov8n.onnx")

# 使用完毕后卸载模型释放内存
model.unload()
```

== Lua

```lua
local ml = require("wingman.ml")

local model = ml.loadModel("models/yolov8n.onnx")

-- 使用完毕后卸载模型释放内存
model.unload()
```

:::

---

## 完整示例

### YOLO 目标检测自动点击

:::tabs

== Python

```python
from wingman import ml, screen, input, window

# 加载 YOLOv8 模型
model = ml.load_model("models/yolov8n.onnx")
if not model:
    print("模型加载失败")
    exit(1)

# 获取前台窗口
hwnd = window.get_foreground()
print(f"前台窗口: {window.get_title(hwnd)}")

# 目标类别和置信度阈值
TARGET_CLASS = "enemy"
CONFIDENCE_THRESHOLD = 0.5

# 检测循环
for i in range(10):
    # 截图
    img = screen.capture_window(hwnd)

    # 运行检测
    results = model.detect(img, CONFIDENCE_THRESHOLD)

    # 点击目标
    for obj in results:
        if obj['class'] == TARGET_CLASS:
            print(f"检测到 {obj['class']}: {obj['confidence']:.2f}")
            input.click(obj['center']['x'], obj['center']['y'])

    util.sleep(500)
```

== Lua

```lua
local ml = require("wingman.ml")
local screen = require("wingman.screen")
local input = require("wingman.input")
local window = require("wingman.window")
local util = require("wingman.util")

-- 加载 YOLOv8 模型
local model = ml.loadModel("models/yolov8n.onnx")
if not model then
    print("模型加载失败")
    return
end

-- 获取前台窗口
local hwnd = window.getForeground()
print("前台窗口: " .. window.getTitle(hwnd))

-- 目标类别和置信度阈值
local TARGET_CLASS = "enemy"
local CONFIDENCE_THRESHOLD = 0.5

-- 检测循环
for i = 1, 10 do
    -- 截图
    local img = screen.captureWindow(hwnd)

    -- 运行检测
    local results = model.detect(img, CONFIDENCE_THRESHOLD)

    -- 点击目标
    for _, obj in ipairs(results) do
        if obj.class == TARGET_CLASS or obj.label == TARGET_CLASS then
            print(string.format("检测到 %s: %.2f", obj.class or obj.label, obj.confidence))
            input.click(obj.center.x, obj.center.y)
        end
    end

    util.sleep(500)
end
```

:::

### 图像分类

:::tabs

== Python

```python
from wingman import ml, screen

# 加载分类模型（如 ResNet）
model = ml.load_model("models/resnet.onnx")
if not model:
    print("模型加载失败")
    exit(1)

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 运行分类
label, confidence = model.classify(img)

print(f"分类结果: {label}")
print(f"置信度: {confidence:.2f}")

# 根据分类结果执行操作
if label == "combat":
    print("检测到战斗场景")
elif label == "menu":
    print("检测到菜单界面")
```

== Lua

```lua
local ml = require("wingman.ml")
local screen = require("wingman.screen")

-- 加载分类模型（如 ResNet）
local model = ml.loadModel("models/resnet.onnx")
if not model then
    print("模型加载失败")
    return
end

-- 截图
local img = screen.capture(0, 0, 1920, 1080)

-- 运行分类
local label, confidence = model.classify(img)

print("分类结果: " .. label)
print("置信度: " .. string.format("%.2f", confidence))

-- 根据分类结果执行操作
if label == "combat" then
    print("检测到战斗场景")
elseif label == "menu" then
    print("检测到菜单界面")
end
```

:::

---

## 可用接口

### `load_model(path, provider?)` / `loadModel(path, provider?)`

加载 ONNX 格式模型。

**参数：**
- `path` - 模型文件路径（.onnx）
- `provider` - 可选，执行提供器：`"cpu"`（默认）、`"cuda"`（GPU）、`"dml"`（DirectML）

**返回：**
- `Model` - 模型对象，失败返回 `null`/`nil`

### Model 对象方法

#### `detect(image, confidenceThreshold?, nmsThreshold?)` / `detect(image, confidenceThreshold?, nmsThreshold?)`

运行目标检测（用于 YOLO 等检测模型）。

**参数：**
- `image` - 图像对象
- `confidenceThreshold` - 可选，置信度阈值（0-1），默认 0.5
- `nmsThreshold` - 可选，NMS 阈值（0-1），默认 0.45

**返回：**
- `array` - 检测结果数组
  - `class` / `label` - 类别名称
  - `confidence` - 置信度（0-1）
  - `center` - 中心坐标 `{x, y}`
  - `bbox` - 边界框 `{x, y, width, height}`

#### `classify(image)` / `classify(image)`

运行图像分类。

**参数：**
- `image` - 图像对象

**返回：**
- `string, number` - 类别名称和置信度

#### `get_input_info()` / `getInputInfo()`

获取模型输入信息。

**返回：**
- `array` - 输入信息数组 `[{name, shape}]`

#### `get_output_info()` / `getOutputInfo()`

获取模型输出信息。

**返回：**
- `array` - 输出信息数组 `[{name, shape}]`

#### `unload()` / `unload()`

卸载模型，释放内存。

**返回：**
- `nil`

### `get_available_execution_providers()` / `getAvailableExecutionProviders()`

获取系统可用的执行提供器列表。

**返回：**
- `array` - 执行提供器名称数组

---

## 模型准备

### YOLOv8 模型转换

```bash
# 安装 Ultralytics
pip install ultralytics

# 导出 ONNX 模型
yolo export model=yolov8n.pt format=onnx

# 将生成的 yolov8n.onnx 放到项目目录
cp yolov8n.onnx models/
```

### 支持的模型格式

- ONNX (.onnx) - 通用格式
- 支持的模型类型：
  - 目标检测：YOLOv5、YOLOv8 等
  - 图像分类：ResNet、EfficientNet 等
  - 语义分割：U-Net、DeepLab 等

---

## 编译选项

启用 ML 支持需要在 CMake 配置时添加：

```bash
cmake -B build -DWINGMAN_ENABLE_ML=ON ...
```

依赖项（通过 vcpkg 自动安装）：
- onnxruntime
- CUDA（可选，用于 GPU 加速）
