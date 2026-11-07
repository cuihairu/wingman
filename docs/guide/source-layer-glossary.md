> [中文](../zh/guide/source-layer-glossary.md) | **English**

# Source Layer Glossary

The table below records the implementation-facing vocabulary used by the 2026-11-07 source analysis. File references point to the primary definition or wiring point, not every caller.

## Application Entry And Lifecycle

| Term | Definition | Evidence |
|---|---|---|
| Desktop runtime process | `wingman-agent` starts embedded script mode first, then falls back to GUI mode; explicit arguments go through command dispatch. | `apps/agent/src/main.cpp:24`, `apps/agent/src/main.cpp:76`, `apps/agent/src/main.cpp:102` |
| Runtime command dispatch | The CLI turns argv into a vector and delegates command handling to runtime commands. | `apps/agent/src/main.cpp:110`, `apps/agent/src/cli.cpp:46`, `apps/agent/src/commands/start_command.cpp:72` |
| Agent lifecycle | Runtime initialization loads a TOML file or accepts an explicit object; shutdown stops remote and local links in sequence. | `apps/agent/src/agent.cpp:172`, `apps/agent/src/agent.cpp:191`, `apps/agent/src/agent.cpp:237` |
| Local IPC server | Local runtime capability is published through a dispatcher-backed IPC server, including runtime status and remote-config access. | `apps/agent/src/agent.cpp:292`, `apps/agent/src/agent.cpp:297`, `apps/agent/src/agent.cpp:314` |
| Server run lifecycle | Orchestrator configuration is loaded, directories are created, SQLite is opened and migrated, and registry/HTTP/TCP/workflow/vault components are assembled. | `orchestrator/server/main.go:50`, `orchestrator/server/main.go:64`, `orchestrator/server/main.go:68`, `orchestrator/server/main.go:85`, `orchestrator/server/main.go:97`, `orchestrator/server/main.go:127`, `orchestrator/server/main.go:144` |
| Dashboard application lifecycle | Umi initializes authentication state, runtime layout, request error handling and WebSocket wiring before routes render. | `orchestrator/dashboard/src/app.tsx:36`, `orchestrator/dashboard/src/app.tsx:40`, `orchestrator/dashboard/src/app.tsx:47`, `orchestrator/dashboard/src/app.tsx:193` |
| Desktop GUI shell | Tauri manages the Svelte frontend, global shortcuts, tray and an explicitly registered command surface. | `apps/gui/src-tauri/src/main.rs:10`, `apps/gui/src-tauri/src/main.rs:13`, `apps/gui/src-tauri/src/main.rs:23`, `apps/gui/src-tauri/src/main.rs:40` |
| Android host process | Kotlin initialization is a thin Android application shell for the NDK-backed agent. | `apps/android/app/src/main/java/com/wingman/agent/WingmanApplication.kt:9` |

## Remote Agent Control

| Term | Definition | Evidence |
|---|---|---|
| Remote client | Agentcore owns TCP transport, registration metadata, authentication material, reconnect/heartbeat state and outbox accounting. | `libs/agentcore/src/remote_client.cpp:30`, `libs/agentcore/src/remote_client.cpp:47`, `libs/agentcore/src/remote_client.cpp:69`, `libs/agentcore/src/remote_client.cpp:79` |
| Remote registration | The client sends `agent.register`, consumes the ACK and records successful registration. | `libs/agentcore/src/remote_client.cpp:372`, `libs/agentcore/src/remote_client.cpp:463`, `libs/agentcore/src/remote_client.cpp:486` |
| Remote heartbeat | Heartbeats report link counters and are retained by the server registry. | `libs/agentcore/src/remote_client.cpp:356`, `libs/agentcore/src/remote_client.cpp:374`, `orchestrator/server/internal/agent/registry.go:47`, `orchestrator/server/internal/agent/registry.go:171` |
| Reconnect policy | The client uses exponential backoff with jitter and an interruptible sleep. | `libs/agentcore/src/remote_client.cpp:271`, `libs/agentcore/src/remote_client.cpp:299`, `libs/agentcore/src/remote_client.cpp:313` |
| Frame listener | The Go server accepts runtime-only outbound TCP connections, assigns connection IDs and starts per-connection read loops. | `orchestrator/server/internal/agent/listener.go:232`, `orchestrator/server/internal/agent/listener.go:264`, `orchestrator/server/internal/agent/listener.go:303` |
| Outbound command | A server command is a 16-byte frame plus JSON request; sequence values are paired with pending response channels. | `orchestrator/server/internal/agent/listener.go:308`, `orchestrator/server/internal/agent/listener.go:325`, `orchestrator/server/internal/agent/listener.go:339` |
| Remote command bridge | Remote `agent.*` commands are converted into shared RPC requests and handled by the desktop runtime dispatcher. | `apps/agent/src/agent.cpp:77`, `apps/agent/src/agent.cpp:87`, `apps/agent/src/agent.cpp:97` |
| Android agent runtime | The NDK agent owns the remote client, one active script, asset dispatch and shared status JSON. | `apps/android/cpp/agent/android_agent.cpp:110`, `apps/android/cpp/agent/android_agent.cpp:132`, `apps/android/cpp/agent/android_agent.cpp:166` |

