#include <gtest/gtest.h>
#include "wingman/ml.hpp"
#include "wingman/screen.hpp"
#include <cstring>

using namespace wingman;

// ========== TensorDataType Enum ==========

TEST(TensorDataTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(TensorDataType::FLOAT32), 0);
    EXPECT_EQ(static_cast<int>(TensorDataType::FLOAT64), 1);
    EXPECT_EQ(static_cast<int>(TensorDataType::INT8), 2);
    EXPECT_EQ(static_cast<int>(TensorDataType::INT16), 3);
    EXPECT_EQ(static_cast<int>(TensorDataType::INT32), 4);
    EXPECT_EQ(static_cast<int>(TensorDataType::INT64), 5);
    EXPECT_EQ(static_cast<int>(TensorDataType::UINT8), 6);
    EXPECT_EQ(static_cast<int>(TensorDataType::UINT16), 7);
    EXPECT_EQ(static_cast<int>(TensorDataType::UINT32), 8);
    EXPECT_EQ(static_cast<int>(TensorDataType::UINT64), 9);
    EXPECT_EQ(static_cast<int>(TensorDataType::BOOL), 10);
}

// ========== TensorData ==========

TEST(TensorDataTest, ElementCount1D) {
    TensorData td;
    td.shape = {10};
    td.dataType = TensorDataType::FLOAT32;
    EXPECT_EQ(td.elementCount(), 10u);
}

TEST(TensorDataTest, ElementCount2D) {
    TensorData td;
    td.shape = {3, 4};
    td.dataType = TensorDataType::FLOAT32;
    EXPECT_EQ(td.elementCount(), 12u);
}

TEST(TensorDataTest, ElementCount4D) {
    TensorData td;
    td.shape = {1, 3, 224, 224};
    td.dataType = TensorDataType::FLOAT32;
    EXPECT_EQ(td.elementCount(), 1u * 3 * 224 * 224);
}

TEST(TensorDataTest, ByteSizeFloat32) {
    TensorData td;
    td.shape = {4};
    td.dataType = TensorDataType::FLOAT32;
    EXPECT_EQ(td.byteSize(), 16u);  // 4 * 4 bytes
}

TEST(TensorDataTest, ByteSizeFloat64) {
    TensorData td;
    td.shape = {2};
    td.dataType = TensorDataType::FLOAT64;
    EXPECT_EQ(td.byteSize(), 16u);  // 2 * 8 bytes
}

TEST(TensorDataTest, ByteSizeInt32) {
    TensorData td;
    td.shape = {3};
    td.dataType = TensorDataType::INT32;
    EXPECT_EQ(td.byteSize(), 12u);  // 3 * 4 bytes
}

TEST(TensorDataTest, ByteSizeInt64) {
    TensorData td;
    td.shape = {2};
    td.dataType = TensorDataType::INT64;
    EXPECT_EQ(td.byteSize(), 16u);  // 2 * 8 bytes
}

TEST(TensorDataTest, ByteSizeUint8) {
    TensorData td;
    td.shape = {10};
    td.dataType = TensorDataType::UINT8;
    EXPECT_EQ(td.byteSize(), 10u);  // 10 * 1 byte
}

// ========== Tensor Helper Functions ==========

TEST(TensorTest, CreateFloat32) {
    auto tensor = Tensor::createFloat32({2, 3}, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});
    EXPECT_EQ(tensor.dataType, TensorDataType::FLOAT32);
    EXPECT_EQ(tensor.shape.size(), 2u);
    EXPECT_EQ(tensor.shape[0], 2);
    EXPECT_EQ(tensor.shape[1], 3);
    EXPECT_EQ(tensor.data.size(), 6u * sizeof(float));

    const float* data = reinterpret_cast<const float*>(tensor.data.data());
    EXPECT_FLOAT_EQ(data[0], 1.0f);
    EXPECT_FLOAT_EQ(data[5], 6.0f);
}

TEST(TensorTest, CreateInt32) {
    auto tensor = Tensor::createInt32({3}, {10, 20, 30});
    EXPECT_EQ(tensor.dataType, TensorDataType::INT32);
    EXPECT_EQ(tensor.shape.size(), 1u);
    EXPECT_EQ(tensor.shape[0], 3);
    EXPECT_EQ(tensor.data.size(), 3u * sizeof(int32_t));

    const int32_t* data = reinterpret_cast<const int32_t*>(tensor.data.data());
    EXPECT_EQ(data[0], 10);
    EXPECT_EQ(data[2], 30);
}

