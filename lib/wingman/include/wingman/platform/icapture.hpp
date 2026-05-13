#pragma once

#include "wingman/platform/platform_types.hpp"
#include "wingman/screen.hpp"
#include <memory>
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief 捕获后端接口
 *
 * 提供屏幕和窗口捕获的统一抽象接口，支持多种后端实现。
 */
class ICapture {
public:
    virtual ~ICapture() = default;

    /**
     * @brief 初始化捕获后端
     * @param config 捕获配置
     * @return 成功返回 true
     */
    virtual bool initialize(const CaptureConfig& config) = 0;

    /**
     * @brief 关闭捕获后端，释放资源
     */
    virtual void shutdown() = 0;

    // ========== 屏幕捕获 ==========

    /**
     * @brief 捕获整个显示器
     * @param monitorIndex 显示器索引（0 为主显示器）
     * @return 捕获的位图，失败返回 nullptr
     */
    virtual std::unique_ptr<Bitmap> captureScreen(int monitorIndex = 0) = 0;

    /**
     * @brief 捕获指定区域
     * @param region 捕获区域
     * @return 捕获的位图，失败返回 nullptr
     */
    virtual std::unique_ptr<Bitmap> captureRegion(const Rect& region) = 0;

    /**
     * @brief 捕获指定显示器的指定区域
     * @param monitorIndex 显示器索引
     * @param region 捕获区域（相对于显示器）
     * @return 捕获的位图，失败返回 nullptr
     */
    virtual std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) = 0;

    // ========== 窗口捕获 ==========

    /**
     * @brief 捕获窗口
     * @param hwnd 窗口句柄
     * @return 捕获的位图，失败返回 nullptr
     */
    virtual std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) = 0;

    /**
     * @brief 按标题查找并捕获窗口
     * @param title 窗口标题（支持部分匹配）
     * @return 捕获的位图，未找到返回 nullptr
     */
    virtual std::unique_ptr<Bitmap> captureWindowByTitle(const std::string& title) = 0;

    // ========== 信息查询 ==========

    /**
     * @brief 获取显示器数量
     */
    virtual int getMonitorCount() = 0;

    /**
     * @brief 获取显示器边界
     * @param monitorIndex 显示器索引
     * @return 显示器边界矩形
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief 获取显示器名称
     * @param monitorIndex 显示器索引
     * @return 显示器名称
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    /**
     * @brief 获取主显示器索引
     */
    virtual int getPrimaryMonitorIndex() = 0;

    /**
     * @brief 获取显示器 DPI 缩放比例
     * @param monitorIndex 显示器索引
     * @return DPI 缩放比例（1.0 = 96dpi, 1.5 = 144dpi, 2.0 = 192dpi）
     */
    virtual double getDpiScale(int monitorIndex) = 0;

    /**
     * @brief 获取支持的显示模式
     * @param monitorIndex 显示器索引
     * @return 显示模式列表
     */
    virtual std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) = 0;

    /**
     * @brief 获取当前显示模式
     * @param monitorIndex 显示器索引
     * @return 当前显示模式
     */
    virtual std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) = 0;

    // ========== 状态查询 ==========

    /**
     * @brief 检查后端是否可用
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取后端信息
     */
    virtual BackendInfo getBackendInfo() const = 0;

    /**
     * @brief 检查是否支持窗口捕获
     */
    virtual bool supportsWindowCapture() const = 0;

    /**
     * @brief 检查是否支持光标捕获
     */
    virtual bool supportsCursorCapture() const = 0;

    // ========== 配置 ==========

    /**
     * @brief 获取当前配置
     */
    virtual CaptureConfig getConfig() const = 0;

    /**
     * @brief 更新配置
     * @note 某些配置可能需要重新初始化才能生效
     */
    virtual bool updateConfig(const CaptureConfig& config) = 0;
};

} // namespace wingman::platform
