#pragma once

#include "wingman/platform/platform_factory.hpp"

namespace wingman::platform::mock {

// 前向声明
class MockCapture;
class MockInput;

/**
 * @brief Mock 平台工厂（用于测试）
 *
 * 允许注入自定义实现进行单元测试。
 */
class MockPlatformFactory : public IPlatformFactory {
public:
    MockPlatformFactory() = default;
    ~MockPlatformFactory() override = default;

    std::unique_ptr<ICapture> createCapture(const CaptureConfig& config) override;
    std::unique_ptr<IInput> createInput(const InputConfig& config) override;
    std::unique_ptr<IWindow> createWindow() override;
    std::unique_ptr<IScreen> createScreen() override;

    std::string getPlatformName() const override {
        return "Mock";
    }

    bool supportsCaptureBackend(CaptureBackend backend) const override {
        (void)backend;
        return true;
    }

    bool supportsInputBackend(InputBackend backend) const override {
        (void)backend;
        return true;
    }

    CaptureBackend getRecommendedCaptureBackend() const override {
        return CaptureBackend::Mock;
    }

    InputBackend getRecommendedInputBackend() const override {
        return InputBackend::Mock;
    }

    std::vector<CaptureBackend> getAvailableCaptureBackends() const override {
        return {CaptureBackend::Mock};
    }

    std::vector<InputBackend> getAvailableInputBackends() const override {
        return {InputBackend::Mock};
    }
};

} // namespace wingman::platform::mock
