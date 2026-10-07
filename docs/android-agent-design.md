# Android 端侧 Agent 设计（A1 落地版）

> 日期：2026-09-19。
> 前置调研：`docs/mobile-support-feasibility.md`（可行性、三模式对比、第三方方案调研）。
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
| A2 脚本能力（已实施，2026-09-21） | `platform/android` 宿主桥（dispatchGesture 手势注入）/MediaProjection 采集真实现，找色找图，`screenshot.capture` 远程截图 | 租户目录 + stub 骨架 |
| A3 可靠性（已落地，2026-09-30） | 开机自启、崩溃自重启、断连缓存自治、机型保活指引（见 §7 落地摘要；token 认证 P1 已于 2026-09-20 落地，见 §8） | 设计约束成文 |
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

原则（承可行性文档 4.3）：**系统权限与 Android 平台特有 API 的活全在 Kotlin；可移植的活
（通信/脚本/日志）全在 C++**。

### 5.4 Kotlin 壳

**WingmanService**（Foreground Service，`dataSync` 类型）：
- `onStartCommand`：读 SharedPreferences 配置（host/port/agentId）→ `nativeStart`
- `onDestroy`：`nativeStop`
- 崩溃自愈（A3 落地，2026-09-30）：`START_STICKY`（系统保证路径）+
  CrashRestartHandler 进程级崩溃处理（指数退避闹钟重启 + 崩溃串放弃上限，
  只在「崩溃前服务在跑」时复活）+ 服务内核心看门狗（30s 周期，核心不在跑
  时幂等重拉 `nativeStart`）
- 通知渠道：常驻「Wingman Agent 运行中」（Android 8+ 前台服务要求）

**MainActivity**：状态卡片（连接/脚本状态，来自 nativeStatus + onCoreStatus）、
启停按钮、服务器地址配置输入、无障碍权限引导入口（A2 用，A1 先放置）、
开机自启开关与机型保活指引入口（A3）、受限设置指引入口（A3 部署体验，
Android 13+ 且无障碍未启用时自动弹一次，`RestrictedSettingsPolicy` 判定）。

**AndroidManifest** 权限（A1/A2/A3 实际使用）：
`INTERNET`、`FOREGROUND_SERVICE`、`FOREGROUND_SERVICE_DATA_SYNC`（API 34 要求）、
`POST_NOTIFICATIONS`（前台服务通知）、`RECEIVE_BOOT_COMPLETED`（A3 开机自启）、
`BIND_ACCESSIBILITY_SERVICE`（A2 注入）。

### 5.5 platform/android 租户（A2 已实装）

`lib/wingman/src/platform/android/`：A1 只放入 stub 占位（已删除）；A2 实装为：

- `android_host_bridge.{hpp,cpp}`：`AndroidHostBridge` 抽象（tap/swipe/
  longPress/captureFrame/screenSize，全部**同步阻塞契约**）+ 全局 setter
  （`setGlobalHostBridge`，与 script/modules 的 `setGlobalRecorder` 同款）。
  lib/wingman 不依赖任何 JNI/Android 头；
- `android_capture.{hpp,cpp}`：`AndroidCaptureSource : ICaptureSource`，把桥的
  captureFrame 适配为通用采集源（region 裁剪），供脚本 API 与截图命令消费。

**偏离说明**：设计初期设想把 30 方法的鼠标语义 `IInput` 映射到 dispatchGesture；
A2 实施时确认这是无人消费的死代码路径（Android ScriptRunner 不走桌面模块
注册表），租户只放被消费的 HostBridge + CaptureSource，触屏语义经
`wingman.input.*` Lua API 直接暴露。

### 5.6 反向 JNI 桥与 A2 脚本能力（A2 实装）

**宿主桥模式**：`jni_bridge.cpp`（唯一 JNIEnv 翻译层）实现 `JniHostBridge`，
`nativeStart` 时注册进全局 setter。规则：

- **线程**：native 方法来自 JVM 已 attach 线程；C++ 脚本线程经 thread_local
  RAII（GetEnv → EDETACHED 才 AttachCurrentThread，析构仅 detach 自身 attach 的）；
- **类引用**：不用 FindClass（非主线程 FindClass 应用类走 system classloader
  会失败）——Kotlin 在 onServiceConnected 调 `nativeSetInputBridge(this)`，
  C++ GetObjectClass + GlobalRef + 缓存 jmethodID；
