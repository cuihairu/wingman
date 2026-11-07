> [English](../../guide/source-layer-glossary.md) | **中文**

# 源码分层术语表

下表记录 2026-11-07 源码分析所使用的实现层术语。文件引用指向主要定义或装配点，不是完整调用清单。

## 应用入口与生命周期

| 术语 | 定义 | 证据 |
|---|---|---|
| 桌面运行时进程 | `wingman-agent` 先尝试内嵌脚本，再回退 GUI 模式；显式参数进入命令派发。 | `apps/agent/src/main.cpp:24`、`apps/agent/src/main.cpp:76`、`apps/agent/src/main.cpp:102` |
| 运行时命令派发 | CLI 将 argv 转为向量并交给运行时命令处理。 | `apps/agent/src/main.cpp:110`、`apps/agent/src/cli.cpp:46`、`apps/agent/src/commands/start_command.cpp:72` |
| Agent 生命周期 | 运行时初始化读取 TOML 文件或接收显式对象；关闭时按序停止远程与本地链路。 | `apps/agent/src/agent.cpp:172`、`apps/agent/src/agent.cpp:191`、`apps/agent/src/agent.cpp:237` |
| 本地 IPC 服务 | 本地能力通过调度器支撑的 IPC 服务发布，包含运行状态与远程配置访问。 | `apps/agent/src/agent.cpp:292`、`apps/agent/src/agent.cpp:297`、`apps/agent/src/agent.cpp:314` |
| 服务端运行生命周期 | Orchestrator 加载配置、创建目录、打开并迁移 SQLite，再装配注册表、HTTP、TCP、工作流与保险箱组件。 | `orchestrator/server/main.go:50`、`orchestrator/server/main.go:64`、`orchestrator/server/main.go:68`、`orchestrator/server/main.go:85`、`orchestrator/server/main.go:97`、`orchestrator/server/main.go:127`、`orchestrator/server/main.go:144` |
| Dashboard 应用生命周期 | Umi 先初始化登录态、运行布局、请求错误处理和 WebSocket，再渲染路由。 | `orchestrator/dashboard/src/app.tsx:36`、`orchestrator/dashboard/src/app.tsx:40`、`orchestrator/dashboard/src/app.tsx:47`、`orchestrator/dashboard/src/app.tsx:193` |
| 桌面 GUI 外壳 | Tauri 管理 Svelte 前端、全局快捷键、托盘和显式注册的命令面。 | `apps/gui/src-tauri/src/main.rs:10`、`apps/gui/src-tauri/src/main.rs:13`、`apps/gui/src-tauri/src/main.rs:23`、`apps/gui/src-tauri/src/main.rs:40` |
| Android 宿主进程 | Kotlin 初始化只是 NDK Agent 的 Android Application 外壳。 | `apps/android/app/src/main/java/com/wingman/agent/WingmanApplication.kt:9` |

## 远程 Agent 控制

| 术语 | 定义 | 证据 |
|---|---|---|
| 远程客户端 | Agentcore 拥有 TCP 传输、注册元数据、鉴权材料、重连/心跳状态与出站统计。 | `libs/agentcore/src/remote_client.cpp:30`、`libs/agentcore/src/remote_client.cpp:47`、`libs/agentcore/src/remote_client.cpp:69`、`libs/agentcore/src/remote_client.cpp:79` |
| 远程注册 | 客户端发送 `agent.register`，消费 ACK 并记录注册成功。 | `libs/agentcore/src/remote_client.cpp:372`、`libs/agentcore/src/remote_client.cpp:463`、`libs/agentcore/src/remote_client.cpp:486` |
| 远程心跳 | 心跳上报链路计数，并由服务端注册表保留。 | `libs/agentcore/src/remote_client.cpp:356`、`libs/agentcore/src/remote_client.cpp:374`、`orchestrator/server/internal/agent/registry.go:47`、`orchestrator/server/internal/agent/registry.go:171` |
| 重连策略 | 客户端使用指数退避加抖动，睡眠可被停止信号打断。 | `libs/agentcore/src/remote_client.cpp:271`、`libs/agentcore/src/remote_client.cpp:299`、`libs/agentcore/src/remote_client.cpp:313` |
| Frame 监听器 | Go 服务端接受 Runtime 主动外连，分配连接 ID 并启动逐连接读循环。 | `orchestrator/server/internal/agent/listener.go:232`、`orchestrator/server/internal/agent/listener.go:264`、`orchestrator/server/internal/agent/listener.go:303` |
| 出站命令 | 服务端命令是 16 字节帧加 JSON 请求；sequence 与 pending response channel 配对。 | `orchestrator/server/internal/agent/listener.go:308`、`orchestrator/server/internal/agent/listener.go:325`、`orchestrator/server/internal/agent/listener.go:339` |
| 远程命令桥 | 远程 `agent.*` 命令转为共享 RPC 请求，由桌面 Runtime 调度器处理。 | `apps/agent/src/agent.cpp:77`、`apps/agent/src/agent.cpp:87`、`apps/agent/src/agent.cpp:97` |
| Android Agent 运行时 | NDK Agent 拥有远程客户端、当前脚本、资产分发与共享状态 JSON。 | `apps/android/cpp/agent/android_agent.cpp:110`、`apps/android/cpp/agent/android_agent.cpp:132`、`apps/android/cpp/agent/android_agent.cpp:166` |

