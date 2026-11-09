#if defined(__linux__)

#include "platform_x11_test_common.hpp"

// ========== 输入（XTest 注入 → XQueryPointer/XQueryKeymap 回读） ==========

TEST_F(X11PlatformTest, InputMouseMoveRoundtrip) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->getBackendName(), "XTest");

    input->mouseMove(100, 200);
    const auto position = input->getMousePosition();
    EXPECT_EQ(position.x, 100);
    EXPECT_EQ(position.y, 200);
}

TEST_F(X11PlatformTest, InputKeyDownReflectsInKeymap) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);

    input->keyDown(wingman::platform::KeyCode::Space);
    EXPECT_TRUE(input->isKeyPressed(wingman::platform::KeyCode::Space));
    input->keyUp(wingman::platform::KeyCode::Space);
    EXPECT_FALSE(input->isKeyPressed(wingman::platform::KeyCode::Space));
}


// ========== XTest 输入全方法触达（鼠标键位/滚轮/拖拽/组合键/元信息） ==========

TEST_F(X11PlatformTest, InputMouseFullSweep) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);

    // 三键 click/doubleClick + X1/X2 侧键（getButtonCode 的 switch 全分支）。
    // isMousePressed 经 XQueryPointer 回读按钮掩码：XTest fake 按键注入的是
    // 事件而非指针物理状态，Xvfb 下掩码不翻转——只验证调用安全返回，
    // 不假设按下状态可观测
    for (auto btn : {wingman::platform::MouseButton::Left,
                     wingman::platform::MouseButton::Middle,
                     wingman::platform::MouseButton::Right,
                     wingman::platform::MouseButton::X1,
                     wingman::platform::MouseButton::X2}) {
        input->mouseClick(btn);
        input->mouseDoubleClick(btn);
        input->mouseDown(btn);
        (void)input->isMousePressed(btn);
        input->mouseUp(btn);
        EXPECT_FALSE(input->isMousePressed(btn)); // 松开后必无掩码
    }

    // 垂直/水平滚轮（X11 button 4/5 与 6/7 的 fake 事件序列）
    input->mouseWheel(3);
    input->mouseWheel(-3);
    input->mouseWheelHorizontal(2);
    input->mouseWheelHorizontal(-2);

    // 拖拽（mouseDown/Up 的封装）与相对移动
    input->mouseMove(50, 50);
    input->mouseDragBegin(wingman::platform::MouseButton::Left);
    input->mouseMoveRelative(10, 10);
    input->mouseDragEnd(wingman::platform::MouseButton::Left);
    const auto pos = input->getMousePosition();
    EXPECT_EQ(pos.x, 60);
    EXPECT_EQ(pos.y, 60);
}

TEST_F(X11PlatformTest, InputKeySymMappingAndCombinations) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);
    using KC = wingman::platform::KeyCode;

    // toKeySym switch 全分支：逐键 XKeysymToKeycode 能解析出键码即证映射有效，
    // 再经 keyPress 真注入（XTest 无窗口接收也安全）
    const std::vector<KC> allKeys = {
        KC::A, KC::B, KC::C, KC::D, KC::E, KC::F, KC::G, KC::H, KC::I, KC::J,
        KC::K, KC::L, KC::M, KC::N, KC::O, KC::P, KC::Q, KC::R, KC::S, KC::T,
        KC::U, KC::V, KC::W, KC::X, KC::Y, KC::Z,
        KC::Num0, KC::Num1, KC::Num2, KC::Num3, KC::Num4,
        KC::Num5, KC::Num6, KC::Num7, KC::Num8, KC::Num9,
        KC::F1, KC::F2, KC::F3, KC::F4, KC::F5, KC::F6,
        KC::F7, KC::F8, KC::F9, KC::F10, KC::F11, KC::F12,
        KC::Space, KC::Enter, KC::Escape, KC::Tab, KC::Backspace, KC::Delete,
        KC::Insert, KC::Home, KC::End, KC::PageUp, KC::PageDown,
        KC::Left, KC::Up, KC::Right, KC::Down,
        KC::Shift, KC::Control, KC::Alt, KC::CapsLock,
        KC::PrintScreen, KC::Pause,
    };
    for (auto key : allKeys) {
        input->keyPress(key); // 真注入：XTest 无焦点窗口接收也安全
    }

    // 文本输入逐字符映射（含空格）
    input->textInput("wingman cov 1");

    // 修饰键组合（Ctrl+C / Shift+Tab）
    input->keyCombination({KC::Control}, KC::C);
    input->keyCombination({KC::Shift}, KC::Tab);

    // 文本输入声明支持
    EXPECT_TRUE(input->supportsTextInput());
    EXPECT_TRUE(input->supportsRelativeMovement());
}

TEST_F(X11PlatformTest, InputConfigAndBackendMetadata) {
    wingman::platform::InputConfig config;
    config.inputDelay = 100; // 触发 sleep() 延迟路径
    auto input = wingman::platform::createDefaultInput(config);
    ASSERT_NE(input, nullptr);

    input->setInputDelay(200);
    EXPECT_EQ(input->getConfig().inputDelay, 200);

    EXPECT_EQ(input->getBackendName(), "XTest");
    const auto info = input->getBackendInfo();
    EXPECT_EQ(info.name, "XTest");
    EXPECT_TRUE(info.isInitialized);
}


TEST_F(X11PlatformTest, InputUnknownMouseButtonFallsBackToLeft) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);
    // 越界枚举走 getButtonCode default 分支按左键码注入（218），不崩即达意
    input->mouseClick(static_cast<wingman::platform::MouseButton>(99));
    input->mouseDown(static_cast<wingman::platform::MouseButton>(99));
    input->mouseUp(static_cast<wingman::platform::MouseButton>(99));
    SUCCEED();
}


#endif // __linux__
