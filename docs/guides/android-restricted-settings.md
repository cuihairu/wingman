> [中文](../zh/guides/android-restricted-settings.md) | **English**

# Android Agent Restricted-Settings Guide (Android 13+, A3)

> Applies to: Wingman Android Agent (apps/android, A3 deployment experience landed 2026-09-30).
> Companion design: §5.2 (three tiers of restricted-settings solutions) in
> `docs/mobile-support-feasibility.md`, and D7 (adb appears only at deployment
> time) in `docs/mobile-automation-design.md`.
> Sister page: `docs/guides/android-keep-alive.md` — this page solves
> 「无障碍**开不了**」 (accessibility **won't turn on**), the keep-alive page
> solves 「开了之后**被杀**」 (killed **after it is on**); the two pages
> complement each other.

## Background: What "Restricted Settings" Are

From Android 13 (API 33) onward, apps installed via **sideloading** (non-store
channels: adb install, direct APK download, etc.) are flagged by the system as
"restricted settings" — their governed sensitive permissions and services (the
accessibility service is one of them) are **silently blocked**: the user can
find the toggle in system settings, but it **won't turn on** (tapping does
nothing, or it says Android restricts it).

This is the single biggest source of out-of-box failures on the device-side
Agent (risk-table rating in mobile-support-feasibility.md: **high**): every
injection capability of the Agent (the A2 dispatchGesture gesture) presupposes
the accessibility service being enabled; without lifting restricted settings,
none of the following steps can even begin.

**Unaffected devices**: Android 12 (API 32) and below; apps installed through
app-store channels (most store installs do not trigger the flag).

## Symptom Identification

| Symptom | Points to |
|---------|-----------|
| 系统设置 → 无障碍 → Wingman 注入服务, the toggle won't turn on | This page (restricted settings) |
| The toggle turns on but the service is repeatedly stopped | Keep-alive page (vendor power-saving strategies) |
| The toggle turns on and the app's internal state is normal, but injection has no effect | A2 capability issue (investigate separately) |

## Three Tiers of Solutions (choose one per scenario)

### Option 1: Manual Allow (single device, no computer needed)

1. 系统设置 → **应用** → **Wingman Agent** → enter the app info page;
2. Tap the top-right **⋮（更多）** → **允许受限制的设置** (the system pops a confirmation);
3. Go back to the 无障碍 settings — the toggle now turns on normally;
4. Return to the app and tap the 「无障碍设置」 button to double-check that the service is enabled.

> On some custom ROMs the ⋮ menu sits in a different place, or the entry name
> differs slightly (「允许受限制的设置」 also appears as 「允许受限设置」); if
> you cannot find the entry, update the system app component first, or switch
> to Option 2.

### Option 2: adb Pre-Authorization (scriptable, idempotent)

The repo provides a one-shot script `scripts/android-restricted-settings.sh`
(three-state convention shared with the other verify-*.sh scripts:
PASS=0 / FAIL=1 / SKIP=2):

```sh
# 检查当前状态（默认动作）
scripts/android-restricted-settings.sh check
# 幂等预授权（allow 后自动复核）
scripts/android-restricted-settings.sh allow
# 只打印状态 / 还原为 default
scripts/android-restricted-settings.sh status
scripts/android-restricted-settings.sh revoke
# 多包名部署
WINGMAN_ANDROID_PKG=com.example.wingman2 scripts/android-restricted-settings.sh allow
```

Verdict rules: **no adb / no authorized device → SKIP** (environment not
ready, does not count as failure); package not installed → FAIL (install the
APK first); API < 33 → PASS (not governed). The script is backed by a Go
contract test guard
(`orchestrator/server/integration/android_restricted_settings_script_test.go`,
a fake adb driver that never touches real devices).

The equivalent raw commands (the script is an idempotent wrapper around them):

```sh
adb shell appops set com.wingman.agent ACCESS_RESTRICTED_SETTINGS allow
adb shell appops get com.wingman.agent ACCESS_RESTRICTED_SETTINGS
```

> **Boundary (design decision D7)**: adb appears only on the deployment /
> one-time authorization path. This script plus this document are the entirety
> of it — no runtime link (the agent itself, the remote control plane) depends
> on adb.

### Option 3: Device Owner / MDM Bulk Management

For large-scale multi-device deployments, fold 「允许受限设置」 into the mobile
device management policy, delivered centrally by the MDM platform (or a
device-owner app registered with `dpm set-device-owner`), avoiding per-device
manual operation. Key points:

- Prerequisite: the device is already enrolled as a Device Owner (consumer retail devices usually do not apply);
- This repo does not ship an MDM configuration — push the same appops /
  restricted-settings policy via your enterprise's existing MDM platform; the
  exact policy item names follow the platform documentation.

## In-App Guidance

The main screen provides two layers of guidance (landed in A3):

- **Persistent button** 「受限设置指引（Android 13+）」: view the full text of the three tiers at any time;
- **Automatic prompt**: on Android 13+ with accessibility not enabled,
  returning to the app foreground pops the guidance dialog once; after one
  confirmation it stops popping automatically (the button stays).

> Verdict rule for the automatic prompt (honest boundary): the public API
> **cannot distinguish** "accessibility is blocked by restricted settings" from
> "the user simply hasn't enabled it yet" — the system does not expose that
> state. So it triggers per `RestrictedSettingsPolicy` (API ≥ 33 and not
> enabled and not confirmed); Android 12 and below never trigger it. The
> "accessibility failure detection report" relies on a reserved slot in
> device.capabilities and is registered separately as not implemented.

## How to Verify

1. (script path) `check` outputs `PASS ... allow`;
2. (manual path) the toggle in the accessibility settings turns on normally and stays enabled;
3. Beyond the 「核心运行」 entry in the app status area, remotely dispatch an
   injection-dependent command (e.g. `screenshot.capture`) to verify the pipeline.

## Known Boundaries (documented, not defects)

- **ROM differences**: a few ROMs do not support this appop — if the recheck
  after `allow` still does not read allow, the script FAILs explicitly and
  points back to the manual path; it never reports false success;
- **Real-device acceptance scope**: the steps on this page were written against
  the vendors' official documentation, and the script behavior was verified in
  AOSP / near-stock environments; per-device, screenshot-level acceptance of
  the ⋮ menu on the five vendors' (Xiaomi/Huawei/OPPO/vivo/Samsung) custom ROMs
  was not executed on this machine (no real-device environment) and is
  registered for spot checks on real hardware;
- **Store installs are not affected**: builds distributed via app stores are
  not governed by restricted settings (when shipping via stores, this page only
  needs to be archived for reference).
