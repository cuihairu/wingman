#include <gtest/gtest.h>
#include "wingman/ml.hpp"

using namespace wingman;

class MLTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// TensorDataType Tests
// ============================================================================

TEST_F(MLTest, TensorDataTypeValues) {
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

// ============================================================================
// TensorData Tests
// ============================================================================

TEST_F(MLTest, TensorDataDefaults) {
    TensorData tensor;
    EXPECT_EQ(tensor.dataType, TensorDataType::FLOAT32);
    EXPECT_TRUE(tensor.shape.empty());
    EXPECT_TRUE(tensor.data.empty());
}

TEST_F(MLTest, TensorDataElementCount) {
    TensorData tensor;
    tensor.shape = {2, 3, 4}; // 24 elements
    EXPECT_EQ(tensor.elementCount(), 24);
}

TEST_F(MLTest, TensorDataByteSize) {
    TensorData tensor;
    tensor.shape = {2, 2}; // 4 elements
    tensor.dataType = TensorDataType::FLOAT32; // 4 bytes per element
    EXPECT_EQ(tensor.byteSize(), 16);
}

TEST_F(MLTest, TensorDataWithValues) {
    TensorData tensor;
    tensor.dataType = TensorDataType::UINT8;
    tensor.shape = {10, 20};
    tensor.data.resize(200);

    EXPECT_EQ(tensor.shape.size(), 2);
    EXPECT_EQ(tensor.shape[0], 10);
    EXPECT_EQ(tensor.shape[1], 20);
    EXPECT_EQ(tensor.data.size(), 200);
}

// ============================================================================
// ModelOutput Tests
// ============================================================================

TEST_F(MLTest, ModelOutputDefaults) {
    ModelOutput output;
    EXPECT_TRUE(output.name.empty());
    EXPECT_EQ(output.tensor.dataType, TensorDataType::FLOAT32);
}

TEST_F(MLTest, ModelOutputWithValues) {
    ModelOutput output;
    output.name = "output1";
    output.tensor.shape = {1, 10};

    EXPECT_EQ(output.name, "output1");
    EXPECT_EQ(output.tensor.shape.size(), 2);
}

// ============================================================================
// InferenceResult Tests
// ============================================================================

TEST_F(MLTest, InferenceResultDefaults) {
    InferenceResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.outputs.empty());
    EXPECT_EQ(result.inferenceTimeMs, 0.0);
}

TEST_F(MLTest, InferenceResultWithValues) {
    InferenceResult result;
    result.success = true;
    result.inferenceTimeMs = 50.5;

    ModelOutput output;
    output.name = "output1";
    result.outputs.push_back(output);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.inferenceTimeMs, 50.5);
    EXPECT_EQ(result.outputs.size(), 1);
    EXPECT_EQ(result.outputs[0].name, "output1");
}

// ============================================================================
// ModelEngine Tests
// ============================================================================

TEST_F(MLTest, ModelEngineConstruction) {
    ModelEngine engine;
    EXPECT_NO_THROW();
    EXPECT_FALSE(engine.isModelLoaded());
}

TEST_F(MLTest, ModelEngineLoadNonExistentModel) {
    ModelEngine engine;
    bool result = engine.loadModel("nonexistent_model.onnx");
    // Should return false for non-existent model
    EXPECT_FALSE(result);
}

TEST_F(MLTest, ModelEngineUnloadModel) {
    ModelEngine engine;
    EXPECT_NO_THROW(engine.unloadModel());
}

TEST_F(MLTest, ModelEngineGetInputInfo) {
    ModelEngine engine;
    auto info = engine.getInputInfo();
    EXPECT_TRUE(info.empty());
}

TEST_F(MLTest, ModelEngineGetOutputInfo) {
    ModelEngine engine;
    auto info = engine.getOutputInfo();
    EXPECT_TRUE(info.empty());
}

TEST_F(MLTest, ModelEngineRunWithoutModel) {
    ModelEngine engine;
    std::map<std::string, TensorData> inputs;
    auto result = engine.run(inputs);
    EXPECT_FALSE(result.success);
}

TEST_F(MLTest, ModelEngineRunSingleInput) {
    ModelEngine engine;
    TensorData input;
    input.dataType = TensorDataType::FLOAT32;
    input.shape = {1, 3, 224, 224};

    auto result = engine.run("input", input);
    EXPECT_FALSE(result.success); // No model loaded
}
