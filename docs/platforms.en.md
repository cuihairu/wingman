> [中文](platforms.md) | **English**

# Platform Support and API Availability

> Last verified: 2026-10-15 (re-scanned item by item against current source code, vcpkg.json, CMake, and ci.yml). This document is the **single reconciliation table** for platform information:
> When the README's promises, the scope CI actually verifies, and the true conditional-compilation coverage of the code disagree,
> the current state recorded here prevails, and this document is where it gets revised.

## Why a Separate Platform Document

Wingman's platform information was previously scattered across three places: the README badges claiming "supports Windows, macOS, Linux",
the CI configuration defining "which platforms actually verify each commit", and the conditional compilation in the code
deciding "which platform truly has a backend for each feature". These three evolve independently and cross-check nothing — what the README
claims CI may not test, features CI compiles may not have implementations on all platforms, and users ultimately can only get
"does my platform work, and if not, why" answered through one centralized document.

Essentially, a per-platform document aligns these three threads:

1. **Users find information by platform**—the support matrix is the entry point, the API availability list is the answer;
2. **CI verifies by platform**—the [CI verification mapping](#ci-verification-mapping) section maps every job and matrix entry one to one,
   and any promise without CI backing must be honestly visible (see the honest annotations in that section);
3. **Code is conditionally compiled by platform**—platform macros are only allowed in the platform tenant directories
   (`lib/wingman/src/platform/`, enforced by `scripts/check_platform_boundary.sh`,
   see [Architecture Decisions · Platform Macro Boundary](architecture-decisions.md)),
   which guarantees that "whether a platform has a capability" can always be answered from a single source of truth in the tenant directories.

## Platform Support Matrix

| Platform | Status | Architecture | Minimum Version Requirement | How to Obtain | Known Limitations |
|------|------|------|--------------|----------|----------|
| Windows | Supported | x86_64 | Windows 10 / Server 2016+ | `wingman-*-windows-x64.zip` from [Nightly](https://github.com/cuihairu/wingman/releases/tag/nightly) or Release; for source builds see [BUILD](../BUILD.md) (vcpkg triplet `x64-windows-static`) | OCR / ML / Python engines are optional switches, off by default (see [API availability](#api-availability-annotations)) |
| Linux | Supported | x86_64 | Ubuntu 22.04+ (GCC 11+), X11 display server | `wingman-*-linux-x64.tar.gz` from Nightly / Release (includes AppImage and deb); source build (triplet `x64-linux`) | X11 only; Wayland needs the XWayland compatibility layer, native Wayland not supported; no UI automation backend (see [environment differences](#environment-differences-explained)) |
| macOS | Supported | x86_64 (Intel) / arm64 (Apple Silicon, agent single binary only) | macOS 12+ (deployment target `MACOSX_DEPLOYMENT_TARGET=12`) | agent: `wingman-agent-*-macos-{x64,arm64}.tar.gz` (the `build-agent.yml` matrix includes arm64); packaging (dmg/app/GUI): `wingman-*-macos-x64.tar.gz` (x64 only); source build (triplet `x64-osx`) | No OpenCV (`vcpkg.json` does not cover macOS) → vision goes through the `src/vision_stub.cpp` stub; find-color/find-image/find-contour always return no-hit or empty values, so vision-class automation is unavailable on macOS (see [API availability](#api-availability-annotations)); CI verification is best-effort (continue-on-error) |
| Android | Experimental | arm64 | Android 9.0+ (minSdk 28, targetSdk 34) | `wingman-*-android-arm64` APK from Nightly / Release (debug signed); for source builds see [apps/android/README.md](../apps/android/README.md) | A2 capability surface only (the three Lua subtables `wingman.input/screen/vision`, see below); no official signing; A3 reliability (boot autostart/crash auto-restart/watchdog/keep-alive guidance) landed 2026-09-30, official signing belongs to A3-P2 |
| iOS | Planned | arm64 | — | — | On-device Agent infeasible (sandbox restrictions); only the "host-controlled" route has a design, see the [iOS section](#ios-why-planned-instead-of-supported) |

Supplementary notes on obtaining builds: Nightly builds daily from the main branch and **keeps only the latest set for the current day** (named by
commit sha, in the form `2026.09.24-nightly-<sha>`); Release is triggered by tags, artifacts verified against SHA256SUMS.
Both are pre-release artifacts; the official version number currently sits at v0.1.x (see [CHANGELOG](../CHANGELOG.md)).

## API Availability Annotations

**This is the core purpose of this document**: Wingman does not yet have a fully unified cross-platform API—the same feature name may
"differ per platform: have a real backend / have a degraded implementation / not exist at all". The table below lets users see at a glance whether their platform can use it.

Annotation meanings: ✅ fully supported · ⚠️ limited/degraded (reason given) · ❌ unavailable (reason given) · 📋 planned (iOS).

### Script API (Lua `wingman.*` modules, desktop Runtime)

The desktop Runtime's script API is organized in three groups: "platform-bound → pure logic → optional capabilities". The reason for this grouping
is that the three groups **fail for different reasons**: the platform-bound group depends on whether the target system has the corresponding
system capability (e.g., Linux has no UI Automation framework), the pure-logic group is theoretically available everywhere, and the optional-capabilities
group depends on whether the corresponding dependency switch was enabled at build time.

**Platform-bound group** (each feature depends on one platform backend; backends live in the `lib/wingman/src/platform/` tenant directories):

| Feature | Windows | Linux | macOS | Android | iOS |
|--------|---------|-------|-------|---------|-----|
| `wingman.screen` (screenshot/color pickup/region query) | ✅ GDI | ✅ X11 | ✅ Cocoa | ⚠️ via A-line subtables (see below) | 📋 WDA backend |
| `wingman.input` (keyboard/mouse injection/movement trajectories) | ✅ SendInput | ✅ XTest | ✅ CGEvent | ⚠️ touch semantics via A-line subtables | 📋 WDA backend |
| `wingman.window` (window find/activate/bounds) | ✅ Win32 | ✅ X11 | ✅ Cocoa | ❌ no window-manager semantics on-device (no such requirement) | 📋 |
| `wingman.clipboard` (clipboard read/write) | ✅ | ✅ X11 | ✅ Cocoa | ❌ not implemented (no such API surface on the A line) | 📋 |
| `wingman.filewatcher` (file change monitoring) | ✅ ReadDirectoryChangesW | ✅ inotify | ✅ FSEvents | ❌ not implemented | 📋 |
| `wingman.process` (process enumerate/start/terminate) | ✅ Win32 | ✅ POSIX | ✅ POSIX | ❌ not implemented | 📋 |
| `wingman.macro` (macro record/playback) | ✅ system hooks | ✅ XRecord | ✅ Cocoa events | ❌ not implemented | 📋 |
| `wingman.uia` (UI element tree automation) | ✅ UI Automation | ❌ **no backend**: Linux has no equivalent framework; calls degrade to returning failure with a warning | ✅ AXUIElement | ❌ not implemented | 📋 |
| `wingman.hotkey` (global hotkey listening) | ✅ GetAsyncKeyState | ✅ XQueryKeymap (X11) | ⚠️ CGEventSourceKeyState requires Accessibility permission; real behavior pending on-device verification | ❌ not implemented | 📋 |

**Pure-logic group** (does not touch platform system capabilities, behavior identical across the three desktop platforms; the Android A-line script surface is not introduced):

| Feature | Windows / Linux / macOS | Android | Notes |
|--------|-------------------------|---------|------|
| `wingman.event` / `fsm` / `bt` / `smarttrigger` | ✅ | ❌* | Event bus, state machine, behavior tree (`bt`), smart trigger (`smarttrigger`) |
| `wingman.task` (async task/retry/timeout) | ✅ | ❌* | The A line has its own lightweight execution model and does not go through the desktop module registry |
| `wingman.transport` (TCP/UDP send/receive) | ✅ | ❌* | Android's networking surface is in the Kotlin shell and the `libs/transport` library layer, not in the Lua module layer |
| `wingman.inbox` / `team` / `notify` / `orchestration` | ✅ | ❌* | Cloud-control downlink/team/notification surface, part of desktop Runtime semantics |
| `wingman.http` (HTTP client) | ✅ | ❌* | Depends on libcurl; available when the build carries `WINGMAN_HAS_CURL` |
| `wingman.json` / `ini` / `kv` / `db` / `config` | ✅ | ❌* | Serialization and storage surface |
| `wingman.crypto` / `security` (incl. TOTP) | ✅ | ❌* | |
| `wingman.human` (humanized trajectories) / `gameprofile` / `verification` / `timer` / `util` / `system` | ✅ | ❌* | The information-collection part of `system` depends on platform backends, present on all three desktops |
| `wingman.file` (file IO: read/write/move-copy/directory) | ✅ | ❌* | Pure `std::filesystem`, identical across the three desktop platforms; `wingman.filewatcher` provides change monitoring |
| `wingman.node` (node state/heartbeat) | ✅ | ❌* | `getWindows` goes through `Window::enumerate()` (depends on the platform window backend, present on all three desktops); `sendHeartbeat` currently only logs and does not actually send |
| `wingman.systemwatch` (process/window change observation) | ✅ | ❌* | Polling-based; underlying it is the `Process` / `Window` facades (see the platform-bound group) |
| `wingman.script` (script management control surface) | ✅ | ❌* | Goes through `ScriptManager`, no platform dependency |

\* The meaning of the "❌\*" annotation: not a missing capability but **architectural non-applicability**—the Android Agent's (A line)
ScriptRunner does not load the desktop module registry (see
[Android Design · 5.5 deviation note](android-agent-design.md)); it only exposes the three subtables below.
Porting the full desktop module set onto the device is a candidate direction for later A-line milestones and is not currently committed to.

> `wingman.hotkey` appears only in the **platform-bound group** above (it depends on the platform input backend) and is not listed in this group—
> it does not satisfy the grouping premise of "does not touch platform system capabilities".
>
> All four `wingman.debugger` methods (`start`/`stop`/`breakpoint`/`breakHere`) are **stubs** in
> `debugger_module.cpp` (`start` always `false`, `stop` always `nil`,
> with a comment stating "debugger header not yet implemented"), so neither the three platforms nor Android provides a usable implementation;
> it never constituted a platform difference and is not listed in any table.

**Optional-capabilities group** (depends on build switches and dependency availability; features off by default are not distributed with artifacts):

| Feature | Windows | Linux | macOS | Android | Reason for Non-Applicability |
|--------|---------|-------|-------|---------|------------|
| `wingman.vision` **find-color/find-image/template matching** (`findColor`/`findImage` etc.) | ✅ | ✅ | ❌ no OpenCV → goes through the stub | ✅ (A-line subtable; bitmap-first difference see below) | The script layer goes through `Vision::findColor` / `Vision::findImage`; `src/vision.cpp` uses OpenCV directly (`opencv2/imgproc.hpp` etc.), compiled in only when OpenCV is hit; otherwise macOS/Android compile `src/vision_stub.cpp` and all vision functions return no-hit/empty values |
| `wingman.perf` (vision performance tuning) | ✅ | ✅ | ⚠️ stub | ❌ | Without OpenCV, `performance_module.cpp` takes the `#else` branch: the module still registers, but `preloadImage`/`clearCache` etc. always return `nil`/`0` (the `HAS_OPENCV` macro is an either/or based on the build switch) |
| `wingman.vision` **OpenCV-accelerated analysis** (`WINGMAN_ENABLE_VISION`) | ✅ | ✅ (vcpkg `vision` feature) | ❌ no OpenCV dependency (manifest does not cover macOS) | ❌ | macOS/Android builds do not carry OpenCV; when it needs to be introduced, go through the vcpkg manifest, do not hand-probe system libraries |
| `wingman.ocr` (text recognition, `WINGMAN_ENABLE_OCR`) | ✅ (Tesseract) | ❌ | ❌ | ❌ | The vcpkg `ocr` feature covers Windows only; without OCR, script calls degrade to returning empty results |
| `wingman.ml` (ONNX inference, `WINGMAN_ENABLE_ML`) | ✅ (ONNX Runtime) | ✅ (`ml` feature, locally verified working with 1.23.2) | ❌ | ❌ (bindings same-shaped but always false; NDK dependencies not settled) | The macOS manifest does not cover onnxruntime |
| Python dual engine (`WINGMAN_ENABLE_PYTHON`) | ⚠️ experimental | ✅ (CI full test suite includes the Python engine) | ❌ | ❌ | Linux: the `C++ Linux (full tests, Python engine)` job runs the full suite; Windows: link chain + best-effort tests; macOS unverified |

### Android On-Device Script Surface (A Line)

The Android Agent's Lua environment has only the three subtables under `wingman` (A2 script capability,
see [Android Design · §5.5/5.6](android-agent-design.md)):

| Subtable | Capabilities | Behavioral Differences vs Desktop |
|------|------|------------------|
| `wingman.input` | `click`/`tap`/`longPress`/`swipe`/`delay` (injected via the accessibility service Gesture API) | Touch semantics (no hover/right-click/scroll wheel); `delay` can be interrupted by the stop flag |
| `wingman.screen` | `capture`/`getScreenWidth`/`getScreenHeight` (MediaProjection capture) | Frames come from a push-style ImageReader cache, not on-demand screenshots |
| `wingman.vision` | `findColor`/`findImage` etc. lookups | **bitmap-first**: capture once, analyze many times—structurally different from desktop's "re-capture on every lookup"; mind the staleness when writing scripts |

When the bridge is not authorized (accessibility/screen-cast permission not granted), functions degrade to returning `false`/`nil` instead of
throwing—scripts should check return values and handle fallback themselves; this is the A line's defensive convention.

### C++ Public Header Layer (Embedding/Secondary-Development Perspective)

The public headers under `lib/wingman/include/wingman/` fall into three layers, with the same logic as above:

| Layer | Representative Headers | Applicability |
|----|-----------|--------|
| Platform abstraction interfaces | `platform/iscreen.hpp`, `iwindow.hpp`, `iinput.hpp`, `icapture.hpp`, `iclipboard.hpp`, `ifilewatcher.hpp` | The interfaces themselves are platform-independent; implementation existence matches the "platform-bound group". Android has only the `capture::ICaptureSource` adaptation (`lib/wingman/src/platform/android/android_capture.hpp`, with `AndroidCaptureSource` as its sole implementation) |
| Platform-dependent facades | `screen.hpp`, `window.hpp`, `clipboard.hpp`, `filewatcher.hpp`, `process.hpp`, `recorder.hpp`, `system.hpp`, `ui_automation.hpp` | Same as the corresponding script API rows; `ui_automation.hpp` has no backend on Linux (`linux_ui_automation_factory.cpp` returns `nullptr`; the facade layer warns and then yields empty semantics) |
| Cross-platform logic | `event.hpp`, `trigger.hpp`, `behavior_tree.hpp`, `config.hpp`, `kvstore.hpp`, `crypt.hpp`, `storage/*`, `script/*`, `rpc/*` etc. | Identical across the three desktop platforms; Android reuses a subset (`libs/agentcore`, `libs/androidagent`) |
| Platform-restricted (**private headers, not in the `include/` public surface**) | `src/platform/win/screenshot_reporter.hpp` | **Windows-only compilation** (dedicated to the Windows crash/screenshot reporting chain; `lib/wingman/CMakeLists.txt` attaches the source files only inside the `if(WIN32)` block); secondary developers must include that path themselves |

The IPC factory (`ipc/ipc_factory.hpp`) selects automatically per platform: Windows uses Named Pipes (endpoint name
`wingman`), Unix-family uses Unix domain sockets (endpoint `$XDG_RUNTIME_DIR/wingman.sock`,
macOS goes through `$TMPDIR`, both fall back to `/tmp/wingman.sock`). The TCP fallback (`127.0.0.1:9800`) is **off by default**—
it is created only when `IpcConfig::allowTcpFallback` or `preferredTransport = TcpPipe` is explicitly set
(`createTcpChannel` in `lib/wingman/src/platform/ipc_factory.cpp`); it is an explicit debug aid, not an
automatic fallback. Embedders need not be aware of the difference between the first two; if the TCP fallback is enabled, mind
the firewall allowance for that port during cross-platform joint debugging.

## Environment Differences Explained

The differences below all come from actual pitfalls hit (CHANGELOG has the corresponding fix records); this is not a theoretical list of differences.
Each item is expanded as "difference → why → impact on scripts/usage".

### Display Server and Screenshots

- **Linux supports X11 only**: screenshots, color pickup, injection, and macro recording (XRecord) are all built on the X protocol.
  The reason is historical and current reality—XTest/XRecord is the de facto standard for Linux desktop injection, while Wayland deliberately
  provides no global injection channel out of security-model concerns. Impact: Wayland sessions must switch to XWayland or an X11 session;
  headless environments can run the full test suite with a virtual display (Xvfb)—the CI Linux test job and local coverage collection
  both run this way.
- **Graceful degradation without a display**: when Linux has no `DISPLAY`, screen/window queries return degraded values instead of crashing
  (there is a dedicated tolerant X error handler; errors like BadWindow surface as degraded return values—this was fixed after hitting the
  pitfall of "a dead-handle query killing the whole process"). Impact: scripts can probe capabilities in headless environments and branch on their own.
- **The macOS capture backend is optional**: ScreenCaptureKit capture is controlled by the compile switch
  (`HAVE_SCREENCAPTUREKIT`). Reason: the framework has a system-version threshold; with it off, things degrade to the Cocoa basic path and still work.
- **Android capture is push-style**: MediaProjection frames are pushed by an independent ImageReader thread into a cache
  (see the bitmap-first entry in the table above), unlike desktop's pull-style "capture at query time" semantics. This is a consequence of the
  permission model—every active screenshot on Android may trigger a system prompt, and the push-style cache avoids that.

### Permissions and Files

- **Permission-class defenses fail under root**: `chmod 000` has no effect on root (root ignores file permission bits);
  scenarios that rely on "unreadable files" for degradation testing/verification behave differently in root environments. Impact: some permission
  branches are unreachable when running Desktop tests as root (the tests explicitly skip via `geteuid()`).
- **Special device files are Linux-specific**: `/dev/full` (writes always fail with a full error), `/proc` (process info), etc.
  do not exist on Windows/macOS; any script or test depending on them needs platform branches.
- **macOS sandbox and Accessibility authorization**: CGEvent injection and macro recording require the "Accessibility"
  authorization; without it, injection silently does nothing. Reason: Apple treats global event injection as a high-privilege operation.
  Windows has no such layer (SendInput works by default), and Linux XTest depends on X session authorization.

### Processes and Signals

- **Asymmetric signal semantics**: POSIX has `SIGPIPE` (writing to an already-closed socket kills the process outright); Windows does not.
  Historically, the dual IPC channel once caused the process to die immediately on remote disconnect because `SIGPIPE` was not ignored (fixed).
  Impact: when adding socket write paths, secondary developers must follow the existing `MSG_NOSIGNAL`/ignore handling; passing tests on
  Windows alone does not mean safe.
- **Different subprocess models**: POSIX uses the fork/exec model (`posix_process.cpp`), Windows uses the
  CreateProcess family (`win32_process.cpp`). Behavior differences visible to scripts: Windows cannot get fork semantics
  (e.g., copy-on-write memory snapshot usage); Linux's `waitForever` blocking semantics are implemented on Windows
  via handle waiting—same behavior, completely different implementation paths.

### Clipboard Asynchrony (Linux-Specific)

On Linux, `xclip` forks a daemon on every `setText`/`clear` that **asynchronously** takes over the clipboard
selection—reading immediately after writing may hit the old owner (measured takeover can exceed 500ms). On Windows/macOS,
clipboard writes are synchronous system-call semantics. Impact: scripts on Linux that "assert immediately after writing" must tolerate a brief
delay window; the library has wrapped this with polling so the script layer is usually unaware, but when mixing clipboard usage across toolchains, know about this layer.

### IPC and Networking

- **IPC channels take three platform-specific forms**: Windows named pipes / Unix domain sockets / TCP fallback, chosen by the factory per
  platform. The local UI control plane follows the local IPC constraint of [architecture decisions](architecture-decisions.md)
  (the Runtime opens no local HTTP/WebSocket services), so IPC behavior differences do not leak into the remote plane.
- **There is exactly one remote plane**: on all platforms, remote control goes through an outbound TCP connection to the Go Server (Runtime-as-Agent),
  including the Android device side. This is a deliberate architectural constraint, ensuring "platform differences" stop at the execution plane and never enter the control plane.

## iOS: Why "Planned" Instead of "Supported"

iOS's status is not "not done yet" but "impossible on-device". The App Store sandbox does not allow cross-app
injection or global screenshots—**no legitimate on-device execution channel exists**. This is the core conclusion of the
[feasibility analysis](mobile-support-feasibility.md) and the premise of every judgment in this section.

The only legitimate route that exists is "host control": the iOS device acts as a controlled peripheral; the desktop Runtime connects via the USB
channel (usbmuxd port forwarding) to WebDriverAgent (WDA, the community packaging of Apple's official test framework) on the device,
with screenshots and touch injection going through XCTest capabilities. The full design of this route is in
[iOS Support Design](ios-agent-design.md) (I1, design only, not implemented); its constraints determine that it is "planned":

- **The signing chain cannot be automated**: developer account + certificates + provisioning profiles (free certificates expire in 7 days) + target-device
  developer mode + a Mac running `xcodebuild` to deploy WDA—all are manual prerequisites;
- **Version-following cost**: WDA must be rebuilt and adapted for every major iOS release—a continuous maintenance cost, not a one-time port;
- **Performance ceiling**: screenshot round trips over the USB link take 200-400ms/frame, supporting only a 1-3 fps find-color/find-image cadence;
  high-performance scenarios are not promised;
- **Explicit boundaries**: no jailbreak channel (compliance red line), no iOS on-device Agent (technically infeasible),
  no native iOS Dashboard client (Dashboard runs on the Web, a browser suffices).

The conditions for starting implementation (real demand + Mac signing environment + A-line delivery complete) are written in §7 of the design
document cited above; this document does not expand on them.

## CI Verification Mapping

Every promise in the support matrix must be able to answer "which CI job verifies it". The table below is the actual coverage of the current `.github/workflows/`
([ci.yml](../.github/workflows/ci.yml) plus the packaging workflows):

| Platform | job | Runtime | Verification Depth | Packaging Verification |
|------|-----|----------|----------|----------|
| All platforms | `Platform Boundary Guard` | ubuntu-latest | Platform macro boundary check (every commit): platform macros forbidden in the common layer; the allowlist only shrinks, never grows | — |
| All platforms | `Vault Touchpoint Scan` | ubuntu-latest | Scan for boot-tier plaintext-credential environment-variable reads (anything outside the allowlist fails) | — |
| All platforms | `API Typing Drift Audit` | ubuntu-latest | pyi stub vs C++ module descriptor drift + regenerating Lua hints and verifying sync | — |
| Windows | `C++ Windows` | windows-2022 | **Full C++ test suite + coverage** (OpenCppCoverage, the only C++ job collecting coverage) | ✅ (`build-package.yml`, zip) |
| Windows | `C++ Windows (Python engine)` | windows-2022 | Experimental Python-engine compilation + best-effort tests | — |
| Linux | `C++ Linux (full tests)` | ubuntu-24.04 | **Full C++ build + full core tests** (xvfb-run provides a virtual display; openbox/xclip installed via apt) | — |
| Linux | `C++ Linux (full tests, Python engine)` | ubuntu-24.04 | Same as above + `-DWINGMAN_ENABLE_PYTHON=ON` (full run, proving that enabling Python does not break Lua/existing paths) | — |
| Linux | `C++ ubuntu-22.04` (compat matrix) | ubuntu-22.04 | ⚠️ **Compile-only** for the compat-layer `wingman_transport` single target, **no core tests** | ✅ (tar.gz/AppImage/deb) |
| macOS | `C++ macos-15-intel` (compat matrix) | macos-15-intel | ⚠️ Same as above, plus `continue-on-error: true` (failures do not block merging) | ✅ (tar.gz/dmg/app) |
| Android | — | — | ❌ **No CI test job** | ✅ (debug-signed APK) |
| Go Server | `Go Server` (3 OS matrix) | ubuntu/windows/macos-latest | `go vet` + `go test -race` (all platforms) | — |

> `ci.yml` has 9 jobs in total (of which `cpp-compat` expands 2 matrix entries, `go-build` expands 3).
> `cpp-windows` / `cpp-linux-tests` / `cpp-linux-python-tests` / `cpp-python` are all single-config single-leg
> with no matrix dimension—coverage is collected only in `cpp-windows` (`codecov/codecov-action@v5.0.1`).

**The current state that must be honestly annotated** (i.e., the reconciliation conclusions mentioned in the "why organized this way" section):

1. **Linux's C++ core tests are covered by the `C++ Linux (full tests)` job**: a full
   build + full core test suite on ubuntu-24.04; X11 cases run for real under a virtual display via xvfb-run, not skipped.
   Note, however, that the GCC version this job uses (13) differs from the compat matrix (ubuntu-22.04's GCC 11).
   A full build on the minimum supported toolchain GCC 11 was locally verified on 2026-09-25 (g++-11 full
   build, all vcpkg dependencies rebuilt): **the only gap** is `ScriptValue::objectVal`—a recursive
   `std::unordered_map<std::string, ScriptValue>` member inside the class (the standard grants incomplete-type
   exemption only to vector/list/forward_list; GCC 15's libstdc++ happens to tolerate it, while GCC 11 correctly
   rejects it at pair instantiation), which cascades into 53 failed compile targets in the script subsystem; all other
   targets compile. The fix requires changing the object representation to a standard-exempt container or an
   indirection layer (a breaking refactor touching 243 `fromObject` call sites + 35 `objectVal` accesses),
   not undertaken for now—GCC 11 support stays within the existing commitment scope of the transport
   layer (compat job).
2. **macOS's C++ core still has no test coverage, and the job is allowed to fail** (continue-on-error), meaning
   macOS's "Supported" is currently backed only by packaging success + compat-layer compile success, with no CI evidence for core behavior.
3. **Android has packaging verification only**. Engineering of the signing/release pipeline is in the A3 milestone, and so is a test job.
4. **iOS has no CI at all** (matching the "Planned" status).

Remediation direction and priority are not expanded here (that is a CI evolution decision, not a record of current platform state), but the gaps themselves
must be visible here.

## Local Verification Commands (Linux Development Environment)

Linux is the current primary development platform; the following commands can be used directly from the repository root (for builds on other platforms see
[BUILD](../BUILD.md), where the Windows commands also live):

```bash
# 配置（Debug + 测试；Linux 测试需要 tests feature 与 WINGMAN_BUILD_TESTS）
cmake -B build-cov \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg根>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DWINGMAN_BUILD_TESTS=ON \
  -DBUILD_CORE_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES=tests

# 编译
cmake --build build-cov --target core_tests -j"$(nproc)"

# 运行全量测试（需要 X server；无头环境先启动：Xvfb :98 & export DISPLAY=:98）
./build-cov/lib/wingman/tests/core_tests
```

Two pitfalls already hit:

1. **Tests require counting the PASSED count**, not just checking the exit code—a test process killed on
   timeout can also leave seemingly normal output in the intermediate state before `[  PASSED  ]`;
   verify that `[==========] N tests ran` matches the PASSED count;
2. **GUI/screen-class cases skip in headless environments** (e.g., clipboard-image and window-operation cases); this is
   by design—a full PASSED plus some SKIPPED is the normal shape.

## Cross-Platform Unified API Convergence Route

The current differences (all ⚠️/❌ in the tables above) fall into three categories, with correspondingly three convergence strategies; this document records direction only, no vaporware:

1. **Missing system capability** (no UI Automation on Linux, no OCR/ML dependencies on macOS)—do not force-fit; rely on
   script-side capability probing + explicit degraded return values; whether to introduce new dependencies is decided by the vcpkg manifest's
   platform coverage, not by probing system libraries.
2. **Architectural layering** (Android does not load the desktop module registry)—the convergence direction is "same name, same shape":
   the A-line subtables already share names and shapes with the desktop API (`wingman.input.*` etc.); subtable coverage expands per
   demand going forward, rather than porting the desktop registry wholesale onto the device.
3. **Multi-backend abstraction** (the iOS host-control route)—add no new platform branches; instead add device backends to the existing
   `ICapture`/`IInput` ("a device is just another screen"), with script semantics fully identical to desktop.
   This is the end-state demonstration of platform convergence: scripts unchanged, backends pluggable.

Whenever any step lands, it will first appear as an update to the corresponding row in the [Platform Support Matrix](#platform-support-matrix) and [API Availability Annotations](#api-availability-annotations)
—this document is revised in the same commit as the code.
