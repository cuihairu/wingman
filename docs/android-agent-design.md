# Android 端侧 Agent 设计（A1 落地版）

> 日期：2026-09-19。
> 前置调研：`docs/mobile-support-feasibility.md`（可行性、三模式对比、生态调研）。
> 本文是该调研中「云控模式 + A1 PoC」的工程设计，作为 A2-A4 的实现基线。

---

## 1. 目标与范围

### 1.1 A1（本文实施范围）：链路打通

Go Server 向 Android 设备下发 hello-world Lua 脚本 → 设备端 C++ 核心执行 →
日志实时回传 Dashboard。验收链路：

```
Dashboard 点击运行 → Go Server run_script{content} → 设备执行 Lua
     ↑                                            │
     └──────── Dashboard 实时日志 ← agent.event{script_output}
```

### 1.2 非目标（后续里程碑）

| 里程碑 | 内容 | 本文只做 |
|--------|------|----------|
| A2 能力闭环 | `platform/android` 的 IInput（dispatchGesture）/ICapture（MediaProjection）真实现，找色找图 | 租户目录 + stub 骨架 |
| A3 可靠性 | 开机自启、崩溃自重启、断连缓存自治、token 认证 | 设计约束成文 |
| A4 多设备编排 | Dashboard 设备视图、批量下发、asset.sync 模板分发 | 协议预留 |

### 1.3 硬约束核查（与 `docs/architecture-decisions.md` 对齐）

| # | 约束 | 本设计 |
|---|------|--------|
| 1 | Go Server 是中控 | 不变，新增的只有 run_script payload 的 content 字段 |
| 2 | Agent 主动 outbound | Android Agent 出站 TCP 长链接，行为与桌面 runtime 一致 |
| 3 | 本地 UI 走本地 IPC | Android 端无本地 UI；进程内 JNI 直调（同进程，非 IPC，更无 HTTP） |
| 4 | Runtime 禁 HTTP/WS server | Agent 只有 outbound TCP 客户端 + JNI 进程内调用，零监听端口 |
| 5 | Dashboard 只连 Go Server | 不变 |

---

## 2. 总体架构

```
┌──────────────────── Go Server（现有，改动极小）────────────────────┐
│  FrameListener/Registry（现有）                                    │
│  script.go: run_script 下发增加 content 内联（新增 ~10 行）        │
│  AgentInfo 增加 platform 字段透传 Dashboard（新增）                 │
└───────▲───────────────────────────────────────────────────────────┘
        │ TCP 长链接（16B 帧头 + JSON，出站）— 复用现有协议，零新消息类型
┌───────┴──────────────── Android 设备 ──────────────────────────────┐
│  Kotlin 壳（apps/android/）                                        │
│  ├─ WingmanService        前台服务：托管 C++ 核心生命周期 + 保活     │
│  ├─ MainActivity          状态/启停 UI + 权限引导                    │
│  ├─ WingmanAccessibilityService   A2 骨架（A1 仅声明占位）          │
│  └─ WingmanJni            JNI 声明（窄接口，见 §5.3）               │
│           │ JNI（进程内直调，非 IPC）                               │
│  C++ 核心（NDK 编译，app/src/main/cpp/）                            │
│  ├─ AndroidAgent         装配层：注册/命令分发/日志回传              │
│  ├─ ScriptRunner         Lua 执行线程：print 重定向、协作式停止     │
│  ├─ RemoteClient         ★直接复用 apps/runtime（重连/outbox/心跳） │
│  ├─ libs/transport       ★直接复用（asio 帧协议）                   │
│  └─ libs/lua (sol2)      ★直接复用（Lua 5.5 引擎）                  │
└────────────────────────────────────────────────────────────────────┘
```

**A1 的 C++ 复用面刻意收窄**：不拖入 lib/wingman 本体（vision/OpenCV/完整模块面
是 A2 的事），只编 transport + RemoteClient + Lua。A1 交付的东西全部可以小
工具链验证或代码审阅；`lib/wingman` 的 Android 整体编译在 A2 接入。

---

## 3. 协议设计（复用现有帧协议，零新消息类型）

帧格式与 `orchestrator/server/pkg/agent/client.go` 一致：
16 字节头（length/sequence/type/reserved，小端）+ JSON body；
type ∈ Request(1)/Response(2)/Notify(3)/Error(4)。

### 3.1 复用的既有消息（Android Agent 必须实现）

