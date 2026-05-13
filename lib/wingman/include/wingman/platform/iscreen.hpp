#pragma once

#include "wingman/platform/platform_types.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief 屏幕管理接口
 *
 * 提供显示器信息查询、DPI 缩放、显示模式设置等功能。
 */
class IScreen {
public:
    virtual ~IScreen() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化屏幕管理器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭屏幕管理器
     */
    virtual void shutdown() = 0;

    // ========== 显示器信息 ==========

    /**
     * @brief 获取显示器数量
     */
    virtual int getMonitorCount() = 0;

    /**
     * @brief 获取主显示器索引
     */
    virtual int getPrimaryMonitorIndex() = 0;

    /**
     * @brief 获取主显示器边界
     */
    virtual Rect getPrimaryMonitorBounds() = 0;

    /**
     * @brief 获取指定显示器的边界
     * @param monitorIndex 显示器索引
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief 获取指定显示器的工作区边界（排除任务栏）
     * @param monitorIndex 显示器索引
     */
    virtual Rect getMonitorWorkArea(int monitorIndex) = 0;

    /**
     * @brief 获取显示器名称
     * @param monitorIndex 显示器索引
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    /**
     * @brief 判断是否为主显示器
     * @param monitorIndex 显示器索引
     */
    virtual bool isPrimaryMonitor(int monitorIndex) = 0;

    // ========== DPI 缩放 ==========

    /**
     * @brief 获取 DPI 缩放比例
     * @param monitorIndex 显示器索引
     * @return 缩放比例（1.0 = 96 DPI）
     */
    virtual double getDpiScale(int monitorIndex) = 0;

    /**
     * @brief 获取 DPI 值
     * @param monitorIndex 显示器索引
     */
    virtual int getDpi(int monitorIndex) = 0;

    /**
     * @brief 逻辑坐标转换为物理坐标
     * @param point 逻辑坐标
     * @param monitorIndex 显示器索引
     */
    virtual Point logicalToPhysical(const Point& point, int monitorIndex) = 0;

    /**
     * @brief 物理坐标转换为逻辑坐标
     * @param point 物理坐标
     * @param monitorIndex 显示器索引
     */
    virtual Point physicalToLogical(const Point& point, int monitorIndex) = 0;

    // ========== 显示模式 ==========

    /**
     * @brief 获取支持的显示模式列表
     * @param monitorIndex 显示器索引
     */
    virtual std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) = 0;

    /**
     * @brief 获取当前显示模式
     * @param monitorIndex 显示器索引
     */
    virtual std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) = 0;

    /**
     * @brief 设置显示模式
     * @param monitorIndex 显示器索引
     * @param mode 显示模式
     */
    virtual bool setDisplayMode(int monitorIndex, const DisplayMode& mode) = 0;

    /**
     * @brief 重置显示模式为默认
     * @param monitorIndex 显示器索引
     */
    virtual bool resetDisplayMode(int monitorIndex) = 0;

    // ========== 屏幕状态 ==========

    /**
     * @brief 检查屏幕保护程序是否运行
     */
    virtual bool isScreenSaverRunning() = 0;

    /**
     * @brief 启动屏幕保护程序
     */
    virtual void startScreenSaver() = 0;

    /**
     * @brief 检查显示器是否关闭
     */
    virtual bool isMonitorOff() = 0;

    /**
     * @brief 唤醒显示器
     */
    virtual void wakeUpMonitor() = 0;

    // ========== 虚拟屏幕 ==========

    /**
     * @brief 获取虚拟屏幕边界（所有显示器组成的区域）
     */
    virtual Rect getVirtualScreenBounds() = 0;

    /**
     * @brief 获取包含指定点的显示器索引
     * @param point 点坐标
     */
    virtual int getMonitorFromPoint(const Point& point) = 0;

    /**
     * @brief 获取包含指定窗口的显示器索引
     * @param hwnd 窗口句柄
     */
    virtual int getMonitorFromWindow(WindowHandle hwnd) = 0;

    // ========== 后端信息 ==========

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
