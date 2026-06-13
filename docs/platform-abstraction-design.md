# 平台抽象层设计

> **版本**: v1.0
> **日期**: 2026-05-14
> **状态**: 设计草案

---

## 1. 概述

### 1.1 设计目标

将平台相关代码（screen/window/input/capture）抽象为统一接口，实现：

| 目标 | 说明 |
|------|------|
| **接口统一** | 上层代码不依赖特定平台 API |
| **易于扩展** | 添加新平台只需实现接口 |
| **技术栈切换** | 可替换底层实现（如 GDI+ → DXGI → ScreenCaptureKit） |
| **便于测试** | 可 mock 平台层进行单元测试 |

### 1.2 当前问题

```
┌─────────────────────────────────────────────────────────────┐
│                    当前架构                                  │
├─────────────────────────────────────────────────────────────┤
│  lib/wingman/src/                                          │
│  ├── screen.cpp  ──→ Windows GDI+ (硬编码)                 │
│  ├── window.cpp  ──→ Windows Win32 API (硬编码)            │
│  ├── input.cpp   ──→ Windows SendInput (硬编码)            │
│  └── capture/    ──→ Windows GDI+ (硬编码)                 │
│                                                             │
│  问题：平台代码分散，难以切换实现                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 目标架构

```
┌───────────────────────────────────────────────────────────────────┐
│                        目标架构                                    │
├───────────────────────────────────────────────────────────────────┤
│                                                                   │
│   lib/wingman/                                                    │
│   ├── include/wingman/platform/          ← 平台抽象接口           │
│   │   ├── icapture.hpp                   ← 捕获接口               │
│   │   ├── iinput.hpp                      ← 输入接口               │
│   │   ├── iwindow.hpp                     ← 窗口接口               │
│   │   ├── iscreen.hpp                     ← 屏幕接口               │
│   │   └── platform_factory.hpp            ← 工厂接口               │
│   │                                                                  │
│   ├── src/platform/                       ← 平台实现               │
│   │   ├── win/                            ← Windows 实现          │
│   │   │   ├── gdi_capture.cpp             ← GDI+ 捕获              │
│   │   │   ├── dxgi_capture.cpp            ← DXGI 捕获             │
│   │   │   ├── win32_input.cpp             ← SendInput             │
│   │   │   ├── win32_window.cpp            ← Win32 Window          │
│   │   │   └── win32_screen.cpp            ← GDI+ Screen           │
│   │   │                                                                  │
│   │   ├── mac/                            ← macOS 实现            │
│   │   │   ├── cg_capture.cpp              ← Core Graphics         │
│   │   │   ├── screencapture_capture.cpp   ← ScreenCaptureKit      │
│   │   │   ├── cg_event_input.cpp          ← CGEvent               │
│   │   │   └── cocoa_window.cpp            ← Cocoa Window          │
│   │   │                                                                  │
│   │   ├── linux/                          ← Linux 实现           │
│   │   │   ├── x11_capture.cpp             ← X11 捕获              │
│   │   │   ├── pipewire_capture.cpp        ← PipeWire              │
│   │   │   ├── x11_input.cpp               ← XTest Extension       │
│   │   │   └── x11_window.cpp              ← X11 Window            │
│   │   │                                                                  │
│   │   └── factory.cpp                     ← 平台工厂              │
│   │                                                                  │
│   └── src/ (原有业务逻辑)                                            │
│       ├── screen.cpp  ──→ 使用 IScreen* 接口                         │
│       ├── window.cpp  ──→ 使用 IWindow* 接口                         │
│       └── input.cpp   ──→ 使用 IInput* 接口                          │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## 2. 接口设计

### 2.1 捕获接口 (ICapture)

