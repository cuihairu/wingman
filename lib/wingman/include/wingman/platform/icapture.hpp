#pragma once

#include "wingman/platform/platform_types.hpp"
#include <memory>
#include <string>

// 前向声明 Bitmap
namespace wingman {
class Bitmap;
}

namespace wingman::platform {

/**
 * @brief 捕获配置
 */
struct CaptureConfig {
    CaptureBackend preferredBackend = CaptureBackend::Auto;
    int monitorIndex = 0;
    int fps = 60;
    bool includeCursor = false;
    bool useHardwareAcceleration = true;
};

/**
 * @brief 屏幕捕获接口
 *
 * 提供屏幕、窗口、区域的图像捕获功能。
 */
class ICapture {
public:
    virtual ~ICapture() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化捕获器
     * @param config 配置
     */
    virtual bool initialize(const CaptureConfig& config) = 0;

    /**
     * @brief 关闭捕获器
     */
    virtual void shutdown() = 0;

    // ========== 屏幕捕获 ==========

    /**
     * @brief 捕获整个屏幕
     * @param monitorIndex 显示器索引
     */
    virtual std::unique_ptr<Bitmap> captureScreen(int monitorIndex) = 0;

    /**
     * @brief 捕获屏幕区域
     * @param region 区域
     */
    virtual std::unique_ptr<Bitmap> captureRegion(const Rect& region) = 0;

    /**
     * @brief 捕获指定显示器的区域
     * @param monitorIndex 显示器索引
     * @param region 区域
     */
    virtual std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) = 0;

    // ========== 窗口捕获 ==========

    /**
     * @brief 捕获窗口
     * @param hwnd 窗口句柄
     */
    virtual std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) = 0;

    /**
     * @brief 捕获窗口区域
     * @param hwnd 窗口句柄
     * @param region 相对窗口的区域
     */
    virtual std::unique_ptr<Bitmap> captureWindowRegion(WindowHandle hwnd, const Rect& region) = 0;

    // ========== 信息查询 ==========

    /**
     * @brief 获取显示器数量
     */
    virtual int getMonitorCount() = 0;

    /**
     * @brief 获取显示器边界
     * @param monitorIndex 显示器索引
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief 获取显示器名称
     * @param monitorIndex 显示器索引
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    // ========== 状态 ==========

    /**
     * @brief 检查捕获器是否可用
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
};

} // namespace wingman::platform
