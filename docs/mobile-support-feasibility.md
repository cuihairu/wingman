> [中文](mobile-support-feasibility.md) | **English**

# Mobile Support Feasibility Analysis (Android On-device Agent First)

> Analysis date: 2026-09-19.
> The question being answered: can Wingman support Android / iOS the way Auto.js / Appium do?
> The core premise: **run the Agent on the phone and connect straight to the Go Server over a TCP long-lived connection** (cloud-control mode),
> rather than Auto.js's standalone mode.

---

## 1. Conclusion at a Glance

| Target form | Feasibility | Conclusion |
|---------|--------|------|
| **Android on-device Agent + long-lived connection to Go Server (cloud-controlled)** | ✅ High | **Recommended main line**. There is a commercial precedent (Hamibot is Auto.js + cloud control), and it fits Wingman's existing architecture far better than Auto.js standalone mode |
| Android host-controlled (PC + adb, Appium model) | ✅ High | An alternative/supplementary route, usable as a PoC entry point, but not the target form |
| iOS on-device (any form) | ❌ | **Not feasible**. The App Store sandbox does not allow cross-app injection and there is no legitimate channel; abandoned |
| iOS host-controlled (PC → WDA) | ⚠️ Medium | Feasible but tied to the Mac signing chain (developer certificate + Developer Mode + WDA needing adaptation to each major iOS version); listed as a long-term option |

**Core judgment**: put the control plane (script management, scheduling, orchestration, monitoring) on the Server, and keep the execution plane (the real-time decisions of capture → find image → inject) local on-device — this split both satisfies the real-time requirement (the latency and bandwidth of returning every frame to the Server for a decision is unacceptable) and fully reuses Wingman's existing "Runtime-as-Agent + Go Server as central controller" architecture.

---

## 2. Survey of Third-Party Solutions

### 2.1 Android: two technical routes

| Route | Representative | Mechanism | Root-free |
|------|------|------|---------|
| On-device autonomy (standalone) | Auto.js / AutoX / AutoJs6 | `AccessibilityService`: the system pushes the full-screen widget tree, and a script engine (Rhino/JS) injects gestures via `dispatchGesture` | ✅ |
| On-device + cloud control | **Hamibot** (built on Auto.js), autojs pro9 cloud-control fleet-control systems | An account system on top of standalone capability: the Server delivers scripts → the phone pulls and executes them; supports boot auto-start, crash restart, and grouped-device scheduling | ✅ |
| Host-controlled | Appium UiAutomator2 / openatx uiautomator2 | A UiAutomator test process runs on the device exposing HTTP RPC (atx-agent :7912), invoked from the PC through an adb tunnel; low-latency injection uses minitouch, frame capture uses scrcpy | ✅ |

**There are only two keys to being Root-free: AccessibilityService (on-device) or adb (host-controlled).**

### 2.2 iOS: WDA is the only legitimate channel

- What the Appium XCUITest driver does: use Xcode to sign and deploy WebDriverAgent (WDA, a test-runner App) to a real device, where WDA runs an HTTP service (:8100) exposing XCTest capabilities. On iOS 17+ it is launched via `devicectl`, and on 18+ via RemoteXPC
- Hard constraints: an Apple developer certificate + provisioning profile (free certificates expire after 7 days), device Developer Mode, and the sign-and-deploy step being bound to Mac; major iOS version upgrades often require recompiling WDA
- WDA is a foreground test session and cannot stay resident in the background; **there is no legitimate channel for on-device autonomy on iOS** (App Store auto-clickers are sandbox-limited to controlling only their own internals)
- The iOS edition of 按键精灵 uses a similar channel, and its old jailbreak support only goes up to iOS 14

### 2.3 The Specifics of Game Scenarios (favorable to Wingman)

Auto.js's strength is the **widget tree**, but mobile games are rendered surfaces — the accessibility tree cannot reach controls inside the game. Game automation is naturally a "color search / image search + coordinates + humanized simulation" model — which is exactly Wingman's home turf (pixel detection / OpenCV template matching / pyramid acceleration / Bézier input / random delays). **Migrating to phones is playing at home, and Auto.js's widget-tree advantage cannot be leveraged in game scenarios at all.**

---

## 3. Comparison of the Three Modes

