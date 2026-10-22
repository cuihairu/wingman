> [中文](android-agent.md) | **English**

# Android Agent

The Android on-device Agent: an outbound TCP long-lived connection to the Go orchestrator server, receiving Lua scripts delivered inline and executing them, with logs streamed back to the Dashboard in real time (A1); gesture injection / screen capture / find-color and find-image matching / remote screenshots (A2); boot autostart / crash self-restart / watchdog / device-model keep-alive guidance (A3). For the design and protocols see `docs/android-agent-design.md`; for on-device feasibility background see `docs/mobile-support-feasibility.md`.

```
Kotlin 壳（前台服务/配置/状态）  ──JNI（进程内直调，非 IPC）──▶  C++ 核心
WingmanService / MainActivity                     AndroidAgent = RemoteClient
WingmanAccessibilityService（A2）                 + ScriptRunner(Lua) + transport
```

Architecture hard-constraint self-check: the Agent has only an **outbound** TCP client and zero listening ports; the local UI goes through in-process JNI (no IPC, no HTTP server); the Dashboard connects only to the Go Server.

## Environment Preparation

| Tool | Version |
|------|------|
| JDK | 17+ |
| Android SDK | API 34 (platform-tools / build-tools) |
| Android NDK | r26+ |
| CMake | 3.22.1+ (the cmake in the SDK also works) |
| vcpkg | already bootstrapped; NDK builds go through the in-repo `apps/android/cpp/vcpkg-android.cmake` wrapper toolchain (passed in by gradle via `-DWINGMAN_VCPKG_ROOT=<vcpkg 根>`); `scripts/buildsystems/vcpkg.cmake` is vcpkg's own path, not in-repo |
| Gradle | 8.7+ (or use the local gradle to generate the wrapper) |

Android dependencies use a separate manifest, `apps/android/cpp/vcpkg.json` (`wingman-agent-android`, not the same file as the desktop root `vcpkg.json`): in manifest mode they are installed automatically together with the NDK CMake configuration, so no manual `vcpkg install` is needed. Package set: asio / curl / lua / openssl / sol2 / spdlog / nlohmann-json / opencv4 (opencv4 turns off default features, keeping only image codec features such as jpeg/png/webp/tiff).

Fill in the vcpkg root in `gradle.properties`:

```properties
wingmanVcpkgRoot=/absolute/path/to/vcpkg
```

## Build

```bash
gradle :app:assembleDebug
# 产物 app/build/outputs/apk/debug/app-debug.apk
```

Build chain: AGP externalNativeBuild → NDK CMake (`apps/android/cpp/CMakeLists.txt`) → vcpkg arm64-android dependencies + the in-repo shared libraries `libs/transport`, `libs/agentcore` (remote link, linked from the same source as the desktop runtime), `libs/androidagent` (ScriptRunner + script capability API + a subset of core) → `libwingman_agent.so`.

> [The same-source files on both ends (`libs/agentcore`, `libs/androidagent`, as well as shared
> layers such as `libs/transport`)] are compiled into the desktop `wingman-agent` and the Android
> `libwingman_agent.so` at the same time: changes must pass compilation on both ends and the
> same-source desktop unit tests — do not verify only the desktop side.

> Note: the development machines for this repo (Linux) have had the SDK/NDK since 2026-09, so the
> Kotlin JVM unit tests can be run locally (no NDK/vcpkg dependency): `gradle :app:testDebugUnitTest`
> — covering the pure logic of A3 reliability (RestartPolicy/BootStartGate/WatchdogPolicy,
> `app/src/test`). Full APK assembly is verified by the build-android job of the nightly CI (that
> job runs the JVM unit tests first, then builds the APK). The C++ core logic (ScriptRunner /
> protocol / reconnect) has same-source unit tests on the desktop side
> (`libs/lua/tests/script_runner_test.cpp`, transport_tests, agentcore_test); see the verification
> matrix in §9 of `docs/android-agent-design.md`.

## A1 Link Acceptance Steps

1. Start the Go Server (`orchestrator/server`, listening on agent port 8888 and the Dashboard).
2. Install the APK on the phone, on the same subnet; the first launch creates a persistent notification (required for a foreground service).
3. Enter the server IP / port / device ID in the app → tap "Start Agent". If the server sets `WINGMAN_AGENT_TOKENS` (registration token allowlist), the registration Token must be entered as well (see `docs/agent-token-auth-design.md`; it can be left empty when disabled by default).
4. The device appears in the Dashboard's Agent list (platform=android, coming from agent.register).
5. Create the script `hello.lua` in the Dashboard: `print('hello android')` and run it on that device.
6. Acceptance points:
   - the payload pushed by the server is `{path, content, contentSha256, language}` (content inlined; contentSha256 is the content fingerprint and is recorded under the same convention in the execution record; the language is derived from the file extension);
   - `hello android` appears in the Dashboard's real-time logs (agent.event `script_output` → persisted to ExecutionLog → forwarded over WS), along with `[exec_...] finished`;
   - a long-running infinite-loop script can be stopped (stop_script → forced interruption via a Lua instruction hook).

## A2 Script Capability Steps (gesture injection + screen capture + find-color and find-image matching)

Prerequisite: the A1 link acceptance above is complete (Agent online).