## 协议与传输

| 术语 | 定义 | 证据 |
|---|---|---|
| Agent 帧 | Runtime 到服务端使用 16 字节定长头加 JSON 体。 | `orchestrator/server/internal/agent/client.go:20`、`orchestrator/server/internal/agent/client.go:39`、`libs/agentcore/include/wingman/agentcore/remote_client.hpp:137` |
| 通用传输帧 | 通用 4 字节长度/正文帧支撑 TCP socket 与 session。 | `libs/transport/include/wingman/transport/simple_protocol.hpp:18`、`libs/transport/include/wingman/transport/simple_protocol.hpp:34`、`libs/transport/include/wingman/transport/simple_protocol.hpp:70` |
| 本地 IPC 信封 | GUI-Runtime 消息是长度前缀 JSON 信封，包含 type、method、payload、id 和 timestamp。 | `lib/wingman/include/wingman/ipc/iipc_channel.hpp:26`、`lib/wingman/include/wingman/ipc/iipc_channel.hpp:36`、`apps/gui/src-tauri/src/ipc/client.rs:16` |
| 本地 IPC 传输 | 桌面传输在 Windows 选择命名管道，其他平台选择 Unix socket，TCP 是回退。 | `lib/wingman/src/platform/ipc_factory.cpp:31`、`lib/wingman/src/platform/ipc_factory.cpp:71`、`apps/gui/src-tauri/src/ipc/client.rs:228`、`apps/gui/src-tauri/src/ipc/client.rs:245` |
| RPC 调度器 | 本地 IPC 与远程桥命令共享 JSON call/response 调度器。 | `lib/wingman/src/rpc/rpc_dispatcher.cpp:34`、`apps/agent/src/local_ipc_server.cpp:139`、`apps/agent/src/local_ipc_server.cpp:172`、`apps/agent/src/agent.cpp:97` |
| Dashboard WebSocket | Umi 使用查询参数 token 连接 `/ws`，接收带类型的 `{type,event,data}` 推送。 | `orchestrator/dashboard/src/services/websocket.ts:27`、`orchestrator/dashboard/src/services/websocket.ts:41`、`orchestrator/dashboard/src/services/websocket.ts:95` |
| WebSocket Hub | Go hub 校验查询 token、路由订阅并广播类型化 JSON 通知。 | `orchestrator/server/pkg/websocket/hub.go:85`、`orchestrator/server/pkg/websocket/hub.go:119`、`orchestrator/server/pkg/websocket/hub.go:277`、`orchestrator/server/pkg/websocket/hub.go:343` |
| 代理隧道 | Agent 侧数据面是外连裸 TCP 隧道，不缓存可重放代理数据。 | `libs/agentcore/src/remote_client.cpp:74`、`libs/agentcore/src/proxy_tunnel.cpp:355`、`apps/android/cpp/agent/android_agent.cpp:32` |

## 脚本与运行时能力