## Protocol And Transport

| Term | Definition | Evidence |
|---|---|---|
| Agent frame | Runtime-to-server messages use a fixed 16-byte header and JSON body. | `orchestrator/server/internal/agent/client.go:20`, `orchestrator/server/internal/agent/client.go:39`, `libs/agentcore/include/wingman/agentcore/remote_client.hpp:137` |
| Generic transport frame | A generic 4-byte length/body frame supports TCP sockets and sessions. | `libs/transport/include/wingman/transport/simple_protocol.hpp:18`, `libs/transport/include/wingman/transport/simple_protocol.hpp:34`, `libs/transport/include/wingman/transport/simple_protocol.hpp:70` |
| Local IPC envelope | GUI-runtime messages are a length-prefixed JSON envelope containing type, method, payload, id and timestamp. | `lib/wingman/include/wingman/ipc/iipc_channel.hpp:26`, `lib/wingman/include/wingman/ipc/iipc_channel.hpp:36`, `apps/gui/src-tauri/src/ipc/client.rs:16` |
| Local IPC transport | Desktop transport chooses named pipes on Windows and Unix sockets elsewhere, with TCP as a fallback. | `lib/wingman/src/platform/ipc_factory.cpp:31`, `lib/wingman/src/platform/ipc_factory.cpp:71`, `apps/gui/src-tauri/src/ipc/client.rs:228`, `apps/gui/src-tauri/src/ipc/client.rs:245` |
| RPC dispatcher | Local IPC and remote bridge commands share a JSON call/response dispatcher. | `lib/wingman/src/rpc/rpc_dispatcher.cpp:34`, `apps/agent/src/local_ipc_server.cpp:139`, `apps/agent/src/local_ipc_server.cpp:172`, `apps/agent/src/agent.cpp:97` |
| Dashboard WebSocket | Umi connects to `/ws` with a query token and receives typed `{type,event,data}` pushes. | `orchestrator/dashboard/src/services/websocket.ts:27`, `orchestrator/dashboard/src/services/websocket.ts:41`, `orchestrator/dashboard/src/services/websocket.ts:95` |
| WebSocket hub | The Go hub authenticates query-token clients, routes subscriptions and broadcasts typed JSON notifications. | `orchestrator/server/pkg/websocket/hub.go:85`, `orchestrator/server/pkg/websocket/hub.go:119`, `orchestrator/server/pkg/websocket/hub.go:277`, `orchestrator/server/pkg/websocket/hub.go:343` |
| Proxy tunnel | Agent-side data paths are outbound raw TCP tunnels and do not buffer replayable proxy data. | `libs/agentcore/src/remote_client.cpp:74`, `libs/agentcore/src/proxy_tunnel.cpp:355`, `apps/android/cpp/agent/android_agent.cpp:32` |

## Scripting And Runtime Capabilities

