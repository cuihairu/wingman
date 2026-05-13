#pragma once

#include "wingman/platform/icapture.hpp"
#include "wingman/platform/iinput.hpp"
#include "wingman/platform/iwindow.hpp"
#include "wingman/platform/iscreen.hpp"
#include <memory>
#include <string>

namespace wingman::platform {

/**
 * @brief 平台工厂接口
 *
 * 负责创建平台相关的组件实例。每个平台实现自己的工厂。
 */
class IPlatformFactory {
public:
    virtual ~IPlatformFactory() = default;

    /**
     * @brief 创建捕获实例
     * @param config 捕获配置
     * @return 捕获实例指针
     */
    virtual std::unique_ptr<ICapture> createCapture(const CaptureConfig& config) = 0;

    /**
     * @brief 创建输入实例
     * @param config 输入配置
     * @return 输入实例指针
     */
    virtual std::unique_ptr<IInput> createInput(const InputConfig& config) = 0;

    /**
     * @brief 创建窗口管理实例
     * @return 窗口管理实例指针
     */
    virtual std::unique_ptr<IWindow> createWindow() = 0;

    /**
     * @brief 创建屏幕管理实例
     * @return 屏幕管理实例指针
     */
    virtual std::unique_ptr<IScreen> createScreen() = 0;

    /**
     * @brief 获取平台名称
     * @return 平台名称（Windows/macOS/Linux）
     */
    virtual std::string getPlatformName() const = 0;

    /**
     * @brief 检查是否支持指定的捕获后端
     * @param backend 捕获后端类型
     */
    virtual bool supportsCaptureBackend(CaptureBackend backend) const = 0;

    /**
     * @brief 检查是否支持指定的输入后端
     * @param backend 输入后端类型
     */
    virtual bool supportsInputBackend(InputBackend backend) const = 0;

    /**
     * @brief 获取推荐的捕获后端
     * @return 推荐的捕获后端类型
     */
    virtual CaptureBackend getRecommendedCaptureBackend() const = 0;

    /**
     * @brief 获取推荐的输入后端
     * @return 推荐的输入后端类型
     */
    virtual InputBackend getRecommendedInputBackend() const = 0;

    /**
     * @brief 获取可用的捕获后端列表
     */
    virtual std::vector<CaptureBackend> getAvailableCaptureBackends() const = 0;

    /**
     * @brief 获取可用的输入后端列表
     */
    virtual std::vector<InputBackend> getAvailableInputBackends() const = 0;
};

/**
 * @brief 获取平台工厂实例
 *
 * 返回当前平台的工厂实例。
 *
 * @return 平台工厂引用
 */
IPlatformFactory& getPlatformFactory();

/**
 * @brief 设置平台工厂实例
 *
 * 用于测试或注入自定义工厂。
 *
 * @param factory 工厂实例指针（nullptr 恢复默认）
 */
void setPlatformFactory(std::unique_ptr<IPlatformFactory> factory);

} // namespace wingman::platform