| Dimension | Auto.js standalone | **Wingman cloud control (this proposal)** | Appium host-controlled |
|------|-------------|--------------------------|---------------|
| Script storage / editing | Local on the phone | **Centrally managed on the Server, delivered for execution** | Local on the PC |
| Execution location | On the phone | **On the phone (decisions also local)** | Decisions on the PC, execution on the phone |
| Continuing execution after network loss | ✅ (it is standalone anyway) | ✅ (the Agent caches the current script and runs autonomously, reporting back after reconnecting) | ❌ stops when the PC disconnects |
| Multi-device orchestration / fleet control | ❌ weak | **✅ reusing Go Server orchestration + Team/inbox** | Requires building your own device management |
| Monitoring / log aggregation | ❌ local | **✅ real-time view in the Dashboard** | Requires building your own |
| Does each phone need a PC | No | **No** | Yes |
| Real-time decision latency | Lowest | Lowest (decisions on-device) | Affected by the frame-return link |
| Fit with Wingman's architecture | None | **Native fit (Runtime-as-Agent)** | Requires substantial rework |

The cloud-control model has already been validated commercially: Hamibot (Auto.js + accounts + a script marketplace + delivery-and-execution), and various autojs pro9 cloud-control fleet-control systems (boot auto-start, crash auto-start, app-switch-away auto-start); the target customer base overlaps with Wingman's (multi-instance farms, game studios).

---

## 4. Recommended Architecture: On-device Agent with a Long-lived Connection to the Go Server

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

### 4.1 Mapping of Existing Components for Reuse

| Existing component | Role in the Android on-device Agent | Estimated change |
|---------|------------------------------|---------|
| `libs/transport` client (TCP long-lived connection, 16-byte binary header + JSON body envelope (the naming authority is `docs/protocols.md`), heartbeat, auto-reconnect, register/agent_id) | The Agent ↔ Go Server communication layer | **Almost zero change**, compiled straight to Android |
| Go Server session management (AgentInfo / kGetAgents / heartbeat timeout) | Device onboarding | A small number of new message types (see 4.2) |
| Lua engine (libs/lua) | On-device script execution | NDK cross-compilation |
| vision (OpenCV color search / image search, pyramid, LRU cache) | The core capability for game scenarios | arm64-android build (NEON acceleration) |
| `ICapture` / `IInput` / `IScreen` / `IWindow` platform abstraction interfaces | New `platform/android/` backend | **New implementation** (JNI bridging to the Kotlin layer, see 4.3) |
| event / fsm / task / timer / orchestration / clip modules | Script-layer API | Zero change |
| Trigger system / macro record & replay / humanized simulation | Reused directly on-device | Minor adaptation (recording depends on injecting the event source) |
| Dashboard | The entry point for device management / monitoring | Add a device view (the device list already exists on the Agents page) |
| Python engine (libs/python) | Deferred on-device | Python+NDK carries high size/maintenance cost, so **Lua-first**; evaluated later as needed |

### 4.2 Protocol Extension Points (reusing the existing TCP protocol, no architecture change)

The current protocol (`docs/protocols.md`) has no numeric "business code" concept: frame types take only four values, Request/Response/Notify/Error, and application-level messages are distinguished by a string `type` inside the body, dispatched by type on the server side in `internal/agent/listener.go` (existing Notify types such as `agent.*` / `team.*` / `inbox.*` / `proxy.*`). Extending means adding one string type and one dispatch case:

- Script delivery: Request `run_script` (content inlined) — **already landed** (A1, see `docs/android-agent-design.md` §3.2); incremental version sync of the `script.pull` type is reserved (not implemented)
- `device.capabilities` capability reporting: the register capabilities field is already passed through; actively reporting changes is reserved (not implemented)
- `asset.sync` template image / asset sync: reserved (not implemented, A4)
- On-device log / screenshot event reporting: Notify `agent.event`{event:"script_output"} — **served by the existing mechanism** (landed with A1, forwarded by the server to the Dashboard)
- Registration / heartbeat: reuses the existing Notify `agent.register` / `agent.heartbeat` (`agent.register_ack` replies; constants such as kRegister/kSyncTask/kShutdown do not exist)

### 4.3 New Components: the Android Shell and the JNI Bridge

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

Layering principle: **everything touching system permissions (accessibility, screen-projection authorization) lives in the Kotlin shell; everything portable (scripting, vision, orchestration, communication) lives in the C++ core**. The JNI bridge exposes only two narrow interfaces: `captureFrame() -> bitmap` and `inject*()`.

---

## 5. Android Key Technical Points and Countermeasures

### 5.1 Long-Lived Connection Keep-alive

