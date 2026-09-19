# 移动端支持可行性分析（Android 端侧 Agent 优先）

> 分析日期：2026-09-19。
> 回答的问题：Wingman 能否像 Auto.js / Appium 一样支持 Android / iOS？
> 核心设想：**手机上运行 Agent，通过 TCP 长链接直接连 Go Server**（云控模式），
> 而非 Auto.js 的单机模式。

---

## 1. 结论速览

| 目标形态 | 可行性 | 结论 |
|---------|--------|------|
| **Android 端侧 Agent + 长链接 Go Server（云控）** | ✅ 高 | **推荐主线**。有商业化先例（Hamibot 即 Auto.js + 云控），且比 Auto.js 单机模式更贴合 Wingman 现有架构 |
| Android 主机控（PC + adb，Appium 模式） | ✅ 高 | 备选/补充路线，可作为 PoC 切口，但不是目标形态 |
| iOS 端侧（任意形态） | ❌ | **不可行**。App Store 沙箱不允许跨 App 注入，无合法通道，放弃 |
| iOS 主机控（PC → WDA） | ⚠️ 中 | 可行但绑 Mac 签名链（开发者证书 + Developer Mode + WDA 需随 iOS 大版本适配），列为远期可选 |

**核心判断**：控制面（脚本管理、调度、编排、监控）放 Server，执行面（截屏 → 找图 → 注入的实时闭环）留在端侧本地——这个拆分既满足实时性要求（每帧回传 Server 决策的延迟与带宽都不可接受），又完整复用了 Wingman「Runtime-as-Agent + Go Server 中控」的既有架构。

---

## 2. 生态调研

### 2.1 Android：两条技术路线

| 路线 | 代表 | 机制 | 免 Root |
|------|------|------|---------|
| 端侧自治（单机） | Auto.js / AutoX / AutoJs6 | `AccessibilityService`：系统推送全屏控件树，脚本引擎（Rhino/JS）经 `dispatchGesture` 注入手势 | ✅ |
| 端侧 + 云控 | **Hamibot**（基于 Auto.js）、autojs pro9 云控群控 | 单机能力之上加账号体系：Server 下发脚本 → 手机拉取执行；支持开机自启、崩溃重启、设备分组调度 | ✅ |
| 主机控 | Appium UiAutomator2 / openatx uiautomator2 | 设备端跑 UiAutomator 测试进程开 HTTP RPC（atx-agent :7912），PC 经 adb 隧道调用；低延迟注入用 minitouch，取帧用 scrcpy | ✅ |

**免 Root 的钥匙只有两把：AccessibilityService（端侧）或 adb（主机控）。**

### 2.2 iOS：只有 WDA 一条合法通道

- Appium XCUITest driver 的做法：用 Xcode 把 WebDriverAgent（WDA，一个测试 runner App）签名部署到真机，WDA 在设备上跑 HTTP 服务（:8100）暴露 XCTest 能力。iOS 17+ 经 `devicectl` 启动、18+ 经 RemoteXPC
- 硬约束：Apple 开发者证书 + 描述文件（免费证书 7 天过期）、设备 Developer Mode、签名部署环节绑定 Mac；iOS 大版本升级常需重编译 WDA
- WDA 是前台测试会话，无法后台常驻；**端侧自治在 iOS 上不存在合法通道**（App Store 自动点击器受沙箱限制只能控制自己内部）
- 按键精灵 iOS 版走同类通道，旧版越狱支持仅到 iOS 14

### 2.3 游戏场景的特殊性（对 Wingman 有利）

Auto.js 的强项是**控件树**，但手游是渲染表面，无障碍树拿不到游戏内部控件。游戏自动化天然是
「找色/找图 + 坐标 + 人性化模拟」模式——这正是 Wingman 的看家本领（像素检测 / OpenCV 模板匹配 /
金字塔加速 / 贝塞尔输入 / 随机延迟）。**迁移到手机是主场作战，Auto.js 的控件树优势在游戏场景反而发挥不出来。**

---

## 3. 三种模式对比

