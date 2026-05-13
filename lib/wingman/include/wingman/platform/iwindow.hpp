#pragma once

#include "wingman/platform/platform_types.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief 窗口管理接口
 *
 * 提供窗口查找、操作、属性查询等功能。
 */
class IWindow {
public:
    virtual ~IWindow() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化窗口管理器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭窗口管理器
     */
    virtual void shutdown() = 0;

    // ========== 查找窗口 ==========

    /**
     * @brief 按标题查找窗口（返回第一个匹配）
     * @param title 窗口标题（支持部分匹配）
     */
    virtual WindowHandle find(const std::string& title) = 0;

    /**
     * @brief 按标题查找所有匹配窗口
     * @param title 窗口标题（支持部分匹配）
     */
    virtual std::vector<WindowHandle> findAll(const std::string& title) = 0;

    /**
     * @brief 按类名查找窗口
     * @param className 窗口类名
     */
    virtual WindowHandle findByClassName(const std::string& className) = 0;

    /**
     * @brief 按进程 ID 查找窗口
     * @param processId 进程 ID
     */
    virtual std::vector<WindowHandle> findByProcessId(uint32_t processId) = 0;

    /**
     * @brief 枚举所有顶层窗口
     */
    virtual std::vector<WindowInfo> enumerate() = 0;

    /**
     * @brief 获取当前前台窗口
     */
    virtual WindowHandle getForeground() = 0;

    // ========== 窗口操作 ==========

    /**
     * @brief 激活窗口（置顶并获取焦点）
     * @param hwnd 窗口句柄
     */
    virtual bool activate(WindowHandle hwnd) = 0;

    /**
     * @brief 最小化窗口
     * @param hwnd 窗口句柄
     */
    virtual bool minimize(WindowHandle hwnd) = 0;

    /**
     * @brief 最大化窗口
     * @param hwnd 窗口句柄
     */
    virtual bool maximize(WindowHandle hwnd) = 0;

    /**
     * @brief 还原窗口
     * @param hwnd 窗口句柄
     */
    virtual bool restore(WindowHandle hwnd) = 0;

    /**
     * @brief 关闭窗口（发送关闭消息）
     * @param hwnd 窗口句柄
     */
    virtual bool close(WindowHandle hwnd) = 0;

    /**
     * @brief 强制关闭窗口
     * @param hwnd 窗口句柄
     */
    virtual bool forceClose(WindowHandle hwnd) = 0;

    /**
     * @brief 隐藏窗口
     * @param hwnd 窗口句柄
     */
    virtual bool hide(WindowHandle hwnd) = 0;

    /**
     * @brief 显示窗口
     * @param hwnd 窗口句柄
     */
    virtual bool show(WindowHandle hwnd) = 0;

    // ========== 窗口属性 ==========

    /**
     * @brief 获取窗口标题
     * @param hwnd 窗口句柄
     */
    virtual std::string getTitle(WindowHandle hwnd) = 0;

    /**
     * @brief 获取窗口边界
     * @param hwnd 窗口句柄
     */
    virtual Rect getBounds(WindowHandle hwnd) = 0;

    /**
     * @brief 设置窗口边界
     * @param hwnd 窗口句柄
     * @param bounds 新边界
     */
    virtual bool setBounds(WindowHandle hwnd, const Rect& bounds) = 0;

    /**
     * @brief 移动窗口
     * @param hwnd 窗口句柄
     * @param x 新 X 坐标
     * @param y 新 Y 坐标
     */
    virtual bool move(WindowHandle hwnd, int x, int y) = 0;

    /**
     * @brief 调整窗口大小
     * @param hwnd 窗口句柄
     * @param width 新宽度
     * @param height 新高度
     */
    virtual bool resize(WindowHandle hwnd, int width, int height) = 0;

    /**
     * @brief 将窗口居中到指定显示器
     * @param hwnd 窗口句柄
     * @param monitorIndex 显示器索引
     */
    virtual bool center(WindowHandle hwnd, int monitorIndex) = 0;

    // ========== 窗口状态 ==========

    /**
     * @brief 检查窗口句柄是否有效
     * @param hwnd 窗口句柄
     */
    virtual bool isValid(WindowHandle hwnd) = 0;

    /**
     * @brief 检查窗口是否可见
     * @param hwnd 窗口句柄
     */
    virtual bool isVisible(WindowHandle hwnd) = 0;

    /**
     * @brief 检查窗口是否为前台窗口
     * @param hwnd 窗口句柄
     */
    virtual bool isForeground(WindowHandle hwnd) = 0;

    /**
     * @brief 检查窗口是否最小化
     * @param hwnd 窗口句柄
     */
    virtual bool isMinimized(WindowHandle hwnd) = 0;

    /**
     * @brief 检查窗口是否最大化
     * @param hwnd 窗口句柄
     */
    virtual bool isMaximized(WindowHandle hwnd) = 0;

    /**
     * @brief 获取窗口所属进程 ID
     * @param hwnd 窗口句柄
     */
    virtual std::optional<uint32_t> getProcessId(WindowHandle hwnd) = 0;

    // ========== 等待操作 ==========

    /**
     * @brief 等待窗口出现
     * @param title 窗口标题
     * @param timeoutMs 超时时间（毫秒）
     */
    virtual bool waitFor(const std::string& title, int timeoutMs) = 0;

    /**
     * @brief 等待窗口关闭
     * @param title 窗口标题
     * @param timeoutMs 超时时间（毫秒）
     */
    virtual bool waitClose(const std::string& title, int timeoutMs) = 0;

    /**
     * @brief 等待窗口成为前台窗口
     * @param hwnd 窗口句柄
     * @param timeoutMs 超时时间（毫秒）
     */
    virtual bool waitForForeground(WindowHandle hwnd, int timeoutMs) = 0;

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