| 方向 | 消息 | 说明 |
|------|------|------|
| agent→server | Notify `agent.register` | `{agentId, hostname, platform, capabilities}`，后两字段新增、server 弱依赖 |
| agent→server | Notify `agent.heartbeat` | RemoteClient 现成 |
| agent→server | Notify `agent.event` `{event, data}` | `event="script_output"` 已被 server 转发 Dashboard |
| server→agent | Request `run_script` | **payload 扩展**：`{path, content?, language?}` |
| server→agent | Request `stop_script` | 停止当前脚本 |
| agent→server | Response（同 sequence） | 命令应答，RemoteClient 现成 |

### 3.2 run_script 的 content 扩展（本设计唯一的协议变更）

server 侧三条脚本下发路径（单发 / 批量 / 工作流步骤）统一在下发时读取
脚本文件内容内联：

```json
{ "path": "scripts/hello.lua", "content": "print('hello')", "language": "lua" }
```

- 桌面 runtime：忽略 `content`，继续用 `path` —— **向后兼容，桌面零改动**。
  （桌面工作流的既有部署契约是 server 与 agent 共享文件系统，故 server 读
  不到文件即 agent 也读不到，下发前失败只是把错误暴露得更早更清晰。）
- Android Agent：优先 `content`；无 `content` 时回错误（设备无服务器文件系统）。
- 大小约束：脚本 ≤ 1MB（server 侧校验，帧上限 16MB 的安全余量）。
- `language` 字段预留：A1 仅 `lua`；Python 端侧按移动端双语言决策暂缓。
- 实现收敛在 `internal/scripts`（`ReadInline`/`LanguageOf`/`MaxInlineScriptSize`），
  handlers 与 workflow 引擎共用。

### 3.3 注册与能力上报（弱约定）

`agent.register` 增加可选字段，server 读到即存 `AgentInfo.Platform`，Dashboard
的 agents API 自动带出（A1 不做 Dashboard UI，字段先透传）：

```json
{
  "agentId": "android-pixel-8",
  "hostname": "Pixel 8",
  "platform": "android",
  "capabilities": { "apiLevel": 34, "hasAccessibility": true, "abi": "arm64-v8a" }
}
```

### 3.4 预留（不在 A1 实现，仅命名占位）

`script.ack`（脚本版本确认）、`asset.sync`（模板图分发）、`device.capabilities`
变更上报 —— 均可用现有 Notify/Request 载体表达，A2/A3 按需启用，无需破坏兼容。

---

## 4. Go Server 改动清单（本设计全部 server 侧改动）

| 文件 | 改动 |
|------|------|
| `internal/scripts/inline.go` | `ReadInline`/`LanguageOf`/`MaxInlineScriptSize`：内容内联共享实现 |
| `internal/handlers/script.go` | 单发 `run_script` 下发时读文件内容，payload 增加 `content`/`language`；>1MB 拒绝 |
| `internal/handlers/batch.go` | 批量下发同上（先解析目标，零目标零下发不读文件） |
| `internal/workflow/engine.go` | 工作流 `runScriptOnce` 同上（读失败判步骤失败） |
| `internal/agent/registry.go` | `AgentInfo` 增加 `Platform` 字段（空值视为 desktop，兼容旧 agent） |
| `pkg/agent/listener.go` | `handleRegister` 读取 `platform` 透传给 Registry（~3 行） |
| 测试 | registry 平台字段测试 + 三条路径 content 下发测试 |

---

## 5. Android 端设计

### 5.1 工程结构

```
apps/android/
├── README.md                      # 环境/构建/真机部署说明
├── settings.gradle.kts
├── build.gradle.kts               # AGP 版本与仓库
├── gradle.properties
└── app/
    ├── build.gradle.kts           # externalNativeBuild → NDK CMake
    └── src/main/
        ├── AndroidManifest.xml    # 权限与组件声明
        ├── cpp/
        │   ├── CMakeLists.txt     # NDK 构建：wingman_agent JNI so
        │   ├── jni_bridge.cpp     # JNI 边界（唯一接触 JNIEnv 的翻译层）
        │   └── agent/
        │       ├── android_agent.hpp/.cpp   # 装配：RemoteClient+命令分发+日志回传
        │       └── script_runner.hpp/.cpp   # Lua 执行线程
        ├── java/com/wingman/agent/
        │   ├── MainActivity.kt
        │   ├── WingmanService.kt
        │   ├── WingmanAccessibilityService.kt   # A2 骨架
        │   └── WingmanJni.kt
        └── res/                   # 最小资源（主题/布局/字符串/图标）
```