| 维度 | Auto.js 单机 | **Wingman 云控（本方案）** | Appium 主机控 |
|------|-------------|--------------------------|---------------|
| 脚本存放/编辑 | 手机本地 | **Server 集中管理，下发执行** | PC 本地 |
| 执行位置 | 手机端 | **手机端（本地闭环）** | PC 决策，手机执行 |
| 断网后继续执行 | ✅（本来就单机） | ✅（Agent 缓存当前脚本自治运行，重连后汇报） | ❌ PC 断则停 |
| 多设备编排/群控 | ❌ 弱 | **✅ 复用 Go Server 编排 + Team/inbox** | 需自建设备管理 |
| 监控/日志聚合 | ❌ 本地 | **✅ Dashboard 实时查看** | 需自建 |
| 每台手机需 PC | 否 | **否** | 是 |
| 实时闭环延迟 | 最低 | 最低（决策在端侧） | 受帧回传链路影响 |
| 与 Wingman 架构契合 | 无 | **原生契合（Runtime-as-Agent）** | 需较大改造 |

云控模式在商业上已有验证：Hamibot（Auto.js + 账号 + 脚本市场 + 下发执行）、各类 autojs pro9
云控群控系统（开机自启、崩溃自启、清后台自启），目标客群与 Wingman 重合（多开群控、游戏工作室）。

---

## 4. 推荐架构：端侧 Agent 长链接 Go Server

```
┌─────────────────────── Go Server（中控，现有）───────────────────────┐
│   会话管理 │ Agent 列表 │ 脚本仓库/下发 │ 工作流编排 │ 日志聚合      │
└──────▲──────────────────────▲──────────────────────▲──────────────┘
       │ TCP 长链接（出站）     │                      │
┌──────┴─────────┐   ┌────────┴─────────┐   ┌────────┴─────────┐
│ 桌面 Runtime    │   │ Android Agent    │   │ （远期可选）      │
│ （现有 C++）    │   │ Kotlin 壳         │   │ iOS: PC → WDA    │
│ Win/Linux/macOS │   │  + C++ 核心(JNI) │   └──────────────────┘
└────────────────┘   │ 无障碍+MediaProj │
                     └──────────────────┘
```

### 4.1 现有组件复用映射

| 现有组件 | 在 Android 端侧 Agent 中的角色 | 预估改动 |
|---------|------------------------------|---------|
| `libs/transport` 客户端（TCP 长链接、`length\njson\n` 信封、心跳、自动重连、register/agent_id） | Agent ↔ Go Server 通信层 | **几乎零改动**，直接编译到 Android |
| Go Server 会话管理（AgentInfo / kGetAgents / 心跳超时） | 设备纳管 | 新增少量消息类型（见 4.2） |
| Lua 引擎（libs/lua） | 端侧脚本执行 | NDK 交叉编译 |
| vision（OpenCV 找色找图、金字塔、LRU 缓存） | 游戏场景核心能力 | arm64-android 编译（NEON 加速） |
| `ICapture` / `IInput` / `IScreen` / `IWindow` 平台抽象接口 | 新增 `platform/android/` 后端 | **新实现**（JNI 桥接 Kotlin 层，见 4.3） |
| event / fsm / task / timer / orchestration / clip模块 | 脚本层 API | 零改动 |
| 触发器系统 / 宏录制回放 / 人性化模拟 | 端侧直接复用 | 少量适配（录制依赖注入事件源） |
| Dashboard | 设备管理/监控入口 | 新增设备视图（设备列表已在 Agents 页） |
| Python 引擎（libs/python） | 端侧暂缓 | Python+NDK 体积/维护成本高，**Lua-first**，后期按需评估 |

### 4.2 协议扩展点（沿用现有 TCP 协议，不动架构）

现有协议已支持 1024+ 自定义业务码，新增消息类型即可：

- `script.push` / `script.pull` —— 脚本下发与版本管理（Server 为脚本仓库）
- `device.capabilities` —— 能力上报（屏幕分辨率/密度/朝向、Android 版本、有无 Root）
- `asset.sync` —— 模板图片/资源同步（找图模板由 Server 按脚本打包下发）
- `log.stream` —— 端侧日志/截图事件上报（复用现有事件转发机制）
- 现有 `kRegister / kHeartbeat / kSyncTask / kShutdown` 原样复用

### 4.3 新增组件：Android 壳与 JNI 桥

```
Android App（Kotlin 壳）
├── ForegroundService          ← 长链接保活 + 自动重启
├── WingmanAccessibilityService ← 手势注入 + 控件树 + (API 30+) 截屏
├── MediaProjectionManager     ← 高帧率截屏（30-60fps）
├── JNI 桥                     ← Kotlin ⇆ C++ wingman 核心
└── C++ 核心（NDK 交叉编译）
    ├── libs/transport 客户端   ← 长链接（复用）
    ├── libs/lua + 模块注册表   ← 脚本执行（复用）
    ├── vision (OpenCV)        ← 找图找色（复用）
    └── platform/android/      ← IInput/ICapture 新后端（调 JNI）
```

