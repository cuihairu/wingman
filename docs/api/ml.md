# API: wingman.ml

机器学习模型推理模块，当前提供 ONNX 模型的加载、推理、卸载与输入/输出元数据查询。

::: warning 当前状态
此模块需要编译时启用 `WINGMAN_ENABLE_ML=ON` 才能加载真实模型。未启用时走存根实现：函数仍存在，但 `loadModel()` 返回 `nil`，`run()` 返回 `success=false`，`providers()` 通常只返回 `["cpu"]`。

脚本层当前是 **ID 句柄式 API**，不是 `Model` 对象 API。`detect()` / `classify()` 等 YOLO/分类高层封装尚未暴露到脚本层。
:::

## 模块概述

`ml` 模块当前用于：
- 查询可用执行提供器
- 加载 ONNX 模型并获得 `modelId`
- 查询模型是否已加载
- 查询模型输入/输出名称与 shape
- 执行推理
- 卸载模型

当前已注册函数：

| Python 函数 | Lua 函数 | 说明 |
|------------|----------|------|
| `providers()` | `providers()` | 返回执行提供器名称列表 |
| `load_model(path, ep?)` | `loadModel(path, ep?)` | 加载模型，返回 `modelId` 或空值 |
| `unload(model_id)` | `unload(modelId)` | 卸载模型 |
| `is_loaded(model_id)` | `isLoaded(modelId)` | 判断模型是否仍在注册表中且已加载 |
| `inputs(model_id)` | `inputs(modelId)` | 查询输入信息 |
| `outputs(model_id)` | `outputs(modelId)` | 查询输出信息 |
| `run(model_id, inputs)` | `run(modelId, inputs)` | 执行推理，返回结果对象 |

Python 绑定也保留原始 camelCase 名称，例如 `ml.loadModel()` 和 `ml.isLoaded()` 仍可用；推荐 Python 新代码使用 snake_case。

---

## 查询执行提供器

### providers()

**说明**：返回 ML 引擎知道的执行提供器名称列表。

**函数签名**：

```python
providers() -> list[str]
```

```lua
providers() -> table
```

**返回**：
- 启用 ML 的构建通常返回 `["cpu", "cuda", "dml"]`
- 未启用 ML 的存根构建返回 `["cpu"]`

:::tabs

== Python

```python:line-numbers
from wingman import ml

print(ml.providers())
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local providers = wingman.ml.providers()
for _, provider in ipairs(providers) do
    print(provider)
end
```

:::

---

## 加载 ONNX 模型

### load_model(path, ep?) / loadModel(path, ep?)

**说明**：加载 ONNX 模型，成功时返回模型 ID 字符串。

**函数签名**：

```python
load_model(path: str, ep: str = "cpu") -> str | None
```

```lua
loadModel(path: string, ep: string = "cpu") -> string | nil
```

**参数**：
- `path` - ONNX 模型路径
- `ep` - 执行提供器，常用值为 `"cpu"`、`"cuda"`、`"dml"`；具体可用性取决于构建与运行环境

**返回**：
- 成功：`modelId`，例如 `"ml-1"`
- 失败：`None` / `nil`

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/yolov8n.onnx", "cpu")
if model_id is None:
    print("模型加载失败，检查 WINGMAN_ENABLE_ML、模型路径和 ONNX Runtime 依赖")
else:
    print("模型 ID:", model_id)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/yolov8n.onnx", "cpu")
if not modelId then
    print("模型加载失败，检查 WINGMAN_ENABLE_ML、模型路径和 ONNX Runtime 依赖")
else
    print("模型 ID:", modelId)
end
```

:::

---

## 查询加载状态

### is_loaded(model_id) / isLoaded(modelId)

**说明**：判断模型 ID 是否对应一个已加载模型。

**函数签名**：

```python
is_loaded(model_id: str) -> bool
```

```lua
isLoaded(modelId: string) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/model.onnx")
if model_id and ml.is_loaded(model_id):
    print("模型已加载")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/model.onnx")
if modelId and wingman.ml.isLoaded(modelId) then
    print("模型已加载")
end
```

:::

---

## 查询输入信息

### inputs(model_id) / inputs(modelId)

**说明**：返回模型输入列表。

**函数签名**：

```python
inputs(model_id: str) -> list[dict]
```

```lua
inputs(modelId: string) -> table
```

**返回格式**：

```json
[
  {"name": "images", "shape": [1, 3, 640, 640]}
]
```

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/model.onnx")
if model_id:
    for item in ml.inputs(model_id):
        print(item["name"], item["shape"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/model.onnx")
if modelId then
    for _, item in ipairs(wingman.ml.inputs(modelId)) do
        print(item.name, table.concat(item.shape, "x"))
    end
end
```

:::

---

## 查询输出信息

### outputs(model_id) / outputs(modelId)

**说明**：返回模型输出列表。

**函数签名**：

```python
outputs(model_id: str) -> list[dict]
```

```lua
outputs(modelId: string) -> table
```

**返回格式**：