1. **Enable accessibility injection**: in the app tap "Accessibility Settings" → enable the Wingman injection service in system settings (the service registers itself into the C++ core automatically once connected).
2. **Enable screen-cast capture**: in the app tap "Start Screen Cast" → the system pops up the MediaProjection authorization dialog → agree. On Android 14+ the dialog pops up again every time screen casting is restarted (system constraint).
3. **Run the sample script** (pushed by the Dashboard; the `wingman.*` APIs have the same names and shape as on desktop):

   ```lua
   -- 找红色按钮并点击（0xRRGGBB 或 {r,g,b} 表）
   local pt = wingman.vision.findColor(0xE23B3B, 10)
   if pt then
       wingman.input.tap(pt.x, pt.y)
       wingman.input.delay(500)
   end
   -- 滑动列表
   wingman.input.swipe(540, 1800, 540, 600, 400)
   -- 模板找图（相对路径按 app external files 解析）
   local m = wingman.vision.findImage('templates/ok.png', 0.9)
   if m.found then wingman.input.tap(m.position.x, m.position.y) end
   ```

4. **Remote screenshot acceptance**: add a screenshot step to the Dashboard workflow and select that device → the screenshot is pushed to the screen (same shape as desktop, JPEG q82).

Placing template images (the app's own external files need no storage permission):

```bash
adb push ok.png /sdcard/Android/data/com.wingman.agent/files/templates/
```

**Known A2 prerequisites**: screen casting only produces frames while the device screen is on (off-screen capture is a matter for after A4); when accessibility/screen-cast is not authorized, the script APIs degrade to returning false/nil instead of raising an error.

## A3 Reliability Verification Steps (boot autostart / crash self-restart / watchdog)

1. **Boot autostart**: explicitly tick "Start Agent automatically on boot" in the app (off by default) and configure the server IP → reboot the device; the agent comes online on its own without opening the app (observe in the Dashboard).
2. **Crash self-restart**: after starting the agent, force the process to crash (`adb shell am crash com.wingman.agent` or `adb shell kill <pid>`) → the agent comes back online within seconds (dual path via START_STICKY/alarm); after 5 consecutive crashes (within a 10-minute window) automatic restart stops, and opening the app to start it manually clears the counter.
3. **Watchdog**: while the agent is running, watch logcat `WingmanService` — when the core is not running due to an exception it is pulled back up every 30s (no such log line appears when it is online normally).
4. **Device-model keep-alive**: complete the vendor ROM settings per `docs/guides/android-keep-alive.md`, then leave the device idle for 10-30 minutes and verify the agent does not go offline.

Known boundary: MediaProjection authorization is not restored after a process crash/restart (on Android 14 the single session is one-shot, a system constraint), so "Start Screen Cast" must be tapped again.

## A3 Restricted Settings Verification Steps (Android 13+ sideload out of the box)

Since Android 13, accessibility installed by sideloading is blocked by default under "Restricted settings" (the root cause of the switch not turning on); once unlocked during deployment it does not recur:

1. **Script-based pre-authorization (recommended)**: after connecting a real device to the PC, `scripts/android-restricted-settings.sh check` → FAIL means not authorized; run the idempotent `allow` pre-authorization, after which `check` should PASS;
2. **Manual allow**: system settings → Apps → Wingman Agent → ⋮ → allow restricted settings, after which the accessibility switch can be turned on normally;
3. **In-app guidance**: on Android 13+ with accessibility not yet enabled, the main screen automatically pops the guidance up once (after confirming, it no longer pops up automatically; the "Restricted settings guidance" button stays resident).

For batch/Device Owner paths and troubleshooting see `docs/guides/android-restricted-settings.md`.

## Milestones

- **A1 (this directory)**: the link is established (connect / register / script delivery / log return / stop).
- **A2 script capabilities (implemented, 2026-09-21)**: the host bridge (gesture injection via dispatchGesture) / a real MediaProjection capture implementation (a `platform/android` tenant + a reverse JNI bridge); find-color and find-image matching (ImageAnalyzer, pure C++ without OpenCV: find-color does a direct lookup, find-image goes through PatternMatcher::templateMatch; the same-named desktop API goes through OpenCV cv::matchTemplate, and the two paths differ in score convention); the wingman.input/screen/vision script APIs; screenshot.capture remote screenshots (the NDK's OpenCV is used only for screenshot JPEG encoding). See §5.5/§5.6 of the design document.
- **A3 reliability (remaining items implemented, 2026-09-30)**: boot autostart (BootCompletedReceiver + switch), crash self-restart (backoff alarm + crash-string abandonment + coreRunning gating), the core watchdog, device-model keep-alive guidance (docs/guides/android-keep-alive.md); restricted settings guidance (docs/guides/android-restricted-settings.md + the pre-authorization script + in-app RestrictedSettingsPolicy guidance, landed the same day); disconnect-buffer autonomy has been carried by the ready-made C++ RemoteClient since A1 (outbox/reconnect, covered by agentcore_test). The token auth P1 landed on 2026-09-20 (see docs/agent-token-auth-design.md), with the nightly CI building the APK and the JVM unit tests entering the build-android job. Part of the A3-P2 security evolution landed (2026-10-10: per-agent tokens stored server-side + Dashboard management UI/audit, with zero changes on the agent side; token storage migrated to the Keystore encrypted store + a one-time plaintext migration; challenge-response/TLS not implemented).
- **A4 multi-device orchestration (landed)**: the Dashboard device view (tag grouping / batch delivery / platform dashboard) and the Teams page (2026-10-13); asset.sync template distribution — server-side asset storage and push via `POST /api/assets/sync` + desktop runtime writing to disk (2026-10-13), while the Android on-device side plugs in RpcDispatcher to reuse the same `registerAssetHandlers` and writes into `filesDir/assets` (2026-10-15, guarded by the same-source desktop test `android_asset_test`; the real-device walkthrough awaits a real device).