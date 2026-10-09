<div align="center">

<img src="docs/public/logo.svg" alt="Wingman" width="100" />

# Wingman

[![OS](https://img.shields.io/badge/OS-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)](https://github.com/cuihairu/wingman)
[![CI](https://github.com/cuihairu/wingman/workflows/CI/badge.svg)](https://github.com/cuihairu/wingman/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cuihairu/wingman/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/wingman)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Lua](https://img.shields.io/badge/Lua-5.5-000080.svg?logo=lua&logoColor=white)](https://www.lua.org/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB.svg?logo=python&logoColor=white)](https://www.python.org/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

**Cross-platform programmable automation agent and remote orchestration platform** (game automation is the first vertical)

An automation agent runtime in C++ + Lua/Python, with a Go remote orchestration control plane

> In one sentence: register Windows / macOS / Linux / Android machines as agents and control them
> with scripts, vision, input, OCR/ML and workflows. Game automation is the most polished vertical,
> but the control plane and execution plane were designed as a general agent platform from day one.

English | [简体中文](README.zh.md)

[Docs](docs/README.md) · [Quick Start](docs/guide/getting-started.md) · [Platform Support](docs/platforms.md) · [API Reference](docs/api/overview.md) · [Examples](docs/examples/) · [Contributing](CONTRIBUTING.md)

</div>

> [**Disclaimer**]
>
> This tool is provided for lawful use cases only, including but not limited to: automated testing,
> single-player game assistance, and accessibility support. Any consequences of violating the terms
> of service of any game or software with this tool are borne solely by the user.
> The author assumes no legal liability arising from the use of this tool.

---

## Demo Site

**Demo site https://wingman.cuihairu.site/ ｜ Demo account `demo` / `Wing-demo-Isx4Dvm7eSd6-26!`** (for trial only, data is reset periodically)

> The demo site is deployed and rolled back automatically by CI (released only when all main-branch tests pass). Data is reset periodically and it is intended for trial use only; the demo account has the read-only `viewer` role.

---

## Key Features

- **C++ core** - Engine and script modules implemented in C++23; Lua/Python are script layers only
- **Dual-language scripting** - Lua (sol2) and Python (pybind11) share the same `wingman.*` API
- **Pure user mode** - Only calls public system APIs; no game memory reads/writes, no process injection
- **Programmable** - 41 script modules plus triggers and macros; complex logic written as scripts
- **Cross-platform** - Unified interface abstraction for Windows, macOS and Linux; experimental Android support, iOS planned ([platform details](docs/platforms.md))

### Feature Modules

| Module | Description |
|------|------|
| **Screen operations** | Screenshots, pixel detection, color matching, image search |
| **Input simulation** | Mouse click/move, key send, text input |
| **Window management** | Find window, activate window, get position |
| **Trigger system** | Pixel/image/time condition triggers that execute actions automatically |
| **Macro recording** | Record mouse and keyboard actions, save as scripts for playback |
| **UI Automation** | Windows UIA automation for operating UI controls |
| **OCR** | Tesseract text recognition (optional dependency) |
| **Data persistence** | kv key-value store, SQLite database |
| **Serialization formats** | JSON, INI configuration file parsing |
| **File IO** | File read/write (binary-safe), move/copy, directory operations (`wingman.file`) |
| **Global hotkeys** | Polling global key-state monitoring, combo-key callbacks (`wingman.hotkey`, Python callbacks) |
| **Notifications** | Log/Toast/Webhook/event bridging, script tray intents (`wingman.notify`) |
| **Debugging** | VS Code breakpoint debugging for Lua scripts (debug components required) |
| **Android Agent** | On-device Android agent (experimental, arm64 / Android 9.0+); outbound TCP to the Go control plane, script API mirrors the desktop runtime, unified dispatch and visibility from the Dashboard |
| **Remote orchestration** | Agent registration/heartbeat/command dispatch (16-byte header + JSON frames, per-command timeout), batch operations (fan-out by ID/tag selectors), DAG workflows, RBAC/audit, Guacamole remote desktop (Go control plane) |

> Some advanced modules (OCR, ML/YOLO, remote orchestration, script debugger) depend on optional components or are still under active development. Default availability follows the current build configuration, runtime parameters and the corresponding API docs.

---

## Quick Start

### One-line Agent Install

Install the `wingman-agent` single binary with one command (auto-detects OS and CPU architecture, anonymous download, no GitHub login required; re-running upgrades in place):

**Linux / macOS:**
```bash
curl -fsSL https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.sh | bash
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1 | iex
```

Optional parameters (use `bash -s --` on Linux/macOS, a scriptblock on Windows):

```bash
# Pin a version + register as a persistent service (Linux: systemd user unit; macOS: launchd)
curl -fsSL https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.sh | bash -s -- --version nightly --service
```

```powershell
# Pin a version + register a Windows service (admin required; the service runs in Session 0, headless only)
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1))) -Version nightly -Service
```

| OS | Architecture | Artifact |
|----|------|------|
| Linux | x86_64 (x64) / aarch64 (arm64) | `wingman-agent-*-linux-{x64,arm64}.tar.gz` |
| macOS | x86_64 (x64) / arm64 (Apple Silicon) | `wingman-agent-*-macos-{x64,arm64}.tar.gz` |
| Windows | x64 / arm64 | `wingman-agent-*-windows-{x64,arm64}.zip` |

Notes:

- Installs by default to `~/.local/bin` (Linux/macOS) or `%LOCALAPPDATA%\Programs\Wingman\bin` (Windows), then runs `wingman-agent --version` automatically to verify
- Version selection: picks the most recent release (newest first) that contains an agent artifact for this platform (stable releases first, falling back to the nightly pre-release if no stable agent artifact exists yet)
- Downloads use anonymous GitHub Releases asset direct links; the API has an anonymous rate limit of 60 requests/hour — use `--token`/`-Token` (or the `GITHUB_TOKEN` environment variable) to raise it
- Unknown OS/architecture fails with an explicit error listing the supported range (armv7 etc. currently have no build artifacts)
- For all parameters see the header comments of [scripts/install.sh](scripts/install.sh) / [scripts/install.ps1](scripts/install.ps1)

### Requirements

- **Windows**: Windows 10/11 + Visual Studio 2022
- **macOS**: macOS 12+ + Xcode 14+
- **Linux**: Ubuntu 22.04+ + GCC 11+
- **Common**: CMake 3.20+ + vcpkg

### Install vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### Build and Run

**Windows:**
```bash
# Build
build-scripts\build-runtime-msvc-ninja.bat

# Run a Lua script
.\build-msvc-ninja-vcpkg\apps\agent\wingman-agent.exe script examples\hello.lua
```

**For detailed build steps see the [Build Guide](BUILD.md)**

---

## Android Agent

On-device Android agent (experimental, arm64 / Android 9.0+): outbound TCP to the Go control plane,
with a script API that mirrors the desktop runtime (`wingman.input` / `wingman.screen` /
`wingman.vision`); dispatched and monitored uniformly from the Dashboard.

**Capability list**

| Capability | Description |
|------|------|
| **Gesture injection** | Accessibility-service injection of tap/swipe (`wingman.input.tap` / `swipe` / `delay`) |
| **Screen capture** | Real-time MediaProjection screen capture (produces frames while the screen is on) |
| **Color/image search** | `wingman.vision.findColor` / `findImage` (OpenCV template matching, compiled into the NDK) |
| **Remote screenshots** | Dashboard workflow screenshot steps render directly on screen (same shape as desktop) |
| **Token auth** | Server-side `WINGMAN_AGENT_TOKENS` registration-token whitelist (off by default, may be left empty) |
| **Keep-alive** | Boot autostart, crash auto-restart (exponential backoff), 30s core watchdog, device-specific keep-alive guidance and restricted-settings guidance |

**Getting the APK**

The nightly Release's `wingman-*-android-arm64.apk` (debug signed):

- Release page: [releases/tag/nightly](https://github.com/cuihairu/wingman/releases/tag/nightly)
- Grab the latest group's `wingman-<date>-nightly-<sha>-android-arm64.apk` from Assets
  (nightly builds daily, only the latest of the day is kept)
- Debug signed: when installing on a phone, allow "unknown sources"; no GitHub login needed — anonymous direct download

**Quick start**

1. Start the Go server (`orchestrator/server`; Dashboard on port 9527, agent port 8888).
2. Install the APK on the phone (same network segment as the server), open the app → enter the
   server IP / port / device ID → "Start Agent". The first start creates a persistent notification
   (a foreground-service requirement); if the server has `WINGMAN_AGENT_TOKENS` configured
   (registration-token whitelist), enter the registration token as well.
3. The device appears in the Dashboard's agent list (platform=android).
4. Create a script and run it on that device, with logs streamed back in real time:

   ```lua
   print('hello android')
   ```

5. Enable A2 capabilities: in the app, turn on the injection service under "Accessibility settings"
   and grant "Start screen capture" (on Android 14+ each new capture session prompts again — a
   system constraint), then dispatch a color-search/gesture script:

   ```lua
   local pt = wingman.vision.findColor(0xE23B3B, 10)
   if pt then wingman.input.tap(pt.x, pt.y) end
   wingman.input.swipe(540, 1800, 540, 600, 400)
   ```

6. Remote screenshots: add a screenshot step to a Dashboard workflow targeting the device → the screenshot renders on screen.
7. On Android 13+, accessibility services sideloaded via APK are blocked by "restricted settings" by default: system settings → Apps →
   Wingman Agent → ⋮ → Allow restricted settings, or connect a computer and run
   `scripts/android-restricted-settings.sh allow` to pre-authorize (the app also offers in-app guidance).

For detailed acceptance steps, known limitations and A3 reliability validation see
[apps/android/README.md](apps/android/README.md); design and protocol docs at
[docs/android-agent-design.md](docs/android-agent-design.md); device keep-alive at
[docs/guides/android-keep-alive.md](docs/guides/android-keep-alive.md).

---

## Code Examples

### Lua

```lua
local wingman = require("wingman")

-- Capture and save a screenshot
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)
screenshot:save("screenshot.png")

-- Find a color and click it
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, p in ipairs(points) do
        wingman.input.click(p.x, p.y, "left")
    end
end
```

### Python

```python
from wingman import screen, input

# Capture a screenshot
screenshot = screen.capture(0, 0, 1920, 1080)
screenshot.save("screenshot.png")

# Find a color and click it
points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    for p in points:
        input.click(p["x"], p["y"])
```

**More examples in the [example docs](docs/examples/)**

---

## Documentation

- [Quick Start](docs/guide/getting-started.md) - get running in 5 minutes
- [Build Guide](BUILD.md) - detailed build, test and troubleshooting instructions
- [Platform Support](docs/platforms.md) - platform status, API applicability and environment differences
- [API Reference](docs/api/overview.md) - full API documentation
- [Architecture](docs/architecture.md) - system architecture and design decisions
- [Development Guide](docs/DEVELOPMENT.md) - contribution and development guide
- [VS Code Dev Environment](docs/development-environment.md) - Lua/Python completion, tasks and debug configuration

---

## Architecture Overview

Wingman uses a **C++ core engine + multi-language scripting** architecture:

- **Control plane**: a Go server acts as the remote control plane; runtimes connect to it as agents
- **Local control**: the Tauri GUI controls the runtime over local IPC
- **Script engines**: Lua (sol2) and Python (pybind11) behind a unified interface
- **Modular**: 41 language-agnostic script modules (see the [API docs](docs/api/index.md)), easy to extend

For details see the [architecture doc](docs/architecture.md)

---

## Tech Stack

This project is built on open-source components, primarily:

- Script engines: Lua 5.5 (vcpkg `lua` port, sol2 bindings), Python 3 (pybind11 embedding)
- C++ foundation libraries: asio, cURL, OpenSSL, nlohmann-json, spdlog, SQLite — all managed via the vcpkg manifest
- Vision and recognition: OpenCV (template matching), Tesseract (OCR), ONNX Runtime (ML inference) — all optional dependencies
- Desktop GUI: Tauri + Svelte
- Control panel: Dashboard based on React + Umi, with a Go server
- Remote desktop gateway: based on Apache Guacamole (guacd)
- Build system: CMake + vcpkg

---

## Contributing

See the [development guide](docs/DEVELOPMENT.md) for how to contribute.

---

## License

[Apache-2.0](LICENSE)

---

<div align="center">

**[Back to top](#wingman)**

</div>
