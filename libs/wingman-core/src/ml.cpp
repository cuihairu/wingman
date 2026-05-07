#include "wingman/ml.hpp"
#include <spdlog/spdlog.h>

// ONNX Runtime - vcpkg 安装的路径是直接可用的
#include <onnxruntime_cxx_api.h>
#ifdef _WIN32
#pragma comment(lib, "onnxruntime")
#endif

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
    size_t elemSize = 0;
    switch (dataType) {
        case TensorDataType::FLOAT32: elemSize = 4; break;
        case TensorDataType::FLOAT64: elemSize = 8; break;
        case TensorDataType::INT8:    elemSize = 1; break;
        case TensorDataType::INT16:   elemSize = 2; break;
        case TensorDataType::INT32:   elemSize = 4; break;
        case TensorDataType::INT64:   elemSize = 8; break;
        case TensorDataType::UINT8:   elemSize = 1; break;
        case TensorDataType::UINT16:  elemSize = 2; break;
        case TensorDataType::UINT32:  elemSize = 4; break;
        case TensorDataType::UINT64:  elemSize = 8; break;
        case TensorDataType::BOOL:    elemSize = 1; break;
    }
    return elementCount() * elemSize;
}

// ========== ModelEngine 私有实现 ==========

class ModelEngine::Impl {
public:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    std::string modelPath_;

    Impl() : env_(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "Wingman")),
              sessionOptions_(std::make_unique<Ort::SessionOptions>()) {}
};

// ========== ModelEngine 实现 ==========

ModelEngine::ModelEngine() : impl_(std::make_unique<Impl>()), modelLoaded_(false) {}

ModelEngine::~ModelEngine() {
    unloadModel();
}

bool ModelEngine::loadModel(const std::string& modelPath, const std::string& executionProvider) {
    try {
        // 设置执行提供器
        if (executionProvider == "cuda") {
#ifdef USE_CUDA
            impl_->sessionOptions_->AppendExecutionProvider_CUDA(OrtCUDAProviderOptions{});
#else
            spdlog::warn("CUDA execution provider requested but not available");
#endif
        } else if (executionProvider == "dml") {
#ifdef USE_DML
            impl_->sessionOptions_->AppendExecutionProvider_DML(DML_EXECUTION_PROVIDER);
#else
            spdlog::warn("DML execution provider requested but not available");
#endif
        }

        // 创建会话
        impl_->session_ = std::make_unique<Ort::Session>(
            *impl_->env_,
            modelPath.c_str(),
            *impl_->sessionOptions_
        );

        impl_->modelPath_ = modelPath;
        modelLoaded_ = true;

        spdlog::info("Model loaded: {}", modelPath);
        return true;
    } catch (const Ort::Exception& e) {
        spdlog::error("Failed to load model: {}", e.what());
        return false;
    }
}

void ModelEngine::unloadModel() {
    impl_->session_.reset();
    modelLoaded_ = false;
}

std::vector<std::pair<std::string, TensorShape>> ModelEngine::getInputInfo() const {
    std::vector<std::pair<std::string, TensorShape>> info;

    if (!modelLoaded_) return info;

    try {
        Ort::AllocatorWithDefaultOptions allocator;

        size_t numInputs = impl_->session_->GetInputCount();
        for (size_t i = 0; i < numInputs; i++) {
            char* inputName = impl_->session_->GetInputName(i, allocator);
            auto typeInfo = impl_->session_->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();

            TensorShape shape;
            size_t numDims = tensorInfo.GetDimensionsCount();
            for (size_t j = 0; j < numDims; j++) {
                shape.push_back(tensorInfo.GetShape(j));
            }

            info.push_back({std::string(inputName), shape});
            allocator.Free(inputName);
        }
    } catch (const Ort::Exception& e) {
        spdlog::error("Failed to get input info: {}", e.what());
    }

    return info;
}

std::vector<std::pair<std::string, TensorShape>> ModelEngine::getOutputInfo() const {
    std::vector<std::pair<std::string, TensorShape>> info;

    if (!modelLoaded_) return info;

    try {
        Ort::AllocatorWithDefaultOptions allocator;

        size_t numOutputs = impl_->session_->GetOutputCount();
        for (size_t i = 0; i < numOutputs; i++) {
            char* outputName = impl_->session_->GetOutputName(i, allocator);
            auto typeInfo = impl_->session_->GetOutputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();

            TensorShape shape;
            size_t numDims = tensorInfo.GetDimensionsCount();
            for (size_t j = 0; j < numDims; j++) {
                shape.push_back(tensorInfo.GetShape(j));
            }

            info.push_back({std::string(outputName), shape});
            allocator.Free(outputName);
        }
    } catch (const Ort::Exception& e) {
        spdlog::error("Failed to get output info: {}", e.what());
    }

    return info;
}

