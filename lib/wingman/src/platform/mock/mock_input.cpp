#include "wingman/platform/iinput.hpp"
#include <map>
#include <mutex>

namespace wingman::platform::mock {

/**
 * @brief Mock 输入实现（用于测试）
 */
class MockInput : public IInput {
public:
    MockInput() = default;
    ~MockInput() override {
        shutdown();
    }

    bool initialize(const InputConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = false;
    }

    void mouseMove(int x, int y) override {
        std::lock_guard<std::mutex> lock(mutex_);
        mousePosition_ = Point{x, y};
        mouseMoveCallCount_++;
    }

    void mouseMoveRelative(int deltaX, int deltaY) override {
        std::lock_guard<std::mutex> lock(mutex_);
        mousePosition_.x += deltaX;
        mousePosition_.y += deltaY;
    }

    void mouseDown(MouseButton button) override {
        std::lock_guard<std::mutex> lock(mutex_);
        mouseButtonStates_[button] = true;
        mouseDownCallCount_[button]++;
    }

    void mouseUp(MouseButton button) override {
        std::lock_guard<std::mutex> lock(mutex_);
        mouseButtonStates_[button] = false;
        mouseUpCallCount_[button]++;
    }

    void mouseClick(MouseButton button) override {
        mouseDown(button);
        mouseUp(button);
        clickCallCount_[button]++;
    }

    void mouseDoubleClick(MouseButton button) override {
        mouseClick(button);
        mouseClick(button);
    }

    void mouseWheel(int delta) override {
        std::lock_guard<std::mutex> lock(mutex_);
        scrollDelta_ += delta;
    }

    void mouseWheelHorizontal(int delta) override {
        std::lock_guard<std::mutex> lock(mutex_);
        hScrollDelta_ += delta;
    }

    void keyDown(KeyCode key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        keyStates_[key] = true;
        keyDownCallCount_[key]++;
    }

    void keyUp(KeyCode key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        keyStates_[key] = false;
        keyUpCallCount_[key]++;
    }

    void keyPress(KeyCode key) override {
        keyDown(key);
        keyUp(key);
        keyPressCallCount_[key]++;
    }

    void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) override {
        for (KeyCode mod : modifiers) {
            keyDown(mod);
        }
        keyPress(key);
        for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
            keyUp(*it);
        }
    }

    void textInput(const std::string& text) override {
        std::lock_guard<std::mutex> lock(mutex_);
        inputText_ += text;
    }

    Point getMousePosition() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return mousePosition_;
    }

    bool isKeyPressed(KeyCode key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keyStates_.find(key);
        return it != keyStates_.end() && it->second;
    }

    bool isMouseButtonPressed(MouseButton button) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mouseButtonStates_.find(button);
        return it != mouseButtonStates_.end() && it->second;
    }

    void mouseDragBegin(MouseButton button) override {
        mouseDown(button);
    }

    void mouseDragEnd(MouseButton button) override {
        mouseUp(button);
    }

    void setInputDelay(int delayMicroseconds) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.inputDelay = delayMicroseconds;
    }

    std::string getBackendName() const override {
        return "Mock";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Mock",
            "1.0",
            initialized_,
            "Mock Input for Testing"
        };
    }

    InputConfig getConfig() const override {
        return config_;
    }

    bool supportsTextInput() const override {
        return true;
    }

    bool supportsRelativeMovement() const override {
        return true;
    }

    // ========== Mock 特定方法（用于测试断言）==========

    int getMouseMoveCallCount() const {
        return mouseMoveCallCount_;
    }

    int getMouseDownCallCount(MouseButton button) const {
        auto it = mouseDownCallCount_.find(button);
        return it != mouseDownCallCount_.end() ? it->second : 0;
    }

    int getMouseUpCallCount(MouseButton button) const {
        auto it = mouseUpCallCount_.find(button);
        return it != mouseUpCallCount_.end() ? it->second : 0;
    }

    int getClickCallCount(MouseButton button) const {
        auto it = clickCallCount_.find(button);
        return it != clickCallCount_.end() ? it->second : 0;
    }

    int getKeyDownCallCount(KeyCode key) const {
        auto it = keyDownCallCount_.find(key);
        return it != keyDownCallCount_.end() ? it->second : 0;
    }

    int getKeyUpCallCount(KeyCode key) const {
        auto it = keyUpCallCount_.find(key);
        return it != keyUpCallCount_.end() ? it->second : 0;
    }

    int getKeyPressCallCount(KeyCode key) const {
        auto it = keyPressCallCount_.find(key);
        return it != keyPressCallCount_.end() ? it->second : 0;
    }

    std::string getInputText() const {
        return inputText_;
    }

    void clearInputText() {
        std::lock_guard<std::mutex> lock(mutex_);
        inputText_.clear();
    }

    void setMockKeyState(KeyCode key, bool pressed) {
        std::lock_guard<std::mutex> lock(mutex_);
        keyStates_[key] = pressed;
    }

    void setMockMouseButtonState(MouseButton button, bool pressed) {
        std::lock_guard<std::mutex> lock(mutex_);
        mouseButtonStates_[button] = pressed;
    }

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
