# Wingman Android Agent

Android 端侧 Agent：出站 TCP 长链接连接 Go 中控服务器，接收 Lua 脚本内联下发并
执行，日志实时回传 Dashboard（A1）；手势注入/屏幕采集/找色找图/远程截图（A2）；
开机自启/崩溃自重启/看门狗/机型保活指引（A3）。设计与协议见
`docs/android-agent-design.md`，
端侧可行性背景见 `docs/mobile-support-feasibility.md`。

```
Kotlin 壳（前台服务/配置/状态）  ──JNI（进程内直调，非 IPC）──▶  C++ 核心
WingmanService / MainActivity                     AndroidAgent = RemoteClient
WingmanAccessibilityService（A2）                 + ScriptRunner(Lua) + transport
```

架构硬约束自检：Agent 只有 **出站** TCP 客户端，零监听端口；本地 UI 走进程内
JNI（无 IPC、无 HTTP server）；Dashboard 只连 Go Server。

## 环境准备

| 工具 | 版本 |
|------|------|
| JDK | 17+ |
| Android SDK | API 34（platform-tools / build-tools） |
| Android NDK | r26+ |
| CMake | 3.22.1+（SDK 的 cmake 亦可） |
| vcpkg | 已 bootstrap，工具链 `scripts/buildsystems/vcpkg.cmake` |
| Gradle | 8.7+（或用本机 gradle 生成 wrapper） |

Android 依赖用 `apps/android/cpp/vcpkg.json` 独立清单（`wingman-agent-android`，
manifest 模式随 NDK CMake 配置自动安装，无需手动 `vcpkg install`；包集合为桌面
同源包的子集：asio/curl/lua/openssl/sol2/spdlog/nlohmann-json/opencv4）。

在 `gradle.properties` 填入 vcpkg 根：

```properties
wingmanVcpkgRoot=/absolute/path/to/vcpkg
```

## 构建

```bash
gradle :app:assembleDebug
# 产物 app/build/outputs/apk/debug/app-debug.apk
```

构建链路：AGP externalNativeBuild → NDK CMake（`apps/android/cpp/CMakeLists.txt`）
→ vcpkg arm64-android 依赖 + 仓库内共享库 `libs/transport`、`libs/agentcore`
（远程链路，桌面 runtime 同源链接）、`libs/androidagent`（ScriptRunner +
脚本能力 API + core 子集）→ `libwingman_agent.so`。

> [双端同源文件（`libs/agentcore`、`libs/androidagent`，以及 `libs/transport`]
> 等共享层）同时编译进桌面 `wingman-runtime` 与 Android `libwingman_agent.so`：
> 改动必须过两端编译与桌面同源单测，不要只验证桌面侧。

> 说明：本仓库开发机（Linux）自 2026-09 起具备 SDK/NDK，可本地跑 Kotlin
> JVM 单测（不依赖 NDK/vcpkg）：`gradle :app:testDebugUnitTest`——覆盖 A3
> 可靠性纯逻辑（RestartPolicy/BootStartGate/WatchdogPolicy，`app/src/test`）。
> 完整 APK 组装由 nightly CI 的 build-android job 验证（该 job 先跑 JVM
> 单测再打 APK）。C++ 核心逻辑（ScriptRunner/协议/重连）在桌面环境有同源
> 单测（`libs/lua/tests/script_runner_test.cpp`、transport_tests、
> agentcore_test），见 `docs/android-agent-design.md` §9 验证矩阵。

## 真机端到端验收（A1 验收步骤）

1. 启动 Go Server（`orchestrator/server`，监听 agent 端口 8888 与 Dashboard）。
2. 手机安装 APK，同网段；首次启动会创建常驻通知（前台服务要求）。
3. App 内填入服务器 IP / 端口 / 设备 ID → 「启动 Agent」。若 server 设置了
   `WINGMAN_AGENT_TOKENS`（注册 token 白名单），需同时填入注册 Token
   （见 `docs/agent-token-auth-design.md`；默认关闭可留空）。
4. Dashboard 的 Agent 列表出现该设备（platform=android，来自 agent.register）。
5. Dashboard 新建脚本 `hello.lua`：`print('hello android')` 并运行到该设备。
6. 验收点：
   - server 下发 payload 为 `{path, content, language:"lua"}`（内容内联）；
   - Dashboard 实时日志出现 `hello android`（agent.event `script_output` →
     ExecutionLog 落库 → WS 转发）与 `[exec_...] finished`；
   - 长死循环脚本可被停止（stop_script → Lua 指令 hook 强制中断）。