InferenceResult ModelEngine::run(const std::map<std::string, TensorData>& inputs) {
    InferenceResult result = {false, "", {}, 0.0};

    if (!modelLoaded_) {
        result.error = "Model not loaded";
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        Ort::AllocatorWithDefaultOptions allocator;
        std::vector<Ort::Value> inputTensors;
        std::vector<const char*> inputNames;

        // 准备输入张量
        for (const auto& [name, tensorData] : inputs) {
            // 创建内存信息
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator, OrtMemTypeDefault);

            // 转换数据类型
            ONNXTensorElementDataType onnxType;
            switch (tensorData.dataType) {
                case TensorDataType::FLOAT32: onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT; break;
                case TensorDataType::FLOAT64: onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE; break;
                case TensorDataType::INT8:    onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8; break;
                case TensorDataType::INT16:   onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16; break;
                case TensorDataType::INT32:   onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32; break;
                case TensorDataType::INT64:   onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64; break;
                case TensorDataType::UINT8:   onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8; break;
                case TensorDataType::UINT16:  onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16; break;
                case TensorDataType::UINT32:  onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32; break;
                case TensorDataType::UINT64:  onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64; break;
                case TensorDataType::BOOL:    onnxType = ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL; break;
                default:
                    result.error = "Unsupported data type";
                    return result;
            }

            // 创建输入张量
            inputTensors.push_back(Ort::Value::CreateTensor(
                memoryInfo,
                const_cast<uint8_t*>(tensorData.data.data()),
                tensorData.byteSize(),
                tensorData.shape.data(),
                tensorData.shape.size(),
                onnxType
            ));

            inputNames.push_back(name.c_str());
        }

        // 获取输出名称
        std::vector<const char*> outputNames;
        size_t numOutputs = impl_->session_->GetOutputCount();
        for (size_t i = 0; i < numOutputs; i++) {
            char* outputName = impl_->session_->GetOutputName(i, allocator);
            outputNames.push_back(outputName);
        }

        // 运行推理
        auto outputs = impl_->session_->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            inputTensors.data(),
            inputNames.size(),
            outputNames.data(),
            outputNames.size()
        );

        // 处理输出
        for (size_t i = 0; i < outputs.size(); i++) {
            ModelOutput output;
            output.name = outputNames[i];

            auto tensorInfo = outputs[i].GetTensorTypeAndShapeInfo();
            output.tensor.shape.assign(
                tensorInfo.GetShape(),
                tensorInfo.GetShape() + tensorInfo.GetDimensionsCount()
            );

            // 获取数据类型
            auto type = tensorInfo.GetElementType();
            switch (type) {
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                    output.tensor.dataType = TensorDataType::FLOAT32;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
                    output.tensor.dataType = TensorDataType::INT32;
                    break;
                // ... 其他类型
                default:
                    output.tensor.dataType = TensorDataType::FLOAT32;
            }

            // 复制数据
            size_t byteSize = outputs[i].GetTensorTypeAndShapeInfo().GetElementCount() *
                             (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ? 4 : 4);
            output.tensor.data.assign(
                (uint8_t*)outputs[i].GetTensorRawData(),
                (uint8_t*)outputs[i].GetTensorRawData() + byteSize
            );

            result.outputs.push_back(output);
            allocator.Free((void*)outputNames[i]);
        }

        result.success = true;
    } catch (const Ort::Exception& e) {
        result.error = e.what();
        spdlog::error("Inference failed: {}", e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.inferenceTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

InferenceResult ModelEngine::run(const std::string& inputName, const TensorData& input) {
    return run({{inputName, input}});
}

std::vector<std::string> ModelEngine::getAvailableExecutionProviders() {
    return {"cpu", "cuda", "dml"};
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
    // 假设输入是 BGR 格式
    std::vector<float> data(width * height * 3);

    for (int i = 0; i < width * height; i++) {
        uint8_t b = imageData[i * 3];
        uint8_t g = imageData[i * 3 + 1];
        uint8_t r = imageData[i * 3 + 2];

        // 归一化并标准化 (HWC -> CHW)
        int h = i / width;
        int w = i % width;

        // BGR 通道转 RGB 并归一化
        data[0 * width * height + h * width + w] = ((r / 255.0f) - meanR) / stdR;
        data[1 * width * height + h * width + w] = ((g / 255.0f) - meanG) / stdG;
        data[2 * width * height + h * width + w] = ((b / 255.0f) - meanB) / stdB;
    }

    // 创建张量: CHW = {1, 3, height, width}
    return createFloat32({1, 3, (int64_t)height, (int64_t)width}, data);
}

// ========== ModelHelpers 实现 ==========

std::pair<std::string, float> ModelHelpers::classifyImage(
    ModelEngine& engine,
    const std::string& inputName,
    const uint8_t* imageData,
    int width, int height,
    const std::vector<std::string>& labels
) {
    auto tensor = Tensor::fromImage(imageData, width, height);
    auto result = engine.run(inputName, tensor);

    if (!result.success || result.outputs.empty()) {
        return {"", 0.0f};
    }

    // 找到最大概率的类别
    const auto& output = result.outputs[0];
    const float* data = (const float*)output.tensor.data.data();
    size_t count = output.tensor.elementCount();

    int maxIdx = 0;
    float maxProb = data[0];
    for (size_t i = 1; i < count; i++) {
        if (data[i] > maxProb) {
            maxIdx = (int)i;
            maxProb = data[i];
        }
    }

    std::string label = maxIdx < (int)labels.size() ? labels[maxIdx] : "class_" + std::to_string(maxIdx);
    return {label, maxProb};
}

} // namespace wingman