TEST(TensorTest, FromImage) {
    // 2x2 RGB image
    uint8_t imageData[] = {
        100, 150, 200,   // pixel 0: B=100, G=150, R=200
        50,  75,  25,    // pixel 1: B=50, G=75, R=25
        200, 100, 50,    // pixel 2: B=200, G=100, R=50
        128, 128, 128    // pixel 3: B=128, G=128, R=128
    };

    auto tensor = Tensor::fromImage(imageData, 2, 2);
    EXPECT_EQ(tensor.dataType, TensorDataType::FLOAT32);
    EXPECT_EQ(tensor.shape.size(), 4u);
    EXPECT_EQ(tensor.shape[0], 1);
    EXPECT_EQ(tensor.shape[1], 3);  // CHW channels
    EXPECT_EQ(tensor.shape[2], 2);  // height
    EXPECT_EQ(tensor.shape[3], 2);  // width
}

// ========== InferenceResult ==========

TEST(InferenceResultTest, DefaultConstruction) {
    InferenceResult result{};
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.outputs.empty());
    EXPECT_DOUBLE_EQ(result.inferenceTimeMs, 0.0);
}

// ========== ModelEngine (Stub) ==========

TEST(ModelEngineTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(ModelEngine engine);
}

TEST(ModelEngineTest, InitialState) {
    ModelEngine engine;
    EXPECT_FALSE(engine.isModelLoaded());
}

TEST(ModelEngineTest, LoadModelReturnsFalseStub) {
    ModelEngine engine;
    EXPECT_FALSE(engine.loadModel("nonexistent.onnx"));
    EXPECT_FALSE(engine.isModelLoaded());
}

TEST(ModelEngineTest, UnloadModelDoesNotCrash) {
    ModelEngine engine;
    EXPECT_NO_THROW(engine.unloadModel());
}

TEST(ModelEngineTest, GetInputInfoEmpty) {
    ModelEngine engine;
    EXPECT_TRUE(engine.getInputInfo().empty());
}

TEST(ModelEngineTest, GetOutputInfoEmpty) {
    ModelEngine engine;
    EXPECT_TRUE(engine.getOutputInfo().empty());
}

