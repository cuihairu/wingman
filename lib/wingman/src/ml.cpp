#include "wingman/ml.hpp"
#include "wingman/screen.hpp"
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>

// ONNX Runtime：vcpkg 端口按上游布局装进 include/onnxruntime/ 子目录。
// MSVC 自动链接指令收敛在 platform/win/msvc_link_pragmas.cpp（薄层纪律）
#include <onnxruntime/onnxruntime_cxx_api.h>

namespace wingman {

// ========== TensorData Implementation ==========

size_t TensorData::elementCount() const {
    size_t count = 1;
    for (auto dim : shape) {
        count *= static_cast<size_t>(dim);
    }
    return count;
}

size_t TensorData::byteSize() const {
    // 缺省按 float32（与替身档 ml_stub.cpp 契约一致；ML 档首跑测试揭出
    // 两份实现漂移——旧版未知类型回 0）
    size_t elemSize = 4;
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
        default: break;
    }
    return elementCount() * elemSize;
}

// ========== ModelEngine Private Implementation ==========

class ModelEngine::Impl {
public:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    std::string modelPath_;

    Impl() : env_(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "Wingman")),
              sessionOptions_(std::make_unique<Ort::SessionOptions>()) {}
};

// ========== ModelEngine Implementation ==========

ModelEngine::ModelEngine() : impl_(std::make_unique<Impl>()), modelLoaded_(false) {}

ModelEngine::~ModelEngine() {
    unloadModel();
}

bool ModelEngine::loadModel(const std::string& modelPath, const std::string& executionProvider) {
    try {
        // Set execution providers
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

        // Create session
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
            // ORT 1.14+ 分配式命名接口（裸 GetInputName 已移除）；
            // GetShape() 一次取回全部维（符号维 -1，与旧 per-dim API 同语义）
            auto inputName = impl_->session_->GetInputNameAllocated(i, allocator);
            auto typeInfo = impl_->session_->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            TensorShape shape = tensorInfo.GetShape();
            info.push_back({std::string(inputName.get()), shape});
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
            auto outputName = impl_->session_->GetOutputNameAllocated(i, allocator);
            auto typeInfo = impl_->session_->GetOutputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            TensorShape shape = tensorInfo.GetShape();
            info.push_back({std::string(outputName.get()), shape});
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

        // Prepare input tensors
        for (const auto& [name, tensorData] : inputs) {
            // Create memory info
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator, OrtMemTypeDefault);

            // Convert data type
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

            // Create input tensor
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

        // Get output names：AllocatedStringPtr 持有分配内存至 run 结束
        //（裸指针版 GetOutputName 已在 ORT 1.14 移除，且免手动 Free 悬垂）
        std::vector<Ort::AllocatedStringPtr> outputNamePtrs;
        std::vector<const char*> outputNames;
        size_t numOutputs = impl_->session_->GetOutputCount();
        for (size_t i = 0; i < numOutputs; i++) {
            outputNamePtrs.push_back(
                impl_->session_->GetOutputNameAllocated(i, allocator));
        }
        for (const auto& name : outputNamePtrs) {
            outputNames.push_back(name.get());
        }

        // Run inference
        auto outputs = impl_->session_->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            inputTensors.data(),
            inputNames.size(),
            outputNames.data(),
            outputNames.size()
        );

        // Process output
        for (size_t i = 0; i < outputs.size(); i++) {
            ModelOutput output;
            output.name = outputNames[i];

            auto tensorInfo = outputs[i].GetTensorTypeAndShapeInfo();
            output.tensor.shape = tensorInfo.GetShape(); // vector<int64_t> 直赋

            // 数据类型与元素宽度成对映射（旧实现拷贝宽写死 4 字节，非
            // 4 字节类型会截断/越界；未知类型按 FLOAT32 兜底不越界）
            auto type = tensorInfo.GetElementType();
            size_t elemSize = 4;
            switch (type) {
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
                    output.tensor.dataType = TensorDataType::FLOAT32;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
                    output.tensor.dataType = TensorDataType::FLOAT64;
                    elemSize = 8;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
                    output.tensor.dataType = TensorDataType::INT8;
                    elemSize = 1;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
                    output.tensor.dataType = TensorDataType::INT16;
                    elemSize = 2;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
                    output.tensor.dataType = TensorDataType::INT32;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
                    output.tensor.dataType = TensorDataType::INT64;
                    elemSize = 8;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
                    output.tensor.dataType = TensorDataType::UINT8;
                    elemSize = 1;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
                    output.tensor.dataType = TensorDataType::UINT16;
                    elemSize = 2;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
                    output.tensor.dataType = TensorDataType::UINT32;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
                    output.tensor.dataType = TensorDataType::UINT64;
                    elemSize = 8;
                    break;
                case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
                    output.tensor.dataType = TensorDataType::BOOL;
                    elemSize = 1;
                    break;
                default:
                    output.tensor.dataType = TensorDataType::FLOAT32;
            }

            // Copy data
            const size_t byteSize = tensorInfo.GetElementCount() * elemSize;
            const auto* raw = static_cast<const uint8_t*>(outputs[i].GetTensorRawData());
            output.tensor.data.assign(raw, raw + byteSize);

            result.outputs.push_back(output);
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

// ========== Tensor Helper Functions ==========

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
    // Assume input is in BGR format
    std::vector<float> data(width * height * 3);

    for (int i = 0; i < width * height; i++) {
        uint8_t b = imageData[i * 3];
        uint8_t g = imageData[i * 3 + 1];
        uint8_t r = imageData[i * 3 + 2];

        // Normalize and standardize (HWC -> CHW)
        int h = i / width;
        int w = i % width;

        // Convert BGR channels to RGB and normalize
        data[0 * width * height + h * width + w] = ((r / 255.0f) - meanR) / stdR;
        data[1 * width * height + h * width + w] = ((g / 255.0f) - meanG) / stdG;
        data[2 * width * height + h * width + w] = ((b / 255.0f) - meanB) / stdB;
    }

    // Create tensor: CHW = {1, 3, height, width}
    return createFloat32({1, 3, (int64_t)height, (int64_t)width}, data);
}

// ========== ModelHelpers Implementation ==========

namespace {

// BGR packed 3 通道双线性 resize（目标尺寸同通道布局）
void resizeBilinearBgr(const uint8_t* src, int srcW, int srcH,
                       uint8_t* dst, int dstW, int dstH) {
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    const float sx = static_cast<float>(srcW) / dstW;
    const float sy = static_cast<float>(srcH) / dstH;
    for (int y = 0; y < dstH; ++y) {
        const float fy = std::max(0.0f, (y + 0.5f) * sy - 0.5f);
        const int y0 = std::min(srcH - 1, static_cast<int>(fy));
        const int y1 = std::min(srcH - 1, y0 + 1);
        const float ly = fy - y0;
        for (int x = 0; x < dstW; ++x) {
            const float fx = std::max(0.0f, (x + 0.5f) * sx - 0.5f);
            const int x0 = std::min(srcW - 1, static_cast<int>(fx));
            const int x1 = std::min(srcW - 1, x0 + 1);
            const float lx = fx - x0;
            for (int c = 0; c < 3; ++c) {
                const float p00 = src[(y0 * srcW + x0) * 3 + c];
                const float p01 = src[(y0 * srcW + x1) * 3 + c];
                const float p10 = src[(y1 * srcW + x0) * 3 + c];
                const float p11 = src[(y1 * srcW + x1) * 3 + c];
                const float top = p00 + (p01 - p00) * lx;
                const float bottom = p10 + (p11 - p10) * lx;
                const float v = top + (bottom - top) * ly;
                dst[(y * dstW + x) * 3 + c] =
                    static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, v + 0.5f)));
            }
        }
    }
}

} // namespace