```cpp
namespace wingman::platform {

enum class CaptureBackend : uint8_t {
    Auto,           // 自动选择最佳实现
    GDI,            // Windows GDI+
    DXGI,           // Windows DXGI Desktop Duplication
    DirectX,        // Windows DirectX
    ScreenCaptureKit, // macOS ScreenCaptureKit
    CoreGraphics,   // macOS Core Graphics
    X11,            // Linux X11
    PipeWire,       // Linux PipeWire
};

struct CaptureConfig {
    CaptureBackend preferredBackend = CaptureBackend::Auto;
    int monitorIndex = 0;
    int fps = 60;
    bool includeCursor = true;
};

class ICapture {
public:
    virtual ~ICapture() = default;

    virtual bool initialize(const CaptureConfig& config) = 0;
    virtual void shutdown() = 0;

    // 屏幕捕获
    virtual std::unique_ptr<Bitmap> captureScreen(int monitorIndex = 0) = 0;
    virtual std::unique_ptr<Bitmap> captureRegion(const Rect& region) = 0;

    // 窗口捕获
    virtual std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) = 0;

    // 信息查询
    virtual int getMonitorCount() = 0;
    virtual Rect getMonitorBounds(int monitorIndex) = 0;
    virtual std::string getMonitorName(int monitorIndex) = 0;

    // 状态
    virtual bool isAvailable() const = 0;
    virtual std::string getBackendName() const = 0;
};

} // namespace wingman::platform
```

### 2.2 输入接口 (IInput)

```cpp
namespace wingman::platform {

enum class InputBackend : uint8_t {
    Auto,
    SendInput,      // Windows SendInput
    CGEvent,        // macOS CGEvent
    XTest,          // Linux XTest
    libinput,       // Linux libinput (evdev)
};

struct InputConfig {
    InputBackend preferredBackend = InputBackend::Auto;
    bool simulateHardwareInput = true;  // 硬件模拟 vs 软件模拟
};

class IInput {
public:
    virtual ~IInput() = default;

    virtual bool initialize(const InputConfig& config) = 0;
    virtual void shutdown() = 0;

    // 鼠标操作
    virtual void mouseMove(int x, int y) = 0;
    virtual void mouseDown(MouseButton button) = 0;
    virtual void mouseUp(MouseButton button) = 0;
    virtual void mouseClick(MouseButton button) = 0;
    virtual void mouseWheel(int delta) = 0;

    // 键盘操作
    virtual void keyDown(KeyCode key) = 0;
    virtual void keyUp(KeyCode key) = 0;
    virtual void keyPress(KeyCode key) = 0;
    virtual void textInput(const std::string& text) = 0;

    // 状态查询
    virtual Point getMousePosition() = 0;
    virtual bool isKeyPressed(KeyCode key) = 0;

    virtual std::string getBackendName() const = 0;
};

} // namespace wingman::platform
```

### 2.3 窗口接口 (IWindow)

```cpp
namespace wingman::platform {

class IWindow {
public:
    virtual ~IWindow() = default;

    // 查找窗口
    virtual WindowHandle find(const std::string& title) = 0;
    virtual std::vector<WindowHandle> findAll(const std::string& title) = 0;
    virtual WindowHandle findByClassName(const std::string& className) = 0;
    virtual std::vector<WindowInfo> enumerate() = 0;

    // 窗口操作
    virtual bool activate(WindowHandle hwnd) = 0;
    virtual bool minimize(WindowHandle hwnd) = 0;
    virtual bool maximize(WindowHandle hwnd) = 0;
    virtual bool restore(WindowHandle hwnd) = 0;
    virtual bool close(WindowHandle hwnd) = 0;

    // 窗口属性
    virtual std::string getTitle(WindowHandle hwnd) = 0;
    virtual Rect getBounds(WindowHandle hwnd) = 0;
    virtual bool setBounds(WindowHandle hwnd, const Rect& bounds) = 0;

    // 状态查询
    virtual bool isValid(WindowHandle hwnd) = 0;
    virtual bool isVisible(WindowHandle hwnd) = 0;
    virtual bool isForeground(WindowHandle hwnd) = 0;
    virtual WindowHandle getForeground() = 0;

    // 等待操作
    virtual bool waitFor(const std::string& title, int timeoutMs) = 0;
    virtual bool waitClose(const std::string& title, int timeoutMs) = 0;
};

} // namespace wingman::platform
```