### 5.2 C++ 核心层

**AndroidAgent**（`agent/android_agent.cpp`）职责：
1. `start(config)`：起 RemoteClient（host/port/agentId/重连），注册命令回调
2. 命令分发：`run_script` → ScriptRunner；`stop_script` → ScriptRunner::stop；
   其他命令回 `success:false "not supported"`（协议防御性应答）
3. 日志回传：Lua print / 错误 → `sendAgentEvent("script_output", {executionId, line})`
   （RemoteClient 已有 sendAgentEvent）
4. `stop()`：停脚本 → 停 RemoteClient，幂等

**ScriptRunner**：
- 单执行线程；一次一个脚本（`std::atomic` 状态机 idle/running/stopping）
- sol2 状态注入 `print`/`wingman.log`（重定向到回传回调）+ `wingman.sleep(ms)`
  （协作式：检查停止标志，触发时 `lua_error` 中断脚本）
- 停止：置标志 + `lua_sethook`（LUA_MASKCOUNT，1e7 条指令粒度）强制打断死循环
- 超时：A1 无强制超时（手动 stop）；A3 接 server 下发的 timeout

**复用与边界**：
- `RemoteClient`（apps/runtime）原样编入：它仅依赖 transport + spdlog + nlohmann
  （`event_buffer.hpp` 为独立 bounded queue 头，随编无害）。其内部 `#ifdef _WIN32`
  两处宏不影响 Android 编译（Android 走 POSIX 分支）。
- 不编 lib/wingman、不编 OpenCV —— A2 再接入。
- 所有 OS 交互（网络=asio、线程=std、日志=spdlog + logcat 桥）在 Android 上可用，
  C++ 代码不直接调用任何 Android NDK 专属 API —— JNI 反向：Android API 只在
  Kotlin 侧，A2 时经 JNI 窄接口进 platform/android 租户。

### 5.3 JNI 边界（窄接口）

```kotlin
object WingmanJni {
    external fun nativeStart(configJson: String): Boolean  // host/port/agentId/...
    external fun nativeStop()
    external fun nativeStatus(): String                     // JSON: state/script/execId
}
```

状态展示：A1 由 Kotlin 以 1s 间隔轮询 `nativeStatus`（UI 场景足够，且避免
AttachCurrentThread 的线程管理）；C++ → Kotlin 的事件回调（onCoreStatus 及
A2 的 capture/inject 反向接口）届时以同模式追加，A1 不预埋。

原则（承可行性文档 4.3）：**系统权限与 Android 生态的活全在 Kotlin；可移植的活
（通信/脚本/日志）全在 C++**。

### 5.4 Kotlin 壳

**WingmanService**（Foreground Service，`dataSync` 类型）：
- `onStartCommand`：读 SharedPreferences 配置（host/port/agentId）→ `nativeStart`
- `onDestroy`：`nativeStop`
- 崩溃自愈：`START_STICKY` + `onTaskRemoved` 重启调度（A1 最小自愈，完整自愈三件套在 A3）
- 通知渠道：常驻「Wingman Agent 运行中」（Android 8+ 前台服务要求）

**MainActivity**：状态卡片（连接/脚本状态，来自 nativeStatus + onCoreStatus）、
启停按钮、服务器地址配置输入、无障碍权限引导入口（A2 用，A1 先放置）。

**AndroidManifest** 权限（A1 实际用到的前两个，其余 A2/A3 启用时再加注释说明）：
`INTERNET`、`FOREGROUND_SERVICE`、`FOREGROUND_SERVICE_DATA_SYNC`（API 34 要求）、
`POST_NOTIFICATIONS`（前台服务通知）、`RECEIVE_BOOT_COMPLETED`（A3）、
`BIND_ACCESSIBILITY_SERVICE`（A2 注入）。

### 5.5 platform/android 租户骨架（A2 落地的占位）

`lib/wingman/src/platform/android/`：A1 只放入租户说明与 IInput/ICapture 的
stub 翻译层声明（`android_input.cpp`/`android_capture.cpp` 返回不可用），
`lib/wingman/CMakeLists.txt` 增加 `elseif(ANDROID)` 分支选入 —— 结构就位，
真实 JNI 桥接在 A2 填充（与 Kotlin 的 MediaProjection/AccessibilityService 对接）。

---

## 6. 构建体系