| Limitation | Version | Countermeasure |
|------|------|------|
| Background execution limits (Doze, background services) | Android 8+ | Foreground service (persistent notification, industry practice: IM/push apps all do this) |
| A foreground service cannot be started from the background | Android 12+ | Boot auto-start goes through the `BOOT_COMPLETED` exemption; crash restart relies on in-service self-recovery rather than background wake-up |
| Foreground services must declare a type | Android 14+ | Declare the `connectedDevice`/`dataSync` type and request the corresponding runtime permission |
| Aggressive vendor ROM background killing (MIUI/EMUI/ColorOS) | All versions | The self-healing trio: boot auto-start + service crash self-restart + exponential-backoff reconnect on disconnect (`libs/transport` already has auto-reconnect) |
| TCP connections cut by middleboxes | All versions | Heartbeat keep-alive (existing `startHeartbeat`), with a smart heartbeat interval when necessary |

### 5.2 Permission Friction (important, affects out-of-the-box experience)

- **Android 13+ restricted settings**: accessibility permission for sideloaded apps is blocked by default by the system. Solutions (already landed, see `docs/guides/android-restricted-settings.md`):
  1. Manual by the user: App info → ⋮ → "Allow restricted settings" → then enable accessibility
  2. Bulk deployment scenarios: pre-authorize with `adb shell appops set <pkg> ACCESS_RESTRICTED_SETTINGS allow` (already scripted: `scripts/android-restricted-settings.sh check/allow/revoke/status`, idempotent, guarded by a Go contract test)
  3. Enterprise/studio bulk onboarding: silent authorization in Device Owner mode (the general MDM practice, and also the standard path for fleet-control systems)
  The two-level in-app guidance (auto-popped once on Android 13+ when accessibility is not yet enabled, plus a persistent button) landed with A3's deployment experience (2026-09-30).
- **MediaProjection**: every session requires the user to confirm the screen-projection dialog (it can be sustained by pairing it with the `mediaProjection` foreground service type); guiding the user once is enough
- **Accessibility service toggle**: it can break after an OS update or reboot, so the Agent needs to detect this and report it to the Server to remind the user

### 5.3 Choice of Screenshot Scheme

| Scheme | Frame rate | Limitations | Use |
|------|------|------|------|
| MediaProjection + VirtualDisplay | 30-60fps | Session-level authorization dialog | **Primary scheme**: on-device real-time image search / color search |
| AccessibilityService.takeScreenshot | Rate-limited (about 1 per second) | API 30+ | Fallback / low-frequency scenarios (a candidate scheme, not implemented — a `mobile-automation-design.md` D3 planned item) |
| Widget-tree screenshot (none) | — | — | Widget information goes directly through the accessibility node tree, no screenshot needed |

### 5.4 Input Injection

- `AccessibilityService.dispatchGesture` / `GestureDescription`: multi-touch and Bézier paths are natively supported (continuous strokes), and they map well onto the humanized simulation module (Bézier movement, random jitter)
- Injected events carry a system flag (generated gesture), which some anti-cheat systems can detect — **humanized simulation is a hard requirement here rather than a bonus**, and the documentation should state this explicitly

### 5.5 Porting the C++ Core

- vcpkg supports the `arm64-android` triplet; the project is currently locked to `x64-windows-static`, so the triplet matrix needs to be extended (add an Android job to CI)
- OpenCV / asio / spdlog all compile for Android; Lua is small and dependency-free, so it has the highest priority
- **Lua-first**: on-device script execution supports only Lua initially; Python+NDK carries high size and maintenance cost and is evaluated later as needed (the dual-language layering decision is already recorded in `docs/architecture-decisions.md` under "Scripting Language Strategy": Python first on the desktop, Lua first on mobile, both languages kept long term)

---

## 6. Compliance Check Against the Architecture Hard Constraints

| # | Hard constraint | On-device Agent proposal | Compliant |
|---|--------|----------------|------|
| 1 | The Go Server is the remote central control orchestrator | Role unchanged, adds device onboarding / script delivery | ✅ |
| 2 | Runtime/Agent actively connects outbound to the Go Server | The Android Agent dials out with a long-lived connection, behaving like the existing runtime | ✅ |
| 3 | The local Tauri UI controls the runtime over local IPC | No local UI on Android (an accessibility floating-window debugging tool can be a long-term enhancement) | ✅ |
| 4 | Runtime is forbidden from introducing an HTTP/WebSocket server | The Agent only has an outbound TCP client; **if WDA is integrated in the future, WDA's HTTP service runs on the device and the Agent is the client** (recorded in the architecture decision document) | ✅ |
| 5 | Dashboard/remote clients connect only to the Go Server | Unchanged, the Dashboard's new device view goes through the Server API | ✅ |

---

## 7. Risk List

