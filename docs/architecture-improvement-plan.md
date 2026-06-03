# Wingman 架构改进方案
# 参考 scrcpy 架构设计

> **版本**: v1.0
> **日期**: 2026-05-13
> **状态**: 设计草案

---

## 目录

1. [概述](#1-概述)
2. [当前架构分析](#2-当前架构分析)
3. [三流分离架构](#3-三流分离架构)
4. [组件职责重构](#4-组件职责重构)
5. [资源生命周期管理](#5-资源生命周期管理)
6. [事件驱动架构](#6-事件驱动架构)
7. [捕获源抽象](#7-捕获源抽象)
8. [网络协议优化](#8-网络协议优化)
9. [实施路线图](#9-实施路线图)

---

## 1. 概述

### 1.1 设计目标

参考 scrcpy 的成熟架构，对 wingman 进行以下改进：

| 目标 | 说明 | 优先级 |
|------|------|--------|
| **三流分离** | 控制、屏幕、事件三流独立 | P0 |
| **职责清晰** | 组件单一职责，边界明确 | P0 |
| **资源管理** | 统一的生命周期管理 | P1 |
| **事件驱动** | 异步事件总线替代轮询 | P1 |
| **可扩展性** | 抽象接口便于扩展 | P2 |

### 1.2 架构对比

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            scrcpy 架构                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│    Desktop Client (C)                    Android Server (Java)              │
│    ┌─────────────────────────┐           ┌─────────────────────────┐        │
│    │      Screen (SDL)       │           │     ScreenCapture       │        │
│    │      Demuxer            │◄──────────┤     Encoder (MediaCodec)│        │
│    │      Decoder (FFmpeg)   │           │     Streamer            │        │
│    └─────────────────────────┘           └─────────────────────────┘        │
│                                                                             │
│    ┌─────────────────────────┐           ┌─────────────────────────┐        │
│    │   InputManager (SDL)    │──────────►│     Controller          │        │
│    │   Controller            │           │     (Input Injection)    │        │
│    └─────────────────────────┘           └─────────────────────────┘        │
│                                                                             │
│                      video socket │ audio socket │ control socket           │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                          wingman 当前架构                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│    apps/runtime                                                              │
│    ┌─────────────────────────┐                                             │
│    │ StandaloneMode          │                                             │
│    │ ActiveMode (TcpClient)  │                                             │
│    │ RemoteClient (TcpClient)│                                             │
│    └─────────────────────────┘                                             │
│               │                                                               │
│               ▼                                                               │
│    lib/wingman                                                               │
│    ┌───────────┬───────────┬───────────┬───────────┐                       │
│    │  Screen   │   Input   │  Trigger  │    Lua    │                       │
│    │  (GDI+)   │(SendInput)│  Engine   │ Bindings  │                       │
│    └───────────┴───────────┴───────────┴───────────┘                       │
│               │                                                               │
│               ▼                                                               │
│    libs/transport                                                             │
│    ┌─────────────────────────────────────────────────┐                     │
│    │  Transport (TCP/WebSocket)                      │                     │
│    │  Session │ Channel │ Message │ Handler          │                     │
│    └─────────────────────────────────────────────────┘                     │
│                                                                             │
│    单一 Socket 连接，所有消息通过 Channel 多路复用                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 当前架构分析

### 2.1 优点

| 方面 | 说明 |
|------|------|
| **模块化** | apps + lib + libs 分层清晰 |
| **可测试** | 每个模块有独立测试 |
| **多模式** | Active/Passive/Standalone 三种运行模式 |
| **Transport 抽象** | 支持 TCP/WebSocket 切换 |
| **Lua 集成** | 脚本能力完善 |

### 2.2 待改进点

| 问题 | 影响 | 改进方向 |
|------|------|----------|
| 单一 Socket | 大数据（屏幕）阻塞控制指令 | 三流分离 |
| 职责混合 | Screen 类混合捕获和查找 | 拆分职责 |
| 轮询模式 | 性能开销，响应延迟 | 事件驱动 |
| 资源管理 | 缺少统一的生命周期 | RAII + 接口 |
| 扩展性 | 硬编码 Windows GDI+ | 抽象捕获源 |

---

## 3. 三流分离架构

### 3.1 设计理念

scrcpy 使用三个独立的 Socket 连接，各自优化：

```
                    ┌─────────────────┐
                    │   wingman       │
                    │   (Client/      │
                    │    Server)      │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│ Control Stream │   │ Screen Stream  │   │ Event Stream  │
│               │   │               │   │               │
│ • 低延迟      │   │ • 高带宽      │   │ • 低频        │
│ • 小包        │   │ • 批量传输    │   │ • 断线重连    │
│ • TCP_NODELAY │   │ • TCP_CORK    │   │ • keep-alive  │
│ • 命令/响应   │   │ • 视频流      │   │ • 通知推送    │
└───────────────┘   └───────────────┘   └───────────────┘
```

### 3.2 StreamType 定义

```cpp
// file: libs/transport/include/wingman/transport/stream_type.hpp

#pragma once

namespace wingman::transport {

enum class StreamType : uint8_t {
    CONTROL = 0,    // 控制流 - 命令、响应、配置
    SCREEN  = 1,    // 屏幕流 - 截图、帧数据
    EVENT   = 2,    // 事件流 - 触发器、通知、日志
};

// 流参数配置
struct StreamParams {
    StreamType type;
    bool tcpNoDelay = false;      // 禁用 Nagle 算法
    bool tcpCork = false;         // 批量传输
    bool keepAlive = false;       // 保持连接
    int sendBufferSize = 0;       // 发送缓冲区大小
    int recvBufferSize = 0;       // 接收缓冲区大小
    int keepAliveIdle = 0;        // keep-alive 空闲时间
    int keepAliveInterval = 0;    // keep-alive 间隔
    int keepAliveCount = 0;       // keep-alive 重试次数

    // 获取默认配置
    static StreamParams getDefault(StreamType type) {
        switch (type) {
            case StreamType::CONTROL:
                return {
                    .type = type,
                    .tcpNoDelay = true,       // 低延迟优先
                    .keepAlive = true,
                    .keepAliveIdle = 30,
                    .keepAliveInterval = 5,
                    .keepAliveCount = 3
                };
            case StreamType::SCREEN:
                return {
                    .type = type,
                    .tcpCork = true,          // 批量传输
                    .sendBufferSize = 256 * 1024,  // 256KB
                    .recvBufferSize = 256 * 1024
                };
            case StreamType::EVENT:
                return {
                    .type = type,
                    .tcpNoDelay = true,
                    .keepAlive = true,
                    .keepAliveIdle = 60,
                    .keepAliveInterval = 10,
                    .keepAliveCount = 3
                };
        }
        return {};
    }
};

} // namespace wingman::transport
```

### 3.3 StreamChannel 实现

```cpp
// file: libs/transport/include/wingman/transport/stream_channel.hpp

#pragma once

#include "wingman/transport/stream_type.hpp"
#include <functional>
#include <memory>
#include <system_error>

namespace wingman::transport {

// 前向声明
class SocketHandle;

// 数据接收回调
using StreamDataCallback = std::function<void(const uint8_t* data, size_t size)>;

// 错误回调
using StreamErrorCallback = std::function<void(const std::error_code& ec)>;

// 流通道状态
enum class StreamState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief 单向流通道
 *
 * 每个流类型使用独立的 Socket 连接，针对该流的特性进行优化。
 */
class StreamChannel {
public:
    StreamChannel(StreamType type, const StreamParams& params);
    ~StreamChannel();

    // 禁止拷贝
    StreamChannel(const StreamChannel&) = delete;
    StreamChannel& operator=(const StreamChannel&) = delete;

    /**
     * @brief 连接到远程端点
     */
    bool connect(const std::string& host, int port);

    /**
     * @brief 绑定并监听（服务端模式）
     */
    bool listen(const std::string& host, int port);

    /**
     * @brief 接受连接（服务端模式）
     */
    std::unique_ptr<StreamChannel> accept();

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发送数据
     */
    bool send(const uint8_t* data, size_t size);
    bool send(const std::vector<uint8_t>& data);

    /**
     * @brief 开始接收数据（异步）
     */
    void startReceiving(StreamDataCallback dataCallback,
                       StreamErrorCallback errorCallback = nullptr);

    /**
     * @brief 停止接收
     */
    void stopReceiving();

    // 获取器
    StreamType getType() const { return type_; }
    StreamState getState() const { return state_; }
    const std::string& getRemoteEndpoint() const { return remoteEndpoint_; }
    int getLocalPort() const { return localPort_; }

    // 设置器
    void setDataCallback(StreamDataCallback cb) { dataCallback_ = std::move(cb); }
    void setErrorCallback(StreamErrorCallback cb) { errorCallback_ = std::move(cb); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    StreamType type_;
    StreamParams params_;
    StreamState state_ = StreamState::Disconnected;
    std::string remoteEndpoint_;
    int localPort_ = 0;

    StreamDataCallback dataCallback_;
    StreamErrorCallback errorCallback_;

    void applySocketOptions();
    void setState(StreamState state);
};

/**
 * @brief 流通道对（双向通信）
 *
 * 客户端使用：一个 StreamChannelPair 包含请求和响应通道
 */
class StreamChannelPair {
public:
    StreamChannelPair(const StreamParams& requestParams,
                     const StreamParams& responseParams);

    // 连接到远程
    bool connect(const std::string& host, int requestPort, int responsePort);

    // 断开连接
    void disconnect();

    // 获取通道
    StreamChannel* getRequestChannel() { return requestChannel_.get(); }
    StreamChannel* getResponseChannel() { return responseChannel_.get(); }

private:
    std::unique_ptr<StreamChannel> requestChannel_;
    std::unique_ptr<StreamChannel> responseChannel_;
};

} // namespace wingman::transport
```

### 3.4 StreamManager 实现

```cpp
// file: libs/transport/include/wingman/transport/stream_manager.hpp

#pragma once

#include "wingman/transport/stream_channel.hpp"
#include "wingman/transport/stream_type.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace wingman::transport {

/**
 * @brief 管理所有流通道的生命周期
 *
 * 负责创建、销毁和查找流通道。
 */
class StreamManager {
public:
    StreamManager();
    ~StreamManager();

    /**
     * @brief 创建流通道
     */
    std::shared_ptr<StreamChannel> createStream(
        StreamType type,
        const std::string& name,
        const StreamParams& params = StreamParams::getDefault(type)
    );

    /**
     * @brief 获取流通道
     */
    std::shared_ptr<StreamChannel> getStream(const std::string& name) const;

    /**
     * @brief 移除流通道
     */
    void removeStream(const std::string& name);

    /**
     * @brief 获取指定类型的所有流
     */
    std::vector<std::shared_ptr<StreamChannel>> getStreamsByType(StreamType type) const;

    /**
     * @brief 断开所有流
     */
    void disconnectAll();

    /**
     * @brief 获取流数量
     */
    size_t getStreamCount() const;

    // 单例访问（可选）
    static StreamManager& instance();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<StreamChannel>> streams_;

    std::string generateStreamName(StreamType type);
};

} // namespace wingman::transport
```

### 3.5 使用示例

```cpp
// 客户端：创建三条独立流
auto manager = &StreamManager::instance();

// 控制流 - 用于发送命令
auto controlStream = manager->createStream(
    StreamType::CONTROL,
    "control",
    StreamParams::getDefault(StreamType::CONTROL)
);
controlStream->connect("192.168.1.100", 9001);

// 屏幕流 - 用于接收屏幕数据
auto screenStream = manager->createStream(
    StreamType::SCREEN,
    "screen",
    StreamParams::getDefault(StreamType::SCREEN)
);
screenStream->connect("192.168.1.100", 9002);
screenStream->startReceiving([](const uint8_t* data, size_t size) {
    // 处理屏幕数据
    onScreenDataReceived(data, size);
});

// 事件流 - 用于接收事件通知
auto eventStream = manager->createStream(
    StreamType::EVENT,
    "event",
    StreamParams::getDefault(StreamType::EVENT)
);
eventStream->connect("192.168.1.100", 9003);
eventStream->startReceiving([](const uint8_t* data, size_t size) {
    // 处理事件数据
    onEventDataReceived(data, size);
});
```

---

## 4. 组件职责重构

### 4.1 当前问题分析

```cpp
// 当前 Screen 类职责过重
class Screen {
    // 职责1: 屏幕捕获
    static Bitmap capture(const Rect& region);
    static Color getPixel(int x, int y);

    // 职责2: 颜色查找（应该独立）
    static bool findColor(const Color& color, const Rect& region, ...);
    static std::vector<Point> findColors(...);

    // 职责3: 图像匹配（应该独立）
    static bool findImage(const std::string& imagePath, ...);

    // 职责4: 屏幕信息（应该独立）
    static int getScreenWidth();
    static Rect getScreenBounds();
};
```

### 4.2 重构方案

#### 4.2.1 捕获源抽象

```cpp
// file: lib/wingman/include/wingman/capture/capture_source.hpp

#pragma once

#include "wingman/screen.hpp"
#include <memory>
#include <vector>

namespace wingman::capture {

/**
 * @brief 捕获源抽象接口
 *
 * 支持不同的捕获源：屏幕、窗口、相机等
 */
class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;

    /**
     * @brief 捕获画面
     */
    virtual std::unique_ptr<Bitmap> capture(const Rect& region) = 0;

    /**
     * @brief 获取边界
     */
    virtual Rect getBounds() const = 0;

    /**
     * @brief 检查可用性
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief 获取名称
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief 屏幕捕获源
 */
class ScreenCaptureSource : public ICaptureSource {
public:
    ScreenCaptureSource(int monitorIndex = 0);

    std::unique_ptr<Bitmap> capture(const Rect& region) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

private:
    int monitorIndex_;
    Rect bounds_;
};

/**
 * @brief 窗口捕获源
 */
class WindowCaptureSource : public ICaptureSource {
public:
    explicit WindowCaptureSource(HWND hwnd);

    std::unique_ptr<Bitmap> capture(const Rect& region) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    HWND getHwnd() const { return hwnd_; }

private:
    HWND hwnd_;
    std::string windowTitle_;
};

/**
 * @brief 捕获源管理器
 */
class CaptureSourceManager {
public:
    static CaptureSourceManager& instance();

    /**
     * @brief 注册捕获源
     */
    void registerSource(std::shared_ptr<ICaptureSource> source);

    /**
     * @brief 获取默认屏幕捕获源
     */
    std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

    /**
     * @brief 按名称获取捕获源
     */
    std::shared_ptr<ICaptureSource> getSource(const std::string& name);

    /**
     * @brief 列出所有捕获源
     */
    std::vector<std::shared_ptr<ICaptureSource>> listSources();

private:
    std::unordered_map<std::string, std::shared_ptr<ICaptureSource>> sources_;
    std::mutex mutex_;
};

} // namespace wingman::capture
```

#### 4.2.2 图像分析器

```cpp
// file: lib/wingman/include/wingman/vision/image_analyzer.hpp

#pragma once

#include "wingman/screen.hpp"
#include "wingman/capture/capture_source.hpp"
#include <functional>
#include <vector>

namespace wingman::vision {

/**
 * @brief 颜色匹配结果
 */
struct ColorMatch {
    Point position;
    Color foundColor;
    int distance;  // 与目标颜色的距离
};

/**
 * @brief 图像匹配结果
 */
struct ImageMatch {
    Point position;
    double confidence;
    Rect matchedRegion;
};

/**
 * @brief 图像分析器
 *
 * 职责：从位图中查找颜色、图像等
 */
class ImageAnalyzer {
public:
    /**
     * @brief 查找单个颜色点
     *
     * @param bitmap 要分析的位图
     * @param target 目标颜色
     * @param region 搜索区域
     * @param tolerance 容差
     * @return 找到的点
     */
    std::optional<ColorMatch> findColor(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance
    );

    /**
     * @brief 查找所有颜色点
     */
    std::vector<ColorMatch> findColors(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance,
        int maxCount = 0
    );

    /**
     * @brief 查找图像
     *
     * @param bitmap 要搜索的位图
     * @param templatePath 模板图像路径
     * @param region 搜索区域
     * @param threshold 匹配阈值 (0.0 - 1.0)
     * @return 匹配结果
     */
    std::optional<ImageMatch> findImage(
        const Bitmap& bitmap,
        const std::string& templatePath,
        const Rect& region,
        double threshold = 0.8
    );

    /**
     * @brief 从捕获源查找颜色（便捷方法）
     */
    std::optional<ColorMatch> findColorFromSource(
        std::shared_ptr<capture::ICaptureSource> source,
        const Color& target,
        const Rect& region,
        int tolerance
    );

    /**
     * @brief 异步查找颜色
     */
    void findColorAsync(
        const Bitmap& bitmap,
        const Color& target,
        const Rect& region,
        int tolerance,
        std::function<void(std::optional<ColorMatch>)> callback
    );

private:
    // 内部实现
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman::vision
```

#### 4.2.3 屏幕管理器（重组）

```cpp
// file: lib/wingman/include/wingman/screen_manager.hpp

#pragma once

#include "wingman/capture/capture_source.hpp"
#include "wingman/vision/image_analyzer.hpp"
#include <memory>

namespace wingman {

/**
 * @brief 屏幕管理器
 *
 * 职责：协调捕获源和分析器，提供高层 API
 */
class ScreenManager {
public:
    ScreenManager();
    ~ScreenManager();

    /**
     * @brief 捕获屏幕
     */
    std::unique_ptr<Bitmap> capture(const Rect& region = {});

    /**
     * @brief 获取像素
     */
    Color getPixel(int x, int y);

    /**
     * @brief 查找颜色
     */
    std::optional<Point> findColor(
        const Color& color,
        const Rect& region,
        int tolerance
    );

    /**
     * @brief 查找图像
     */
    std::optional<Point> findImage(
        const std::string& imagePath,
        const Rect& region,
        double threshold
    );

    /**
     * @brief 获取屏幕边界
     */
    Rect getBounds() const;

    /**
     * @brief 设置捕获源
     */
    void setCaptureSource(std::shared_ptr<capture::ICaptureSource> source);

    /**
     * @brief 获取捕获源
     */
    std::shared_ptr<capture::ICaptureSource> getCaptureSource() const;

private:
    std::shared_ptr<capture::ICaptureSource> captureSource_;
    std::unique_ptr<vision::ImageAnalyzer> analyzer_;
};

// 保持向后兼容：静态 API 委托给单例
class Screen {
public:
    static std::unique_ptr<Bitmap> capture(const Rect& region = {});
    static Color getPixel(int x, int y);
    static bool findColor(const Color& color, const Rect& region,
                         int tolerance, Point& result);
    static std::vector<Point> findColors(const Color& color, const Rect& region,
                                         int tolerance, int maxCount = 0);
    static bool findImage(const std::string& imagePath, const Rect& region,
                          double threshold, Point& result);
    static Rect getScreenBounds();
};

} // namespace wingman
```

### 4.3 职责划分对比

| 之前 | 之后 | 职责 |
|------|------|------|
| `Screen::capture()` | `ScreenCaptureSource::capture()` | 屏幕捕获 |
| `Screen::findColor()` | `ImageAnalyzer::findColor()` | 图像分析 |
| `Screen::findImage()` | `ImageAnalyzer::findImage()` | 图像分析 |
| `Screen::getScreenWidth()` | `ScreenCaptureSource::getBounds()` | 捕获源信息 |

---

## 5. 资源生命周期管理

### 5.1 IComponent 接口

```cpp
// file: lib/wingman/include/wingman/core/component.hpp

#pragma once

#include <string>
#include <stdexcept>

namespace wingman::core {

/**
 * @brief 组件状态
 */
enum class ComponentState : uint8_t {
    Uninitialized,  // 未初始化
    Initializing,   // 初始化中
    Ready,          // 就绪
    Running,        // 运行中
    Paused,         // 暂停
    Stopping,       // 停止中
    Stopped,        // 已停止
    Error           // 错误状态
};

/**
 * @brief 组件异常
 */
class ComponentException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief 组件接口
 *
 * 所有需要生命周期管理的组件都应该实现此接口。
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    /**
     * @brief 初始化组件
     *
     * @return true 成功
     * @return false 失败
     * @throws ComponentException 初始化失败
     */
    virtual bool initialize() = 0;

    /**
     * @brief 启动组件
     */
    virtual bool start() = 0;

    /**
     * @brief 暂停组件
     */
    virtual void pause() = 0;

    /**
     * @brief 恢复组件
     */
    virtual void resume() = 0;

    /**
     * @brief 停止组件
     */
    virtual void stop() = 0;

    /**
     * @brief 关闭组件，释放资源
     */
    virtual void shutdown() = 0;

    /**
     * @brief 获取组件状态
     */
    virtual ComponentState getState() const = 0;

    /**
     * @brief 检查组件是否就绪
     */
    bool isReady() const {
        auto s = getState();
        return s == ComponentState::Ready || s == ComponentState::Running;
    }

    /**
     * @brief 检查组件是否运行中
     */
    bool isRunning() const {
        return getState() == ComponentState::Running;
    }

    /**
     * @brief 获取组件名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取组件版本
     */
    virtual std::string getVersion() const { return "1.0.0"; }

protected:
    IComponent() = default;
};

/**
 * @brief 组件基类
 *
 * 提供默认的状态管理实现
 */
class ComponentBase : public IComponent {
public:
    ~ComponentBase() override { shutdown(); }

    bool initialize() override;
    bool start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    void shutdown() override;

    ComponentState getState() const override { return state_; }
    std::string getName() const override { return componentName_; }

    /**
     * @brief 设置组件名称
     */
    void setName(const std::string& name) { componentName_ = name; }

protected:
    /**
     * @brief 子类实现：初始化逻辑
     */
    virtual bool onInitialize() { return true; }

    /**
     * @brief 子类实现：启动逻辑
     */
    virtual bool onStart() { return true; }

    /**
     * @brief 子类实现：暂停逻辑
     */
    virtual void onPause() {}

    /**
     * @brief 子类实现：恢复逻辑
     */
    virtual void onResume() {}

    /**
     * @brief 子类实现：停止逻辑
     */
    virtual void onStop() {}

    /**
     * @brief 子类实现：关闭逻辑
     */
    virtual void onShutdown() {}

    void setState(ComponentState state);

private:
    ComponentState state_ = ComponentState::Uninitialized;
    std::string componentName_;
};

} // namespace wingman::core
```

### 5.2 ComponentImpl 实现

```cpp
// file: lib/wingman/src/core/component.cpp

#include "wingman/core/component.hpp"

namespace wingman::core {

bool ComponentBase::initialize() {
    if (state_ != ComponentState::Uninitialized && state_ != ComponentState::Stopped) {
        return false;
    }

    setState(ComponentState::Initializing);

    if (onInitialize()) {
        setState(ComponentState::Ready);
        return true;
    }

    setState(ComponentState::Error);
    return false;
}

bool ComponentBase::start() {
    if (!isReady()) {
        return false;
    }

    setState(ComponentState::Running);

    if (onStart()) {
        return true;
    }

    setState(ComponentState::Error);
    return false;
}

void ComponentBase::pause() {
    if (state_ != ComponentState::Running) return;

    onPause();
    setState(ComponentState::Paused);
}

void ComponentBase::resume() {
    if (state_ != ComponentState::Paused) return;

    onResume();
    setState(ComponentState::Running);
}

void ComponentBase::stop() {
    if (state_ != ComponentState::Running && state_ != ComponentState::Paused) {
        return;
    }

    setState(ComponentState::Stopping);
    onStop();
    setState(ComponentState::Stopped);
}

void ComponentBase::shutdown() {
    if (state_ == ComponentState::Uninitialized ||
        state_ == ComponentState::Stopped) {
        return;
    }

    if (state_ == ComponentState::Running || state_ == ComponentState::Paused) {
        stop();
    }

    onShutdown();
    setState(ComponentState::Stopped);
}

void ComponentBase::setState(ComponentState state) {
    state_ = state;
}

} // namespace wingman::core
```

### 5.3 应用示例：ScreenCaptureSource

```cpp
// file: lib/wingman/src/capture/screen_capture_source.cpp

#include "wingman/capture/capture_source.hpp"
#include "wingman/core/component.hpp"

namespace wingman::capture {

class ScreenCaptureSourceImpl : public core::ComponentBase {
public:
    ScreenCaptureSourceImpl(int monitorIndex)
        : monitorIndex_(monitorIndex) {
        setName("ScreenCaptureSource");
    }

    ~ScreenCaptureSourceImpl() override {
        shutdown();
    }

    std::unique_ptr<Bitmap> capture(const Rect& region) {
        if (!isRunning()) {
            throw ComponentException("ScreenCaptureSource not running");
        }

        // ... 捕获逻辑
        return {};
    }

    Rect getBounds() const { return bounds_; }

protected:
    bool onInitialize() override {
        // 获取显示器信息
        DISPLAY_INFO info;
        if (!GetDisplayInfo(monitorIndex_, &info)) {
            return false;
        }
        bounds_ = {info.x, info.y, info.width, info.height};
        return true;
    }

    bool onStart() override {
        // 创建 DC、位图等资源
        hdc_ = GetDC(nullptr);
        if (!hdc_) return false;
        // ...
        return true;
    }

    void onStop() override {
        // 释放资源
        if (hdc_) {
            ReleaseDC(nullptr, hdc_);
            hdc_ = nullptr;
        }
    }

private:
    int monitorIndex_;
    Rect bounds_;
    HDC hdc_ = nullptr;
    // ... 其他资源
};

} // namespace wingman::capture
```

---

## 6. 事件驱动架构

### 6.1 事件总线设计

```cpp
// file: lib/wingman/include/wingman/core/event_bus.hpp

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <condition_variable>

namespace wingman::core {

/**
 * @brief 事件类型标识符
 */
using EventType = uint32_t;

/**
 * @brief 事件优先级
 */
enum class EventPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief 事件基类
 */
class Event {
public:
    virtual ~Event() = default;

    EventType getType() const { return type_; }
    EventPriority getPriority() const { return priority_; }
    uint64_t getTimestamp() const { return timestamp_; }
    uint64_t getSequenceId() const { return sequenceId_; }

    void setPriority(EventPriority priority) { priority_ = priority; }

protected:
    Event(EventType type, EventPriority priority = EventPriority::Normal)
        : type_(type)
        , priority_(priority)
        , timestamp_(getCurrentTimestamp())
        , sequenceId_(nextSequenceId()++) {}

private:
    EventType type_;
    EventPriority priority_;
    uint64_t timestamp_;
    uint64_t sequenceId_;

    static uint64_t getCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
    }

    static uint64_t& nextSequenceId() {
        static uint64_t id = 0;
        return id;
    }
};

/**
 * @brief 事件回调函数
 */
template<typename T>
using EventCallback = std::function<void(const T&)>;

using GenericEventCallback = std::function<void(const Event&)>;

/**
 * @brief 事件订阅信息
 */
struct EventSubscription {
    GenericEventCallback callback;
    std::string subscriberName;
    bool once = false;  // 是否只触发一次
    EventPriority minPriority = EventPriority::Low;  // 最低优先级过滤
};

/**
 * @brief 事件总线
 *
 * 负责事件的发布、订阅和分发。
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    /**
     * @brief 订阅事件
     */
    template<typename T>
    uint64_t subscribe(EventCallback<T> callback,
                      const std::string& subscriberName = "",
                      bool once = false) {
        auto genericCallback = [callback](const Event& event) {
            callback(static_cast<const T&>(event));
        };

        return subscribeInternal(T::StaticType, genericCallback, subscriberName, once);
    }

    /**
     * @brief 取消订阅
     */
    void unsubscribe(uint64_t subscriptionId);
    void unsubscribe(const std::string& subscriberName);

    /**
     * @brief 发布事件（同步）
     */
    void publish(const Event& event);

    /**
     * @brief 发布事件（异步，加入队列）
     */
    void publishAsync(std::unique_ptr<Event> event);

    /**
     * @brief 启动事件分发线程
     */
    void start();

    /**
     * @brief 停止事件分发
     */
    void stop();

    /**
     * @brief 处理所有待处理事件（同步模式）
     */
    void processEvents();

    /**
     * @brief 获取队列大小
     */
    size_t getQueueSize() const;

private:
    uint64_t subscribeInternal(EventType type,
                               GenericEventCallback callback,
                               const std::string& subscriberName,
                               bool once);

    struct QueuedEvent {
        std::unique_ptr<Event> event;
        uint64_t timestamp;
    };

    std::mutex mutex_;
    std::unordered_map<EventType, std::unordered_map<uint64_t, EventSubscription>> subscriptions_;
    std::unordered_map<uint64_t, EventType> subscriptionIdToType_;
    uint64_t nextSubscriptionId_ = 1;

    // 异步分发
    std::queue<QueuedEvent> eventQueue_;
    std::condition_variable queueCondition_;
    std::thread dispatchThread_;
    std::atomic<bool> running_{false};

    void dispatchLoop();
    void dispatchEvent(const Event& event);
};

} // namespace wingman::core
```

### 6.2 预定义事件类型

```cpp
// file: lib/wingman/include/wingman/core/events.hpp

#pragma once

#include "wingman/core/event_bus.hpp"
#include "wingman/screen.hpp"
#include <string>

namespace wingman::core {

// 事件类型定义
namespace EventTypes {
    constexpr EventType ScreenCaptured = 1;
    constexpr EventType ColorFound = 2;
    constexpr EventType ImageFound = 3;
    constexpr EventType TriggerFired = 4;
    constexpr EventType InputReceived = 5;
    constexpr EventType ScriptCompleted = 6;
    constexpr EventType ErrorOccurred = 7;
    constexpr EventType ConnectionStateChanged = 8;
}

/**
 * @brief 屏幕捕获事件
 */
class ScreenCapturedEvent : public Event {
public:
    static constexpr EventType StaticType = EventTypes::ScreenCaptured;

    ScreenCapturedEvent(std::unique_ptr<Bitmap> bitmap, const Rect& region)
        : Event(StaticType)
        , bitmap_(std::move(bitmap))
        , region_(region) {}

    const Bitmap& getBitmap() const { return *bitmap_; }
    const Rect& getRegion() const { return region_; }

private:
    std::unique_ptr<Bitmap> bitmap_;
    Rect region_;
};

/**
 * @brief 颜色找到事件
 */
class ColorFoundEvent : public Event {
public:
    static constexpr EventType StaticType = EventTypes::ColorFound;

    ColorFoundEvent(Color target, Point position, int distance)
        : Event(StaticType, EventPriority::Normal)
        , target_(target)
        , position_(position)
        , distance_(distance) {}

    Color getTarget() const { return target_; }
    Point getPosition() const { return position_; }
    int getDistance() const { return distance_; }

private:
    Color target_;
    Point position_;
    int distance_;
};

/**
 * @brief 触发器触发事件
 */
class TriggerFiredEvent : public Event {
public:
    static constexpr EventType StaticType = EventTypes::TriggerFired;

    TriggerFiredEvent(const std::string& triggerId,
                     const std::string& triggerName)
        : Event(StaticType, EventPriority::High)
        , triggerId_(triggerId)
        , triggerName_(triggerName) {}

    const std::string& getTriggerId() const { return triggerId_; }
    const std::string& getTriggerName() const { return triggerName_; }

private:
    std::string triggerId_;
    std::string triggerName_;
};

/**
 * @brief 错误事件
 */
class ErrorEvent : public Event {
public:
    static constexpr EventType StaticType = EventTypes::ErrorOccurred;

    ErrorEvent(const std::string& source,
              const std::string& message,
              int errorCode = 0)
        : Event(StaticType, EventPriority::Critical)
        , source_(source)
        , message_(message)
        , errorCode_(errorCode) {}

    const std::string& getSource() const { return source_; }
    const std::string& getMessage() const { return message_; }
    int getErrorCode() const { return errorCode_; }

private:
    std::string source_;
    std::string message_;
    int errorCode_;
};

/**
 * @brief 连接状态变化事件
 */
class ConnectionStateChangedEvent : public Event {
public:
    static constexpr EventType StaticType = EventTypes::ConnectionStateChanged;

    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting,
        Error
    };

    ConnectionStateChangedEvent(const std::string& endpoint, State state)
        : Event(StaticType, EventPriority::High)
        , endpoint_(endpoint)
        , state_(state) {}

    const std::string& getEndpoint() const { return endpoint_; }
    State getState() const { return state_; }

private:
    std::string endpoint_;
    State state_;
};

} // namespace wingman::core
```

### 6.3 使用示例

```cpp
// 订阅事件
auto bus = std::make_shared<EventBus>();
bus->start();

// 订阅屏幕捕获事件
bus->subscribe<ScreenCapturedEvent>([](const ScreenCapturedEvent& e) {
    const auto& bitmap = e.getBitmap();
    // 处理捕获的屏幕
}, "ScreenHandler");

// 订阅颜色找到事件
bus->subscribe<ColorFoundEvent>([](const ColorFoundEvent& e) {
    auto pos = e.getPosition();
    // 在找到的位置执行操作
    input->mouseClick(MouseButton::Left);
}, "ClickHandler");

// 发布事件
auto bitmap = Screen::capture();
bus->publish(ScreenCapturedEvent(std::move(bitmap), {0, 0, 1920, 1080}));

// 异步发布
bus->publishAsync(std::make_unique<ColorFoundEvent>(
    Color::fromRGB(0xFF0000),
    Point{100, 200},
    0
));

// 停止事件总线
bus->stop();
```

---

## 7. 捕获源抽象

已在第 4.2 节详细说明。

### 7.1 扩展示例：相机捕获源

```cpp
// file: lib/wingman/include/wingman/capture/camera_capture_source.hpp

#pragma once

#include "wingman/capture/capture_source.hpp"

namespace wingman::capture {

/**
 * @brief 相机捕获源
 *
 * 支持 USB 摄像头、虚拟摄像头等
 */
class CameraCaptureSource : public ICaptureSource {
public:
    explicit CameraCaptureSource(int cameraIndex = 0);

    ~CameraCaptureSource() override;

    std::unique_ptr<Bitmap> capture(const Rect& region) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    /**
     * @brief 设置分辨率
     */
    bool setResolution(int width, int height);

    /**
     * @brief 设置帧率
     */
    bool setFrameRate(int fps);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman::capture
```

---

## 8. 网络协议优化

### 8.1 设计理念

由于已经采用三流分离架构，每个流只处理一种类型的数据，协议可以大幅简化：

| 流类型 | 数据内容 | 协议格式 |
|--------|----------|----------|
| Control Stream | JSON/Protobuf 消息 | `[长度] [消息]` |
| Screen Stream | 编码后的画面数据 | `[长度] [数据]` |
| Event Stream | JSON 事件通知 | `[长度] [消息]` |

**不需要 MAGIC 的原因**：
1. 每个 Socket 连接只传输一种类型的数据
2. 连接建立时已协商好数据格式
3. TCP 是可靠传输，不需要额外的同步标记

### 8.2 简化协议格式

```cpp
// file: libs/transport/include/wingman/transport/simple_protocol.hpp

#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <system_error>

namespace wingman::transport {

/**
 * @brief 简化的消息格式
 *
 * 由于三流分离，每个流只处理一种数据类型：
 *
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * |                    LENGTH (4 bytes, network byte order)           |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * |                          PAYLOAD (LENGTH bytes)                   |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 *
 * 即：[4字节长度] [数据]
 */
class SimpleMessage {
public:
    static constexpr size_t MAX_MESSAGE_SIZE = 256 * 1024 * 1024;  // 256MB
    static constexpr size_t LENGTH_SIZE = 4;

    /**
     * @brief 创建消息
     */
    static std::unique_ptr<SimpleMessage> create(const std::vector<uint8_t>& payload);

    /**
     * @brief 从字符串创建消息
     */
    static std::unique_ptr<SimpleMessage> create(const std::string& payload);

    /**
     * @brief 序列化消息（网络字节序）
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief 获取负载
     */
    const std::vector<uint8_t>& getPayload() const { return payload_; }

    /**
     * @brief 获取负载作为字符串
     */
    std::string getPayloadAsString() const {
        return std::string(payload_.begin(), payload_.end());
    }

private:
    std::vector<uint8_t> payload_;

    explicit SimpleMessage(const std::vector<uint8_t>& payload);
};

/**
 * @brief 消息接收器
 *
 * 处理 TCP 流的分包和粘包
 */
class MessageReceiver {
public:
    /**
     * @brief 接收数据，返回完整消息
     *
     * @param data 新接收到的数据
     * @return 完整的消息列表（可能为空）
     */
    std::vector<std::unique_ptr<SimpleMessage>> receive(const uint8_t* data, size_t size);

    /**
     * @brief 清除缓冲区
     */
    void clear();

    /**
     * @brief 获取缓冲区大小
     */
    size_t getBufferSize() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;

    /**
     * @brief 尝试解析消息
     */
    std::unique_ptr<SimpleMessage> tryParseMessage();
};

/**
 * @brief 协议辅助函数
 */
namespace Protocol {

/**
 * @brief 写入消息（完整发送）
 */
bool sendMessage(int socket, const SimpleMessage& message, std::error_code& ec);

/**
 * @brief 读取消息（完整接收）
 */
std::unique_ptr<SimpleMessage> readMessage(int socket, std::error_code& ec);

/**
 * @brief 32位整数转网络字节序
 */
inline uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong & 0xFF0000) >> 8) |
           ((hostlong & 0xFF000000) >> 24);
}

/**
 * @brief 网络字节序转32位整数
 */
inline uint32_t ntohl(uint32_t netlong) {
    return ((netlong & 0xFF) << 24) |
           ((netlong & 0xFF00) << 8) |
           ((netlong & 0xFF0000) >> 8) |
           ((netlong & 0xFF000000) >> 24);
}

} // namespace Protocol

} // namespace wingman::transport
```

### 8.3 实现

```cpp
// file: libs/transport/src/simple_protocol.cpp

#include "wingman/transport/simple_protocol.hpp"
#include <cstring>

namespace wingman::transport {

// SimpleMessage

std::unique_ptr<SimpleMessage> SimpleMessage::create(const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_MESSAGE_SIZE) {
        return nullptr;
    }
    return std::unique_ptr<SimpleMessage>(new SimpleMessage(payload));
}

std::unique_ptr<SimpleMessage> SimpleMessage::create(const std::string& payload) {
    std::vector<uint8_t> data(payload.begin(), payload.end());
    return create(data);
}

std::vector<uint8_t> SimpleMessage::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(LENGTH_SIZE + payload_.size());

    // 写入长度（网络字节序）
    uint32_t length = Protocol::htonl(static_cast<uint32_t>(payload_.size()));
    result.insert(result.end(), reinterpret_cast<uint8_t*>(&length),
                  reinterpret_cast<uint8_t*>(&length) + LENGTH_SIZE);

    // 写入数据
    result.insert(result.end(), payload_.begin(), payload_.end());

    return result;
}

SimpleMessage::SimpleMessage(const std::vector<uint8_t>& payload)
    : payload_(payload) {}

// MessageReceiver

std::vector<std::unique_ptr<SimpleMessage>> MessageReceiver::receive(
    const uint8_t* data, size_t size) {

    // 将新数据追加到缓冲区
    buffer_.insert(buffer_.end(), data, data + size);

    std::vector<std::unique_ptr<SimpleMessage>> messages;

    // 尝试解析消息
    while (auto msg = tryParseMessage()) {
        messages.push_back(std::move(msg));
    }

    return messages;
}

void MessageReceiver::clear() {
    buffer_.clear();
}

std::unique_ptr<SimpleMessage> MessageReceiver::tryParseMessage() {
    // 检查是否有足够的数据读取长度
    if (buffer_.size() < LENGTH_SIZE) {
        return nullptr;
    }

    // 读取长度
    uint32_t length;
    std::memcpy(&length, buffer_.data(), LENGTH_SIZE);
    length = Protocol::ntohl(length);

    // 检查长度是否合法
    if (length > MAX_MESSAGE_SIZE) {
        clear();  // 清除非法数据
        return nullptr;
    }

    // 检查是否有完整的数据
    if (buffer_.size() < LENGTH_SIZE + length) {
        return nullptr;  // 数据不完整，等待更多数据
    }

    // 提取数据
    std::vector<uint8_t> payload(
        buffer_.begin() + LENGTH_SIZE,
        buffer_.begin() + LENGTH_SIZE + length
    );

    // 从缓冲区移除已处理的数据
    buffer_.erase(
        buffer_.begin(),
        buffer_.begin() + LENGTH_SIZE + length
    );

    return SimpleMessage::create(payload);
}

// Protocol

bool Protocol::sendMessage(int socket, const SimpleMessage& message, std::error_code& ec) {
    auto data = message.serialize();

    const uint8_t* ptr = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
#ifdef _WIN32
        int sent = ::send(socket, reinterpret_cast<const char*>(ptr),
                         static_cast<int>(remaining), 0);
#else
        ssize_t sent = ::send(socket, ptr, remaining, 0);
#endif

        if (sent <= 0) {
            // 错误处理
            return false;
        }

        ptr += sent;
        remaining -= sent;
    }

    return true;
}

std::unique_ptr<SimpleMessage> Protocol::readMessage(int socket, std::error_code& ec) {
    // 先读取长度
    uint8_t lengthBytes[LENGTH_SIZE];
    size_t bytesRead = 0;

    while (bytesRead < LENGTH_SIZE) {
#ifdef _WIN32
        int n = ::recv(socket, reinterpret_cast<char*>(lengthBytes) + bytesRead,
                       LENGTH_SIZE - bytesRead, 0);
#else
        ssize_t n = ::recv(socket, lengthBytes + bytesRead, LENGTH_SIZE - bytesRead, 0);
#endif

        if (n <= 0) {
            return nullptr;
        }

        bytesRead += n;
    }

    uint32_t length;
    std::memcpy(&length, lengthBytes, LENGTH_SIZE);
    length = Protocol::ntohl(length);

    if (length > SimpleMessage::MAX_MESSAGE_SIZE) {
        return nullptr;
    }

    // 读取数据
    std::vector<uint8_t> payload(length);
    bytesRead = 0;

    while (bytesRead < length) {
#ifdef _WIN32
        int n = ::recv(socket, reinterpret_cast<char*>(payload.data()) + bytesRead,
                       length - bytesRead, 0);
#else
        ssize_t n = ::recv(socket, payload.data() + bytesRead, length - bytesRead, 0);
#endif

        if (n <= 0) {
            return nullptr;
        }

        bytesRead += n;
    }

    return SimpleMessage::create(payload);
}

} // namespace wingman::transport
```

### 8.4 使用示例

```cpp
// 发送消息
auto msg = SimpleMessage::create(R"({"action":"click","x":100,"y":200})");
bool success = Protocol::sendMessage(socket, *msg, ec);

// 接收消息
MessageReceiver receiver;
while (true) {
    uint8_t buffer[4096];
    int n = recv(socket, buffer, sizeof(buffer), 0);

    if (n > 0) {
        auto messages = receiver.receive(buffer, n);
        for (const auto& msg : messages) {
            std::string json = msg->getPayloadAsString();
            // 处理消息
            handleMessage(json);
        }
    }
}
```

---

## 9. 实施路线图

### 9.1 Phase 1: 三流分离 (2周)

| 任务 | 工作量 | 依赖 |
|------|--------|------|
| StreamType 和 StreamParams | 1天 | - |
| StreamChannel 实现 | 3天 | - |
| StreamManager 实现 | 2天 | StreamChannel |
| 集成到 Transport 层 | 3天 | StreamManager |
| 测试 | 3天 | 全部 |

**交付物**:
- `libs/transport/include/wingman/transport/stream_*.hpp`
- 单元测试

### 9.2 Phase 2: 组件重构 (2周)

| 任务 | 工作量 | 依赖 |
|------|--------|------|
| IComponent 接口 | 1天 | - |
| CaptureSource 抽象 | 3天 | - |
| ImageAnalyzer 拆分 | 3天 | - |
| ScreenManager 重组 | 2天 | - |
| 向后兼容适配 | 2天 | - |
| 测试 | 3天 | 全部 |

**交付物**:
- `lib/wingman/include/wingman/capture/*.hpp`
- `lib/wingman/include/wingman/vision/*.hpp`
- `lib/wingman/include/wingman/core/component.hpp`

### 9.3 Phase 3: 事件驱动 (1周)

| 任务 | 工作量 | 依赖 |
|------|--------|------|
| EventBus 实现 | 2天 | - |
| 预定义事件 | 1天 | EventBus |
| 集成到现有模块 | 2天 | - |
| 测试 | 2天 | 全部 |

**交付物**:
- `lib/wingman/include/wingman/core/event_bus.hpp`
- `lib/wingman/include/wingman/core/events.hpp`

### 9.4 Phase 4: 协议优化 (1周)

| 任务 | 工作量 | 依赖 |
|------|--------|------|
| SimpleMessage 实现 | 1天 | - |
| MessageReceiver 实现 | 2天 | - |
| Protocol 辅助函数 | 1天 | - |
| 测试 | 2天 | 全部 |

**交付物**:
- `libs/transport/include/wingman/transport/simple_protocol.hpp`

### 9.5 总计

| Phase | 时间 | 累计 |
|-------|------|------|
| Phase 1 | 2周 | 2周 |
| Phase 2 | 2周 | 4周 |
| Phase 3 | 1周 | 5周 |
| Phase 4 | 1周 | 6周 |

---

## 10. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 破坏现有功能 | 高 | 保持向后兼容，逐步迁移 |
| 性能回归 | 中 | 基准测试，性能监控 |
| 开发周期过长 | 中 | 分阶段交付，优先核心功能 |
| API 兼容性 | 中 | 提供适配层，标记 deprecated |

---

## 附录 A: 文件结构

```
wingman/
├── libs/transport/include/wingman/transport/
│   ├── stream_type.hpp           # NEW
│   ├── stream_channel.hpp        # NEW
│   ├── stream_manager.hpp        # NEW
│   └── simple_protocol.hpp       # NEW (简化协议，无 MAGIC)
│
├── lib/wingman/include/wingman/
│   ├── capture/                  # NEW
│   │   ├── capture_source.hpp
│   │   ├── screen_capture_source.hpp
│   │   ├── window_capture_source.hpp
│   │   ├── camera_capture_source.hpp
│   │   └── capture_source_manager.hpp
│   │
│   ├── vision/                   # NEW
│   │   ├── image_analyzer.hpp
│   │   └── pattern_matcher.hpp
│   │
│   ├── core/                     # NEW
│   │   ├── component.hpp
│   │   ├── event_bus.hpp
│   │   └── events.hpp
│   │
│   ├── screen_manager.hpp        # MODIFIED
│   └── screen.hpp                # DEPRECATED (使用 screen_manager.hpp)
│
└── docs/
    └── architecture-improvement-plan.md
```

---

## 附录 B: 术语表

| 术语 | 说明 |
|------|------|
| Stream | 独立的数据流，使用独立的 Socket 连接 |
| Channel | 消息通道，在单个 Stream 中多路复用 |
| Frame | 消息帧，包含类型、长度、负载、校验 |
| Component | 组件，实现 IComponent 接口的生命周期管理对象 |
| Event | 事件，异步通知机制 |
| CaptureSource | 捕获源，抽象的图像捕获接口 |

---

**文档版本**: v1.0
**最后更新**: 2026-05-13
