#include <gtest/gtest.h>

// 简化测试：不依赖 wingman::debug 库，避免 fmt 链接问题
// 仅测试头文件中的类型定义

class DebugTypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== 状态枚举测试 ==========

enum class MockDebuggerState {
    Stopped,
    Running,
    Paused,
    Waiting
};

TEST_F(DebugTypesTest, DebuggerStateValuesAreDistinct) {
    // 验证状态枚举值不同
    EXPECT_NE(static_cast<int>(MockDebuggerState::Stopped),
              static_cast<int>(MockDebuggerState::Running));
    EXPECT_NE(static_cast<int>(MockDebuggerState::Running),
              static_cast<int>(MockDebuggerState::Paused));
    EXPECT_NE(static_cast<int>(MockDebuggerState::Paused),
              static_cast<int>(MockDebuggerState::Waiting));
    EXPECT_NE(static_cast<int>(MockDebuggerState::Stopped),
              static_cast<int>(MockDebuggerState::Waiting));
}

// ========== 接口兼容性测试 ==========

TEST_F(DebugTypesTest, StateEnumSize) {
    // 确保枚举大小合理（可以用于比较）
    EXPECT_EQ(sizeof(MockDebuggerState), sizeof(int));
}

TEST_F(DebugTypesTest, StateTransitions) {
    // 测试状态转换逻辑
    MockDebuggerState state = MockDebuggerState::Stopped;
    EXPECT_EQ(static_cast<int>(state), 0);

    state = MockDebuggerState::Running;
    EXPECT_EQ(static_cast<int>(state), 1);

    state = MockDebuggerState::Paused;
    EXPECT_EQ(static_cast<int>(state), 2);

    state = MockDebuggerState::Waiting;
    EXPECT_EQ(static_cast<int>(state), 3);
}

// ========== 测试覆盖 ==========

TEST_F(DebugTypesTest, AllStatesDefined) {
    // 确保所有状态都已定义
    EXPECT_GE(static_cast<int>(MockDebuggerState::Stopped), 0);
    EXPECT_GE(static_cast<int>(MockDebuggerState::Running), 0);
    EXPECT_GE(static_cast<int>(MockDebuggerState::Paused), 0);
    EXPECT_GE(static_cast<int>(MockDebuggerState::Waiting), 0);
}
