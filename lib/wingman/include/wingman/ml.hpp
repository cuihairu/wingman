#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace wingman {

// Forward declarations
class Bitmap;

// 张量数据类型
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

// 张量形状
using TensorShape = std::vector<int64_t>;

// 张量数据
struct TensorData {
    TensorDataType dataType;
    TensorShape shape;
    std::vector<uint8_t> data;

    size_t elementCount() const;
    size_t byteSize() const;
};

// 模型输出
struct ModelOutput {
    std::string name;
    TensorData tensor;
};

// 推理结果
struct InferenceResult {
    bool success;
    std::string error;
    std::vector<ModelOutput> outputs;
    double inferenceTimeMs;
};

// AI/ML 模型推理引擎
class ModelEngine {
public:
    ModelEngine();
    ~ModelEngine();

    // 加载模型
    bool loadModel(const std::string& modelPath, const std::string& executionProvider = "cpu");

    // 卸载模型
    void unloadModel();

    // 检查模型是否已加载
    bool isModelLoaded() const { return modelLoaded_; }

    // 获取模型输入信息
    std::vector<std::pair<std::string, TensorShape>> getInputInfo() const;

    // 获取模型输出信息
    std::vector<std::pair<std::string, TensorShape>> getOutputInfo() const;

    // 运行推理
    InferenceResult run(const std::map<std::string, TensorData>& inputs);

    // 运行推理（单输入单输出简化版）
    InferenceResult run(const std::string& inputName, const TensorData& input);

    // 获取支持的执行提供器
    static std::vector<std::string> getAvailableExecutionProviders();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool modelLoaded_;
};

// 辅助函数：创建张量数据
namespace Tensor {
    // 从 vector 创建 float32 张量
    TensorData createFloat32(const TensorShape& shape, const std::vector<float>& data);

    // 从 vector 创建 int32 张量
    TensorData createInt32(const TensorShape& shape, const std::vector<int32_t>& data);

    // 从图像数据创建张量 (HWC -> CHW, 归一化)
    TensorData fromImage(const uint8_t* imageData, int width, int height,
                        float meanR = 0.485f, float meanG = 0.456f, float meanB = 0.406f,
                        float stdR = 0.229f, float stdG = 0.224f, float stdB = 0.225f);

    // 获取张量数据
    template<typename T>
    std::vector<T> getData(const TensorData& tensor);
}

// 常用模型助手
class ModelHelpers {
public:
    // 图像分类
    static std::pair<std::string, float> classifyImage(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height,
        const std::vector<std::string>& labels = {}
    );

    // 目标检测
    struct Detection {
        float x, y, width, height;
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

    // 分割
    static Bitmap segment(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height
    );
};

} // namespace wingman