### 2.4 屏幕接口 (IScreen)

```cpp
namespace wingman::platform {

class IScreen {
public:
    virtual ~IScreen() = default;

    // 显示器信息
    virtual int getMonitorCount() = 0;
    virtual Rect getPrimaryMonitorBounds() = 0;
    virtual Rect getMonitorBounds(int monitorIndex) = 0;
    virtual std::string getMonitorName(int monitorIndex) = 0;

    // 屏幕设置
    virtual bool setDisplayMode(int monitorIndex, const DisplayMode& mode) = 0;
    virtual DisplayMode getDisplayMode(int monitorIndex) = 0;
    virtual std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) = 0;

    // DPI 缩放
    virtual double getDpiScale(int monitorIndex) = 0;
    virtual Point getDpiAwarePosition(const Point& pos, int monitorIndex) = 0;
};

} // namespace wingman::platform
```

#### 唯一规范约束

`IScreen` 是屏幕抽象的**唯一规范**。完整的多显示器 API 定义在
`lib/wingman/include/wingman/platform/iscreen.hpp`，包括 `getMonitorCount`、
`getPrimaryMonitorIndex`、`getMonitorBounds(int)`、`getMonitorName(int)`、
`getDpiScale(int)` 等。

旧的静态 `wingman::Screen` 类（`lib/wingman/include/wingman/screen.hpp` 与
`lib/wingman/src/screen.cpp`）只支持主屏，**已冻结**：

- 不得在旧 `Screen` 类上新增多显示器 API，避免出现两套并行的多显示器抽象
- 新功能（如虚拟桌面、按 displayId 截图、显示器枚举）必须通过 `IScreen` 实现
- 历史 `Screen` 调用方暂保持不变，迁移/删除作为独立工作推进

多显示器截图的 RPC 通道与传输约束见 `docs/architecture-decisions.md` 的
*Display Selection* 小节。

---

## 3. 平台工厂

### 3.1 工厂接口

```cpp
namespace wingman::platform {

class IPlatformFactory {
public:
    virtual ~IPlatformFactory() = default;

    virtual std::unique_ptr<ICapture> createCapture(const CaptureConfig& config) = 0;
    virtual std::unique_ptr<IInput> createInput(const InputConfig& config) = 0;
    virtual std::unique_ptr<IWindow> createWindow() = 0;
    virtual std::unique_ptr<IScreen> createScreen() = 0;

    virtual std::string getPlatformName() const = 0;
};

// 全局访问
IPlatformFactory& getPlatformFactory();

} // namespace wingman::platform
```

### 3.2 使用示例

```cpp
// 使用平台抽象层
using namespace wingman::platform;

auto& factory = getPlatformFactory();

// 创建捕获实例
auto capture = factory.createCapture({CaptureBackend::Auto});
auto bitmap = capture->captureScreen();

// 创建输入实例
auto input = factory.createInput({InputBackend::Auto});
input->mouseClick(MouseButton::Left);

// 窗口操作
auto window = factory.createWindow();
auto hwnd = window->find("Notepad");
window->activate(hwnd);
```

---

## 4. 实现策略

### 4.1 编译时选择（推荐）

```cmake
# CMakeLists.txt
if(WIN32)
    target_sources(wingman PRIVATE
        src/platform/win/win32_capture.cpp
        src/platform/win/win32_input.cpp
        src/platform/win/win32_window.cpp
    )
elseif(APPLE)
    target_sources(wingman PRIVATE
        src/platform/mac/screencapture_capture.cpp
        src/platform/mac/cg_event_input.cpp
        src/platform/mac/cocoa_window.cpp
    )
else()
    target_sources(wingman PRIVATE
        src/platform/linux/x11_capture.cpp
        src/platform/linux/x11_input.cpp
        src/platform/linux/x11_window.cpp
    )
endif()
```

