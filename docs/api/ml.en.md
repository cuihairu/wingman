> [中文](ml.md) | **English**

# ML/ONNX

This document is lengthy (522 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [ml.md](ml.md) for the original.

- 模块概述
- 查询执行提供器
  - providers()
- 加载 ONNX 模型
  - load_model(path, ep?) / loadModel(path, ep?)
- 查询加载状态
  - is_loaded(model_id) / isLoaded(modelId)
- 查询输入信息
  - inputs(model_id) / inputs(modelId)
- 查询输出信息
  - outputs(model_id) / outputs(modelId)
- 运行推理
  - run(model_id, inputs) / run(modelId, inputs)
- 卸载模型
  - unload(model_id) / unload(modelId)
- 完整示例
- 尚未暴露的高层能力
- 模型准备
  - YOLOv8 模型转换
  - 编译选项
