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

    // Segmentation
    static Bitmap segment(
        ModelEngine& engine,
        const std::string& inputName,
        const uint8_t* imageData,
        int width, int height
    );
};

} // namespace wingman
