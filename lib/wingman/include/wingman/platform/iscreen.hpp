#pragma once

#include "wingman/platform/platform_types.hpp"
#include "wingman/screen.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief 屏幕管理接口
 *
 * 提供显示器信息和设置的统一抽象接口。
 */
class IScreen {
public:
    virtual ~IScreen() = default;

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
     * @brief 获取显示器边界
     * @param monitorIndex 显示器索引
     */
    virtual Rect getMonitorBounds(int monitorIndex) = 0;

    /**
     * @brief 获取显示器工作区（排除任务栏）
     * @param monitorIndex 显示器索引
     */
    virtual Rect getMonitorWorkArea(int monitorIndex) = 0;

    /**
     * @brief 获取显示器名称
     * @param monitorIndex 显示器索引
     */
    virtual std::string getMonitorName(int monitorIndex) = 0;

    /**
     * @brief 获取显示器是否为主显示器
     * @param monitorIndex 显示器索引
     */
    virtual bool isPrimaryMonitor(int monitorIndex) = 0;

    // ========== DPI 和缩放 ==========

    /**
     * @brief 获取显示器 DPI 缩放比例
     * @param monitorIndex 显示器索引
     * @return 缩放比例（1.0 = 96dpi, 1.5 = 144dpi, 2.0 = 192dpi）
     */
    virtual double getDpiScale(int monitorIndex) = 0;

    /**
     * @brief 获取显示器原始 DPI
     * @param monitorIndex 显示器索引
     * @return DPI 值
     */
    virtual int getDpi(int monitorIndex) = 0;

    /**
     * @brief 将逻辑坐标转换为物理坐标
     * @param point 逻辑坐标
     * @param monitorIndex 显示器索引
     */
    virtual Point logicalToPhysical(const Point& point, int monitorIndex) = 0;

    /**
     * @brief 将物理坐标转换为逻辑坐标
     * @param point 物理坐标
     * @param monitorIndex 显示器索引
     */
    virtual Point physicalToLogical(const Point& point, int monitorIndex) = 0;

    // ========== 显示模式 ==========

    /**
     * @brief 获取支持的显示模式
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
     * @return 成功返回 true
     */
    virtual bool setDisplayMode(int monitorIndex, const DisplayMode& mode) = 0;

    /**
     * @brief 重置显示模式为系统默认
     * @param monitorIndex 显示器索引
     * @return 成功返回 true
     */
    virtual bool resetDisplayMode(int monitorIndex) = 0;

    // ========== 屏幕保护和电源 ==========

    /**
     * @brief 检查屏幕保护是否运行
     */
    virtual bool isScreenSaverRunning() = 0;

    /**
     * @brief 启动屏幕保护
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

    // ========== 多显示器布局 ==========

    /**
     * @brief 获取所有显示器的组合边界
     */
    virtual Rect getVirtualScreenBounds() = 0;

    /**
     * @brief 获取包含指定点的显示器索引
     * @param point 屏幕坐标点
     * @return 显示器索引，未找到返回 -1
     */
    virtual int getMonitorFromPoint(const Point& point) = 0;

    /**
     * @brief 获取窗口所在的显示器索引
     * @param hwnd 窗口句柄
     * @return 显示器索引，未找到返回 -1
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