| Term | Definition | Evidence |
|---|---|---|
| Script engine contract | Language engines implement lifecycle, module registration, file/string execution and function calls. | `lib/wingman/include/wingman/script/iscript_engine.hpp:22`, `lib/wingman/include/wingman/script/iscript_engine.hpp:57`, `lib/wingman/include/wingman/script/iscript_engine.hpp:80` |
| Engine registry | Factory/registry maps language names to registered engines. | `lib/wingman/include/wingman/script/script_engine_factory.hpp:20`, `lib/wingman/include/wingman/script/script_engine_factory.hpp:36` |
| Lua engine | The Lua adapter registers the `wingman` table and module functions when an engine initializes. | `libs/lua/src/lua_script_engine.cpp:34`, `libs/lua/src/lua_script_engine.cpp:53`, `libs/lua/src/lua_script_engine.cpp:110` |
| Script module set | `registerAllModules` installs the complete 41-descriptor scripting facade. | `lib/wingman/src/script/modules/module_registry.cpp:54`, `lib/wingman/src/script/modules/module_registry.cpp:101` |
| Script manager | A manager owns script configurations, lifecycle state, engine instances, output callbacks and auto reload. | `lib/wingman/include/wingman/script_manager.hpp:34`, `lib/wingman/include/wingman/script_manager.hpp:47`, `lib/wingman/src/script_manager.cpp:242`, `lib/wingman/src/script_manager.cpp:833` |
| Platform capability | Screens, input, windows, clipboard and file watchers use abstract interfaces with platform factories/backends. | `lib/wingman/include/wingman/platform/iscreen.hpp:19`, `lib/wingman/include/wingman/platform/iinput.hpp:17`, `lib/wingman/include/wingman/platform/iwindow.hpp:19`, `lib/wingman/src/platform/input_factory.cpp:28`, `lib/wingman/src/platform/screen_backend.cpp:47`, `lib/wingman/src/platform/window_backend.cpp:281` |
| Event buffer | Runtime events are buffered for GUI pull; dropped events are reported. | `libs/agentcore/include/wingman/agentcore/event_buffer.hpp:10`, `libs/agentcore/src/event_buffer.cpp:14`, `apps/gui/src/lib/stores/events.ts:9`, `apps/gui/src/lib/stores/events.ts:153` |
| Runtime event bus | An in-memory thread-safe publish/subscribe facility for runtime components. | `lib/wingman/include/wingman/core/event_bus.hpp:17`, `lib/wingman/src/core/event_bus.cpp:13`, `lib/wingman/src/core/event_bus.cpp:56` |
| Vision analysis | Image, color and template operations are separated from platform capture through `ImageAnalyzer`. | `lib/wingman/include/wingman/vision/image_analyzer.hpp:22`, `lib/wingman/include/wingman/vision/image_analyzer.hpp:44` |
| Embedded resource pack | The executable can carry compressed/encrypted script bytes with header metadata, checksum and password handling. | `apps/agent/src/resource_loader.cpp:42`, `apps/agent/src/resource_loader.cpp:127`, `apps/agent/src/resource_loader.cpp:149`, `apps/agent/src/resource_loader.cpp:167` |

## Orchestration And Workflow

| Term | Definition | Evidence |
|---|---|---|
| Agent registry | Registry holds live connections, status/resources/tags/capabilities, heartbeats and change notifications. | `orchestrator/server/internal/agent/registry.go:26`, `orchestrator/server/internal/agent/registry.go:31`, `orchestrator/server/internal/agent/registry.go:61`, `orchestrator/server/internal/agent/registry.go:96` |
| Workflow engine | The workflow engine models definitions, executions, dependency steps, schedules, cancellation and execution records. | `orchestrator/server/internal/workflow/engine.go:39`, `orchestrator/server/internal/workflow/engine.go:54`, `orchestrator/server/internal/workflow/engine.go:395`, `orchestrator/server/internal/workflow/engine.go:1302` |
| Workflow templates | Built-in templates instantiate common workflows from declarative payload builders. | `orchestrator/server/internal/workflow/templates.go:16`, `orchestrator/server/internal/workflow/templates.go:27` |
| HTTP routes | RegisterRoutes consolidates middleware, authentication, RBAC and resource-specific API groups. | `orchestrator/server/internal/handlers/routes.go:38`, `orchestrator/server/internal/handlers/routes.go:49`, `orchestrator/server/internal/handlers/routes.go:213`, `orchestrator/server/internal/handlers/routes.go:271` |
| API client facade | Dashboard workflow/agent/script operations are centralized in a typed service facade over HTTP. | `orchestrator/dashboard/src/services/wingman.ts:360`, `orchestrator/dashboard/src/services/wingman.ts:460`, `orchestrator/dashboard/src/services/wingman.ts:622`, `orchestrator/dashboard/src/services/wingman.ts:743` |
| HTTP transport | Browser requests attach bearer and scope headers to the same-origin API path. | `orchestrator/dashboard/src/services/core/http.ts:1`, `orchestrator/dashboard/src/services/core/http.ts:16`, `orchestrator/dashboard/src/services/core/http.ts:44` |
| Remote desktop ticket | Guacamole tickets bind users/agents/tokens and issue Guacamole connect parameters. | `orchestrator/server/internal/remoteticket/ticket.go:19`, `orchestrator/server/internal/remoteticket/ticket.go:26` |
| Session vault provider | Vault implementations are selected at startup; local provider is the default implementation. | `orchestrator/server/internal/config/config.go:62`, `orchestrator/server/internal/vault/registry.go:16`, `orchestrator/server/internal/vault/local.go:26` |
| Asset store | Server assets are content files plus JSON metadata sidecars under a sanitized root. | `orchestrator/server/internal/assets/store.go:36`, `orchestrator/server/internal/assets/store.go:83`, `orchestrator/server/internal/assets/store.go:128`, `orchestrator/server/internal/assets/store.go:170` |

