# iOS 支持设计（I1 主机控，远期可选）

> 日期：2026-09-19。
> 前置结论：`docs/mobile-support-feasibility.md` §2.2/§5 —— **iOS 端侧 Agent
> 任意形态均不可行**（App Store 沙箱禁止跨 App 注入，无合法执行通道）；
> 唯一合法路径是 **iOS 主机控（PC → WebDriverAgent）**，列为远期可选 I1。
> 本文给出 I1 的工程设计，**本提交只含设计，不含实现**；实施启动与否取决于
> 是否出现真实需求（有 Mac 签名链的 iOS 测试/挂机场景）。

---

## 1. 定位与模型

### 1.1 与 Android 端侧的本质区别

| | Android（A 线） | iOS（I1） |
|---|---|---|
| 执行平面位置 | 设备端（C++ 核心跑在手机上） | **PC 主机**（现有桌面 runtime 内） |
| iOS 设备角色 | — | 被控外设（如同接了一台显示器+键鼠的机器） |
| 长链接 | 设备 → Go Server | iOS 设备不联网参与；PC runtime → Go Server 照旧 |
| 通道 | 出站 TCP | usbmuxd/RemoteXPC + WDA（HTTP over USB，仅本机回环） |
| 签名要求 | 无 | 开发者证书 + 描述文件 + 设备 Developer Mode |
| 架构约束核查 | agent 出站 ✓ | **runtime 不新增任何监听面**：WDA 自身是 USB 上的本地服务，runtime 作为其客户端；约束 4（runtime 禁 HTTP/WS server）不破 |

结论：I1 不是把 Wingman 移植到 iOS，而是给 **桌面 runtime 的
ICapture/IInput 增加一个 iOS 设备后端**。设备 = 一种"屏幕"。

### 1.2 为什么是 WDA

iOS 自动化的合法通道只有 XCTest 框架。WDA 是 Apple 官方测试 runner 的
社区打包（Appium XCUITest driver 同款），在真机上暴露 XCTest 能力：

- 截屏：`GET /screenshot`（JPEG/PNG）
- 触摸注入：`/session/{id}/actions`（W3C actions，支持滑动/长按/多指）
- 元素查找/输入：`/elements`、`/element/{id}/value`（XCUITest 元素树）
- 启动：iOS 17+ `devicectl`、iOS 18+ RemoteXPC；WDA 需随 iOS 大版本重编译

Appium/按键精灵 iOS 版均走此通道；越狱路线（仅老版本）不符合本项目合规边界，不做。

---

## 2. 架构

```
┌────────────────────── PC 主机（现有 runtime 进程）────────────────────┐
│  wingman 核心（模块编排/脚本引擎/vision）                              │
│      │ 虚接口（既有）                                                 │
│      ├─ ICaptureSource ◀── WdaCaptureBackend（新）                    │
│      └─ IInput         ◀── WdaInputBackend（新）                      │
│                 │ 本地 HTTP（usbmuxd 隧道之上，127.0.0.1 回环）        │
└─────────────────┼───────────────────────────────────────────────────┘
                  │ USB
        ┌─────────┴─────────┐
        │ usbmuxd / devicectl │
        │  WDA App（真机）    │ ← XCTest（Apple 合法通道）
        └───────────────────┘
```

- **新增面最小化**：runtime 加两个 IBackend + 一个设备管理器；vision/找色找图
  复用现有模块（截屏拿回本机内存，匹配在 PC 做，与桌面同路径）。
- **不进 A 线主干**：Android 的云控协议（run_script/agent.event）与 iOS 无关；
  iOS 会话属于 PC runtime 本地能力，Go Server 视角只是一个普通桌面 agent。

---

## 3. 组件设计

### 3.1 WDA 连接管理（`platform/ios/wda_client`）

- 设备发现：`libimobiledevice`（usbmuxd 协议）枚举 UDID；或 `devicectl list`。
- WDA 启停：签名安装由 **人工/脚本前置**（证书问题无法自动化闭环），
  runtime 只负责经 usbmuxd 端口转发连接已运行的 WDA（:8100）。
- 会话：`POST /session` 建会话，心跳保活；断开重连。

### 3.2 ICapture 后端（WdaCaptureBackend）

- `capture()` → `GET /screenshot` → 解码为 wingman::Bitmap；
- 帧率预期：USB 上 200-400ms/帧（JPEG 编解码 + USB 往返），只支撑找色找图
  节奏（1-3 fps），不支持录制/高性能场景 —— 与桌面 GDI 捕获定位不同，A 面文档明示。

### 3.3 IInput 后端（WdaInputBackend）

- 点击/滑动/长按/输入文本 → W3C actions JSON；
- 坐标系：WDA 返回设备像素，与 capture 同源，无 DPI 换算问题；
- 坐标校准：脚本用相对比例坐标（模板匹配中心归一化），避免跨分辨率重写。

### 3.4 脚本 API 面

与桌面完全一致（`wingman.click/scroll/findImage/...`）——**这是 I1 的核心价值**：
同一套 Lua 脚本语义跑在"Windows 窗口"或"iOS 设备屏幕"上，仅 capture/inject
后端不同。iOS 专属能力（如 Springboard 操作）A2 后按需增补。

---

## 4. 构建与依赖

| 项 | 方案 |
|----|------|
| usbmuxd 协议 | `libimobiledevice`/`libusbmuxd`（LGPL，vcpkg 有 `libimobiledevice` port） |
| WDA 通信 | 运行时经端口转发，C++ 侧 HTTP 客户端复用现有 curl 依赖 |
| iOS 大版本适配 | WDA 重编译属**环境前置**（Mac 签名链），runtime 侧只对 API 差异做版本探测 |
| 平台租户 | `lib/wingman/src/platform/ios/`（主机侧，非设备侧），复用 §8.4 独立库纪律 |

签名链约束（无法绕过，README 明示）：开发者账号 + 证书 + 描述文件（免费证书
7 天过期）+ 目标设备 Developer Mode + Mac 执行 `xcodebuild` 部署 WDA。

---

## 5. 里程碑（I1 内部）

| 阶段 | 内容 | 验收 |
|------|------|------|
| I1.0 | WDA 客户端 + 设备枚举 + 截屏 | PC 脚本 `wingman.findColor()` 命中 iOS 屏幕色块 |
| I1.1 | 触摸注入 + 文本输入 | 脚本完成一次真实 App 操作（打开→点击→确认） |
| I1.2 | 多设备 | 两台设备并发会话，脚本按设备名路由 |

---

## 6. 风险与边界

| 风险 | 应对 |
|------|------|
| WDA 随 iOS 大版本失效 | 版本探测 + 明确报错；适配投入记入维护成本，**不背主线成本** |
| 签名/证书链依赖 Mac | 环境前置文档化；不自动化（合规与工程量都不可行） |
| 帧率天花板（1-3 fps） | 定位为测试/轻量挂机，不做高性能场景承诺 |
| 越狱诱惑 | 明确拒绝：出 App Store 沙箱即出合规边界 |

**不做什么**：iOS 端侧 Agent（不可行）、iOS Dashboard 客户端（Dashboard 走
Web，任意浏览器可用，无需原生 App）、越狱通道（合规红线）。

---

## 7. 启动条件

I1 保持"远期可选"：当出现以下任一信号再启动实施——
1. 有真实用户提出 iOS 场景需求（测试/挂机）且能提供 Mac + 开发者账号环境；
2. A 线（Android A2-A4）交付完毕且 vision 模块 NDK 化稳定，有富余投入。

启动时本文即为实施基线，先落 I1.0。
