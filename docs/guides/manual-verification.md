> [中文](../zh/guides/manual-verification.md) | **English**

# Manual Verification Guide (macOS Runtime / Linux XRecord Desktop)

Of the two cross-platform verification items, the macOS leg depends on a real
macOS desktop session (headless CI cannot cover it); the Linux XRecord leg has
been verified to actually run under Xvfb (RECORD 1.13) — after the
recording-start timing defect was fixed on 2026-09-24, `RecorderX11E2E`
executes for real under Xvfb (verified passing on this machine on 2026-09-30), and
only environments with no X server / no RECORD are judged SKIP. This document
is a step-by-step operating guide for the two verification scripts:
prerequisites, system authorization items, expected output, and pass criteria.
The scripts automate the assertions themselves; humans are only responsible
for preparing the environment, responding to authorization dialogs, and
recording the results.

## Overview

| Script | Platform | Coverage | Corresponding test suite |
|------|------|--------|--------------|
| `scripts/verify-macos-runtime.sh` | Real macOS desktop session | UDS IPC / clipboard / CGWindowList screenshots / FileWatcher / CGEvent input | `UnixSocketChannelTest` / `ClipboardTest` / `ScreenTest` / `FileWatcherTest` / `InputTest` |
| `scripts/verify-xrecord-desktop.sh` | Real Linux desktop X session | XRecord capture → JSON serialization closed loop | `RecorderX11E2E` |

Conventions shared by both scripts: dependencies always go through the vcpkg
manifest (`VCPKG_MANIFEST_FEATURES=tests`), with no fallback to system
libraries; the test binary is auto-built when missing, and `--build` forces a
rebuild; **SKIP (exit 2) means 「环境不具备、未验证」 (environment unavailable,
not verified) — neither a pass nor a failure** — to prevent false greens in
headless or wrong environments.

---

## macOS: `scripts/verify-macos-runtime.sh`

### Prerequisites

- **macOS desktop session**: run it in a local terminal after logging into the
  GUI (SSH sessions have no WindowServer; screenshots and event injection are
  guaranteed to fail).
- **Xcode Command Line Tools**: `xcode-select --install` (the compiler
  toolchain; if it reports `xcodebuild requires Xcode`, see the
  BUILD.md「macOS：Xcode 未找到」 section).
- **vcpkg**: after installing, `export VCPKG_ROOT=<vcpkg 根目录>`. If the
  script cannot find `VCPKG_ROOT`, it exits with a clear error (exit 1) and
  never falls back to system libraries.
- The first run auto-configures and builds `core_tests` (the triplet is
  auto-selected per arch: `arm64-osx` / `x64-osx`); after changing C++ code,
  re-run with `--build`.

### Authorization Items (TCC, system dialogs)

| Authorization | Triggered by | Action |
|------|--------|------|
| **屏幕录制** | `ScreenTest` (CGWindowList screenshots) | 系统设置 → 隐私与安全性 → 屏幕录制, check the terminal app running the script (Terminal/iTerm/VS Code); after granting, **restart the terminal** before re-running |
| **辅助功能** | `InputTest` (CGEvent event injection) | 系统设置 → 隐私与安全性 → 辅助功能, check the same terminal app |

Without authorization the corresponding suite fails (empty/black screenshots,
injected events having no effect) but does not crash — complete the
authorization first and re-run. The first run may be interrupted once by each
of the two dialogs; once granted, subsequent cases in the same run pass.

### Operation Steps

1. Open a terminal in the GUI session and `cd` to the repo root;
2. `export VCPKG_ROOT=<vcpkg 根目录>`;
3. `scripts/verify-macos-runtime.sh` (add `--build` on the first run / after
   code changes);
4. Grant each item as the TCC authorization dialogs pop up; if failures appear
   due to missing authorization, grant it, restart the terminal, and re-run.

### Expected Output

```
==> 运行 macOS 平台套件（ClipboardTest.*:FileWatcherTest.*:ScreenTest.*:InputTest.*:UnixSocketChannelTest.*）
[ RUN      ] UnixSocketChannelTest.ConstructServerMode
[       OK ] ...
...
[  PASSED  ] <N> tests.
PASS: macOS 可自动化验证项全部通过。
提示: 以下仍属人工观察项——CGEvent 注入的焦点/权限手感（辅助功能授权）、
      录屏授权弹窗行为、通知/系统休眠交互等，需真机桌面会话中人工确认。
```

Output when the environment is invalid: not macOS →
`SKIP: 本脚本仅用于 macOS（当前 Linux）` (exit 2); missing `VCPKG_ROOT` →
`FAIL: 未设置 VCPKG_ROOT…` (exit 1).

### Pass Criteria

