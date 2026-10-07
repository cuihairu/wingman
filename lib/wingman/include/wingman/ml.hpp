#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace wingman {

// Forward declarations
class Bitmap;

// Tensor data type
enum class TensorDataType {
    FLOAT32,
    FLOAT64,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    BOOL
};

// Tensor shape
using TensorShape = std::vector<int64_t>;

// Tensor data
struct TensorData {
    TensorDataType dataType;
    TensorShape shape;
    std::vector<uint8_t> data;

    size_t elementCount() const;
    size_t byteSize() const;
};

// Model output
struct ModelOutput {
    std::string name;
    TensorData tensor;
};

// Inference result
struct InferenceResult {
    bool success;
    std::string error;
    std::vector<ModelOutput> outputs;
    double inferenceTimeMs;
};

// AI/ML model inference engine
class ModelEngine {
public:
    ModelEngine();
    ~ModelEngine();

    // Load model
    bool loadModel(const std::string& modelPath, const std::string& executionProvider = "cpu");

    // Unload model
    void unloadModel();

    // Check if model is loaded
    bool isModelLoaded() const { return modelLoaded_; }

    // Get model input info
    std::vector<std::pair<std::string, TensorShape>> getInputInfo() const;

    // Get model output info
    std::vector<std::pair<std::string, TensorShape>> getOutputInfo() const;

    // Run inference
    InferenceResult run(const std::map<std::string, TensorData>& inputs);

    // Run inference (single input/output simplified)
    InferenceResult run(const std::string& inputName, const TensorData& input);

    // Get available execution providers
    static std::vector<std::string> getAvailableExecutionProviders();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool modelLoaded_;
};

// Helper: create tensor data
namespace Tensor {
    // Create float32 tensor from vector
    TensorData createFloat32(const TensorShape& shape, const std::vector<float>& data);

    // Create int32 tensor from vector
    TensorData createInt32(const TensorShape& shape, const std::vector<int32_t>& data);

    // Create tensor from image data (HWC -> CHW, normalize)
    TensorData fromImage(const uint8_t* imageData, int width, int height,
                        float meanR = 0.485f, float meanG = 0.456f, float meanB = 0.406f,
                        float stdR = 0.229f, float stdG = 0.224f, float stdB = 0.225f);

    // Get tensor data
    template<typename T>
    std::vector<T> getData(const TensorData& tensor);
}

// Common model helpers
class ModelHelpers {
public:
    // Image classification
    static std::pair<std::string, float> classifyImage(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height,
        const std::vector<std::string>& labels = {}
    );

    // Object detection
    // 约定：输入 BGR packed 3 通道（与 Tensor::fromImage 一致），内部按模型
    // 输入尺寸 resize（无 letterbox，比例失真按对应关系还原）；支持 YOLOv5
    // [1,N,5+C]（cx,cy,w,h,obj,classes，conf=obj×cls）与 YOLOv8 [1,4+C,N]
    // （cx,cy,w,h,classes）两种导出格式，按输出形状自动判别。
    struct Detection {
        float x, y, width, height; // 原图像素坐标（左上角+尺寸）
        int classId;
        float confidence;
    };
    static std::vector<Detection> detectObjects(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height,
        float confThreshold = 0.5f,
        float nmsThreshold = 0.45f
    );

    // ===== 检测后处理纯函数（单测面；不依赖 onnxruntime 会话）=====

    // 解码 YOLO 检测输出为候选框：坐标为模型输入空间像素（x/y 左上角、
    // w/h 尺寸），conf ≥ confThreshold 才保留。v5/v8 布局按输出张量形状
    // 自动判别；非法形状/非 float32 返回空。
    static std::vector<Detection> decodeYoloOutput(
        const TensorData& output,
        float confThreshold);

    // IoU 贪心 NMS：按 confidence 降序保留，与已保留框 IoU > iouThreshold
    // 的抑制（保留先出现者）。
    static std::vector<Detection> nmsBoxes(
        std::vector<Detection> detections,
        float iouThreshold);

    // Segmentation
    static Bitmap segment(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height
    );
};

} // namespace wingman
