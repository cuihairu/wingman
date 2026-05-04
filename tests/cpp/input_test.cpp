#include <gtest/gtest.h>
#include "wingman/input.hpp"

using namespace wingman;

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Input Tests
// ============================================================================

TEST_F(InputTest, DelayFunction) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(100);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(duration.count(), 90); // Allow some margin
    EXPECT_LT(duration.count(), 200); // But not too long
}

TEST_F(InputTest, RandomDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::randomDelay(50, 100);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(duration.count(), 40);
    EXPECT_LT(duration.count(), 150);
}