- **Pass**: exit 0 with the final line `PASS: macOS 可自动化验证项全部通过。`
  and zero `[  FAILED  ]` across the five suites;
- **Fail**: exit 1 with `FAIL: macOS 平台套件存在失败用例（exit=N）` — check
  the two TCC authorizations first, then investigate the specific case output;
- **Manual observation items** (not covered by the script; a human must record
  them):
  - The **focus/permission feel** of CGEvent injection: after accessibility
    authorization, does the foreground app actually receive the injected keys,
    and does the injection target behave as expected after focus switches;
  - **Screen-recording dialog behavior**: the degraded screenshot return value
    after authorization is denied;
  - The **activate background-activation semantics** of `cocoa_window` (known
    semantic doubts such as background activation often being ineffective and
    hide actually being minimize);
  - Notification / system-sleep interactions.

---

## Linux: `scripts/verify-xrecord-desktop.sh`

### Prerequisites

- **A real Linux desktop X session** (GNOME/KDE/XFCE etc.) with `DISPLAY` set.
  Wayland sessions do not apply (the script depends on the X11 RECORD extension
  and goes through the `XOpenDisplay` path).
- An X server with the **RECORD extension** (enabled by default on standard
  Xorg). Xvfb (RECORD 1.13) has been verified to work: after the
  recording-start timing defect was fixed on 2026-09-24, `EnableContext` works
  normally and the cases execute for real under Xvfb (the script actually run
  under local Xvfb PASSED on 2026-09-30 — the record is kept in the script's
  header comment and the 「真机验证自动化」 entry of
  `docs/development-todo.md`). GTEST_SKIP happens only in
  environments with no X server (`DISPLAY` unavailable) or no RECORD.
- A build configuration has been completed once per BUILD.md (the script
  reuses the existing `build/` cache and only auto-builds when the test binary
  is missing; `--build` forces a rebuild).
- **Focus warning**: the tests inject keys via XTest, which go to the
  currently focused window — **F13** is injected first (harmless on almost
  every desktop); if that keycode does not exist, it falls back to the
  ordinary key `a` and says so in the output. While it runs, do not leave
  focus on a sensitive window/input field.

### Operation Steps

1. Open a terminal in the real desktop session and `cd` to the repo root;
2. `echo $DISPLAY` to confirm it is non-empty (empty means the terminal is not
   inside an X11 graphical session);
3. `scripts/verify-xrecord-desktop.sh` (add `--build` on the first run / after
   code changes);
4. Watch the `[ RUN ] / [ OK ] / [ SKIPPED ]` lines in the output.

### Expected Output

```
==> 运行 RecorderX11E2E.*（DISPLAY=:0）
[ RUN      ] RecorderX11E2E.RecordsXTestInjectedKeyEvents
[       OK ] RecorderX11E2E.RecordsXTestInjectedKeyEvents
[ RUN      ] RecorderX11E2E.SavesCapturedEventsToJSON
[       OK ] RecorderX11E2E.SavesCapturedEventsToJSON
PASS: XRecord 捕获 + JSON 序列化闭环验证通过。
```

Output when the environment is invalid: empty `DISPLAY` →
`SKIP: DISPLAY 为空——请在真实桌面 X 会话的终端中运行` (exit 2); no X server
or RECORD unavailable (all cases `SKIPPED`) → `SKIP: 用例全部跳过——当前是
Xvfb/无头环境…请在真实桌面（GNOME/KDE/XFCE 等）的终端重跑本脚本` (exit 2,
judged **not verified**; this message is the script's older wording — the
meaning is that the current environment cannot verify it; Xvfb itself already
works, see the prerequisites above).

### Pass Criteria

- **Pass**: exit 0 with `PASS: XRecord 捕获 + JSON 序列化闭环验证通过。`;
- **Not verified**: exit 2 (environment unavailable; neither a failure nor a
  pass);
- **Fail**: exit 1 (a case `[  FAILED  ]` or a non-zero process exit).
- **Manual observation item**: the **timing feel** of playback — automation
  covers only the 「捕获 → 保存」 (capture → save) steps; the replay rhythm of
  a real keyboard/mouse recording (press/release interval fidelity, perceived
  replay speed) must be judged by a human.

---

## Result Backfill

After verification, backfill the corresponding entry in `docs/development-todo.md` (check it and
note): the date, the platform version (macOS version / Linux distribution +
desktop environment), a summary of the script output (the PASS line + the case
count), and the conclusions for the manual observation items. Headless CI
cannot replace the macOS leg; the Linux leg's Xvfb (RECORD 1.13) has been
verified to run, but a real desktop session remains the recommended standard
(replay timing feel requires human observation). If the environment is not
available for now, simply leave the entry unchecked.
