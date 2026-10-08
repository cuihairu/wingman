# YOLO 模型使用指南

本指南面向初学者，说明如何准备 YOLO ONNX 模型，在 Wingman 中验证模型加载与输入/输出元数据，并通过 `vision.aiSetupLocal` 接入本地检测。

::: warning 状态
Wingman 脚本层没有名为 `detect()` 的 YOLO 高层 API，`wingman.ml` 模块也确实不提供 detect——但**本地检测链路已可用**：`vision.aiSetupLocal`（本地 ONNX 检测 provider，需 `WINGMAN_ENABLE_ML` 构建）+ `vision.aiLocate` / `vision.aiElements` 组成完整闭环，见「第五步」。
:::

## 什么是 YOLO？

YOLO (You Only Look Once) 是一种实时目标检测算法，可以：
- **检测图像中的多个对象**
- **识别对象类别**（人、车、动物等）
- **定位对象位置**（边界框坐标）
- **实时处理**（速度快）

## 什么是 ONNX？

ONNX (Open Neural Network Exchange) 是一种开放的格式，用于表示机器学习模型。Wingman 的 ML 模块基于 ONNX Runtime 加载模型，这意味着：

1. **跨平台兼容** - Windows、macOS、Linux 都能运行
2. **无需训练** - 直接使用预训练模型
3. **高性能** - ONNX Runtime 针对推理进行了优化

---

## 第一步：安装 Ultralytics

Ultralytics 是 YOLO 的官方 Python 库，用于训练和导出模型。

```bash
pip install ultralytics
```

> **提示**：建议使用虚拟环境
> ```bash
> python -m venv venv
> source venv/bin/activate  # Linux/macOS
> .\venv\Scripts\activate     # Windows
> pip install ultralytics
> ```

---

## 第二步：下载预训练模型

Ultralytics 提供了多种预训练模型，不同规模适合不同需求：

| 模型 | 大小 | 速度 | 精度 | 推荐用途 |
|------|------|------|------|----------|
| YOLOv8n | 6.3 MB | 最快 | 中等 | 实时游戏、资源受限环境 |
| YOLOv8s | 21.5 MB | 快 | 良好 | 平衡性能和精度 |
| YOLOv8m | 49.7 MB | 中等 | 高 | 高精度需求 |
| YOLOv8l | 83.7 MB | 慢 | 很高 | 离线处理 |
| YOLOv8x | 130.5 MB | 最慢 | 最高 | 最高精度需求 |

对于游戏自动化，**推荐使用 YOLOv8n 或 YOLOv8s**，因为它们速度快且足够准确。

### 自动下载（首次使用时）

首次运行时，Ultralytics 会自动从网上下载模型：

```python
from ultralytics import YOLO

# 自动下载 YOLOv8n（nano）
model = YOLO('yolov8n.pt')

# 自动下载 YOLOv8s（small）
model = YOLO('yolov8s.pt')
```

### 手动下载

如果网络受限，可以手动下载：

