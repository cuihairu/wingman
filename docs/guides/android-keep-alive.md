> [中文](android-keep-alive.md) | **English**

# Android Agent Per-Device Keep-Alive Guide (A3)

> Applies to: Wingman Android Agent (apps/android, A3 reliability landed 2026-09-30).
> Companion design: §7 (lifecycle and keep-alive) in `docs/android-agent-design.md`.
> Sister page: `docs/guides/android-restricted-settings.md` — when the accessibility
> service **won't turn on** for a sideloaded install on Android 13+ that is a
> restricted-settings problem; read that page first, then come back.

## Background: What the Agent's Own Keep-Alive Already Does

Three layers of mechanism on the app side (no user configuration needed):

| Mechanism | Covered scenario |
|-----------|------------------|
| Foreground service + START_STICKY | After the user kills the app / the system reclaims the process, the system relaunches it |
| Crash self-restart (CrashRestartHandler + alarm) | After an uncaught exception, the process restarts with exponential backoff (1s→60s cap); after 5 consecutive crashes within 10 minutes it gives up and waits for the user or the next boot to reset |
| Core watchdog (inside WingmanService) | When the service is alive but the C++ core is not running, relaunch nativeStart every 30s |
| Boot autostart (BootCompletedReceiver) | Autostart after boot / app overlay install, per the toggle (off by default; must be explicitly enabled in the app) |

Network disconnection is not part of this: the C++ RemoteClient has its own
exponential-backoff reconnection + bounded outbox (events are cached while
disconnected and flushed on reconnect), and scripts keep executing locally
during disconnections.

## Why Manual Settings Are Still Needed

Most Chinese domestic vendor ROMs layer aggressive power-saving strategies on
top of AOSP: even when the app holds a foreground service, it can still be
frozen (cached/freezer) or killed after sitting idle in the background, and
**START_STICKY's relaunch is skipped** (some implementations remove the task
outright). This is beyond what code can self-heal — the system-level
"untrusted" verdict can only be explicitly exempted by the user in Settings.

Typical symptom of being frozen: the agent shows as offline in the Dashboard,
but the app icon is still on the device, and the agent comes back online the
moment the user opens it (the process was frozen, not killed).

## Per-Device Settings Checklist

### Xiaomi / Redmi (MIUI / HyperOS)

1. 设置 → 应用设置 → 应用管理 → Wingman Agent → turn on **自启动**;
2. On the same page → **省电策略 → 无限制**;
3. Long-press / pull down the recent-tasks card → **锁定** (a locked card is not removed by one-tap cleanup).

### Huawei / Honor (EMUI / HarmonyOS / MagicOS)

1. 设置 → 应用 → 应用启动管理 → Wingman Agent → turn off 「自动管理」, then enable all three manual toggles: **允许自启动、允许关联启动、允许后台活动**;
2. 设置 → 电池 → 更多电池设置 → 「休眠时始终保持网络连接」 may be kept on as needed (cutting the network while the screen is off drops the agent's long connection; reconnection is handled by RemoteClient's backoff).

### OPPO / OnePlus / realme (ColorOS / OxygenOS)

1. 设置 → 电池 → 更多 → 应用耗电管理 → Wingman Agent → **允许完全后台行为**;
2. 设置 → 应用管理 → Wingman Agent → allow **自启动**;
3. Lock the recent-tasks card.

### vivo / iQOO (OriginOS / Funtouch OS)

1. 设置 → 电池 → 后台耗电管理 → Wingman Agent → **允许后台高耗电**;
2. 设置 → 快捷与辅助 / 更多设置 → vivo 手机管家 → 权限管理 → 自启动 allows Wingman Agent.

### Samsung (One UI)

1. 设置 → 电池 → 后台使用限制: confirm Wingman Agent is **not** on the 「从未使用的应用」 cleanup list, and do not add it to **深度休眠应用**;
2. For devices that must stay alive with the screen off: 设置 → 电池 → 不受限制使用 (if the option is provided).

### General (recommended for all devices)

- 系统设置 → 应用 → Wingman Agent → 电池 → **不优化** (disable battery optimization);
- Keep the notification permission enabled (on ROMs where foreground-service notifications get disabled, some vendors also lower the process priority);
- The Android 12+ 「通知」 and 「电池」 settings pages vary heavily across vendors — if you cannot find the given entry, search system settings with keywords like 「省电/后台/自启动」.

## How to Verify

After applying the settings:

1. Start the agent, return to the home screen, and leave the device idle for 10–30 minutes (covering the freeze windows vendors commonly use);
2. Watch the Dashboard to confirm the agent stays online with an uninterrupted heartbeat;
3. Run `adb shell dumpsys activity services com.wingman.agent` (or
   `dumpsys activity services .WingmanService`) to confirm the service is not
   frozen; `dumpsys deviceidle` additionally shows the Doze / power-saving
   whitelist state;
4. Reboot the device once and confirm boot autostart takes effect (the agent comes online automatically without the app being opened).

## Known Boundaries (documented, not defects)

- **MediaProjection authorization is not recoverable**: since Android 14 the
  screen-cast authorization is one-shot per session; after a crash/restart the
  cast does not resume automatically, and the user must tap 「开启投屏」 again to go
  through the system dialog (system constraint, registered in A2); the agent
  pipeline (scripts/gestures/logs) is unaffected.
- **Screen-off capture**: a known A2 premise — the cast only produces frames
  while the device screen is on; capture while keeping the device alive with
  the screen off is a post-A4 item.
- **Recovery after the crash streak gives up**: after 5 consecutive crashes
  within 10 minutes (default threshold), automatic restart stops; the user must
  open the app and start it manually, or wait for the next boot — this prevents
  a "crash-restart" storm loop.
