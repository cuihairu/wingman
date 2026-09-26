# Wingman Android Agent（A1）

Android 端侧 Agent：出站 TCP 长链接连接 Go 中控服务器，接收 Lua 脚本内联下发并
执行，日志实时回传 Dashboard。设计与协议见 `docs/android-agent-design.md`，
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

安装 Android 依赖（与桌面同一份 `vcpkg.json`， triplet 换 android）：

```bash
vcpkg install --triplet arm64-android asio lua sol2 spdlog nlohmann-json
```

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

> 说明：本仓库开发机（Linux，无 SDK/NDK）不构建此工程；Android 侧以
> 「代码交付 + 构建脚本」为准，首次构建在本文件环境准备完成后进行。
> C++ 核心逻辑（ScriptRunner/协议/重连）已在桌面环境有同源单测
> （`libs/lua/tests/script_runner_test.cpp`、transport_tests），见
> `docs/android-agent-design.md` §9 验证矩阵。

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

## A2 能力闭环使用步骤（手势注入 + 屏幕采集 + 找色找图）

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

**A2 已知前提**：投屏仅在设备亮屏时出帧（息屏采集/保活归 A3）；
无障碍/投屏未授权时脚本 API 降级返回 false/nil，不报错。

## 里程碑

- **A1（本目录）**：链路打通（连接/注册/脚本下发/日志回传/停止）。
- **A2 能力闭环（已实施，2026-09-21）**：宿主桥（dispatchGesture 手势注入）/
  MediaProjection 采集真实现（`platform/android` 租户 + 反向 JNI 桥）；
  找色找图（ImageAnalyzer + OpenCV 进 NDK）；wingman.input/screen/vision
  脚本 API；screenshot.capture 远程截图。见设计文档 §5.5/§5.6。
- **A3 可靠性**：开机自启、崩溃自重启、断连自治、token 认证、CI Android job。
- **A4 多设备编排**：Dashboard 设备视图、批量下发、asset.sync 模板分发。
