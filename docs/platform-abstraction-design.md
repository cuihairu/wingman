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

> [更新（2026-09-19）：下图为当前实际形态（非最初的设想稿），并预留 android 租户。]
> 纪律约束见 §8。

```
lib/wingman/
├── include/wingman/platform/            # 【公共契约】接口 + 工厂 + 平台无关类型
│   ├── icapture.hpp / iinput.hpp / iwindow.hpp / iscreen.hpp
│   ├── iclipboard.hpp / ifilewatcher.hpp
│   ├── input_factory.hpp / screen_factory.hpp
│   ├── mock_input.hpp                   # 测试租户（本身平台无关）
│   └── platform_types.hpp               # Bitmap/Rect/KeyCode；OS 句柄一律 opaque
│                                        # 平台私有头禁止放入 include/（历史残留的
│                                        #    win/ 子目录已由 P1 收回至 src/platform/win/）
│
├── src/platform/                        # 【薄层租户】每系统一个目录，互不可见
│   ├── win/                             # namespace wingman::platform::win（P1 统一）
│   ├── mac/                             # namespace wingman::platform::mac
│   ├── linux/                           # namespace wingman::platform::linux（宏守卫见 §8）
│   ├── posix/                           # win/mac 共享的 posix 实现（TriggerManager、
│                                         #   UnixSocketChannel；P1/P2 由 unix/ 更名并入）
│   ├── common/                          # 跨平台传输实现（TcpChannel，P2 收回）
│   ├── ipc_factory.cpp / input_factory.cpp
│   │                                    # 跨平台分发器（按平台选实现，P2 收回 ipc_factory）
│   ├── mock/                            # 测试租户
│   └── android/                         # Android 租户：A1 已入住（占位 stub +
│                                        #   CMake ANDROID 分支）；JNI 桥与
│                                        #   IInput/ICapture 真实现 A2 落地，
│                                        #   见 docs/android-agent-design.md §5.5
│
└── src/{vision,script,ipc,rpc,core}/    # 【公共层】平台宏数量必须为 0（§8 守卫）
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

### 6.4 状态与后续阶段（2026-09-19）

第一至第三阶段已完成（接口 + 三平台实现 + CMake 按平台选源）。当前欠账：
公共路径仍有 31 个文件带平台宏（P0 冻结 47 个，见 §8）。

| 阶段 | 内容 | 状态 |
|------|------|------|
| P0 | 薄层纪律成文 + 边界守卫（`scripts/check_platform_boundary.sh`）+ 迁移清单冻结 | ✅ |
| P1 | 命名统一（`platform::windows`→`::win`）、`unix/`→`posix/` 更名、include 侧平台私有头收回、`linux` 宏守卫 | ✅（2026-09-19） |
| P2 | 泄漏销号：ipc 通道（管道/socket 实现搬入 platform/）、capture_source、transport 宏归位 | ✅（2026-09-19） |
| P3 | 接口补缺：security 探测、recorder 钩子各抽小接口 | 按需 |
| P4 | 遗留静态类下线（`screen.cpp`/`window.cpp`/`clipboard.cpp` 等，ADR 已冻结） | 待办 |
| P5 | android 租户接入（A1 目录/构建分支已就位；真实现随移动端 A2） | A1 部分 ✅ |

---

## 7. 优势总结

| 方面 | 改进前 | 改进后 |
|------|--------|--------|
| **平台耦合** | 每个文件都包含 `#ifdef _WIN32` | 接口统一，平台隔离 |
| **技术栈切换** | 需要修改多处代码 | 只需切换工厂配置 |
| **新增平台** | 散落各处，容易遗漏 | 实现接口即可 |
| **单元测试** | 无法 mock 平台代码 | 可注入 mock 实现 |
| **代码复用** | 平台代码与业务混在一起 | 业务代码完全平台无关 |

---

## 8. 薄层纪律与平台边界守卫

> 立规（2026-09-19，P0）。本节是把「平台代码只有一个家」从约定变成可检查约束的定义。

### 8.1 四条纪律