分层原则：**系统权限相关的活（无障碍、投屏授权）全在 Kotlin 壳；可移植的活（脚本、视觉、编排、通信）全在 C++ 核心**。JNI 桥只暴露两类窄接口：`captureFrame() -> 位图` 和 `inject*()`。

---

## 5. Android 关键技术点与对策

### 5.1 长链接保活

| 限制 | 版本 | 对策 |
|------|------|------|
| 后台运行限制（Doze、后台服务） | Android 8+ | 前台服务（常驻通知，行业惯例：IM/推送类均如此） |
| 后台不能启动前台服务 | Android 12+ | 开机自启走 `BOOT_COMPLETED` 豁免；崩溃重启靠服务内自恢复而非后台拉起 |
| 前台服务必须声明类型 | Android 14+ | 声明 `connectedDevice`/`dataSync` 类型并申请对应运行时权限 |
| 厂商 ROM 激进杀后台（MIUI/EMUI/ColorOS） | 全版本 | 自愈三件套：开机自启 + 服务崩溃自重启 + 断线指数退避重连（`libs/transport` 已有自动重连） |
| TCP 被中间设备掐断 | 全版本 | 心跳保活（现有 `startHeartbeat`），必要时智能心跳间隔 |

### 5.2 权限摩擦（重要，影响开箱体验）

- **Android 13+ 受限设置**：侧载 App 的无障碍权限默认被系统屏蔽。解法（文档化到用户手册）：
  1. 用户手动：应用信息 → ⋮ → 「允许受限制的设置」→ 再开无障碍
  2. 批量部署场景：`adb shell appops set <pkg> ACCESS_RESTRICTED_SETTINGS allow` 预授权
  3. 企业/工作室批量纳管：Device Owner 模式静默授权（MDM 通用做法，也是群控系统的标准路径）
- **MediaProjection**：每次会话需用户确认投屏弹窗（配合前台服务类型 `mediaProjection` 可持续）；引导一次即可
- **无障碍服务开关**：系统改版/重启可能失效，Agent 需检测并上报 Server 提醒用户

### 5.3 截屏方案选择

| 方案 | 帧率 | 限制 | 用途 |
|------|------|------|------|
| MediaProjection + VirtualDisplay | 30-60fps | 会话级授权弹窗 | **主方案**：找图/找色实时闭环 |
| AccessibilityService.takeScreenshot | 限流（约 1 次/秒） | API 30+ | 兜底/低频场景 |
| 控件树截图（无） | — | — | 控件信息直接走无障碍节点树，无需截屏 |

### 5.4 输入注入

- `AccessibilityService.dispatchGesture` / `GestureDescription`：多点触控、贝塞尔路径原生支持（连续笔画），与人性化模拟模块（贝塞尔移动、随机抖动）映射良好
- 注入事件带系统标记（generated gesture），部分反外挂可检测——**人性化模拟在这边是刚需而非加分项**，建议文档明确声明

### 5.5 C++ 核心移植

- vcpkg 支持 `arm64-android` triplet；项目当前锁定 `x64-windows-static`，需扩展 triplet 矩阵（CI 加 Android job）
- OpenCV / asio / spdlog 均可 Android 编译；Lua 体积小、无依赖，优先级最高
- **Lua-first**：端侧脚本执行先只支持 Lua；Python+NDK 体积与维护成本高，后期按需评估
  （双语言分层决策已记入 `docs/architecture-decisions.md`「Scripting Language Strategy」：
  桌面 Python 优先、移动端 Lua 优先，双语言长期保持）

---

## 6. 架构硬约束合规核查

| # | 硬约束 | 端侧 Agent 方案 | 合规 |
|---|--------|----------------|------|
| 1 | Go Server 是远程中控编排器 | 角色不变，新增设备纳管/脚本下发 | ✅ |
| 2 | Runtime/Agent 主动 outbound 连 Go Server | Android Agent 出站长链接，行为同现有 runtime | ✅ |
| 3 | 本地 Tauri UI 经本地 IPC 控制 runtime | Android 端不做本地 UI（无障碍悬浮窗调试工具可作为远期增强） | ✅ |
| 4 | Runtime 禁止引入 HTTP/WebSocket server | Agent 只有 outbound TCP 客户端；**未来若接 WDA，WDA 的 HTTP 服务在设备上，Agent 是 client**（记入架构决策文档） | ✅ |
| 5 | Dashboard/远程客户端只连 Go Server | 不变，Dashboard 新增设备视图走 Server API | ✅ |

