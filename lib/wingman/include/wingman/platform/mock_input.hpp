#pragma once

#include "wingman/platform/iinput.hpp"
#include <map>
#include <mutex>

namespace wingman::platform::mock {

class MockInput : public IInput {
public:
    MockInput() = default;
    ~MockInput() override {
        shutdown();
    }

    bool initialize(const InputConfig& config) override;
    void shutdown() override;

    void mouseMove(int x, int y) override;
    void mouseMoveRelative(int deltaX, int deltaY) override;
    void mouseDown(MouseButton button) override;
    void mouseUp(MouseButton button) override;
    void mouseClick(MouseButton button) override;
    void mouseDoubleClick(MouseButton button) override;
    void mouseWheel(int delta) override;
    void mouseWheelHorizontal(int delta) override;

    void keyDown(KeyCode key) override;
    void keyUp(KeyCode key) override;
    void keyPress(KeyCode key) override;
    void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) override;
    void textInput(const std::string& text) override;

    Point getMousePosition() override;
    bool isKeyPressed(KeyCode key) override;
    bool isMousePressed(MouseButton button) override;

    void mouseDragBegin(MouseButton button) override;
    void mouseDragEnd(MouseButton button) override;
    void setInputDelay(int delayMicroseconds) override;
    InputConfig getConfig() const override;
    bool supportsTextInput() const override;
    bool supportsRelativeMovement() const override;

    std::string getBackendName() const override;
    BackendInfo getBackendInfo() const override;

    bool isInitialized() const;
    int getMouseMoveCallCount() const;
    int getClickCallCount(MouseButton button) const;
    int getKeyPressCallCount(KeyCode key) const;
    int getScrollDelta() const;
    int getHorizontalScrollDelta() const;
    std::string getInputText() const;

private:
    InputConfig config_;
    bool initialized_ = false;
    Point mousePosition_{0, 0};
    int mouseMoveCallCount_ = 0;
    std::map<MouseButton, bool> mouseButtonStates_;
    std::map<MouseButton, int> mouseDownCallCount_;
    std::map<MouseButton, int> mouseUpCallCount_;
    std::map<MouseButton, int> clickCallCount_;
    int scrollDelta_ = 0;
    int hScrollDelta_ = 0;
    std::map<KeyCode, bool> keyStates_;
    std::map<KeyCode, int> keyDownCallCount_;
    std::map<KeyCode, int> keyUpCallCount_;
    std::map<KeyCode, int> keyPressCallCount_;
    std::string inputText_;
    mutable std::mutex mutex_;
};

} // namespace wingman::platform::mock
