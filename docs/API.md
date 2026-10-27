> [中文](zh/API.md) | **English**

# Wingman Runtime Local IPC API

> This document describes the current runtime local IPC control plane. The Runtime must not provide a WebSocket/HTTP server as the local UI or remote control plane.
>
> Current constraints see `docs/architecture-decisions.md`:
>
> - Remote orchestration: `runtime agent -> outbound transport -> Go orchestrator -> dashboard`
> - Local standalone UI: `Tauri UI -> Tauri Rust backend -> local IPC -> runtime`
>
> The Go orchestrator's remote API should be documented separately; do not mix the dashboard/browser WebSocket protocol into runtime local IPC.

The Wingman Runtime's local control plane should be provided via local IPC, for the Tauri UI to call.

## Starting the Runtime

```bash
# 默认配置
wingman-agent start

# 指定配置文件
wingman-agent start --config agent.toml

# 本地 GUI / 单机模式，启动本地 IPC listener
wingman-agent start --standalone
```

> The remote Agent uses `server_ip` / `server_port` from `agent.toml` to connect to the Go orchestrator. The local GUI uses `--standalone` to start the runtime local IPC.

## Local IPC Command Interface

The IPC transport uses length-prefixed frames: `uint32 little-endian length + JSON envelope`. The concrete transport is selected automatically per platform: Windows defaults to Named Pipe, macOS/Linux default to Unix Domain Socket.

The current IPC envelope is shared by the GUI Rust backend and the C++ runtime. Do not turn this into WebSocket JSON-RPC, and do not add an HTTP/WebSocket server for the local UI.

### Message Format

