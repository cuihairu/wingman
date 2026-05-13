#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>

namespace wingman::platform {

/**
 * @brief 输入配置
 */
struct InputConfig {
    InputBackend preferredBackend = InputBackend::Auto;
    bool simulateHardwareInput = true;
    int defaultDelay = 10;  // 默认操作间隔（毫秒）
    int inputDelay = 10000;  // 输入延迟（微秒）
};

/**
 * @brief 输入模拟接口
 *
 * 提供鼠标和键盘输入模拟功能。
 */
class IInput {
public:
    virtual ~IInput() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化输入模拟器
     * @param config 配置
     */
    virtual bool initialize(const InputConfig& config) = 0;

    /**
     * @brief 关闭输入模拟器
     */
    virtual void shutdown() = 0;

    // ========== 鼠标操作 ==========

    /**
     * @brief 移动鼠标到绝对坐标
     * @param x X 坐标
     * @param y Y 坐标
     */
    virtual void mouseMove(int x, int y) = 0;

    /**
     * @brief 相对移动鼠标
     * @param deltaX X 方向偏移
     * @param deltaY Y 方向偏移
     */
    virtual void mouseMoveRelative(int deltaX, int deltaY) = 0;

    /**
     * @brief 按下鼠标按钮
     * @param button 鼠标按钮
     */
    virtual void mouseDown(MouseButton button) = 0;

    /**
     * @brief 释放鼠标按钮
     * @param button 鼠标按钮
     */
    virtual void mouseUp(MouseButton button) = 0;

    /**
     * @brief 点击鼠标按钮
     * @param button 鼠标按钮
     */
    virtual void mouseClick(MouseButton button) = 0;

    /**
     * @brief 双击鼠标按钮
     * @param button 鼠标按钮
     */
    virtual void mouseDoubleClick(MouseButton button) = 0;

    /**
     * @brief 滚动鼠标滚轮
     * @param delta 滚动量（正数向上，负数向下）
     */
    virtual void mouseWheel(int delta) = 0;

    /**
     * @brief 水平滚动鼠标滚轮
     * @param delta 滚动量
     */
    virtual void mouseWheelHorizontal(int delta) = 0;

    // ========== 键盘操作 ==========

    /**
     * @brief 按下键
     * @param key 键码
     */
    virtual void keyDown(KeyCode key) = 0;

    /**
     * @brief 释放键
     * @param key 键码
     */
    virtual void keyUp(KeyCode key) = 0;

    /**
     * @brief 按键（按下后释放）
     * @param key 键码
     */
    virtual void keyPress(KeyCode key) = 0;

    /**
     * @brief 输入文本
     * @param text 文本内容
     */
    virtual void textInput(const std::string& text) = 0;

    // ========== 状态查询 ==========

    /**
     * @brief 获取当前鼠标位置
     */
    virtual Point getMousePosition() = 0;

    /**
     * @brief 检查键是否被按下
     * @param key 键码
     */
    virtual bool isKeyPressed(KeyCode key) = 0;

    /**
     * @brief 检查鼠标按钮是否被按下
     * @param button 鼠标按钮
     */
    virtual bool isMousePressed(MouseButton button) = 0;

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