- **手势**：C++ `tap/swipe/longPress` → Kotlin `performGesture(x1,y1,x2,y2,
  durationMs)` 返回 seq → 主线程 `dispatchGesture` → GestureResultCallback →
  `nativeOnGestureResult(seq, completed)` 唤醒 C++ promise 等待；超时
  durationMs+2s 兜底；tap/longPress 为同点手势（duration 区分）；
- **采集**：帧走推送——ImageReader `onImageAvailable`（独立 HandlerThread）
  `acquireLatestImage` 取最新、拷平面字节、立即 close，`nativeOnFrame` 推给
  C++ 缓存；RGBA→BGRA 换序与 rowStride 逐行拷贝在 C++（`captureFrame` 纯读
  缓存，无 JNI）。屏幕尺寸随首帧建立（取帧前 getScreenWidth 返回 0）。

**权限 UX**（首次使用需两步引导，均在 App 内）：
无障碍设置开启 Wingman 注入服务 → App 点「开启投屏」→ 系统
MediaProjection 授权对话框 → 结果转发 WingmanService（API 34 硬约束：
先以 `mediaProjection` 类型 startForeground，再 getMediaProjection +
createVirtualDisplay，否则 SecurityException）。投屏授权单会话一次性
（Android 14+），每次重开投屏都会再弹系统对话框。

**脚本 API**（挂 ScriptRunner 的 `wingman` 表，与桌面同名同形，脚本可跨端；
桥缺失/投屏未授权时降级返回 false/nil，不抛错）：

```lua
wingman.input.click(x, y[, durationMs=60]) -> bool   -- >=500ms 走长按
wingman.input.swipe(x1,y1,x2,y2[, durationMs=300]) -> bool
wingman.input.delay(ms)                              -- 50ms 分片 + 停止中断
wingman.screen.getScreenWidth()/getScreenHeight() -> int
wingman.screen.getPixel(x,y) -> {r,g,b,a}
wingman.screen.capture() -> bool                     -- 仅报成功与否（桌面同形）
wingman.screen.findColor(color, region, tolerance=10) -> {point|nil, found}
wingman.screen.findColors(color, region, tolerance=10, maxCount=0) -> {{x,y}}
wingman.screen.findImage(path, region?, threshold=0.9) -> {point|nil, found}
wingman.vision.findColor(color[, tolerance=10][, region]) -> {x,y}|nil
wingman.vision.findAllColors(color[, tolerance][, region]) -> {{x,y}}
wingman.vision.hasColor(color[, tolerance][, region]) -> bool
wingman.vision.getDominantColor([region]) -> {r,g,b,a}  -- 均色近似桌面 kmeans k=1
wingman.vision.findImage(templatePath[, threshold=0.9][, region]) -> {found, position?, confidence?, region?}
```

color 接受 `0xRRGGBB` 整数或 `{r,g,b}` 表；findImage 相对路径按 configJson
`filesDir` 解析（app external files，`adb push` 可写）。找色找图走桌面同款
`ImageAnalyzer`（bitmap-first：每 API 调用取一帧再分析，与桌面 Vision 每次查找
重新截屏的结构差异——投屏取帧代价更高）。

**AI 视觉识别**：桌面侧已落地
`wingman.vision.aiSetup/aiSetupStatus/aiLocate/aiElements`
（OpenAI 兼容 provider，截帧→JPEG→视觉模型→像素包围盒，aiElements 一次
调用批量取元素，凭据走 `crypto.encryptAES` 加密面，详见 docs/api/vision.md
「AI 视觉识别」节）。Android 侧已接入（2026-10-07，桌面同源单测绿，真机
走查证据见 development-todo 独立项）：同形四函数挂 `android_script_api.cpp`
（无 OpenCV 构建注册桌面 vision_stub 同语义假体）；截帧经
`VisionAi::setFrameProvider` 注入 captureFrame 适配——NDK 下公共层
`Screen::capture` 无 `__ANDROID__` 装配（平台宏冻结不新增），注入点让
captureFrame 位图复用同一 provider 通路。
HTTP/解密按 cherry-pick 模式收口：`http.cpp`（`WINGMAN_HAS_CURL`）与
`crypt.cpp` 进 androidagent 目标，android vcpkg 清单补 `curl`/`openssl`
（与 opencv4 同款清单必含），缺失时 CMake 明确报错不降级。

**远程截图**：AndroidAgent 响应 `screenshot.capture`（与桌面 screenshot_handler
同形：region 参数、4K clamp、BGRA→BGR→JPEG q82→base64 data URI），Go server
workflow 的 screenshot 步骤零改动覆盖 Android 设备。displayId 接受即忽略。

---

## 6. 构建体系

### 6.1 依赖（全部 vcpkg，遵守项目依赖管理规则）