1. **薄层只做翻译**：`src/platform/<os>/` 只负责 OS API ⇄ 接口结构体（`Bitmap`/`Rect`/`KeyCode`）
   的转换，禁止业务逻辑、禁止反向 include 公共实现、禁止策略（重试/缓存/调度等一律在公共层）。
2. **公共层零平台宏**：`_WIN32`/`__APPLE__`/`__linux__`/`__ANDROID__`/`_MSC_VER` 等条件编译
   只允许出现在 `src/platform/`。公共层新增平台分支即违规。
3. **平台私有头不进 `include/`**：只有公共契约放 `include/wingman/platform/`；每个 OS 的实现头
   留在 `src/platform/<os>/` 内部（历史残留的 `include/wingman/platform/win/` 由 P1 收回）。
4. **新增平台 = 新增一个租户目录**：实现既有接口即可，公共层零改动（android 将是第一个验证
   本纪律的新租户）。

### 8.2 namespace 规范

- 接口与工厂：`wingman::platform`；实现：`wingman::platform::<os>`（`win` / `mac` / `linux` /
  `posix` / `mock` / `android`）。
- `platform::windows` 与 `platform::win` 曾两套拼写并存，P1 已统一为 `win`。
- `posix/` 目录（P1 由 `unix/` 更名）存放 win/mac 共享的 posix 实现；其中只补公共类成员函数
  的文件（如 `posix_trigger.cpp` 实现 `TriggerManager`）用 plain `namespace wingman`，
  不新定义类型、不算租户 namespace 例外。
- [`linux`（及 `unix`）是 GCC/Clang 非 strict 模式（`gnu++*`）下的预定义宏，会把]
  `namespace linux` 预处理成非法的 `::1`。约束已双重落地（P1）：
  1. 根 CMake 与 lib CMake 均 `set(CMAKE_CXX_EXTENSIONS OFF)`，全仓 strict `-std=c++23`；
  2. `src/platform/linux/strict_std.hpp` 编译守卫（`#ifdef linux #error`），在
     `x11_factory.cpp`（linux 租户 unity 中枢）最先 include，防止绕过 CMake 直接编译时
     静默踩坑。

### 8.3 边界守卫

```bash
scripts/check_platform_boundary.sh   # 无依赖，CI 首个 job 运行（Platform Boundary Guard）
```

- 扫描 C++ 生产代码（`lib/wingman`、`libs/`、`apps/`，排除 `lib/wingman/src/platform/`、
  `libs/transport/src/platform/`（§8.4）与全部 `tests/`），命中平台宏且不在迁移清单中的
  文件即失败并列出位置。
- 迁移清单 `scripts/platform_boundary_allowlist.txt`（P0 冻结时的 47 个历史欠账文件）**只减不增**：
  每完成一处迁移删除一行并跑守卫验证；新增文件入清单须经维护者批准并在 PR 中说明理由。
  P1 收回 2 行、P2 收回 14 行（ipc 6 + capture_source 2 + transport 6），现存 31 行。
- 欠账清零后，本守卫退化为纯红线检查（清单为空、只拦新增违规）。

### 8.4 独立库薄层

可独立移植的库（当前仅 `libs/transport`，需随 Android Agent 交叉编译）**自带**薄层目录，
不依赖 `lib/wingman/src/platform/`：

- `libs/transport/src/platform/socket_compat.hpp`：统一 Winsock / POSIX 的头卫生
  （WIN32_LEAN_AND_MEAN/NOMINMAX/_WIN32_WINNT、`Get`/`min`/`max` undef）、句柄类型
  （`SocketType`）、值宏（`INVALID_SOCKET_VALUE`/`SOCKET_ERROR_VALUE`）与原语
  （`ensureWinsock`/`closeSocketCompat`/`sendCompat`/`recvCompat`/`lastSocketError`/
  `isWouldBlock`/`setTcpKeepAlive`）。
- 公共头经 `#include "platform/socket_compat.hpp"` 引用；transport 的 CMake 以
  `$<BUILD_INTERFACE>` 暴露 `src/`，安装/导出接口不外泄内部树。
- 守卫（§8.3）将 `libs/transport/src/platform/` 与 `lib/wingman/src/platform/`
  同等豁免。后续新增独立可移植库时比照办理。
