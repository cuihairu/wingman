# YOLO 模型使用指南

本指南面向初学者，详细介绍如何使用 YOLO 模型进行目标检测，并将模型集成到 Wingman 自动化脚本中。

## 什么是 YOLO？

YOLO (You Only Look Once) 是一种实时目标检测算法，可以：
- **检测图像中的多个对象**
- **识别对象类别**（人、车、动物等）
- **定位对象位置**（边界框坐标）
- **实时处理**（速度快）

## 什么是 ONNX？

ONNX (Open Neural Network Exchange) 是一种开放的格式，用于表示机器学习模型。Wingman 使用 ONNX Runtime 来运行 YOLO 模型，这意味着：

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

## 第四步：在 Wingman 中使用

将导出的 `.onnx` 文件放入项目目录，然后在脚本中加载使用。

### Python 示例

```python
from wingman import vision, screen

# 加载 YOLO 模型
model = vision.load_yolo("yolov8n.onnx")

# 截取屏幕
img = screen.capture(0, 0, 1920, 1080)

# 检测目标
results = model.detect(img)

# 处理结果
for result in results:
    print(f"类别: {result['class']}, 置信度: {result['confidence']:.2f}")
    print(f"位置: x={result['x']}, y={result['y']}, w={result['width']}, h={result['height']}")
```

### Lua 示例

```lua
local vision = require("wingman.vision")
local screen = require("wingman.screen")

-- 加载 YOLO 模型
local model = vision.loadYolo("yolov8n.onnx")

-- 截取屏幕
local img = screen.capture(0, 0, 1920, 1080)

-- 检测目标
local results = model:detect(img)

-- 处理结果
for i, result in ipairs(results) do
    print("类别: " .. result.class .. ", 置信度: " .. result.confidence)
    print(string.format("位置: x=%d, y=%d, w=%d, h=%d",
        result.x, result.y, result.width, result.height))
end
```

---

## 实战案例

### 案例 1：检测游戏中的敌人

```python
from wingman import vision, screen, input

model = vision.load_yolo("yolov8n.onnx")

while True:
    # 截屏
    img = screen.capture(0, 0, 1920, 1080)

    # 检测人物类别（COCO 数据集中类别 0 是 person）
    results = model.detect(img, filter_classes=["person"])

    # 如果检测到人，点击目标
    for result in results:
        if result['confidence'] > 0.7:  # 置信度阈值
            # 计算中心点
            center_x = result['x'] + result['width'] // 2
            center_y = result['y'] + result['height'] // 2
            input.click(center_x, center_y)
            print(f"点击敌人: {result['class']}")
            break
```

### 案例 2：计数器

```python
from wingman import vision, screen

model = vision.load_yolo("yolov8n.onnx")

while True:
    img = screen.capture(0, 0, 1920, 1080)
    results = model.detect(img)

    # 统计各类别数量
    counts = {}
    for result in results:
        cls = result['class']
        counts[cls] = counts.get(cls, 0) + 1

    print(f"检测到: {counts}")
    # 输出示例: {'person': 3, 'car': 1}
```

---

## 检测结果格式

`detect()` 方法返回的结果是一个列表，每个元素包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| `class` | string | 类别名称（如 "person", "car"） |
| `class_id` | number | 类别 ID（0=person, 2=car 等） |
| `confidence` | number | 置信度（0-1） |
| `x` | number | 边界框左上角 X |
| `y` | number | 边界框左上角 Y |
| `width` | number | 边界框宽度 |
| `height` | number | 边界框高度 |

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

A: 可能原因：
- 置信度阈值太高，尝试降低 `filter_confidence` 参数
- 对象太小，尝试使用更高分辨率训练的模型
- 类别不在 COCO 数据集中

### Q: 检测速度太慢？

A: 优化方法：
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