```
vcpkg install --triplet arm64-android asio lua sol2 spdlog nlohmann-json opencv4
```

（A2 起 NDK 增加 opencv4：`default-features:false` + jpeg/png 等，见
`apps/android/cpp/vcpkg.json`——关默认特性避免拉 GUI 面。sol2 头-only；
lua 为 sol2 依赖。Android Gradle 与 vcpkg 工具链对接见 6.2。）

### 6.2 构建链路

```
gradle :app:assembleDebug
  └─ externalNativeBuild → NDK CMake (cpp/CMakeLists.txt)
       └─ -DCMAKE_TOOLCHAIN_FILE=cpp/vcpkg-android.cmake（入口 helper）
            ├─ set VCPKG_CHAINLOAD_TOOLCHAIN_FILE=<ANDROID_NDK>/build/cmake/android.toolchain.cmake
            ├─ include $VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake（manifest 安装依赖）
            └─ vcpkg.cmake 内部 chainload NDK 工具链 → sysroot/编译标志齐全
          -DVCPKG_TARGET_TRIPLET=arm64-android
          → libwingman_agent.so（JNI so，含 transport/RemoteClient/lua/glue）
```

- NDK 工具链由 AGP 提供；vcpkg **不自动加载** NDK 工具链（官方 triplet 亦然），
  须经入口 helper 显式 chainload。直接把 CMAKE_TOOLCHAIN_FILE 指向 vcpkg.cmake
  会导致 ANDROID_* 变量失效、无 sysroot，CMake 不产出 File API toolchains
  对象，AGP 解析 reply 报 "sysroot has not been initialized"。
- 远程 CI 的 Android job 与签名/发布流程属于 A3 工程化，A1 先本地构建。

### 6.3 环境要求（README 展开）

JDK 17+、Android SDK（API 34）、NDK r26+、vcpkg（android triplet 已 bootstrap）、
Go 1.2x（server 侧）。本仓库开发机（Linux）自 2026-09 起具备 SDK/NDK，可本地跑
JVM 单测（`gradle :app:testDebugUnitTest`，不依赖 NDK/vcpkg）；完整 APK 组装
（NDK + vcpkg arm64-android）由 nightly CI 的 build-android job 验证
（该 job 自 A3 起先跑 JVM 单测再打 APK）。

---

## 7. 生命周期与保活（A1 最小 + A3 完整约束）

A3 可靠性剩余项于 2026-09-30 落地。逐场景的实现归属：

| 场景 | A1 行为 | A3 落地（2026-09-30） |
|------|---------|---------|
| 用户杀 App | Foreground 服务进程优先级高，不易被杀 | 前台服务优先级 + START_STICKY 系统拉起（系统保证路径）+ 厂商 ROM 白名单指引（见下） |
| 崩溃 | `START_STICKY` 系统拉起 | 三层：START_STICKY（系统保证）+ CrashRestartHandler/AlarmReceiver 指数退避加速重启（1s→60s 封顶；10 分钟窗口内连崩 5 次放弃，防崩溃风暴）+ 核心看门狗（服务存活但 C++ 核心不在跑时 30s 周期幂等重拉）。**状态恢复**：配置持久化于 SharedPreferences、null-intent 重建链路现成；仅在「崩溃前服务在跑」时复活（`coreRunning` 门控）；MediaProjection 授权系统约束单会话一次性、不可恢复（A2 已登记） |
| 断网 | RemoteClient 指数退避重连（现成） | C++ 侧现成（agentcore_test 覆盖：重连后退避、断连事件入 outbox、重连冲刷、超容量丢弃）+ 脚本断连自治（执行线程独立于连接）——A3 无 C++ 改动，App 侧以核心看门狗补「服务在跑但核心不在跑」的自愈 |
| 厂商 ROM 杀后台 | 未处理（A1 文档引导用户加白名单） | `docs/guides/android-keep-alive.md`（小米/华为/OPPO/vivo/三星逐机型步骤 + 验证方法 + 已知边界）+ App 内「机型保活指引」对话框 |
| 脚本执行中断连 | 脚本继续跑，日志丢入 outbox 重连补发（RemoteClient outbox 现成） | 同左；asset 缓存归 A4 |
| 开机/覆盖安装 | 无 | BootCompletedReceiver（BOOT_COMPLETED + MY_PACKAGE_REPLACED，均在系统后台 FGS 启动豁免名单内）+ 用户开关（**默认关、显式开启**——2026-09-30 按指令校正；原落地轮「默认开」为登记假设，特权行为须用户显式勾选）+ 已配置服务器地址才放行（`BootStartGate.DEFAULT_ENABLED` 单一来源） |