| Risk | Level | Mitigation |
|------|------|------|
| Android 13+ restricted settings cause a high out-of-the-box failure rate | High | Documented guidance + adb pre-authorization script + Device Owner bulk deployment option (✅ landed 2026-09-30, see §5.2 and docs/guides/android-restricted-settings.md) |
| Vendor ROM background killing / blocked auto-start | High | The self-healing trio (5.1); maintain "keep-alive settings guidance" per device model |
| Mobile game anti-detection (generated gesture flags, screen-projection icon) | Medium | Strengthen humanized simulation; clarify the product positioning and compliance statement |
| Major iOS version changes | — | iOS is only a long-term option and does not carry main-line cost |
| Size: NDK artifacts of OpenCV + Lua + transport | Low | Trim OpenCV modules as needed (imgproc/core is sufficient) |
| Workload: Kotlin shell + JNI + CI across three platforms | Medium | Phase it (see 8), with A1 PoC validating the long-lived connection and script delivery path first |

---

## 8. Phased Roadmap

- [x] **A1 PoC: bring up the link** (✅ implemented 2026-09-19; the engineering design and acceptance steps are in `docs/android-agent-design.md`; the full engineering work is in `apps/android/`)
  - [x] Kotlin shell: ForegroundService + long-lived connection (reusing libs/transport + RemoteClient compiled to Android)
  - [x] The portable surface of the C++ core is in place (transport + RemoteClient + Lua/Sol2 + ScriptRunner; lib/wingman proper compiling for Android comes in with A2, with the tenant directory and build branch already in place)
  - [ ] End-to-end acceptance on a real device (requires an Android SDK/NDK environment; steps in apps/android/README.md)
- [x] **A2 scripting capability: automation becomes usable** (✅ implemented 2026-09-21; design and acceptance steps in `docs/android-agent-design.md` §5.5/§5.6 and the `docs/android-agent.md` milestones)
  - [x] `platform/android/` IInput backend (dispatchGesture, JNI)
  - [x] `platform/android/` ICapture backend (MediaProjection as the primary channel; the takeScreenshot fallback is not part of the A2 delivery and is unimplemented — a `docs/mobile-automation-design.md` D3 planned item)
  - [x] Color search / image search / pixel detection work on phone frames; the screen/input script APIs are fully wired
  - [ ] The trigger system running on-device (timed / pixel triggers) — not included in the A2 scope, still unimplemented (same convention as `mobile-automation-design.md` §5 "on-device bring-up (post-A2)")
- [ ] **A3 reliability and deployment experience**
  - [x] Boot auto-start, crash self-restart, autonomous operation while disconnected (cached script keeps executing, reports after reconnecting) (✅ 2026-09-30, 6cecfad)
  - [x] Restricted-settings guidance + adb pre-authorization script (✅ 2026-09-30: the script + in-app guidance + the manual `docs/guides/android-restricted-settings.md`)
  - [ ] Reporting of accessibility failures (depends on the reserved device.capabilities slot, not done, logged)
  - [ ] Template image asset.sync, script version management
- [ ] **A4 multi-device orchestration**
  - [ ] Integrating the Team/inbox module into the on-device Agent
  - [ ] Dashboard device view (grouping, bulk delivery, status overview)
- [ ] **I1 (long-term option): iOS host control** — a WDA backend for PC-side ICapture/IInput (usbmuxd/libimobiledevice), dependent on the Mac signing chain

---

## 9. References

- [Auto.js forks (GitHub)](https://github.com) / AutoX / [AutoJs6 guide (CSDN)](https://blog.csdn.net)
- [Hamibot official site](https://hamibot.com) (the commercial precedent of Auto.js + cloud control), [examples of cloud-control fleet-control systems (CSDN)](https://blog.csdn.net)
- [uiautomator2 (openatx, GitHub)](https://github.com), [pyminitouch + scrcpy Root-free solution (CSDN)](https://blog.csdn.net)
- [Appium XCUITest driver: running preinstalled WDA](https://appium.github.io/appium-xcuitest-driver/latest/guides/run-preinstalled-wda), [capability reference (iOS 17+ devicectl / 18+ RemoteXPC)](https://appium.github.io/appium-xcuitest-driver/12.12/reference/capabilities), [real device requirements](https://appium.readthedocs.io/en/latest/en/drivers/ios-xcuitest-real-devices)
- [WDA architecture analysis (trinhngocthuyen.com)](https://trinhngocthuyen.com/posts/tech/mobile-e2e-wda), [Introduction to Appium Drivers](https://appium.io), [iOS automation tooling 2026 comparison (drizz.dev)](https://www.drizz.dev/post/ios-automation-testing-tools-in-2026)
- [Official Android: restricted settings (Google Help)](https://support.google.com/android/answer/12623953?hl=zh-Hans), [background foreground-service start restrictions](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start)
- [按键精灵 iOS jailbreak-free forum](https://bbs.vrbrothers.com)