# Wingman Architecture Decisions

This document records hard architecture constraints for Wingman. It is written for maintainers and AI coding agents. Do not introduce code that violates these decisions without an explicit architecture change approved by the project owner.

## Non-Negotiable Control Plane

Wingman has two separate control paths:

1. Remote orchestration: `runtime agent -> outbound transport -> Go orchestrator -> dashboard`.
2. Local standalone UI: `Tauri UI -> Tauri Rust backend -> local IPC -> runtime`.

The runtime must not become a general-purpose HTTP or WebSocket server for control traffic.

## Remote Orchestration

The Go server is the central orchestrator.

- Runtime connects outbound to the Go orchestrator as an agent.
- The Go server owns authentication, authorization, dashboard APIs, node registry, command dispatch, status aggregation, logs, and remote events.
- Dashboard and remote clients connect only to the Go server, not directly to runtime.
- The Go server should not depend on dialing a runtime listener such as `localhost:8888`.
- Agent communication must use a clearly versioned transport protocol with bounded frame sizes and cross-language tests.

The intended command flow is:

```text
runtime -> server: agent.register
server -> runtime: register.ack

runtime -> server: agent.heartbeat
server -> runtime: heartbeat.ack

server -> runtime: command.run_script
runtime -> server: command.result

runtime -> server: event.script_log
runtime -> server: event.status_changed
```

Every remote command must have at least:

- `agent_id`
- `command_id`
- `type`
- `payload`
- `timeout`

Every command result must include:

- `command_id`
- `success`
- `error`
- `data`

## Local Standalone UI

Local UI control must use local IPC, not runtime WebSocket/HTTP.

Preferred IPC selection:

- Windows: default to Named Pipe.
- macOS/Linux: default to Unix Domain Socket.
- Windows Unix Domain Socket may be supported through runtime capability detection, but it is not the default primary path.
- Local TCP is for explicit debug fallback only and must be disabled by default.

Recommended configuration shape:

```cpp
enum class IpcTransport {
    Auto,
    NamedPipe,
    UnixSocket,
    LocalTcp
};

struct IpcConfig {
    IpcTransport transport = IpcTransport::Auto;
    std::string endpoint;
    bool allowTcpFallback = false;
};
```

Auto mode should resolve as:

```text
Windows:
  NamedPipe first
  UnixSocket only if explicitly preferred or Named Pipe is unavailable and AF_UNIX probes successfully
  LocalTcp only if allowTcpFallback is true

macOS/Linux:
  UnixSocket first
  LocalTcp only if allowTcpFallback is true
```

## Forbidden Changes

Do not add these without explicit owner approval:

- A runtime HTTP server for local UI control.
- A runtime WebSocket server for local UI control.
- Browser-to-runtime direct control APIs.
- Dashboard code that connects directly to `ws://127.0.0.1:<runtime-port>`.
- Go orchestrator code that assumes runtime is a passive listener.
- New TCP listeners in runtime for production control paths.
- New third-party network server frameworks in runtime for local UI control.

If a local control feature is needed, implement it through the IPC abstraction.

## Allowed WebSocket Usage

WebSocket is allowed at the Go server boundary for dashboard/browser communication.

```text
dashboard <-> Go server WebSocket
```

WebSocket is not allowed as the runtime local-control mechanism.

## Dispatcher Reuse

Command handling may be shared across local IPC and remote agent commands. Prefer a transport-agnostic command dispatcher:

```text
IPC frame -> CommandDispatcher -> runtime services
agent command -> CommandDispatcher -> runtime services
```

The dispatcher must not depend on WebSocket, HTTP, or browser concepts.

## Documentation Requirement

When changing runtime control, local UI, or remote orchestration code, update this document and `docs/architecture.md` in the same change. If implementation is experimental, mark it explicitly as experimental instead of presenting it as the stable architecture.