实现要点（Kotlin 壳内，全部本地组件、零新增网络面）：

- **纯逻辑与框架分离**：退避/放弃策略（`RestartPolicy`）、开机放行判定
  （`BootStartGate`）、看门狗判定（`WatchdogPolicy`）为无 Android 依赖的
  纯 Kotlin 对象，JVM 单测直测（`app/src/test`，15 例）；Receiver/Service
  只做薄壳装配。
- **崩溃自重启的诚实边界**：AlarmManager 调度是 best-effort——API 31+
  后台 FGS 启动限制下 `startForegroundService` 可能被系统拒绝（捕获并
  记录），系统 START_STICKY 路径兜底；设备 Doze 下 `setAndAllowWhileIdle`
  的触发精度受系统节流影响。
- **崩溃退避的清零语义**：用户启动 / 开机 / 覆盖安装路径清零崩溃串
  （新的使用意愿）；崩溃闹钟路径保持累积（防风暴）。

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
- A3 可靠性组件（2026-09-30，§7）不改变信任模型：开机/崩溃接收器、
  AlarmManager 与看门狗全部为本地组件，不新增监听端口、不新增网络出向
  目标，不参与 register/鉴权链路。

---

## 9. 测试与验证

| 层 | 验证方式 | 本机可验 |
|----|----------|----------|
| Go 协议（content 下发/platform 注册） | `go test ./...` 新增用例 | ✅ |
| C++ 帧协议/重连 | transport_tests 既有 118 例（Android 编译同一份代码） | ✅（Linux 面） |
| ScriptRunner Lua 语义 | 抽为纯逻辑单测（注入 fake 回调），跑在 core_tests | ✅ |
| A2 脚本能力 API（input/screen/vision） | FakeHostBridge 直测（合成帧找色/手势透传/降级），跑在 lua_tests | ✅ |
| A3 可靠性纯逻辑（RestartPolicy/BootStartGate/WatchdogPolicy） | Kotlin JVM 单测 15 例（`gradle :app:testDebugUnitTest`，org.json 以 Maven 真实现入测试类路径） | ✅（2026-09-30 起） |
| A3 部署体验纯逻辑（RestrictedSettingsPolicy） | Kotlin JVM 单测 6 例（API 门槛边界 + 提示矩阵） | ✅（2026-09-30 起） |
| A3 受限设置预授权脚本（scripts/android-restricted-settings.sh） | Go 契约测试 11 例（假 adb 驱动、净化 PATH，`orchestrator/server/integration/`，不碰真设备） | ✅ |
| 断连缓存自治（outbox/重连冲刷） | agentcore_test 既有 28 例（Android 编译同一份代码） | ✅（Linux 面） |
| JNI 桥/Kotlin/Gradle | 需 Android SDK；A1 交付 + README，接手环境首次构建验证 | ❌ |
| 端到端链路 | 真机/模拟器 + Go Server 联调 | ❌（A1 验收步骤写入 README） |

ScriptRunner 的 Lua 执行与停止语义不依赖 Android（纯 sol2），因此把它的单测
放进桌面 core_tests（`script_runner_test.cpp`，用 Lua 5.5 源码编译，本机可跑）
—— A1 交付中唯一的新 C++ 逻辑由此获得真实测试覆盖。

---

## 10. A2 实施摘要（2026-09-21）

- C++ 租户：`AndroidHostBridge` 抽象 + `AndroidCaptureSource`；Bitmap 纯核心
  自 screen.cpp 抽至 bitmap.cpp（零平台宏，NDK/lua_tests 轻量链接；screen.cpp
  两处 linux guard 加 `!defined(__ANDROID__)`，桌面行为不变）
- 脚本 API：`android_script_api.cpp` 挂 wingman.input/screen/vision；测试
  `libs/lua/tests/android_api_test.cpp`（FakeHostBridge）
- 反向 JNI 桥：jni_bridge.cpp 增 JNI_OnLoad/JniEnvGuard/JniHostBridge 与
  nativeSetInputBridge/nativeOnGestureResult/nativeOnFrame
- Kotlin：WingmanAccessibilityService 实装 dispatchGesture；新增
  ScreenCaptureManager（MediaProjection/VirtualDisplay/ImageReader 生命周期）；
  WingmanService 投屏动作与 startForeground 类型时序；MainActivity 投屏授权流