## A2 脚本能力使用步骤（手势注入 + 屏幕采集 + 找色找图）

前置：完成上面 A1 链路验收（Agent 在线）。

1. **开启无障碍注入**：App 内点「无障碍设置」→ 系统设置里启用
   Wingman 注入服务（服务连接后自动注册进 C++ 核心）。
2. **开启投屏采集**：App 内点「开启投屏」→ 系统弹出 MediaProjection
   授权对话框 → 同意。Android 14+ 每次重开投屏都会再弹一次（系统约束）。
3. **跑示例脚本**（Dashboard 下发，`wingman.*` API 与桌面同名同形）：

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

4. **远程截图验收**：Dashboard workflow 加 screenshot 步骤选该设备 →
   截图上屏（与桌面同形，JPEG q82）。

模板图放置（app 对自身 external files 免存储权限）：

```bash
adb push ok.png /sdcard/Android/data/com.wingman.agent/files/templates/
```

**A2 已知前提**：投屏仅在设备亮屏时出帧（息屏采集属 A4 之后事项）；
无障碍/投屏未授权时脚本 API 降级返回 false/nil，不报错。

## A3 可靠性验证步骤（开机自启 / 崩溃自重启 / 看门狗）

1. **开机自启**：App 内显式勾选「开机自动启动 Agent」（默认关）并配置过服务器
   IP → 重启设备，agent 未打开 App 即自动上线（Dashboard 观察）。
2. **崩溃自重启**：启动 agent 后强制崩溃进程
   （`adb shell am crash com.wingman.agent` 或 `adb shell kill <pid>`）→
   数秒内 agent 重新上线（START_STICKY/闹钟双路径）；连续崩溃 5 次
   （10 分钟窗口）后自动重启停摆，打开 App 手动启动可清零。
3. **看门狗**：agent 运行中观察 logcat `WingmanService`——核心异常不在跑时
   每 30s 重拉（正常在线时无该日志）。
4. **机型保活**：按 `docs/guides/android-keep-alive.md` 完成厂商 ROM 设置，
   静置 10-30 分钟验证 agent 不离线。

已知边界：MediaProjection 授权在进程崩溃/重启后不恢复（Android 14 单会话
一次性，系统约束），需重新点「开启投屏」。

## A3 受限设置验证步骤（Android 13+ 侧载开箱）

Android 13 起侧载安装的无障碍被「受限设置」默认屏蔽（开关打不开的根因），
部署期一次性解开后不再复发：

1. **脚本预授权（推荐）**：电脑连真机后
   `scripts/android-restricted-settings.sh check` → FAIL 即未授权，
   `allow` 幂等预授权后再 `check` 应 PASS；
2. **手动允许**：系统设置 → 应用 → Wingman Agent → ⋮ → 允许受限制的
   设置，随后无障碍开关即可正常打开；
3. **App 内引导**：Android 13+ 且未开无障碍时主界面自动弹一次指引
   （确认过不再自动弹，「受限设置指引」按钮常驻）。

批量/Device Owner 路径与故障排查见 `docs/guides/android-restricted-settings.md`。

## 里程碑

- **A1（本目录）**：链路打通（连接/注册/脚本下发/日志回传/停止）。
- **A2 脚本能力（已实施，2026-09-21）**：宿主桥（dispatchGesture 手势注入）/
  MediaProjection 采集真实现（`platform/android` 租户 + 反向 JNI 桥）；
  找色找图（ImageAnalyzer + OpenCV 进 NDK）；wingman.input/screen/vision
  脚本 API；screenshot.capture 远程截图。见设计文档 §5.5/§5.6。
- **A3 可靠性（剩余项已实施，2026-09-30）**：开机自启（BootCompletedReceiver
  + 开关）、崩溃自重启（退避闹钟 + 崩溃串放弃 + coreRunning 门控）、核心
  看门狗、机型保活指引（docs/guides/android-keep-alive.md）；受限设置引导
  （docs/guides/android-restricted-settings.md + 预授权脚本 + App 内
  RestrictedSettingsPolicy 引导，同日落地）；断连缓存自治
  自 A1 起由 C++ RemoteClient 现成承担（outbox/重连，agentcore_test 覆盖）。
  token 认证 P1 已于 2026-09-20 落地（见 docs/agent-token-auth-design.md），
  nightly CI 打 APK、JVM 单测入 build-android job。剩余 A3-P2 安全演进。
- **A4 多设备编排**：Dashboard 设备视图、批量下发、asset.sync 模板分发。
