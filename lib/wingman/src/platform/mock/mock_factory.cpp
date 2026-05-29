#include "wingman/platform/mock_factory.hpp"

// Forward declare Mock implementation classes
namespace wingman::platform::mock {
    class MockCapture;
    class MockInput;
}

#include "mock_capture.cpp"
#include "mock_input.cpp"

namespace wingman::platform::mock {

std::unique_ptr<ICapture> MockPlatformFactory::createCapture(const CaptureConfig& config) {
    auto capture = std::unique_ptr<ICapture>(new MockCapture());
    capture->initialize(config);
    return capture;
}

std::unique_ptr<IInput> MockPlatformFactory::createInput(const InputConfig& config) {
    auto input = std::unique_ptr<IInput>(new MockInput());
    input->initialize(config);
    return input;
}

std::unique_ptr<IWindow> MockPlatformFactory::createWindow() {
    // TODO: Implement MockWindow
    return nullptr;
}

std::unique_ptr<IScreen> MockPlatformFactory::createScreen() {
    // TODO: Implement MockScreen
    return nullptr;
}

} // namespace wingman::platform::mock