| 术语 | 定义 | 证据 |
|---|---|---|
| 脚本引擎契约 | 语言引擎实现生命周期、模块注册、文件/字符串执行与函数调用。 | `lib/wingman/include/wingman/script/iscript_engine.hpp:22`、`lib/wingman/include/wingman/script/iscript_engine.hpp:57`、`lib/wingman/include/wingman/script/iscript_engine.hpp:80` |
| 引擎注册表 | 工厂/注册表把语言名映射到已注册引擎。 | `lib/wingman/include/wingman/script/script_engine_factory.hpp:20`、`lib/wingman/include/wingman/script/script_engine_factory.hpp:36` |
| Lua 引擎 | Lua 适配器在初始化时注册 `wingman` 表和模块函数。 | `libs/lua/src/lua_script_engine.cpp:34`、`libs/lua/src/lua_script_engine.cpp:53`、`libs/lua/src/lua_script_engine.cpp:110` |
| 脚本模块集合 | `registerAllModules` 安装完整的 41 个描述符脚本门面。 | `lib/wingman/src/script/modules/module_registry.cpp:54`、`lib/wingman/src/script/modules/module_registry.cpp:101` |
| 脚本管理器 | 管理脚本配置、生命周期状态、引擎实例、输出回调和热重载。 | `lib/wingman/include/wingman/script_manager.hpp:34`、`lib/wingman/include/wingman/script_manager.hpp:47`、`lib/wingman/src/script_manager.cpp:242`、`lib/wingman/src/script_manager.cpp:833` |
| 平台能力 | 屏幕、输入、窗口、剪贴板与文件监听使用抽象接口和平台工厂/后端。 | `lib/wingman/include/wingman/platform/iscreen.hpp:19`、`lib/wingman/include/wingman/platform/iinput.hpp:17`、`lib/wingman/include/wingman/platform/iwindow.hpp:19`、`lib/wingman/src/platform/input_factory.cpp:28`、`lib/wingman/src/platform/screen_backend.cpp:47`、`lib/wingman/src/platform/window_backend.cpp:281` |
| 事件缓冲 | Runtime 事件缓存供 GUI 拉取，并报告丢弃数量。 | `libs/agentcore/include/wingman/agentcore/event_buffer.hpp:10`、`libs/agentcore/src/event_buffer.cpp:14`、`apps/gui/src/lib/stores/events.ts:9`、`apps/gui/src/lib/stores/events.ts:153` |
| Runtime 事件总线 | 面向 Runtime 组件的线程安全内存发布/订阅设施。 | `lib/wingman/include/wingman/core/event_bus.hpp:17`、`lib/wingman/src/core/event_bus.cpp:13`、`lib/wingman/src/core/event_bus.cpp:56` |
| 视觉分析 | 图像、颜色和模板操作通过 `ImageAnalyzer` 与平台采集分离。 | `lib/wingman/include/wingman/vision/image_analyzer.hpp:22`、`lib/wingman/include/wingman/vision/image_analyzer.hpp:44` |
| 内嵌资源包 | 可执行文件可携带压缩/加密脚本字节，含头元数据、校验和与口令处理。 | `apps/agent/src/resource_loader.cpp:42`、`apps/agent/src/resource_loader.cpp:127`、`apps/agent/src/resource_loader.cpp:149`、`apps/agent/src/resource_loader.cpp:167` |

## 编排与工作流

| 术语 | 定义 | 证据 |
|---|---|---|
| Agent 注册表 | Registry 保存活跃连接、状态/资源/标签/能力、心跳和变更通知。 | `orchestrator/server/internal/agent/registry.go:26`、`orchestrator/server/internal/agent/registry.go:31`、`orchestrator/server/internal/agent/registry.go:61`、`orchestrator/server/internal/agent/registry.go:96` |
| 工作流引擎 | Workflow 引擎建模定义、执行、依赖步骤、调度、取消与执行记录。 | `orchestrator/server/internal/workflow/engine.go:39`、`orchestrator/server/internal/workflow/engine.go:54`、`orchestrator/server/internal/workflow/engine.go:395`、`orchestrator/server/internal/workflow/engine.go:1302` |
| 工作流模板 | 内置模板用声明式载荷构造器实例化常见工作流。 | `orchestrator/server/internal/workflow/templates.go:16`、`orchestrator/server/internal/workflow/templates.go:27` |
| HTTP 路由 | RegisterRoutes 统一中间件、认证、RBAC 和资源 API 分组。 | `orchestrator/server/internal/handlers/routes.go:38`、`orchestrator/server/internal/handlers/routes.go:49`、`orchestrator/server/internal/handlers/routes.go:213`、`orchestrator/server/internal/handlers/routes.go:271` |
| API 客户端门面 | Dashboard 工作流/Agent/脚本操作集中在类型化服务门面，基于 HTTP。 | `orchestrator/dashboard/src/services/wingman.ts:360`、`orchestrator/dashboard/src/services/wingman.ts:460`、`orchestrator/dashboard/src/services/wingman.ts:622`、`orchestrator/dashboard/src/services/wingman.ts:743` |
| HTTP 传输 | 浏览器请求向同源 API 附加 Bearer 与范围头。 | `orchestrator/dashboard/src/services/core/http.ts:1`、`orchestrator/dashboard/src/services/core/http.ts:16`、`orchestrator/dashboard/src/services/core/http.ts:44` |
| 远程桌面票据 | Guacamole 票据绑定用户/Agent/token 并生成连接参数。 | `orchestrator/server/internal/remoteticket/ticket.go:19`、`orchestrator/server/internal/remoteticket/ticket.go:26` |
| 会话保险箱 Provider | Vault 实现在启动期选择；local 是默认实现。 | `orchestrator/server/internal/config/config.go:62`、`orchestrator/server/internal/vault/registry.go:16`、`orchestrator/server/internal/vault/local.go:26` |
| 资产存储 | 服务端资产是内容文件加 JSON 元数据 sidecar，位于净化后的根目录。 | `orchestrator/server/internal/assets/store.go:36`、`orchestrator/server/internal/assets/store.go:83`、`orchestrator/server/internal/assets/store.go:128`、`orchestrator/server/internal/assets/store.go:170` |