## State, Configuration And Build

| Term | Definition | Evidence |
|---|---|---|
| Runtime TOML configuration | `AgentConfig` parses/saves capability switches, remote, standalone, debugger, logging, performance and update settings. | `apps/agent/include/wingman/runtime/config.hpp:46`, `apps/agent/include/wingman/runtime/config.hpp:81`, `apps/agent/src/agent_config.cpp:83`, `apps/agent/src/agent_config.cpp:229` |
| Script-level JSON configuration | Runtime/config/game-profile state is stored as a JSON file in the selected config directory. | `lib/wingman/src/config.cpp:11`, `lib/wingman/src/config.cpp:16`, `lib/wingman/src/config.cpp:38` |
| Script configuration | Script paths/languages/sandbox settings and persistent config/env are owned by `ScriptManager`. | `lib/wingman/src/script_manager.cpp:626`, `lib/wingman/src/script_manager.cpp:654`, `lib/wingman/src/script_manager.cpp:669`, `lib/wingman/src/script_manager.cpp:700` |
| Desktop preference store | Svelte settings are persisted in browser localStorage under one JSON key. | `apps/gui/src/lib/stores/settings.ts:11`, `apps/gui/src/lib/stores/settings.ts:23`, `apps/gui/src/lib/stores/settings.ts:36` |
| Dashboard session storage | JWT and selected game/environment scope are persisted locally, then attached to requests/sockets. | `orchestrator/dashboard/src/pages/User/Login/index.tsx:91`, `orchestrator/dashboard/src/pages/User/Login/index.tsx:97`, `orchestrator/dashboard/src/stores/scope.ts:45`, `orchestrator/dashboard/src/services/core/http.ts:4`, `orchestrator/dashboard/src/services/core/http.ts:16` |
| SQLite persistence | GORM models cover users/RBAC/agents/executions/workflows/tokens/vault/session/update state and are auto-migrated. | `orchestrator/server/internal/models/models.go:145`, `orchestrator/server/main.go:68`, `orchestrator/server/main.go:73` |
| Server file stores | Scripts, assets, update artifacts and recordings are placed in configured directories; asset metadata is a JSON sidecar. | `orchestrator/server/internal/config/config.go:18`, `orchestrator/server/internal/config/config.go:21`, `orchestrator/server/internal/config/config.go:24`, `orchestrator/server/internal/assets/store.go:128`, `orchestrator/server/internal/assets/store.go:170` |
| Local key-value storage | LocalStorage writes a JSON file; KVStore has optional persistence and in-memory behavior. | `lib/wingman/src/storage.cpp:79`, `lib/wingman/src/storage.cpp:168`, `lib/wingman/src/storage.cpp:192`, `lib/wingman/include/wingman/kvstore.hpp:120`, `lib/wingman/include/wingman/kvstore.hpp:123` |
| Script SQLite database | `DbConnection` resolves simple names to the data directory, permits `:memory:` and opens SQLite with foreign keys. | `lib/wingman/src/script/modules/db_module.cpp:63`, `lib/wingman/src/script/modules/db_module.cpp:99`, `lib/wingman/src/script/modules/db_module.cpp:123`, `lib/wingman/src/script/modules/db_module.cpp:152`, `lib/wingman/src/script/modules/db_module.cpp:167` |
| Android host bridge | Android gestures/capture/screen-size access is isolated behind an injectable JNI host bridge. | `lib/wingman/src/platform/android/android_host_bridge.hpp:21`, `lib/wingman/src/platform/android/android_host_bridge.hpp:38`, `apps/android/cpp/agent/android_screenshot.cpp:75`, `apps/android/cpp/agent/android_screenshot.cpp:81` |
| Build targets | CMake composes shared libraries, the wingman core and desktop runtime; the dashboard uses npm scripts and Android Gradle wires its NDK build. | `CMakeLists.txt:142`, `CMakeLists.txt:179`, `apps/gui/package.json:8`, `orchestrator/dashboard/package.json:6`, `apps/android/app/build.gradle.kts:71` |
