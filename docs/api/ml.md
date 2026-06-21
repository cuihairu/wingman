# API: wingman.ml

机器学习模型推理模块，支持 ONNX 格式模型（如 YOLO 目标检测）。

::: warning 注意
此模块需要编译时启用 `WINGMAN_ENABLE_ML` 选项（macOS 通常不启用，`wingman.ml.loadModel()` 会返回 `nil`）。未启用时走存根实现。
:::

> ℹ️ **当前实现（ID 句柄式，已注册到 `wingman.ml`）**：
>
> ```lua
> local wingman = require("wingman")
> print(wingman.ml.providers())                    -- 可用执行提供器 ["cpu","cuda","dml"]
> local id = wingman.ml.loadModel("yolov8n.onnx", "cpu")
> if id then
>     print(wingman.ml.isLoaded(id))               -- true
>     print(wingman.ml.inputs(id))                 -- 模型输入 [{name, shape}]
>     print(wingman.ml.outputs(id))                -- 模型输出 [{name, shape}]
>     wingman.ml.unload(id)
> end
> ```
>
> 可用函数：`providers()` / `loadModel(path, ep?)` / `unload(id)` / `isLoaded(id)` / `inputs(id)` / `outputs(id)`。
>
> `detect` / `classify` 的高层封装待 `ModelHelpers::detectObjects` 实现完善 + 图像加载支持后补充；下文相关章节保留作 API 设计参考，形态可能调整。

## 模块概述

ml 模块提供 ONNX 模型的加载和推理功能：
- **模型加载** - 支持动态指定执行提供器（CPU/CUDA/DirectML）
- **目标检测** - YOLO 系列模型的目标检测
- **图像分类** - ResNet 等模型的图像分类
- **模型管理** - 获取模型信息、卸载模型释放内存

---

## 加载 ONNX 模型

### load_model(path, provider?) / loadModel(path, provider?)

**说明**：加载 ONNX 格式的机器学习模型。

**函数签名**：

```python
load_model(path: str, provider: str = "cpu") -> Model
```

```lua
loadModel(path: string, provider: string = "cpu") -> Model | nil, string | nil
```

**参数**：
- `path` - 模型文件路径（.onnx）
- `provider` - 可选，执行提供器：
  - `"cpu"` - CPU 执行（默认）
  - `"cuda"` - CUDA GPU 执行（需要 NVIDIA GPU）
  - `"dml"` - DirectML GPU 执行（Windows）

**返回**：
- Python: `Model` 对象，失败返回 `None`
- Lua: `Model` 对象，失败返回 `nil, error_message`

:::tabs

== Python

```python:line-numbers
from wingman import ml

# 加载 ONNX 模型（CPU）
model = ml.load_model("models/yolov8n.onnx")
if model:
    print("模型加载成功")

# 使用 CUDA 执行提供器（需要 GPU）
model = ml.load_model("models/yolov8n.onnx", "cuda")

# 使用 DirectML 执行提供器（Windows GPU）
model = ml.load_model("models/yolov8n.onnx", "dml")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 加载 ONNX 模型（CPU）
local model, err = ml.loadModel("models/yolov8n.onnx")
if model then
    print("模型加载成功")
else
    print("加载失败: " .. tostring(err))
end

-- 使用 CUDA 执行提供器
local model = ml.loadModel("models/yolov8n.onnx", "cuda")

-- 使用 DirectML 执行提供器
local model = ml.loadModel("models/yolov8n.onnx", "dml")
```

:::

---

## 获取可用执行提供器

### get_available_execution_providers() / getAvailableExecutionProviders()

**说明**：获取系统可用的执行提供器列表。

**函数签名**：

```python
get_available_execution_providers() -> list[str]
```

```lua
getAvailableExecutionProviders() -> table
```

**返回**：
- Python: 执行提供器名称列表
- Lua: 执行提供器名称数组

**常见返回值**：
- `"CPUExecutionProvider"` - CPU 执行（始终可用）
- `"CUDAExecutionProvider"` - CUDA GPU（需要 NVIDIA GPU）
- `"DmlExecutionProvider"` - DirectML（Windows）

:::tabs

== Python

```python:line-numbers
from wingman import ml

providers = ml.get_available_execution_providers()
print(f"可用的执行提供器: {providers}")
# 例如：['CPUExecutionProvider', 'CUDAExecutionProvider']
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local providers = ml.getAvailableExecutionProviders()
print("可用的执行提供器: " .. table.concat(providers, ", "))
-- 例如：CPUExecutionProvider, CUDAExecutionProvider
```

:::

---

## Model 对象：目标检测

### detect(image, confidence_threshold?, nms_threshold?) / detect(image, confidenceThreshold?, nmsThreshold?)

**说明**：运行目标检测（用于 YOLO 等检测模型）。

**函数签名**：

```python
model.detect(image: Image, confidence_threshold: float = 0.5, nms_threshold: float = 0.45) -> list[dict]
```

```lua
model.detect(image: Image, confidenceThreshold: number = 0.5, nmsThreshold: number = 0.45) -> table
```

**参数**：
- `image` - 图像对象（来自 `screen.capture()`）
- `confidence_threshold` - 可选，置信度阈值（0-1），默认 0.5
- `nms_threshold` - 可选，NMS 阈值（0-1），默认 0.45

**返回**：
- 检测结果数组，每个元素包含：
  - `class` / `label` - 类别名称
  - `confidence` - 置信度（0-1）
  - `center` - 中心坐标 `{x: number, y: number}`
  - `bbox` - 边界框 `{x: number, y: number, width: number, height: number}`

