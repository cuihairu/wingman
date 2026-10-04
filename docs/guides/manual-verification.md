# 真机验证指引（macOS 运行时 / Linux XRecord 桌面）

todo.md「跨平台验证」剩余两项均依赖真实 OS 环境——macOS 桌面会话、Linux 真桌面 X
server——headless CI 与 Xvfb 无法覆盖。本文档是两个真机脚本的逐步操作指引：前置
条件、系统授权项、预期输出与通过判定。脚本本身已自动化断言，人工只负责准备环境、
响应授权弹窗与记录结果。

## 总览

| 脚本 | 平台 | 覆盖项 | 对应测试套件 |
|------|------|--------|--------------|
| `scripts/verify-macos-runtime.sh` | macOS 真机桌面会话 | UDS IPC / 剪贴板 / CGWindowList 截图 / FileWatcher / CGEvent 输入 | `UnixSocketChannelTest` / `ClipboardTest` / `ScreenTest` / `FileWatcherTest` / `InputTest` |
| `scripts/verify-xrecord-desktop.sh` | Linux 真桌面 X 会话 | XRecord 捕获 → JSON 序列化闭环 | `RecorderX11E2E` |

两个脚本共用的约定：依赖一律走 vcpkg manifest（`VCPKG_MANIFEST_FEATURES=tests`），
不回退系统库；缺测试二进制时自动构建，`--build` 强制重建；**SKIP（exit 2）表示
「环境不具备、未验证」，不算通过也不算失败**——防止在无头/错误环境误报绿。

---

## macOS：`scripts/verify-macos-runtime.sh`

### 前置条件

- **macOS 桌面会话**：登录 GUI 后在本机终端执行（SSH 会话无 WindowServer，截图与
  事件注入必然失败）。
- **Xcode Command Line Tools**：`xcode-select --install`（编译器链；报
  `xcodebuild requires Xcode` 时见 BUILD.md「macOS：Xcode 未找到」一节）。
- **vcpkg**：安装后 `export VCPKG_ROOT=<vcpkg 根目录>`。脚本找不到 `VCPKG_ROOT`
  会明确报错退出（exit 1），不会尝试系统库。
- 首次运行自动配置并构建 `core_tests`（triplet 按 arch 自动选 `arm64-osx` /
  `x64-osx`）；改动 C++ 代码后重跑加 `--build`。

### 授权项（TCC，系统弹窗）

| 授权 | 触发者 | 操作 |
|------|--------|------|
| **屏幕录制** | `ScreenTest`（CGWindowList 截图） | 系统设置 → 隐私与安全性 → 屏幕录制，勾选运行脚本的终端 App（Terminal/iTerm/VS Code）；授权后**重启终端**再跑 |
| **辅助功能** | `InputTest`（CGEvent 事件注入） | 系统设置 → 隐私与安全性 → 辅助功能，勾选同一终端 App |

未授权时对应套件会失败（截到空图/黑图、注入事件不生效），不会崩溃——先完成授权
再重跑即可。首次运行可能被两个弹窗各打断一次，授权后同一次运行内后续用例即可通过。

### 操作步骤

1. GUI 会话中打开终端，`cd` 到仓库根目录；
2. `export VCPKG_ROOT=<vcpkg 根目录>`；
3. `scripts/verify-macos-runtime.sh`（首次/代码改动后加 `--build`）；
4. 弹出 TCC 授权弹窗时逐项授权；若因未授权出现失败，授权并重启终端后重跑。

### 预期输出

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

环境不合法时的输出：非 macOS → `SKIP: 本脚本仅用于 macOS（当前 Linux）`（exit 2）；
缺 `VCPKG_ROOT` → `FAIL: 未设置 VCPKG_ROOT…`（exit 1）。

### 通过判定

- **通过**：exit 0 且末行 `PASS: macOS 可自动化验证项全部通过。`、五套件零
  `[  FAILED  ]`；