TEST(ModelEngineTest, RunReturnsFailureStub) {
    ModelEngine engine;
    TensorData input = Tensor::createFloat32({1, 3}, {1.0f, 2.0f, 3.0f});

    auto result = engine.run("input", input);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(ModelEngineTest, RunMapReturnsFailureStub) {
    ModelEngine engine;
    TensorData input = Tensor::createFloat32({1, 3}, {1.0f, 2.0f, 3.0f});

    auto result = engine.run({{"input", input}});
    EXPECT_FALSE(result.success);
}

TEST(ModelEngineTest, GetAvailableExecutionProviders) {
    auto providers = ModelEngine::getAvailableExecutionProviders();
    EXPECT_FALSE(providers.empty());
    EXPECT_EQ(providers[0], "cpu");
}

// ========== ModelHelpers (Stub) ==========

TEST(ModelHelpersTest, ClassifyImageReturnsEmptyStub) {
    ModelEngine engine;
    uint8_t img[] = {0, 0, 0};
    auto [label, confidence] = ModelHelpers::classifyImage(engine, "input", img, 1, 1);
    EXPECT_TRUE(label.empty());
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST(ModelHelpersTest, DetectObjectsReturnsEmptyStub) {
    ModelEngine engine;
    uint8_t img[] = {0, 0, 0};
    auto detections = ModelHelpers::detectObjects(engine, "input", img, 1, 1);
    EXPECT_TRUE(detections.empty());
}

// ========== TensorData Edge Cases ==========

TEST(TensorDataTest, EmptyShapeElementCount) {
    // empty shape => product of no dimensions = 1
    TensorData td;
    td.shape = {};
    td.dataType = TensorDataType::FLOAT32;
    EXPECT_EQ(td.elementCount(), 1u);
}

TEST(TensorDataTest, ByteSizeInt8) {
    TensorData td;
    td.shape = {8};
    td.dataType = TensorDataType::INT8;
    EXPECT_EQ(td.byteSize(), 8u);  // 8 * 1 byte
}

TEST(TensorDataTest, ByteSizeBool) {
    TensorData td;
    td.shape = {5};
    td.dataType = TensorDataType::BOOL;
    EXPECT_EQ(td.byteSize(), 5u);  // 5 * 1 byte
}

TEST(TensorDataTest, ByteSizeInt16) {
    TensorData td;
    td.shape = {4};
    td.dataType = TensorDataType::INT16;
    EXPECT_EQ(td.byteSize(), 8u);  // 4 * 2 bytes
}

TEST(TensorDataTest, ByteSizeUint16) {
    TensorData td;
    td.shape = {3};
    td.dataType = TensorDataType::UINT16;
    EXPECT_EQ(td.byteSize(), 6u);  // 3 * 2 bytes
}

TEST(TensorDataTest, ByteSizeUint32) {
    TensorData td;
    td.shape = {5};
    td.dataType = TensorDataType::UINT32;
    EXPECT_EQ(td.byteSize(), 20u);  // 5 * 4 bytes
}

TEST(TensorDataTest, ByteSizeUint64) {
    TensorData td;
    td.shape = {3};
    td.dataType = TensorDataType::UINT64;
    EXPECT_EQ(td.byteSize(), 24u);  // 3 * 8 bytes
}

TEST(TensorDataTest, ByteSizeDefaultCase) {
    // Test with an out-of-range value to hit the default branch
    TensorData td;
    td.shape = {4};
    td.dataType = static_cast<TensorDataType>(99);
    EXPECT_EQ(td.byteSize(), 16u);  // 4 * 4 (default elemSize)
}

// ========== Tensor Edge Cases ==========

TEST(TensorTest, CreateFloat32EmptyData) {
    auto tensor = Tensor::createFloat32({0}, {});
    EXPECT_EQ(tensor.dataType, TensorDataType::FLOAT32);
    EXPECT_EQ(tensor.shape.size(), 1u);
    EXPECT_EQ(tensor.shape[0], 0);
    EXPECT_TRUE(tensor.data.empty());
}

TEST(TensorTest, CreateInt32EmptyData) {
    auto tensor = Tensor::createInt32({0}, {});
    EXPECT_EQ(tensor.dataType, TensorDataType::INT32);
    EXPECT_EQ(tensor.shape.size(), 1u);
    EXPECT_EQ(tensor.shape[0], 0);
    EXPECT_TRUE(tensor.data.empty());
}

TEST(TensorTest, FromImage1x1) {
    // 1x1 RGB image: single pixel R=255, G=128, B=0
    uint8_t imageData[] = {0, 128, 255};  // BGR in memory
    auto tensor = Tensor::fromImage(imageData, 1, 1);
    EXPECT_EQ(tensor.dataType, TensorDataType::FLOAT32);
    EXPECT_EQ(tensor.shape.size(), 4u);
    EXPECT_EQ(tensor.shape[0], 1);
    EXPECT_EQ(tensor.shape[1], 3);
    EXPECT_EQ(tensor.shape[2], 1);
    EXPECT_EQ(tensor.shape[3], 1);
    EXPECT_EQ(tensor.data.size(), 3u * sizeof(float));
}

// ========== InferenceResult Extended ==========

TEST(InferenceResultTest, SuccessWithOutputs) {
    InferenceResult result;
    result.success = true;
    result.inferenceTimeMs = 12.5;

    ModelOutput output;
    output.name = "probabilities";
    output.tensor = Tensor::createFloat32({1, 3}, {0.1f, 0.7f, 0.2f});
    result.outputs.push_back(output);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.outputs.size(), 1u);
    EXPECT_EQ(result.outputs[0].name, "probabilities");
    EXPECT_EQ(result.outputs[0].tensor.elementCount(), 3u);
    EXPECT_DOUBLE_EQ(result.inferenceTimeMs, 12.5);
}

// ========== ModelHelpers::segment (Stub) ==========

TEST(ModelHelpersTest, SegmentDoesNotCrashStub) {
    ModelEngine engine;
    uint8_t img[] = {128, 128, 128};
    Bitmap bm(0, 0);
    EXPECT_NO_THROW(bm = ModelHelpers::segment(engine, "input", img, 1, 1));
}

// ========== Execution Provider Extended ==========

TEST(ModelEngineTest, MultipleExecutionProvidersReturned) {
    auto providers = ModelEngine::getAvailableExecutionProviders();
    // Stub always returns at least {"cpu"}
    EXPECT_GE(providers.size(), 1u);
    // Verify "cpu" is present
    bool hasCpu = false;
    for (const auto& p : providers) {
        if (p == "cpu") hasCpu = true;
    }
    EXPECT_TRUE(hasCpu);
}