:::tabs

== Python

```python:line-numbers
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
    print(f"置信度: {obj['confidence']:.2f}")
    print(f"中心: ({obj['center']['x']}, {obj['center']['y']})")
    print(f"边界框: {obj['bbox']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 加载 YOLO 模型
local model = ml.loadModel("models/yolov8n.onnx")

-- 截图
local img = wingman.screen.capture(0, 0, 1920, 1080)

-- 运行目标检测
local results = model.detect(img, 0.5)

-- 处理检测结果
for i, obj in ipairs(results) do
    print("类别: " .. (obj.class or obj.label))
    print("置信度: " .. string.format("%.2f", obj.confidence))
    print("中心: (" .. obj.center.x .. ", " .. obj.center.y .. ")")
end
```

:::

---

## Model 对象：图像分类

### classify(image) / classify(image)

**说明**：运行图像分类。

**函数签名**：

```python
model.classify(image: Image) -> tuple[str, float]
```

```lua
model.classify(image: Image) -> string, number
```

**参数**：
- `image` - 图像对象（来自 `screen.capture()`）

**返回**：
- Python: `(label: str, confidence: float)` 元组
- Lua: 两个返回值 `label, confidence`

:::tabs

== Python

```python:line-numbers
from wingman import ml, screen

# 加载分类模型
model = ml.load_model("models/resnet.onnx")

# 截图
img = screen.capture(0, 0, 1920, 1080)

# 运行分类
label, confidence = model.classify(img)
print(f"类别: {label}, 置信度: {confidence:.2f}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 加载分类模型
local model = ml.loadModel("models/resnet.onnx")

-- 截图
local img = wingman.screen.capture(0, 0, 1920, 1080)

-- 运行分类
local label, confidence = model.classify(img)
print("类别: " .. label .. ", 置信度: " .. string.format("%.2f", confidence))
```

:::

---

## Model 对象：获取输入信息

### get_input_info() / getInputInfo()

**说明**：获取模型输入信息。

**函数签名**：

```python
model.get_input_info() -> dict[str, tuple]
```

```lua
model.getInputInfo() -> table
```

**返回**：
- Python: 字典 `{name: (shape, type)}`
- Lua: 数组 `[{name: string, shape: table, type: string}]`

:::tabs

== Python

```python:line-numbers
from wingman import ml

model = ml.load_model("models/yolov8n.onnx")

# 获取输入信息
inputs = model.get_input_info()
for name, (shape, dtype) in inputs.items():
    print(f"输入: {name}, 形状: {shape}, 类型: {dtype}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local model = ml.loadModel("models/yolov8n.onnx")

-- 获取输入信息
local inputs = model.getInputInfo()
for i, info in ipairs(inputs) do
    print("输入: " .. info.name)
    print("  形状: " .. table.concat(info.shape, "x"))
    print("  类型: " .. info.type)
end
```

:::

---

## Model 对象：获取输出信息

### get_output_info() / getOutputInfo()

**说明**：获取模型输出信息。

**函数签名**：

```python
model.get_output_info() -> dict[str, tuple]
```

```lua
model.getOutputInfo() -> table
```

**返回**：
- Python: 字典 `{name: (shape, type)}`
- Lua: 数组 `[{name: string, shape: table, type: string}]`

:::tabs

== Python

```python:line-numbers
from wingman import ml

model = ml.load_model("models/yolov8n.onnx")

# 获取输出信息
outputs = model.get_output_info()
for name, (shape, dtype) in outputs.items():
    print(f"输出: {name}, 形状: {shape}, 类型: {dtype}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local model = ml.loadModel("models/yolov8n.onnx")

-- 获取输出信息
local outputs = model.getOutputInfo()
for i, info in ipairs(outputs) do
    print("输出: " .. info.name)
    print("  形状: " .. table.concat(info.shape, "x"))
    print("  类型: " .. info.type)
end
```

:::

---

## Model 对象：卸载模型

### unload() / unload()

**说明**：卸载模型，释放内存。

**函数签名**：

```python
model.unload() -> None
```

```lua
model.unload() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import ml

model = ml.load_model("models/yolov8n.onnx")

# 使用完毕后卸载模型释放内存
model.unload()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local model = ml.loadModel("models/yolov8n.onnx")

-- 使用完毕后卸载模型释放内存
model.unload()
```

:::

---

## 可用接口

### 模块函数

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `load_model(path, provider?)` | `loadModel(path, provider?)` | 加载 ONNX 模型 | path: 模型路径<br>provider: 执行提供器（cpu/cuda/dml）<br>返回: Model 对象 |
| `get_available_execution_providers()` | `getAvailableExecutionProviders()` | 获取可用执行提供器 | 返回: 提供器名称列表 |

### Model 对象方法

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `detect(img, conf?, nms?)` | `detect(img, conf?, nms?)` | 目标检测 | img: 图像对象<br>conf: 置信度阈值(默认0.5)<br>nms: NMS阈值(默认0.45)<br>返回: 检测结果数组 |
| `classify(img)` | `classify(img)` | 图像分类 | img: 图像对象<br>返回: (label, confidence) |
| `get_input_info()` | `getInputInfo()` | 获取输入信息 | 返回: 输入信息 |
| `get_output_info()` | `getOutputInfo()` | 获取输出信息 | 返回: 输出信息 |
| `unload()` | `unload()` | 卸载模型 | 无返回值 |

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

- **ONNX (.onnx)** - 通用格式
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