// decodeYoloOutput/nmsBoxes（检测后处理纯函数）实现在 ml_postprocess.cpp——
// 零 onnxruntime 依赖，与替身档 ml_stub.cpp 共编同一份，两档可测。

std::vector<ModelHelpers::Detection> ModelHelpers::detectObjects(
    ModelEngine& engine,
    const std::string& inputName,
    const uint8_t* imageData,
    int width, int height,
    float confThreshold,
    float nmsThreshold
) {
    if (!engine.isModelLoaded()) {
        spdlog::warn("detectObjects: model not loaded");
        return {};
    }
    const auto inputs = engine.getInputInfo();
    if (inputs.empty() || inputs[0].second.size() < 4 || width <= 0 || height <= 0) {
        return {};
    }
    // NCHW 输入尺寸；YOLO 惯例 0–1 归一（mean=0, std=1）
    const int inH = static_cast<int>(inputs[0].second[2]);
    const int inW = static_cast<int>(inputs[0].second[3]);
    if (inH <= 0 || inW <= 0) {
        return {};
    }
    std::vector<uint8_t> resized(static_cast<size_t>(inW) * inH * 3);
    resizeBilinearBgr(imageData, width, height, resized.data(), inW, inH);
    const TensorData tensor = Tensor::fromImage(
        resized.data(), inW, inH, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    InferenceResult result = engine.run(inputName, tensor);
    if (!result.success || result.outputs.empty()) {
        spdlog::warn("detectObjects: inference failed: {}", result.error);
        return {};
    }
    // 解码输出为模型输入空间像素框，再按比例还原原图像素（简单 resize
    // 无 letterbox，坐标按同比例对应）
    std::vector<Detection> detections =
        decodeYoloOutput(result.outputs[0].tensor, confThreshold);
    const float scaleX = static_cast<float>(width) / inW;
    const float scaleY = static_cast<float>(height) / inH;
    for (auto& det : detections) {
        det.x *= scaleX;
        det.y *= scaleY;
        det.width *= scaleX;
        det.height *= scaleY;
    }
    return nmsBoxes(std::move(detections), nmsThreshold);
}

Bitmap ModelHelpers::segment(
    ModelEngine& engine,
    const std::string& inputName,
    const uint8_t* imageData,
    int width, int height
) {
    // 分割 decode（mask proto）未实现：诚实报错返回空位图，不做静默降级。
    // 检测/分类（detectObjects/classifyImage）是本模块当前支持面。
    (void)engine; (void)inputName; (void)imageData; (void)width; (void)height;
    spdlog::error("ModelHelpers::segment is not implemented yet "
                  "(detection/classification are the supported surface)");
    return Bitmap(0, 0);
}

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

    // Find class with highest probability
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
