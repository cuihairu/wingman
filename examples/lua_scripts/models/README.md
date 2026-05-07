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
