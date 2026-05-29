#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>

namespace wingman::platform {

/**
 * @brief Input configuration
 */
struct InputConfig {
    InputBackend preferredBackend = InputBackend::Auto;
    bool simulateHardwareInput = true;
    int defaultDelay = 10;  // Default operation interval (milliseconds)
    int inputDelay = 10000;  // Input delay (microseconds)
};

/**
 * @brief Input simulation interface
 *
 * Provides mouse and keyboard input simulation functionality.
 */
class IInput {
public:
    virtual ~IInput() = default;

    // ========== Initialization ==========

    /**
     * @brief Initialize input simulator
     * @param config Configuration
     */
    virtual bool initialize(const InputConfig& config) = 0;

    /**
     * @brief Shutdown input simulator
     */
    virtual void shutdown() = 0;

    // ========== Mouse operations ==========

    /**
     * @brief Move mouse to absolute coordinates
     * @param x X coordinate
     * @param y Y coordinate
     */
    virtual void mouseMove(int x, int y) = 0;

    /**
     * @brief Move mouse relatively
     * @param deltaX X direction offset
     * @param deltaY Y direction offset
     */
    virtual void mouseMoveRelative(int deltaX, int deltaY) = 0;

    /**
     * @brief Press mouse button down
     * @param button Mouse button
     */
    virtual void mouseDown(MouseButton button) = 0;

    /**
     * @brief Release mouse button
     * @param button Mouse button
     */
    virtual void mouseUp(MouseButton button) = 0;

    /**
     * @brief Click mouse button
     * @param button Mouse button
     */
    virtual void mouseClick(MouseButton button) = 0;

    /**
     * @brief Double-click mouse button
     * @param button Mouse button
     */
    virtual void mouseDoubleClick(MouseButton button) = 0;

    /**
     * @brief Scroll mouse wheel
     * @param delta Scroll amount (positive up, negative down)
     */
    virtual void mouseWheel(int delta) = 0;

    /**
     * @brief Horizontal scroll mouse wheel
     * @param delta Scroll amount
     */
    virtual void mouseWheelHorizontal(int delta) = 0;

    // ========== Keyboard operations ==========

    /**
     * @brief Press key down
     * @param key Key code
     */
    virtual void keyDown(KeyCode key) = 0;

    /**
     * @brief Release key
     * @param key Key code
     */
    virtual void keyUp(KeyCode key) = 0;

    /**
     * @brief Key press (down then up)
     * @param key Key code
     */
    virtual void keyPress(KeyCode key) = 0;

    /**
     * @brief Input text
     * @param text Text content
     */
    virtual void textInput(const std::string& text) = 0;

    // ========== Status query ==========

    /**
     * @brief Get current mouse position
     */
    virtual Point getMousePosition() = 0;

    /**
     * @brief Check if key is pressed
     * @param key Key code
     */
    virtual bool isKeyPressed(KeyCode key) = 0;

    /**
     * @brief Check if mouse button is pressed
     * @param button Mouse button
     */
    virtual bool isMousePressed(MouseButton button) = 0;

    // ========== Backend information ==========

    /**
     * @brief Get backend name
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief Get backend info
     */
    virtual BackendInfo getBackendInfo() const = 0;
};

} // namespace wingman::platform