```json
[
  {"name": "output0", "shape": [1, 84, 8400]}
]
```

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/model.onnx")
if model_id:
    for item in ml.outputs(model_id):
        print(item["name"], item["shape"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/model.onnx")
if modelId then
    for _, item in ipairs(wingman.ml.outputs(modelId)) do
        print(item.name, table.concat(item.shape, "x"))
    end
end
```

:::

---

## 运行推理

### run(model_id, inputs) / run(modelId, inputs)

**说明**：对已加载模型执行一次推理。

**函数签名**：

```python
run(model_id: str, inputs: list[ModelInput]) -> InferenceResult
```

```lua
run(modelId: string, inputs: table) -> table
```

**参数**：
- `model_id` - `load_model()` 返回的模型 ID
- `inputs` - 输入张量列表，每项为 `{name, data, shape?, dtype?}`：
  - `name` - 输入张量名（见 `inputs()` 返回的元数据）
  - `data` - 数值数组（按行主序展平；整型与浮点均可）
  - `shape` - 可选维度数组；缺省为一维 `[len(data)]`
  - `dtype` - 可选元素类型，默认 `"float32"`；支持 `float32` / `float64` / `int8` / `int16` / `int32` / `int64` / `uint8` / `uint16` / `uint32` / `uint64` / `bool`

**返回**（对应 C++ `InferenceResult`）：

```json
{
  "success": true,
  "error": "",
  "outputs": [{"name": "output0", "shape": [1, 84, 8400], "data": []}],
  "timeMs": 12.3
}
```

- `success` - 推理是否成功
- `error` - 失败原因；成功时为空字符串。模型 ID 不存在、参数不合法、未启用 `WINGMAN_ENABLE_ML`、ONNX Runtime 报错等都会在这里给出
- `outputs` - 输出张量列表，每项 `{name, shape, data}`，`data` 按张量实际元素类型解码为数值数组
- `timeMs` - 推理耗时（毫秒）

失败时 `success` 为 `False` / `false`，`outputs` 为空数组。

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/yolov8n.onnx", "cpu")
if model_id:
    result = ml.run(model_id, [{
        "name": "images",
        "shape": [1, 3, 640, 640],
        "data": [0.0] * (1 * 3 * 640 * 640),
    }])
    if result["success"]:
        for item in result["outputs"]:
            print(item["name"], item["shape"], len(item["data"]))
    else:
        print("推理失败:", result["error"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/yolov8n.onnx", "cpu")
if modelId then
    local result = wingman.ml.run(modelId, {
        { name = "images", shape = {1, 3, 640, 640}, data = {} },
    })
    if result.success then
        for _, item in ipairs(result.outputs) do
            print(item.name, table.concat(item.shape, "x"), #item.data)
        end
    else
        print("推理失败:", result.error)
    end
end
```

:::

---

## 卸载模型

### unload(model_id) / unload(modelId)

**说明**：从脚本层模型注册表中移除模型并释放引擎实例。

**函数签名**：

```python
unload(model_id: str) -> bool
```

```lua
unload(modelId: string) -> boolean
```

**返回**：
- `true` / `True` - 找到并卸载了模型
- `false` / `False` - 模型 ID 不存在或参数无效

:::tabs

== Python

```python:line-numbers
from wingman import ml

model_id = ml.load_model("models/model.onnx")
if model_id:
    print("卸载:", ml.unload(model_id))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local modelId = wingman.ml.loadModel("models/model.onnx")
if modelId then
    print("卸载:", wingman.ml.unload(modelId))
end
```

:::

---

## 完整示例

:::tabs

== Python

```python:line-numbers
from wingman import ml

print("providers:", ml.providers())

model_id = ml.load_model("models/yolov8n.onnx", "cpu")
if not model_id:
    raise RuntimeError("模型加载失败")

print("loaded:", ml.is_loaded(model_id))
print("inputs:", ml.inputs(model_id))
print("outputs:", ml.outputs(model_id))

input_name = ml.inputs(model_id)[0]["name"]
result = ml.run(model_id, [{"name": input_name, "data": [0.0] * 6}])
print("run success:", result["success"], "error:", result["error"])

ml.unload(model_id)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

print("providers:")
for _, provider in ipairs(wingman.ml.providers()) do
    print(" - " .. provider)
end

local modelId = wingman.ml.loadModel("models/yolov8n.onnx", "cpu")
if not modelId then
    error("模型加载失败")
end

print("loaded:", wingman.ml.isLoaded(modelId))
print("inputs:")
for _, item in ipairs(wingman.ml.inputs(modelId)) do
    print(" - " .. item.name .. " " .. table.concat(item.shape, "x"))
end

print("outputs:")
for _, item in ipairs(wingman.ml.outputs(modelId)) do
    print(" - " .. item.name .. " " .. table.concat(item.shape, "x"))
end

local result = wingman.ml.run(modelId, {
    { name = wingman.ml.inputs(modelId)[1].name, data = {0, 0, 0, 0, 0, 0} },
})
print("run success:", result.success, "error:", result.error)

wingman.ml.unload(modelId)
```

:::

---

## 尚未暴露的高层能力

以下能力在文档历史版本中曾以 `Model` 对象形式描述，但当前脚本层尚未实现：
- `model.detect(image, confidenceThreshold, nmsThreshold)`
- `model.classify(image)`
- `model.getInputInfo()` / `model.getOutputInfo()`
- `model.unload()`

推理入口 `run()` 已暴露到脚本层（见上文），底层 C++ 另有 `Tensor::fromImage()` 与部分 `ModelHelpers`，但脚本层还缺少稳定的图像对象到 Tensor 输入路径、检测结果后处理和模型对象生命周期绑定。因此当前脚本请使用本页列出的 ID 句柄式 API。

---

## 模型准备

### YOLOv8 模型转换

```bash
pip install ultralytics
yolo export model=yolov8n.pt format=onnx
```

将生成的 `yolov8n.onnx` 放到脚本可访问的目录，例如 `scripts/models/yolov8n.onnx`。

### 编译选项

启用 ML 支持需要在 CMake 配置时添加：

```bash
cmake -B build -DWINGMAN_ENABLE_ML=ON ...
```

依赖项通过 vcpkg 管理：
- onnxruntime
- CUDA / DirectML（可选，取决于构建配置和平台）
