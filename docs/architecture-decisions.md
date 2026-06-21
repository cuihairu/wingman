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

### Local IPC Wire Contract

The local IPC wire format is:

```text
uint32 little-endian payload_length
JSON envelope bytes
```

The JSON envelope is transport-level and must stay independent from Tauri, browser, HTTP, and WebSocket concepts:

```json
{
  "type": 0,
  "method": "system.getStatus",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`type` uses the C++ `IpcMessageType` numeric enum:

- `0`: request
- `1`: response
- `2`: event
- `3`: error

The dispatcher response is carried inside the response envelope `payload`. GUI code should not parse or construct WebSocket JSON-RPC messages for local runtime control.

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

## Display Selection

Multi-monitor capture is an extension of the existing screenshot path, not a new
control channel.

- Display enumeration flows through a dedicated RPC method `screen.listMonitors`
  on the same dispatcher that already serves `screenshot.capture`. No new
  listener, port, or transport is introduced.
- Display selection flows through the existing `screenshot.capture` payload as
  an optional `displayId` field. Omitting it preserves the current
  primary-monitor behavior, so existing callers do not break.
- The platform abstraction `IScreen` (see `docs/platform-abstraction-design.md`)
  is the single source of truth for monitor enumeration and per-monitor bounds.
  The legacy static `wingman::Screen` class is frozen for the screenshot path
  and will be removed separately; new screenshot code must route through
  `IScreen` rather than extending `Screen` with a parallel multi-monitor API.

This keeps display selection inside the IPC/agent transport contract and avoids
a second screen abstraction.

## Runtime-to-UI Event Delivery (Pull, not Push)

The runtime surfaces events (log lines, trigger fires, screenshot frames,
script state changes) to the local Tauri UI over the existing local IPC, but
uses a **pull/drain model**, not unsolicited push frames:

```text
runtime subsystem -> EventBuffer (in-process, bounded queue)
GUI poller -> RPC `events.drain` -> drains + clears the buffer -> dispatch to stores
```

Rationale: the Rust IPC client (`apps/gui/src-tauri/src/ipc/client.rs`) is a
synchronous request/response client over Windows named pipes (blocking IO). If
the runtime pushed unsolicited `type=2` event frames, they would interleave
with response frames and desynchronize the single reader. A pull model keeps
the client single-request/single-response and avoids an async reader loop.

This is an allowed variation of the wire contract above: events still travel
over the local IPC only (never HTTP/WebSocket). The `type=2` event frame
remains defined for transports that can multiplex; the local-UI path simply
chooses drain-over-RPC. Do not introduce a background reader/`type=2` push
path in the Rust client without first resolving the blocking-IO multiplexing.

Hook points: `EventBuffer::instance().push(method, payload)` is fed by a
spdlog sink (`log.line`), a `TriggerManager::setOnFired` callback
(`trigger.fired`), and script lifecycle transitions in `StandaloneMode`
(`script.state_changed`).

**Screenshots are NOT pushed via the event drain.** Full screenshots are
large base64 payloads; periodic capture would flood the bounded buffer and
the pull model. Screenshots stay on-demand via the existing `screenshot.capture`
RPC (the GUI requests a frame when it needs one). `ScreenshotReporter` is
Windows-only and HTTP-oriented (`serverUrl`); it is not wired into the local
event path. A future "screenshot available" lightweight signal, if needed,
should carry only a notification and let the GUI pull the heavy payload.

## Documentation Requirement

When changing runtime control, local UI, or remote orchestration code, update this document and `docs/architecture.md` in the same change. If implementation is experimental, mark it explicitly as experimental instead of presenting it as the stable architecture.
