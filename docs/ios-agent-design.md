> [中文](zh/ios-agent-design.md) | **English**

# iOS Support Design (I1 host control, long-term optional)

> Date: 2026-09-19.
> Prior conclusion: `docs/mobile-support-feasibility.md` §2.2/§5 — **an iOS on-device agent is not feasible in any form** (the App Store sandbox forbids cross-app injection, no legal execution channel); the only legal path is **iOS host control (PC → WebDriverAgent)**, tracked as long-term optional I1. This document gives the engineering design for I1 — **this delivery is design only, no implementation**; whether implementation starts depends on a real need appearing (an iOS testing/idle-farming scenario with a Mac signing chain).

---

## 1. Positioning and Model

### 1.1 The essential difference from the Android on-device line

| | Android (A line) | iOS (I1) |
|---|---|---|
| Execution plane location | On the device (the C++ core runs on the phone) | **PC host** (inside the existing desktop runtime) |
| iOS device role | — | A controlled peripheral (like a machine with a monitor + keyboard/mouse attached) |
| Long-lived connection | Device → Go Server | The iOS device does not participate in networking; PC runtime → Go Server stays as-is |
| Channel | Outbound TCP | usbmuxd/RemoteXPC + WDA (HTTP over USB, loopback only) |
| Signing requirements | None | Developer certificate + provisioning profile + device Developer Mode |
| Architecture constraint check | Agent outbound ✓ | **The runtime adds no listening surface at all**: WDA itself is a local service over USB and the runtime is its client; constraint 4 (runtime must not host an HTTP/WS server) is not broken |

Conclusion: I1 is not porting Wingman to iOS — it adds **an iOS device backend to the desktop runtime's ICapture/IInput**. The device = just another kind of "screen".

### 1.2 Why WDA

The only legal channel for iOS automation is the XCTest framework. WDA is the community packaging of Apple's official test runner (the same one the Appium XCUITest driver uses); on a real device it exposes XCTest capabilities:

- Screenshot: `GET /screenshot` (JPEG/PNG)
- Touch injection: `/session/{id}/actions` (W3C actions, supports swipe/long-press/multi-finger)
- Element lookup/input: `/elements`, `/element/{id}/value` (XCUITest element tree)
- Launching: iOS 17+ `devicectl`, iOS 18+ RemoteXPC; WDA must be recompiled for each iOS major version

Appium and the iOS edition of 按键精灵 both use this channel; the jailbreak route (only old versions) does not fit this project's compliance boundary and is not pursued.

---

## 2. Architecture

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

- **Minimal new surface**: the runtime gains two IBackends + one device manager; vision/color-and-image matching reuses existing modules (the screenshot lands in local memory, matching happens on the PC, same path as the desktop).
- **Not in the A-line mainline**: Android's cloud-control protocol (run_script/agent.event) has nothing to do with iOS; an iOS session is a local capability of the PC runtime — from the Go Server's perspective it is just an ordinary desktop agent.

---

## 3. Component Design

### 3.1 WDA connection management (`platform/ios/wda_client`)

- Device discovery: `libimobiledevice` (usbmuxd protocol) enumerates UDIDs; or `devicectl list`.
- WDA start/stop: signed installation is a **manual/scripted prerequisite** (certificate issues cannot be handled automatically); the runtime only connects to an already-running WDA (:8100) through usbmuxd port forwarding.
- Session: `POST /session` establishes the session, heartbeats keep it alive; reconnect on drop.

### 3.2 ICapture backend (WdaCaptureBackend)

- `capture()` → `GET /screenshot` → decode into wingman::Bitmap;
- Expected frame rate: 200-400ms/frame over USB (JPEG codec + USB round trip), which only sustains find-color/find-image pacing (1-3 fps) and does not support recording/high-performance scenarios — a different positioning from desktop GDI capture, to be stated clearly in the A-face documentation.

### 3.3 IInput backend (WdaInputBackend)

- Tap/swipe/long-press/text input → W3C actions JSON;
- Coordinate system: WDA returns device pixels, same source as capture, no DPI conversion issue;
- Coordinate calibration: scripts use relative ratio coordinates (template-match centers normalized), avoiding rewrites across resolutions.

### 3.4 Script API surface

Identical to the desktop (`wingman.click/scroll/findImage/...`) — **this is the core value of I1**: the same Lua script semantics run against a "Windows window" or an "iOS device screen", with only the capture/inject backend differing. iOS-specific capabilities (e.g. Springboard operations) would be added later per need, after A2.

---

## 4. Build and Dependencies

| Item | Approach |
|----|------|
| usbmuxd protocol | `libimobiledevice`/`libusbmuxd` (LGPL; vcpkg has a `libimobiledevice` port) |
| WDA communication | Port forwarding at runtime; the C++ HTTP client reuses the existing curl dependency |
| iOS major-version adaptation | WDA recompilation is an **environment prerequisite** (Mac signing chain); the runtime only does version detection for API differences |
| Platform tenant | `lib/wingman/src/platform/ios/` (host side, not device side), reusing the §8.4 separate-library discipline |

Signing-chain constraints (cannot be bypassed, stated in README): developer account + certificate + provisioning profile (free certificates expire after 7 days) + Developer Mode on the target device + a Mac running `xcodebuild` to deploy WDA.

---

## 5. Milestones (inside I1)

| Stage | Content | Acceptance |
|------|------|------|
| I1.0 | WDA client + device enumeration + screenshot | A PC script's `wingman.findColor()` hits a color block on the iOS screen |
| I1.1 | Touch injection + text input | A script completes one real app operation (open → tap → confirm) |
| I1.2 | Multiple devices | Two devices with concurrent sessions, scripts routed by device name |

---

## 6. Risks and Boundaries

| Risk | Mitigation |
|------|------|
| WDA breaks on each iOS major version | Version detection + explicit errors; adaptation effort is maintenance cost and **does not burden the mainline** |
| Signing/certificate chain needs a Mac | Documented as an environment prerequisite; not automated (neither compliance nor effort allows it) |
| Frame rate cap (1-3 fps) | Positioned for testing/lightweight idle-farming, no high-performance promises |
| The jailbreak temptation | Explicitly refused: leaving the App Store sandbox means leaving the compliance boundary |

**What we will not do**: an iOS on-device agent (not feasible), an iOS Dashboard client (the Dashboard is web, usable from any browser, no native app needed), jailbreak channels (compliance red line).

---

## 7. Start Conditions

I1 stays "long-term optional": implementation starts when either signal appears —
1. A real user raises an iOS scenario need (testing/idle-farming) and can provide a Mac + developer account environment;
2. The A line (Android A2-A4) is delivered, the vision module is stable on NDK, and there is spare capacity.

When started, this document is the implementation baseline; I1.0 lands first.
