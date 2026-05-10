#include "wingman/ml.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

// ONNX Runtime stub implementation
// When WINGMAN_ENABLE_ML is not defined, provide stub functions

namespace wingman {

// ========== TensorData 实现 ==========

size_t TensorData::elementCount() const {
    size_t count = 1;
    for (auto dim : shape) {
        count *= static_cast<size_t>(dim);
    }
    return count;
}

size_t TensorData::byteSize() const {
    size_t elemSize = 4; // Default float32
    switch (dataType) {
        case TensorDataType::FLOAT32: elemSize = 4; break;
        case TensorDataType::FLOAT64: elemSize = 8; break;
        case TensorDataType::INT32:   elemSize = 4; break;
        case TensorDataType::INT64:   elemSize = 8; break;
        case TensorDataType::UINT8:   elemSize = 1; break;
        default: break;
    }
    return elementCount() * elemSize;
}

// ========== ModelEngine 私有实现 ==========

class ModelEngine::Impl {
public:
    std::string modelPath_;
};

// ========== ModelEngine 实现 ==========

ModelEngine::ModelEngine() : impl_(std::make_unique<Impl>()), modelLoaded_(false) {}

ModelEngine::~ModelEngine() {
    unloadModel();
}

bool ModelEngine::loadModel(const std::string& /*modelPath*/, const std::string& /*executionProvider*/) {
    spdlog::warn("ML/AI support not enabled (compile with WINGMAN_ENABLE_ML)");
    return false;
}

void ModelEngine::unloadModel() {
    modelLoaded_ = false;
}

std::vector<std::pair<std::string, TensorShape>> ModelEngine::getInputInfo() const {
    return {};
}

std::vector<std::pair<std::string, TensorShape>> ModelEngine::getOutputInfo() const {
    return {};
}

InferenceResult ModelEngine::run(const std::map<std::string, TensorData>& /*inputs*/) {
    InferenceResult result = {false, "ML/AI support not enabled", {}, 0.0};
    return result;
}

InferenceResult ModelEngine::run(const std::string& /*inputName*/, const TensorData& /*input*/) {
    InferenceResult result = {false, "ML/AI support not enabled", {}, 0.0};
    return result;
}

std::vector<std::string> ModelEngine::getAvailableExecutionProviders() {
    return {"cpu"};
}

// ========== Tensor 辅助函数 ==========

TensorData Tensor::createFloat32(const TensorShape& shape, const std::vector<float>& data) {
    TensorData tensor;
    tensor.dataType = TensorDataType::FLOAT32;
    tensor.shape = shape;

    size_t byteSize = data.size() * sizeof(float);
    tensor.data.resize(byteSize);
    std::memcpy(tensor.data.data(), data.data(), byteSize);

    return tensor;
}

TensorData Tensor::createInt32(const TensorShape& shape, const std::vector<int32_t>& data) {
    TensorData tensor;
    tensor.dataType = TensorDataType::INT32;
    tensor.shape = shape;

    size_t byteSize = data.size() * sizeof(int32_t);
    tensor.data.resize(byteSize);
    std::memcpy(tensor.data.data(), data.data(), byteSize);

    return tensor;
}

TensorData Tensor::fromImage(const uint8_t* imageData, int width, int height,
                             float meanR, float meanG, float meanB,
                             float stdR, float stdG, float stdB) {
    std::vector<float> data(width * height * 3);

    for (int i = 0; i < width * height; i++) {
        uint8_t b = imageData[i * 3];
        uint8_t g = imageData[i * 3 + 1];
        uint8_t r = imageData[i * 3 + 2];

        int h = i / width;
        int w = i % width;

        data[0 * width * height + h * width + w] = ((r / 255.0f) - meanR) / stdR;
        data[1 * width * height + h * width + w] = ((g / 255.0f) - meanG) / stdG;
        data[2 * width * height + h * width + w] = ((b / 255.0f) - meanB) / stdB;
    }

    return createFloat32({1, 3, (int64_t)height, (int64_t)width}, data);
}

// ========== ModelHelpers 实现 ==========

std::pair<std::string, float> ModelHelpers::classifyImage(
    ModelEngine& /*engine*/,
    const std::string& /*inputName*/,
    const uint8_t* /*imageData*/,
    int /*width*/, int /*height*/,
    const std::vector<std::string>& /*labels*/
) {
    spdlog::warn("ML/AI support not enabled");
    return {"", 0.0f};
}

std::vector<ModelHelpers::Detection> ModelHelpers::detectObjects(
    ModelEngine& /*engine*/,
    const std::string& /*inputName*/,
    const uint8_t* /*imageData*/,
    int /*width*/, int /*height*/,
    float /*confThreshold*/,
    float /*nmsThreshold*/
) {
    spdlog::warn("ML/AI support not enabled");
    return {};
}

Bitmap ModelHelpers::segment(
    ModelEngine& /*engine*/,
    const std::string& /*inputName*/,
    const uint8_t* /*imageData*/,
    int /*width*/, int /*height*/
) {
    spdlog::warn("ML/AI support not enabled");
    return Bitmap(0, 0);  // Return empty bitmap as sentinel
}

} // namespace wingman