---

## 7. 风险清单

| 风险 | 等级 | 缓解 |
|------|------|------|
| Android 13+ 受限设置导致开箱失败率高 | 高 | 文档引导 + adb 预授权脚本 + Device Owner 批量部署方案 |
| 厂商 ROM 杀后台/自启被拦 | 高 | 自愈三件套（5.1）；按机型维护「保活设置指引」 |
| 手游反检测（generated gesture 标记、投屏图标） | 中 | 人性化模拟强化；明确产品定位与合规声明 |
| iOS 大版本变动 | — | iOS 仅做远期可选，不背主线成本 |
| 体积：OpenCV + Lua + transport 的 NDK 产物 | 低 | 按需裁剪 OpenCV 模块（imgproc/core 足够） |
| 工作量：Kotlin 壳 + JNI + CI 三平台 | 中 | 分阶段（见 8），A1 PoC 先验证长链接与脚本下发闭环 |

---

## 8. 分阶段路线图

- [x] **A1 PoC：链路打通**（✅ 2026-09-19 实施完成，工程设计与验收步骤见
  `docs/android-agent-design.md`；完整工程在 `apps/android/`）
  - [x] Kotlin 壳：ForegroundService + 长链接（复用 libs/transport + RemoteClient 编译到 Android）
  - [x] C++ 核心可移植面就位（transport + RemoteClient + Lua/Sol2 + ScriptRunner；
    lib/wingman 本体 Android 编译随 A2 接入，租户目录与构建分支已就位）
  - [ ] 真机端到端验收（需 Android SDK/NDK 环境，步骤见 apps/android/README.md）
- [ ] **A2 能力闭环：自动化可用**
  - [ ] `platform/android/` IInput 后端（dispatchGesture，JNI）
  - [ ] `platform/android/` ICapture 后端（MediaProjection 主 + takeScreenshot 兜底）
  - [ ] 找色/找图/像素检测对手机截帧可用；screen/input 脚本 API 全通
  - [ ] 触发器系统在端侧跑通（定时/像素触发）
- [ ] **A3 可靠性与部署体验**
  - [ ] 开机自启、崩溃自重启、断连自治（缓存脚本继续执行、重连后汇报）
  - [ ] 受限设置引导 + adb 预授权脚本；无障碍失效检测上报
  - [ ] 模板图片 asset.sync、脚本版本管理
- [ ] **A4 多设备编排**
  - [ ] Team/inbox 模块接入端侧 Agent
  - [ ] Dashboard 设备视图（分组、批量下发、状态大盘）
- [ ] **I1（远期可选）：iOS 主机控**——PC 端 ICapture/IInput 的 WDA 后端（usbmuxd/libimobiledevice），依赖 Mac 签名链

---

## 9. 参考资料

- [Auto.js 二开项目（GitHub）](https://github.com) / AutoX / [AutoJs6 指南（CSDN）](https://blog.csdn.net)
- [Hamibot 官网](https://hamibot.com)（Auto.js + 云控的商业化先例）、[云控群控系统实例（CSDN）](https://blog.csdn.net)
- [uiautomator2（openatx，GitHub）](https://github.com)、[pyminitouch+scrcpy 免 Root 方案（CSDN）](https://blog.csdn.net)
- [Appium XCUITest driver：运行预装 WDA](https://appium.github.io/appium-xcuitest-driver/latest/guides/run-preinstalled-wda)、[能力参考（iOS 17+ devicectl / 18+ RemoteXPC）](https://appium.github.io/appium-xcuitest-driver/12.12/reference/capabilities)、[真机要求](https://appium.readthedocs.io/en/latest/en/drivers/ios-xcuitest-real-devices)
- [WDA 架构解析（trinhngocthuyen.com）](https://trinhngocthuyen.com/posts/tech/mobile-e2e-wda)、[Appium Drivers 介绍](https://appium.io)、[iOS 自动化工具 2026 对比（drizz.dev）](https://www.drizz.dev/post/ios-automation-testing-tools-in-2026)
- [Android 官方：受限设置（Google 帮助）](https://support.google.com/android/answer/12623953?hl=zh-Hans)、[后台启动前台服务限制](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start)
- [按键精灵 iOS 免越狱论坛](https://bbs.vrbrothers.com)