## 状态、配置与构建

| 术语 | 定义 | 证据 |
|---|---|---|
| Runtime TOML 配置 | `AgentConfig` 解析/保存能力开关与 remote、standalone、debugger、logging、performance、update 配置。 | `apps/agent/include/wingman/runtime/config.hpp:46`、`apps/agent/include/wingman/runtime/config.hpp:81`、`apps/agent/src/agent_config.cpp:83`、`apps/agent/src/agent_config.cpp:229` |
| 脚本级 JSON 配置 | Runtime/config/game-profile 状态存为所选配置目录中的 JSON 文件。 | `lib/wingman/src/config.cpp:11`、`lib/wingman/src/config.cpp:16`、`lib/wingman/src/config.cpp:38` |
| 脚本配置 | 脚本路径/语言/沙箱设置和持久化 config/env 由 `ScriptManager` 拥有。 | `lib/wingman/src/script_manager.cpp:626`、`lib/wingman/src/script_manager.cpp:654`、`lib/wingman/src/script_manager.cpp:669`、`lib/wingman/src/script_manager.cpp:700` |
| 桌面偏好存储 | Svelte settings 在浏览器 localStorage 的一个 JSON key 中持久化。 | `apps/gui/src/lib/stores/settings.ts:11`、`apps/gui/src/lib/stores/settings.ts:23`、`apps/gui/src/lib/stores/settings.ts:36` |
| Dashboard 会话存储 | JWT 与选中的 game/env 范围本地持久化，再附加到请求和 socket。 | `orchestrator/dashboard/src/pages/User/Login/index.tsx:91`、`orchestrator/dashboard/src/pages/User/Login/index.tsx:97`、`orchestrator/dashboard/src/stores/scope.ts:45`、`orchestrator/dashboard/src/services/core/http.ts:4`、`orchestrator/dashboard/src/services/core/http.ts:16` |
| SQLite 持久化 | GORM 模型覆盖用户/RBAC/Agent/执行/工作流/token/保险箱/会话/更新状态并自动迁移。 | `orchestrator/server/internal/models/models.go:145`、`orchestrator/server/main.go:68`、`orchestrator/server/main.go:73` |
| 服务端文件存储 | 脚本、资产、更新制品和录像放入配置目录；资产元数据是 JSON sidecar。 | `orchestrator/server/internal/config/config.go:18`、`orchestrator/server/internal/config/config.go:21`、`orchestrator/server/internal/config/config.go:24`、`orchestrator/server/internal/assets/store.go:128`、`orchestrator/server/internal/assets/store.go:170` |
| 本地键值存储 | LocalStorage 写 JSON 文件；KVStore 可持久化也可内存行为。 | `lib/wingman/src/storage.cpp:79`、`lib/wingman/src/storage.cpp:168`、`lib/wingman/src/storage.cpp:192`、`lib/wingman/include/wingman/kvstore.hpp:120`、`lib/wingman/include/wingman/kvstore.hpp:123` |
| 脚本 SQLite 数据库 | `DbConnection` 把简单名解析到数据目录，允许 `:memory:`，并开启外键打开 SQLite。 | `lib/wingman/src/script/modules/db_module.cpp:63`、`lib/wingman/src/script/modules/db_module.cpp:99`、`lib/wingman/src/script/modules/db_module.cpp:123`、`lib/wingman/src/script/modules/db_module.cpp:152`、`lib/wingman/src/script/modules/db_module.cpp:167` |
| Android 宿主桥 | Android 手势/采集/屏幕尺寸访问隔离在可注入的 JNI 宿主桥后。 | `lib/wingman/src/platform/android/android_host_bridge.hpp:21`、`lib/wingman/src/platform/android/android_host_bridge.hpp:38`、`apps/android/cpp/agent/android_screenshot.cpp:75`、`apps/android/cpp/agent/android_screenshot.cpp:81` |
| 构建目标 | CMake 组合共享库、wingman core 与桌面 runtime；Dashboard 使用 npm scripts，Android Gradle 接入 NDK 构建。 | `CMakeLists.txt:142`、`CMakeLists.txt:179`、`apps/gui/package.json:8`、`orchestrator/dashboard/package.json:6`、`apps/android/app/build.gradle.kts:71` |