**Wire request envelope:**
```json
{
  "type": 0,
  "method": "script.list",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`type` currently uses a numeric enum:

- `0`: request
- `1`: response
- `2`: event
- `3`: error

**Wire response envelope:**
```json
{
  "type": 1,
  "method": "script.list",
  "payload": {
    "type": "response",
    "id": "1",
    "data": {
      "success": true,
      "result": {}
    }
  },
  "id": 1,
  "timestamp": 1715299200100
}
```

What the GUI Rust backend returns to a Tauri command is the `payload` inside the envelope.

**Dispatcher error payload:**
```json
{
  "type": "response",
  "id": "1",
  "data": {
    "success": false,
    "error": "错误信息"
  }
}
```

### Supported RPC Methods

Currently **31** unique methods are registered in total (26 registered under `apps/agent/src/rpc/handlers/`, 5 `trigger.*` registered in `lib/wingman/src/rpc/handlers/trigger_handler.cpp`), on the same accounting basis as `docs/protocols.md`:

| Group | Methods |
|------|------|
| system | `getStatus` `getVersion` `isPaused` `togglePause` `pauseAll` `resumeAll` `stopAll` |
| script | `list` `start` `stop` `pause` `resume` `restart` `unload` |
| trigger | `list` `add` `remove` `update` `toggle` |
| screenshot | `capture` |
| screen | `listMonitors` |
| events | `drain` |
| macro | `start` `stop` `play` `status` `save` `load` `clear` |
| config | `getRemote` `setRemote` |

> `trigger.*` is **registered on both sides**: local IPC and the remote agent channel, with both ends sharing the same set of handlers: locally, `LocalIpcServer::start()` installs them into this method group's dispatcher (reuses an external TriggerManager if one exists, otherwise builds its own); the GUI's Tauri commands `get_triggers`/`add_trigger`/`remove_trigger`/`update_trigger`/`toggle_trigger` (`apps/gui/src-tauri/src/commands/triggers.rs`) go through this local IPC; on the remote side it is Dispatcher Reuse (`apps/agent/src/agent.cpp`), accessed by the Dashboard via pass-through from the Go server. See `docs/protocols.md`.
>
> Remote orchestration commands (e.g., `system.shutdown`) likewise go through the agent transport channel (`apps/agent/src/agent.cpp`) and are not part of the local IPC surface.
>
> `asset.sync` is not on the local IPC surface—it is registered only on the remote dispatcher and on the Android device side.

#### system.getStatus
Get system status

```json
{ "type": 0, "method": "system.getStatus", "payload": {}, "id": 1, "timestamp": 1715299200000 }
```

#### system.getVersion
Get version information

```json
{ "type": 0, "method": "system.getVersion", "payload": {}, "id": 2, "timestamp": 1715299200000 }
```

#### script.list
List all available scripts

```json
{ "type": 0, "method": "script.list", "payload": {}, "id": 3, "timestamp": 1715299200000 }
```

#### script.start
Start the specified script

```json
{
  "type": 0,
  "method": "script.start",
  "payload": {
    "path": "scripts/example.lua"
  },
  "id": 4,
  "timestamp": 1715299200000
}
```

#### script.stop
Stop a running script

```json
{
  "type": 0,
  "method": "script.stop",
  "payload": {
    "scriptId": "script-id"
  },
  "id": 5,
  "timestamp": 1715299200000
}
```

#### script.pause / script.resume
Pause/resume a running script (`scriptId` required)

```json
{
  "type": 0,
  "method": "script.pause",
  "payload": { "scriptId": "script-id" },
  "id": 6,
  "timestamp": 1715299200000
}
```

#### script.restart
Restart a loaded script: for a running/paused script, stop → start (path reused, ID unchanged)

```json
{
  "type": 0,
  "method": "script.restart",
  "payload": { "scriptId": "script-id" },
  "id": 7,
  "timestamp": 1715299200000
}
```

#### script.unload
Remove from the script table (only valid for scripts that are not running; stop a running script first)

```json
{
  "type": 0,
  "method": "script.unload",
  "payload": { "scriptId": "script-id" },
  "id": 8,
  "timestamp": 1715299200000
}
```

#### system.isPaused
Query whether any paused script exists; returns `{ "paused": bool }`

```json
{ "type": 0, "method": "system.isPaused", "payload": {}, "id": 9, "timestamp": 1715299200000 }
```

#### system.togglePause
Toggle global pause: if any are paused, resume all; otherwise pause all. Returns `{ "paused": bool, "changedScripts": int }`

```json
{ "type": 0, "method": "system.togglePause", "payload": {}, "id": 10, "timestamp": 1715299200000 }
```

#### system.pauseAll / system.resumeAll
Pause all / resume all. Returns `{ "paused": bool, "changedScripts": int }`

```json
{ "type": 0, "method": "system.pauseAll", "payload": {}, "id": 11, "timestamp": 1715299200000 }
```

#### system.stopAll
Stop all. Returns `{ "stoppedScripts": int }`

```json
{ "type": 0, "method": "system.stopAll", "payload": {}, "id": 12, "timestamp": 1715299200000 }
```

#### screenshot.capture
Capture the screen as JPEG (base64 data URL). Requires `WINGMAN_ENABLE_VISION` (OpenCV), otherwise returns an error. Optional parameters:
- `region` - `{x, y, width, height}`; omitted or empty → the whole screen (or the specified monitor)
- `displayId` - monitor index (-1/omitted → primary screen); `region` is relative to that monitor's origin

Returns `{ "image": "data:image/jpeg;base64,...", "width", "height", "timestamp", "region" }`

```json
{
  "type": 0,
  "method": "screenshot.capture",
  "payload": { "displayId": 0, "region": { "x": 0, "y": 0, "width": 800, "height": 600 } },
  "id": 13,
  "timestamp": 1715299200000
}
```

#### screen.listMonitors
List monitors; returns `{ "monitors": [{ "id", "name", "isPrimary", "bounds" }], "primaryId": int }`

```json
{ "type": 0, "method": "screen.listMonitors", "payload": {}, "id": 14, "timestamp": 1715299200000 }
```

#### events.drain
Drain the local event stream (GUI polling). The event method set: `tray.show`/`tray.hide`/`tray.badge`/`tray.tooltip` (script tray intents), `trigger.fired`/`trigger.action`, `systemwatch.process`/`systemwatch.window`/`systemwatch.error`, `filewatcher.changed`/`filewatcher.error`, `macro.state`/`macro.recorded`, `script.state_changed`, `script.output`, `connection.ipc_client`. Parameter `max` (default 500, single-drain cap). Returns:

```json
{
  "type": 0,
  "method": "events.drain",
  "payload": { "max": 500 },
  "id": 15,
  "timestamp": 1715299200000
}
```

Response `result`: `{ "events": [...], "remaining": int, "dropped": int }` (`remaining` = remaining in the buffer, `dropped` = cumulative dropped count).

#### trigger.list
List all triggers. No parameters. Returns `{ "triggers": [ ... ] }`, each item's fields:

| Field | Description |
|------|------|
| `id` | Trigger ID, a **string** (`remove`/`update`/`toggle` pass it back by this) |
| `name` / `enabled` / `oneShot` / `cooldown` | Top-level switch and cooldown (`cooldown` unit is ms) |
| `type` | Condition type (see table below, taken from `condition.type`) |
| `condition` | `{ type, value, region: {x, y, width, height}, tolerance, interval, enabled }` |
| `actions` | Array, each item `{ type, value, x, y, delay }` |
| `lastTriggered` | Whether it has ever fired (bool) |

```json
{ "type": 0, "method": "trigger.list", "payload": {}, "id": 16, "timestamp": 1715299200000 }
```

Condition types: `ColorFound` `ColorLost` `ImageFound` `ImageLost` `WindowOpened` `WindowClosed` `ProcessStarted` `ProcessStopped` `TimeElapsed` `HotkeyPressed` `PixelChanged`; action types: `RunScript` `Click` `KeyPress` `Type` `StopScript` `PauseScript` `ShowMessage` `PlayAudio` `Log` `Delay`.

#### trigger.add
Add a new trigger. Parameter `config` (an object; giving the config body directly at the top level also works—in that case the whole payload is treated as the config source). `config` fields:

| Field | Description |
|------|------|
| `name` | Name; defaults to `Unnamed Trigger` when omitted |
| `enabled` / `oneShot` | Defaults `true` / `false` |
| `cooldown` | Cooldown in ms, default 0 |
| `condition.type` | Default `ColorFound` |
| `condition.value` | Condition value (interpreted by type, e.g., color/window title/image path) |
| `condition.region` | `{x, y, width, height}`; all zeros means no region restriction |
| `condition.tolerance` | Default 10 |
| `condition.interval` | Polling interval in ms, default 1000 |
| `condition.enabled` | Default `true` |
| `actions` | Array, **replace-whole-table** semantics (when present, clears first then rebuilds) |

Returns `{ "id": string }`.

```json
{
  "type": 0,
  "method": "trigger.add",
  "payload": {
    "config": {
      "name": "血量监控",
      "cooldown": 2000,
      "condition": { "type": "ColorFound", "value": "0xFF0000", "tolerance": 15, "interval": 2000, "region": { "x": 50, "y": 50, "width": 20, "height": 20 } },
      "actions": [{ "type": "KeyPress", "value": "2" }]
    }
  },
  "id": 17,
  "timestamp": 1715299200000
}
```

The `type` field is case- and underscore-insensitive (`ColorFound` / `colorfound` / `color_found` are equivalent; aliases `pixel`→`ColorFound`, `image`→`ImageFound`; action-side aliases `macro`→`RunScript`, `key`→`KeyPress`), and the numeric enums of `TriggerType` / `BasicTriggerAction` are also accepted. Unrecognizable types fall back to `ColorFound` / `Log`.

#### trigger.remove / trigger.update / trigger.toggle
All three locate the trigger by `id` (`id` may be passed as a string or a number). `update` additionally carries a `config` patch (field semantics same as `add`; `actions` is replace-whole-table); when `id` does not exist, returns `{ "success": false, "error": "Trigger not found" }`. `toggle` flips back and forth on `enabled` and returns the post-toggle `{ "enabled": bool }`.

```json
{ "type": 0, "method": "trigger.toggle", "payload": { "id": "3" }, "id": 18, "timestamp": 1715299200000 }
```

#### macro.start / macro.stop / macro.status / macro.clear
Macro recording control. `status` returns `{ "recording": bool, "paused": bool, "eventCount": int }`; the others take no parameters.

```json
{ "type": 0, "method": "macro.status", "payload": {}, "id": 19, "timestamp": 1715299200000 }
```

#### macro.play
Play back a macro. Parameters: `speed` (percent, default 100, internally clamped ≥1), `repeat` (count, default 1, clamped ≥1). **Playback executes synchronously on the calling thread; the GUI side should call it asynchronously to avoid blocking**.

```json
{
  "type": 0,
  "method": "macro.play",
  "payload": { "speed": 150, "repeat": 2 },
  "id": 20,
  "timestamp": 1715299200000
}
```

#### macro.save / macro.load
Save/load a macro by JSON path (`path` required). Returns `{ "success": true, "eventCount": int }`

```json
{
  "type": 0,
  "method": "macro.save",
  "payload": { "path": "macros/demo.json" },
  "id": 21,
  "timestamp": 1715299200000
}
```

#### config.getRemote / config.setRemote
Read/apply the remote registration configuration (`serverIp` / `serverPort` / `registerToken`, local IPC only, no corresponding remote command). The `setRemote` request body is a config patch (partial update); on success it returns the full configuration after applying; taking effect while running triggers an agent reconnect.

```json
{
  "type": 0,
  "method": "config.setRemote",
  "payload": { "serverIp": "192.168.1.10", "serverPort": 9000, "registerToken": "..." },
  "id": 22,
  "timestamp": 1715299200000
}
```

### Heartbeat/Ping

The current runtime local IPC does not implement a standalone ping/pong. The GUI should use `system.getStatus` as the connection health check.

## Usage Examples

### JavaScript (Tauri)
```javascript
import { invoke } from '@tauri-apps/api/core';

await invoke('connect_ipc', { endpoint: 'wingman' });
const status = await invoke('get_system_status');
```

The current GUI already uses dedicated Tauri commands, e.g., `connect_ipc`, `get_system_status`, `get_scripts`. New UI features should reuse these commands first, or add a new Tauri command that calls the runtime through the Rust IPC client.