- 远程截图：`android_screenshot.cpp`（与桌面同形），AndroidAgent 分发
- 构建：NDK cherry-pick lib/wingman 子集 + opencv4（关默认特性）
- 真机验收：见 apps/android/README.md A2 步骤（息屏不出帧是 A2 已知前提，
  息屏采集/保活归 A3）

---

## 10'. A3 可靠性实施摘要（2026-09-30）

- Kotlin 壳：`WingmanApplication`（进程级崩溃处理器安装）+ 
  `CrashRestartHandler`/`CrashAlarmReceiver`（退避闹钟重启，`coreRunning`
  门控只复活「崩溃前在跑」的服务）+ `BootCompletedReceiver`（开机/覆盖安装
  自启，`BootStartGate` 放行）+ `WingmanService` 核心看门狗 + 
  `MainActivity` 开机自启开关与保活指引对话框
- 纯逻辑对象 `RestartPolicy`/`BootStartGate`/`WatchdogPolicy`（零 Android
  依赖）+ JVM 单测 15 例（`app/src/test`，JUnit 4；org.json 以 Maven 真实现
  入测试类路径——android.jar 对其只部分真实实现）
- Manifest：`RECEIVE_BOOT_COMPLETED` 权限启用、两 Receiver 注册
  （exported=false，系统保护广播可送达）
- CI：build-package 的 build-android job 在打 APK 前先跑
  `gradle :app:testDebugUnitTest`（此前 Kotlin 逻辑无任何 CI 门禁）
- 机型保活指引：`docs/guides/android-keep-alive.md`
- 断连缓存自治：C++ 侧（RemoteClient 重连/outbox/脚本自治）自 A1 起现成、
  agentcore_test 28 例覆盖，本轮零 C++ 改动；App 侧以看门狗补进程内自愈
- 遗留登记：MediaProjection 授权崩溃后不可恢复（系统约束）；A3-P2 安全
  演进与 A4 多设备编排见 §1.2 与 agent-token-auth-design.md §6

---

## 10''. A3 受限设置引导实施摘要（2026-09-30）

- **范围**：A3「可靠性与部署体验」第二项——Android 13（API 33）起侧载
  App 的无障碍被「受限设置」默认屏蔽，是端侧开箱失败最高来源
  （mobile-support-feasibility.md §5.2，风险表评级：高）。三档解法
  （手动允许 / adb 预授权 / Device Owner）一次落地。
- **脚本**：`scripts/android-restricted-settings.sh check/allow/revoke/status`
  ——幂等 adb 预授权（allow 后复核）、三态约定同 verify-\*.sh（无 adb/
  无设备 SKIP、包未装 FAIL、API<33 PASS「不受约束」）、`tr -d '\r'` 处理
  adb shell 的 CRLF 行尾、`WINGMAN_ANDROID_PKG` 贯穿全部命令。
- **契约护栏**：`orchestrator/server/integration/android_restricted_settings_script_test.go`
  11 例——假 adb（状态文件驱动 + calls 调用记录 + CRLF 输出）+ 净化 PATH，
  全路径不碰真设备；与 guacd 脚本契约测试同框架（bash 3.2 兼容口径）。
- **App 内引导**：`RestrictedSettingsPolicy`（纯逻辑：API≥33 且无障碍未启用
  且未确认 → 提示）+ MainActivity 自动弹一次（`restrictedHintAck` prefs 键，
  弹过即落 ack）+ 常驻「受限设置指引」按钮（三档解法全文）。诚实边界：
  公开 API 无法区分「被受限设置挡住」与「未开启」，按口径触发；
  「无障碍失效检测上报」（依赖 device.capabilities 预留槽位）登记未做。
- **adb 边界（设计决策 D7 复述）**：adb 只出现在部署/一次性授权路径——
  即本脚本与手册，任何运行时链路零 adb 依赖。
- **手册**：`docs/guides/android-restricted-settings.md`（症状识别/三档
  解法/验证/已知边界，入文档站「进阶指南」，与保活指引互链）。
- **测试**：JVM 单测 +6（21 例全绿）、Go 契约 11 例全绿；五厂商定制 ROM
  真机逐机型验收登记未做（本机无真机）。

---

## 10'. A1 实施清单（本文对应提交的内容）

- [x] 本设计文档
- [x] Go：run_script content 下发 + AgentInfo.Platform + 测试
- [x] C++：ScriptRunner（含桌面可跑的单测）+ AndroidAgent 装配 + JNI 桥
- [x] apps/android 完整工程（Gradle/Kotlin/Manifest/NDK CMake）
- [x] lib/wingman：platform/android 租户 stub + CMake ANDROID 分支
- [x] apps/android/README.md（环境、构建、真机端到端验收步骤）