### 4.2 运行时切换（可选）

```cpp
// 允许在同一系统上切换不同实现
auto capture = factory.createCapture({
    .preferredBackend = CaptureBackend::DXGI  // 使用 DXGI 而非 GDI+
});
```

### 4.3 技术栈切换示例

```cpp
// Windows: 从 GDI+ 切换到 DXGI
auto capture = factory.createCapture({
    .preferredBackend = CaptureBackend::DXGI
});

// macOS: 从 Core Graphics 切换到 ScreenCaptureKit
auto capture = factory.createCapture({
    .preferredBackend = CaptureBackend::ScreenCaptureKit
});
```

---

## 5. 目录结构

```
lib/wingman/
├── include/wingman/platform/
│   ├── icapture.hpp          # 捕获接口
│   ├── iinput.hpp            # 输入接口
│   ├── iwindow.hpp           # 窗口接口
│   ├── iscreen.hpp           # 屏幕接口
│   ├── platform_types.hpp    # 通用类型定义
│   └── platform_factory.hpp  # 工厂接口
│
├── src/platform/
│   ├── win/
│   │   ├── gdi_capture.cpp           # GDI+ 捕获实现
│   │   ├── dxgi_capture.cpp          # DXGI 捕获实现
│   │   ├── sendinput_input.cpp       # SendInput 实现
│   │   ├── win32_window.cpp          # Win32 窗口实现
│   │   └── win32_screen.cpp          # Win32 屏幕实现
│   │
│   ├── mac/
│   │   ├── cg_capture.cpp            # Core Graphics 捕获
│   │   ├── sc_capture.cpp            # ScreenCaptureKit 捕获
│   │   ├── cgevent_input.cpp         # CGEvent 输入
│   │   ├── cocoa_window.cpp          # Cocoa 窗口
│   │   └── cocoa_screen.cpp          # Cocoa 屏幕
│   │
│   ├── linux/
│   │   ├── x11_capture.cpp           # X11 捕获
│   │   ├── pipewire_capture.cpp      # PipeWire 捕获
│   │   ├── xtest_input.cpp           # XTest 输入
│   │   ├── x11_window.cpp            # X11 窗口
│   │   └── x11_screen.cpp            # X11 屏幕
│   │
│   └── factory.cpp                   # 平台工厂实现
│
└── src/
    ├── screen.cpp          # 使用 IScreen 接口
    ├── window.cpp          # 使用 IWindow 接口
    └── input.cpp           # 使用 IInput 接口
```

---

## 6. 迁移路径

### 6.1 第一阶段：创建接口层

1. 定义 `ICapture`, `IInput`, `IWindow`, `IScreen` 接口
2. 定义平台工厂接口
3. 编写平台无关的业务逻辑层

### 6.2 第二阶段：实现 Windows 平台

1. 将现有 `screen.cpp`, `window.cpp`, `input.cpp` 的 Windows 实现移至 `src/platform/win/`
2. 实现 Windows 平台工厂
3. 更新原有代码使用接口

### 6.3 第三阶段：实现 macOS/Linux

1. 实现 macOS 平台（ScreenCaptureKit, CGEvent, Cocoa）
2. 实现 Linux 平台（X11/PipeWire, XTest）
3. 添加编译条件

---

## 7. 优势总结

| 方面 | 改进前 | 改进后 |
|------|--------|--------|
| **平台耦合** | 每个文件都包含 `#ifdef _WIN32` | 接口统一，平台隔离 |
| **技术栈切换** | 需要修改多处代码 | 只需切换工厂配置 |
| **新增平台** | 散落各处，容易遗漏 | 实现接口即可 |
| **单元测试** | 无法 mock 平台代码 | 可注入 mock 实现 |
| **代码复用** | 平台代码与业务混在一起 | 业务代码完全平台无关 |
