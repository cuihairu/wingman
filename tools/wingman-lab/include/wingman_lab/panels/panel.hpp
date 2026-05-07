#pragma once

#include "wingman_lab/common.hpp"
#include <functional>

namespace wingman::lab {

// ========== Panel 基类 ==========

class Panel {
public:
    virtual ~Panel() = default;

    virtual void initialize() = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;

    // 设置测试回调
    using TestCallback = std::function<void(const TestResult&)>;
    void setTestCallback(TestCallback callback) { testCallback_ = std::move(callback); }

    // 设置代码生成回调
    using CodeCallback = std::function<void(const std::string&)>;
    void setCodeCallback(CodeCallback callback) { codeCallback_ = std::move(callback); }

protected:
    TestCallback testCallback_;
    CodeCallback codeCallback_;

    void notifyTest(const TestResult& result) {
        if (testCallback_) testCallback_(result);
    }

    void notifyCode(const std::string& code) {
        if (codeCallback_) codeCallback_(code);
    }
};

// ========== 各功能面板 ==========

class CapturePanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

private:
    CaptureConfig config_;
    std::string capturedPath_;
};

class PixelPanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

private:
    PixelConfig config_;
    uint32_t pickedColor_ = 0x000000;
};

class ImagePanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

private:
    ImageConfig config_;
    std::string templatePath_;
};

class OCRPanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

private:
    OCRConfig config_;
    std::string imagePath_;
};

class MLPanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

private:
    MLConfig config_;
};

class CodeGenPanel : public Panel {
public:
    void initialize() override;
    void render() override;
    void shutdown() override;

    void setGeneratedCode(const std::string& code) {
        generatedCode_ = code;
    }

private:
    std::string generatedCode_;
    bool autoCopy_ = false;
};

} // namespace wingman::lab
