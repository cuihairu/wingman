#include "wingman/platform/mock_input.hpp"

namespace wingman::platform::mock {

bool MockInput::initialize(const InputConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    initialized_ = true;
    return true;
}

void MockInput::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
}

void MockInput::mouseMove(int x, int y) {
    std::lock_guard<std::mutex> lock(mutex_);
    mousePosition_ = Point{x, y};
    mouseMoveCallCount_++;
}

void MockInput::mouseMoveRelative(int deltaX, int deltaY) {
    std::lock_guard<std::mutex> lock(mutex_);
    mousePosition_.x += deltaX;
    mousePosition_.y += deltaY;
}

void MockInput::mouseDown(MouseButton button) {
    std::lock_guard<std::mutex> lock(mutex_);
    mouseButtonStates_[button] = true;
    mouseDownCallCount_[button]++;
}

void MockInput::mouseUp(MouseButton button) {
    std::lock_guard<std::mutex> lock(mutex_);
    mouseButtonStates_[button] = false;
    mouseUpCallCount_[button]++;
}

void MockInput::mouseClick(MouseButton button) {
    mouseDown(button);
    mouseUp(button);
    clickCallCount_[button]++;
}

void MockInput::mouseDoubleClick(MouseButton button) {
    mouseClick(button);
    mouseClick(button);
}

void MockInput::mouseWheel(int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    scrollDelta_ += delta;
}

void MockInput::mouseWheelHorizontal(int delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    hScrollDelta_ += delta;
}

void MockInput::keyDown(KeyCode key) {
    std::lock_guard<std::mutex> lock(mutex_);
    keyStates_[key] = true;
    keyDownCallCount_[key]++;
}

void MockInput::keyUp(KeyCode key) {
    std::lock_guard<std::mutex> lock(mutex_);
    keyStates_[key] = false;
    keyUpCallCount_[key]++;
}

void MockInput::keyPress(KeyCode key) {
    keyDown(key);
    keyUp(key);
    keyPressCallCount_[key]++;
}

void MockInput::keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) {
    for (KeyCode mod : modifiers) {
        keyDown(mod);
    }
    keyPress(key);
    for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
        keyUp(*it);
    }
}

void MockInput::textInput(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputText_ += text;
}

Point MockInput::getMousePosition() {
    std::lock_guard<std::mutex> lock(mutex_);
    return mousePosition_;
}

bool MockInput::isKeyPressed(KeyCode key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keyStates_.find(key);
    return it != keyStates_.end() && it->second;
}

bool MockInput::isMousePressed(MouseButton button) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = mouseButtonStates_.find(button);
    return it != mouseButtonStates_.end() && it->second;
}

void MockInput::mouseDragBegin(MouseButton button) {
    mouseDown(button);
}

void MockInput::mouseDragEnd(MouseButton button) {
    mouseUp(button);
}

void MockInput::setInputDelay(int delayMicroseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.inputDelay = delayMicroseconds;
}

InputConfig MockInput::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool MockInput::supportsTextInput() const {
    return true;
}

bool MockInput::supportsRelativeMovement() const {
    return true;
}

std::string MockInput::getBackendName() const {
    return "Mock";
}

BackendInfo MockInput::getBackendInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return BackendInfo{
        "Mock",
        "1.0",
        initialized_,
        "Mock Input for Testing"
    };
}

bool MockInput::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

int MockInput::getMouseMoveCallCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mouseMoveCallCount_;
}

int MockInput::getClickCallCount(MouseButton button) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clickCallCount_.find(button);
    return it != clickCallCount_.end() ? it->second : 0;
}

int MockInput::getKeyPressCallCount(KeyCode key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keyPressCallCount_.find(key);
    return it != keyPressCallCount_.end() ? it->second : 0;
}

int MockInput::getScrollDelta() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scrollDelta_;
}

int MockInput::getHorizontalScrollDelta() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hScrollDelta_;
}

std::string MockInput::getInputText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inputText_;
}

} // namespace wingman::platform::mock
