#pragma once

#include "wingman/platform/platform_types.hpp"
#include "wingman/screen.hpp"
#include <vector>
#include <optional>

namespace wingman::platform {

/**
 * @brief 窗口管理接口
 *
 * 提供窗口查找、操作和信息查询的统一抽象接口。
 */
class IWindow {
public:
    virtual ~IWindow() = default;

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
     * @brief 按标题查找窗口
     * @param title 窗口标题（支持部分匹配）
     * @return 窗口句柄，未找到返回 nullptr
     */
    virtual WindowHandle find(const std::string& title) = 0;

    /**
     * @brief 查找所有匹配标题的窗口
     * @param title 窗口标题（支持部分匹配）
     * @return 窗口句柄列表
     */
    virtual std::vector<WindowHandle> findAll(const std::string& title) = 0;

    /**
     * @brief 按类名查找窗口
     * @param className 窗口类名
     * @return 窗口句柄，未找到返回 nullptr
     */
    virtual WindowHandle findByClassName(const std::string& className) = 0;

    /**
     * @brief 按进程 ID 查找窗口
     * @param processId 进程 ID
     * @return 窗口句柄列表
     */
    virtual std::vector<WindowHandle> findByProcessId(uint32_t processId) = 0;

    /**
     * @brief 枚举所有顶层窗口
     * @return 窗口信息列表
     */
    virtual std::vector<WindowInfo> enumerate() = 0;

    /**
     * @brief 获取前台窗口
     * @return 窗口句柄，无前台窗口返回 nullptr
     */
    virtual WindowHandle getForeground() = 0;

    // ========== 窗口操作 ==========

    /**
     * @brief 激活窗口（置顶）
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool activate(WindowHandle hwnd) = 0;

    /**
     * @brief 最小化窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool minimize(WindowHandle hwnd) = 0;

    /**
     * @brief 最大化窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool maximize(WindowHandle hwnd) = 0;

    /**
     * @brief 还原窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool restore(WindowHandle hwnd) = 0;

    /**
     * @brief 关闭窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool close(WindowHandle hwnd) = 0;

    /**
     * @brief 强制关闭窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool forceClose(WindowHandle hwnd) = 0;

    /**
     * @brief 隐藏窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool hide(WindowHandle hwnd) = 0;

    /**
     * @brief 显示窗口
     * @param hwnd 窗口句柄
     * @return 成功返回 true
     */
    virtual bool show(WindowHandle hwnd) = 0;

    // ========== 窗口属性 ==========

    /**
     * @brief 获取窗口标题
     * @param hwnd 窗口句柄
     * @return 窗口标题
     */
    virtual std::string getTitle(WindowHandle hwnd) = 0;

    /**
     * @brief 获取窗口边界
     * @param hwnd 窗口句柄
     * @return 窗口边界矩形
     */
    virtual Rect getBounds(WindowHandle hwnd) = 0;

    /**
     * @brief 设置窗口边界
     * @param hwnd 窗口句柄
     * @param bounds 新的边界
     * @return 成功返回 true
     */
    virtual bool setBounds(WindowHandle hwnd, const Rect& bounds) = 0;

    /**
     * @brief 移动窗口
     * @param hwnd 窗口句柄
     * @param x 目标 X 坐标
     * @param y 目标 Y 坐标
     * @return 成功返回 true
     */
    virtual bool move(WindowHandle hwnd, int x, int y) = 0;

    /**
     * @brief 调整窗口大小
     * @param hwnd 窗口句柄
     * @param width 新宽度
     * @param height 新高度
     * @return 成功返回 true
     */
    virtual bool resize(WindowHandle hwnd, int width, int height) = 0;

    /**
     * @brief 居中窗口到显示器
     * @param hwnd 窗口句柄
     * @param monitorIndex 显示器索引
     * @return 成功返回 true
     */
    virtual bool center(WindowHandle hwnd, int monitorIndex = 0) = 0;

    // ========== 状态查询 ==========

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
     * @return 找到返回 true
     */
    virtual bool waitFor(const std::string& title, int timeoutMs = 5000) = 0;

    /**
     * @brief 等待窗口关闭
     * @param title 窗口标题
     * @param timeoutMs 超时时间（毫秒）
     * @return 已关闭返回 true
     */
    virtual bool waitClose(const std::string& title, int timeoutMs = 5000) = 0;

    /**
     * @brief 等待窗口变为前台
     * @param hwnd 窗口句柄
     * @param timeoutMs 超时时间（毫秒）
     * @return 成功返回 true
     */
    virtual bool waitForForeground(WindowHandle hwnd, int timeoutMs = 5000) = 0;

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
