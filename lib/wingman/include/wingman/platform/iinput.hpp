#pragma once

#include "wingman/platform/platform_types.hpp"
#include "wingman/screen.hpp"
#include <string>

namespace wingman::platform {

/**
 * @brief 输入后端接口
 *
 * 提供鼠标和键盘输入模拟的统一抽象接口。
 */
class IInput {
public:
    virtual ~IInput() = default;

    /**
     * @brief 初始化输入后端
     * @param config 输入配置
     * @return 成功返回 true
     */
    virtual bool initialize(const InputConfig& config) = 0;

    /**
     * @brief 关闭输入后端
     */
    virtual void shutdown() = 0;

    // ========== 鼠标操作 ==========

    /**
     * @brief 移动鼠标到指定位置
     * @param x 屏幕 X 坐标
     * @param y 屏幕 Y 坐标
     */
    virtual void mouseMove(int x, int y) = 0;

    /**
     * @brief 相对移动鼠标
     * @param deltaX X 轴偏移量
     * @param deltaY Y 轴偏移量
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
     * @param delta 滚动增量（正值向上，负值向下）
     */
    virtual void mouseWheel(int delta) = 0;

    /**
     * @brief 水平滚动
     * @param delta 滚动增量
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
     * @brief 按键（按下并释放）
     * @param key 键码
     */
    virtual void keyPress(KeyCode key) = 0;

    /**
     * @brief 组合按键
     * @param modifiers 修饰键列表（Ctrl, Alt, Shift 等）
     * @param key 主键
     */
    virtual void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) = 0;

    /**
     * @brief 输入文本
     * @param text 要输入的文本（支持 Unicode）
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
    virtual bool isMouseButtonPressed(MouseButton button) = 0;

    // ========== 高级功能 ==========

    /**
     * @brief 开始鼠标拖拽
     * @param button 鼠标按钮
     */
    virtual void mouseDragBegin(MouseButton button) = 0;

    /**
     * @brief 结束鼠标拖拽
     * @param button 鼠标按钮
     */
    virtual void mouseDragEnd(MouseButton button) = 0;

    /**
     * @brief 设置输入延迟（用于防检测）
     * @param delayMicroseconds 延迟时间（微秒）
     */
    virtual void setInputDelay(int delayMicroseconds) = 0;

    // ========== 状态查询 ==========

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取后端信息
     */
    virtual BackendInfo getBackendInfo() const = 0;

    /**
     * @brief 获取当前配置
     */
    virtual InputConfig getConfig() const = 0;

    /**
     * @brief 检查是否支持文本输入
     */
    virtual bool supportsTextInput() const = 0;

    /**
     * @brief 检查是否支持相对移动
     */
    virtual bool supportsRelativeMovement() const = 0;
};

} // namespace wingman::platform