1. 访问 [Ultralytics 发布页](https://github.com/ultralytics/ultralytics/releases)
2. 下载 `.pt` 文件
3. 保存到本地目录

---

## 第三步：导出为 ONNX 格式

Wingman 只支持 ONNX 格式，需要将 PyTorch 模型（`.pt`）导出。

### 基本导出

```python
from ultralytics import YOLO

# 加载模型
model = YOLO('yolov8n.pt')

# 导出为 ONNX
model.export(format='onnx')
```

这会生成 `yolov8n.onnx` 文件。

### 高级导出选项

```python
model.export(
    format='onnx',
    imgsz=640,              # 输入图像尺寸（默认 640）
    half=True,              # 使用 FP16 精度（更快，精度略降）
    simplify=True,          # 简化模型
    opset=12,              # ONNX opset 版本
    dynamic=False          # 动态输入尺寸（Wingman 暂不支持）
)
```

### 推荐配置（游戏自动化）

```python
from ultralytics import YOLO

# 使用 nano 模型，速度优先
model = YOLO('yolov8n.pt')

# 导出时优化速度
model.export(
    format='onnx',
    imgsz=640,
    half=True,          # FP16 更快
    simplify=True,      # 简化模型
    opset=12
)
```

---

## 第四步：在 Wingman 中验证模型

将导出的 `.onnx` 文件放入项目目录，然后使用 `wingman.ml` 加载模型并检查输入/输出元数据。

::: warning 当前限制
`wingman.ml` 模块只提供模型加载、元数据查询与原始推理：`providers()` / `loadModel()` / `isLoaded()` / `inputs()` / `outputs()` / `run()` / `unload()`——没有 `detect()` 这类 YOLO 高层封装。

检测结果走 vision 模块的本地检测 provider（见下一节「第五步」）；本步只验证 ONNX 模型能被 Wingman 加载并读出输入/输出元数据。
:::

### Python 示例

```python
from wingman import ml

print("providers:", ml.providers())

model_id = ml.load_model("yolov8n.onnx", "cpu")
if model_id is None:
    raise RuntimeError("模型加载失败，请检查 WINGMAN_ENABLE_ML、模型路径和 ONNX Runtime 依赖")

print("loaded:", ml.is_loaded(model_id))

print("inputs:")
for item in ml.inputs(model_id):
    print(f"  {item['name']}: {item['shape']}")

print("outputs:")
for item in ml.outputs(model_id):
    print(f"  {item['name']}: {item['shape']}")

ml.unload(model_id)
```

### Lua 示例

```lua
local wingman = require("wingman")

print("providers:")
for _, provider in ipairs(wingman.ml.providers()) do
    print("  " .. provider)
end

local modelId = wingman.ml.loadModel("yolov8n.onnx", "cpu")
if not modelId then
    error("模型加载失败，请检查 WINGMAN_ENABLE_ML、模型路径和 ONNX Runtime 依赖")
end

print("loaded:", wingman.ml.isLoaded(modelId))

print("inputs:")
for _, item in ipairs(wingman.ml.inputs(modelId)) do
    print("  " .. item.name .. ": " .. table.concat(item.shape, "x"))
end

print("outputs:")
for _, item in ipairs(wingman.ml.outputs(modelId)) do
    print("  " .. item.name .. ": " .. table.concat(item.shape, "x"))
end

wingman.ml.unload(modelId)
```

---

## 第五步：接入本地检测（vision.aiSetupLocal + aiLocate/aiElements）

验证模型能加载之后，用 vision 模块的本地 ONNX 检测 provider 直接得到检测结果（截图 → 预处理 → 推理 → YOLO 输出解码 → 标签映射，全部进程内完成，不依赖外部推理服务）。

### aiSetupLocal(cfg)

`cfg` 字段：

- `modelPath`（string，必填）— ONNX 模型文件路径
- `labels`（string 列表，可选）— classId→名称映射表；`aiLocate` 的 `desc` 与 label 按双向大小写不敏感子串匹配，labels 缺项回退 `class_N`
- `minConfidence`（float，默认 0.5）— 检测置信阈值

需 `WINGMAN_ENABLE_ML` 构建（onnxruntime）；未构建 ML 时 `aiSetupLocal` 恒返回 `false`，不静默降级。与 `aiSetup` 的 HTTP 视觉 provider 互斥，后调用者生效。

模型约定：YOLOv5 输出布局 `[1,N,5+C]` 与 YOLOv8 布局 `[1,4+C,N]` 按输出形状自动判别，两种导出都能直接用；输入按模型 NCHW 尺寸 resize 到 0–1 归一，输出框还原为原图像素坐标。

### Python 示例

```python
from wingman import vision, input

assert vision.aiSetupLocal({
    "modelPath": "yolov8n.onnx",
    "labels": ["person", "car", "button"],
    "minConfidence": 0.6,
})

box = vision.aiLocate("button")   # 单个最佳匹配
if box["found"]:
    input.click(box["x"] + box["w"] // 2, box["y"] + box["h"] // 2)

page = vision.aiElements("")      # desc 传空 = 列出全部检出元素
for elem in page["elements"]:
    print(elem)
```

### Lua 示例

```lua
local wingman = require("wingman")

assert(wingman.vision.aiSetupLocal({
    modelPath = "yolov8n.onnx",
    labels = { "person", "car", "button" },
    minConfidence = 0.6,
}))

local box = wingman.vision.aiLocate("button")
if box.found then
    wingman.input.click(box.x + box.w / 2, box.y + box.h / 2)
end

local page = wingman.vision.aiElements("")
for _, elem in ipairs(page.elements) do
    print(elem.label, elem.x, elem.y, elem.w, elem.h, elem.confidence)
end
```

> 可运行示例另见 `examples/lua_scripts/onnx_object_detection.lua`（演示 `wingman.ml` 加载 + `run()` 原始推理通路；注意该文件头部「detect/classify 高层封装尚未接入脚本层」的注释已过时，以本页与 `docs/api/vision.md` 口径为准）。

---

## 能力现状与边界

目标检测链路（截图/Tensor 转换、YOLO 输出后处理含 NMS 与类别标签映射、脚本层入口、置信度阈值等性能配置）已由 vision 模块本地 provider 落地。`wingman.ml` 仍是 ONNX Runtime 模型加载/元数据/原始推理入口（`run()`），不提供 YOLO 高层封装。

当前本地检测的已知边界：

- 预处理为简单 resize、无 letterbox，极端长宽比画面检测框会有系统性偏移
- 分割（segment）模型未支持，遇到会报错返回空结果
- `aiSetupLocal` 需 `WINGMAN_ENABLE_ML` 构建；未构建时恒返回 `false`

---

## 检测结果格式

脚本层没有返回 `class` / `class_id` / `width` / `height` 字段的 detect()；已落地的是以下两套形状（类别经 labels 表映射为 `label`，缺项回退 `class_N`）。

`vision.aiLocate(desc, region?)` — 单个最佳匹配：

| 字段 | 类型 | 说明 |
|------|------|------|
| `found` | bool | 是否命中 |
| `label` | string | 类别名称（labels 映射） |
| `confidence` | number | 置信度（0-1） |
| `x` | number | 边界框左上角 X（原图像素） |
| `y` | number | 边界框左上角 Y（原图像素） |
| `w` | number | 边界框宽度 |
| `h` | number | 边界框高度 |

未命中返回 `{found=false, error}`。

`vision.aiElements(desc, region?)` — 批量结果，返回 `{found, elements:[{label, x, y, w, h, confidence}]}`，每个元素含 `label` / `x` / `y` / `w` / `h` / `confidence`；失败与「无匹配」同形返回 `{found=false, error}`。

---

## COCO 数据集类别

默认的 YOLO 模型是在 COCO 数据集上训练的，支持 80 个类别：

| ID | 类别 | ID | 类别 |
|----|------|----|------|
| 0 | person | 1 | bicycle |
| 2 | car | 3 | motorcycle |
| 5 | bus | 7 | truck |
| ... | ... | ... | ... |

完整列表请参考 [COCO 数据集文档](https://cocodataset.org/#overview)。

---

## 性能优化建议

1. **使用较小的模型** - YOLOv8n 比 YOLOv8x 快 20 倍
2. **降低输入尺寸** - `imgsz=320` 比 `imgsz=640` 快 4 倍
3. **使用 FP16** - 导出时设置 `half=True`
4. **缩小检测区域** - 只截取屏幕相关区域
5. **降低检测频率** - 每 100-200ms 检测一次即可

---

## 常见问题

### Q: 模型检测不到对象？

A: 先用本指南第四步的元数据示例确认模型能加载；再用第五步的 `vision.aiSetupLocal` + `vision.aiLocate` 实测。仍检不到时按顺序排查：`aiSetupLocal` 是否返回 true（需 `WINGMAN_ENABLE_ML` 构建、模型路径正确）、`labels`/`desc` 是否能子串匹配上（labels 缺项会回退 `class_N`）、`minConfidence` 是否设得过高。

### Q: 检测速度太慢？

A: 可从这些方向优化：
- 使用 YOLOv8n 模型
- 减小输入尺寸 `imgsz=320`
- 使用 FP16 精度 `half=True`
- 只检测屏幕的一部分

### Q: 能否检测自定义类别？

A: 可以，但需要：
1. 收集自定义数据集
2. 使用 Ultralytics 训练自定义模型
3. 导出为 ONNX 格式

---

## 参考资源

- [Ultralytics 官方文档](https://docs.ultralytics.com/)
- [ONNX Runtime 文档](https://onnxruntime.ai/)
- [COCO 数据集](https://cocodataset.org/)
- [Wingman Vision API](../api/vision.md)