- **失败**：exit 1 且 `FAIL: macOS 平台套件存在失败用例（exit=N）`——先检查两个
  TCC 授权，再排查具体用例输出；
- **人工观察项**（脚本不覆盖，需真人记录）：
  - CGEvent 注入的**焦点/权限手感**：辅助功能授权后，前台应用是否真实收到注入
    按键、焦点切换后注入去向是否符合预期；
  - **录屏授权弹窗行为**：拒绝授权后截图返回值的降级表现；
  - `cocoa_window` 的 **activate 后台激活语义**（后台激活常无效、hide 实为最小化
    等已知语义疑点，见 todo.md 三后端装配断链条目）；
  - 通知 / 系统休眠交互。

---

## Linux：`scripts/verify-xrecord-desktop.sh`

### 前置条件

- **Linux 真桌面 X 会话**（GNOME/KDE/XFCE 等），`DISPLAY` 已设置。Wayland 会话
  不适用（脚本依赖 X11 RECORD 扩展，走 `XOpenDisplay` 链路）。
- X server 带 **RECORD 扩展**（标准 Xorg 默认启用）。Xvfb 的 RECORD
  `EnableContext` 必然失败（`XRecordBadContext`），用例会 GTEST_SKIP——这就是
  必须真桌面的原因。
- 已按 BUILD.md 完成过一次构建配置（脚本复用既有 `build/` 缓存，缺测试二进制时
  才自动构建；`--build` 强制重建）。
- **焦点注意**：测试经 XTest 注入按键，会进入当前焦点窗口——优先注入 **F13**
  （绝大多数桌面无副作用），键码不存在时回退普通键 `a` 并在输出中提示。运行期间
  不要把焦点放在敏感窗口/输入框上。

### 操作步骤

1. 真桌面会话中打开终端，`cd` 到仓库根目录；
2. `echo $DISPLAY` 确认非空（为空说明该终端不在 X11 图形会话内）；
3. `scripts/verify-xrecord-desktop.sh`（首次/代码改动后加 `--build`）；
4. 观察输出的 `[ RUN ] / [ OK ] / [ SKIPPED ]` 行。

### 预期输出

```
==> 运行 RecorderX11E2E.*（DISPLAY=:0）
[ RUN      ] RecorderX11E2E.RecordsXTestInjectedKeyEvents
[       OK ] RecorderX11E2E.RecordsXTestInjectedKeyEvents
[ RUN      ] RecorderX11E2E.SavesCapturedEventsToJSON
[       OK ] RecorderX11E2E.SavesCapturedEventsToJSON
PASS: XRecord 捕获 + JSON 序列化闭环验证通过。
```

环境不合法时的输出：`DISPLAY` 为空 → `SKIP: DISPLAY 为空——请在真实桌面 X 会话
的终端中运行`（exit 2）；Xvfb/无 RECORD 环境 → 用例全部 `SKIPPED` 后
`SKIP: 用例全部跳过——当前是 Xvfb/无头环境…请在真实桌面（GNOME/KDE/XFCE 等）的
终端重跑本脚本`（exit 2，判定为**未验证**）。

### 通过判定

- **通过**：exit 0 且 `PASS: XRecord 捕获 + JSON 序列化闭环验证通过。`；
- **未验证**：exit 2（环境不具备，不算失败也不算通过）；
- **失败**：exit 1（用例 `[  FAILED  ]` 或进程非零退出）。
- **人工观察项**：回放（playback）**时序手感**——自动化只覆盖「捕获 → 保存」
  两步；真实键鼠录制后的回放节奏（按下/释放间隔保持、回放速率观感）需真人操作
  判定，见 todo.md「Linux 宏录制（XRecord）真桌面验证」条目。

---

## 结果回填

验证完成后回填 todo.md 对应条目（勾选并注明）：日期、平台版本（macOS 版本 /
Linux 发行版 + 桌面环境）、脚本输出摘要（PASS 行 + 用例数）、人工观察项的结论。
Xvfb/CI 无法替代本页两项——如环境暂缺，保留条目不勾选即可。