### 6.1 依赖（全部 vcpkg，遵守项目依赖管理规则）

```
vcpkg install --triplet arm64-android asio lua sol2 spdlog nlohmann-json
```

（sol2 头-only；lua 为 sol2 依赖。Android Gradle 与 vcpkg 工具链对接见 6.2。）

### 6.2 构建链路

```
gradle :app:assembleDebug
  └─ externalNativeBuild → NDK CMake (app/src/main/cpp/CMakeLists.txt)
       └─ -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
          -DVCPKG_TARGET_TRIPLET=arm64-android
          → libwingman_agent.so（JNI so，含 transport/RemoteClient/lua/glue）
```

- NDK 工具链由 AGP 提供，vcpkg 工具链文件叠加（vcpkg 官方支持的组合）。
- 远程 CI 的 Android job 与签名/发布流程属于 A3 工程化，A1 先本地构建。

### 6.3 环境要求（README 展开）

JDK 17+、Android SDK（API 34）、NDK r26+、vcpkg（android triplet 已 bootstrap）、
Go 1.2x（server 侧）。本仓库开发机（Linux）当前无 SDK/NDK，A1 的 Android 工程
以「代码交付 + 构建脚本」为准，Go 侧与桌面 C++ 回归在本机完整验证。

---

## 7. 生命周期与保活（A1 最小 + A3 完整约束）

| 场景 | A1 行为 | A3 目标 |
|------|---------|---------|
| 用户杀 App | Foreground 服务进程优先级高，不易被杀 | 保活三件套 |
| 崩溃 | `START_STICKY` 系统拉起 | 崩溃自重启 + 状态恢复 |
| 断网 | RemoteClient 指数退避重连（现成） | 重连后冲刷 outbox（现成）+ 脚本自治 |
| 厂商 ROM 杀后台 | 未处理（A1 文档引导用户加白名单） | 机型指引清单 |
| 脚本执行中断连 | 脚本继续跑，日志丢入 outbox 重连补发（RemoteClient outbox 现成） | 同左 + asset 缓存 |

---

## 8. 安全（A1 基线 + A3 演进）

- A1：与现有桌面 agent 相同的信任模型（局域网/内网部署，register 无鉴权）；
  脚本内容经 TLS 与否取决于 server 部署形态，与桌面一致，A1 不新增面。
- A3-P1（已落地）：register 增加 token 白名单校验（`WINGMAN_AGENT_TOKENS`，
  默认关闭、完全向后兼容），协议与端侧改动见
  `docs/agent-token-auth-design.md`；Android 侧 token 经 App 配置页写入
  SharedPreferences，随 configJson 传入 C++ 核心。
- A3-P2（演进）：per-agent token + Dashboard 管理与审计、token 迁移
  Android Keystore、challenge-response（需 NDK 引入 OpenSSL）与 TLS，
  见 `docs/agent-token-auth-design.md` §6。

---

## 9. 测试与验证

| 层 | 验证方式 | 本机可验 |
|----|----------|----------|
| Go 协议（content 下发/platform 注册） | `go test ./...` 新增用例 | ✅ |
| C++ 帧协议/重连 | transport_tests 既有 118 例（Android 编译同一份代码） | ✅（Linux 面） |
| ScriptRunner Lua 语义 | 抽为纯逻辑单测（注入 fake 回调），跑在 core_tests | ✅ |
| JNI 桥/Kotlin/Gradle | 需 Android SDK；A1 交付 + README，接手环境首次构建验证 | ❌ |
| 端到端链路 | 真机/模拟器 + Go Server 联调 | ❌（A1 验收步骤写入 README） |

ScriptRunner 的 Lua 执行与停止语义不依赖 Android（纯 sol2），因此把它的单测
放进桌面 core_tests（`script_runner_test.cpp`，用 Lua 5.5 源码编译，本机可跑）
—— A1 交付中唯一的新 C++ 逻辑由此获得真实测试覆盖。

---

## 10. A1 实施清单（本文对应提交的内容）

- [x] 本设计文档
- [x] Go：run_script content 下发 + AgentInfo.Platform + 测试
- [x] C++：ScriptRunner（含桌面可跑的单测）+ AndroidAgent 装配 + JNI 桥
- [x] apps/android 完整工程（Gradle/Kotlin/Manifest/NDK CMake）
- [x] lib/wingman：platform/android 租户 stub + CMake ANDROID 分支
- [x] apps/android/README.md（环境、构建、真机端到端验收步骤）
