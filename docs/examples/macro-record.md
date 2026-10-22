> [中文](macro-record.md) | **English**

# Macro Record

Demonstrates how to record and replay mouse and keyboard operations.

> macOS recording relies on CGEventTap; you must grant the running process Accessibility permission in "System Settings → Privacy & Security → Accessibility".

## Code

```lua
-- examples/lua_scripts/macro_record.lua

local wingman = require("wingman")

print("Macro Recording Example")

print("录制将在 3 秒后开始...")
wingman.util.sleep(3000)

-- 开始录制
wingman.macro.start()
print("Recording started...")

-- 录制 10 秒
wingman.util.sleep(10000)

-- 停止录制
wingman.macro.stop()
print(string.format("Recording stopped. Captured %d events", wingman.macro.getEventCount()))

-- 回放录制的操作（100% 速度，播放 1 次）
print("Playing back recorded events...")
wingman.macro.playback(100, 1)

print("Done")
```

## Running

```bash
wingman-agent.exe script examples/lua_scripts/macro_record.lua
```

## Interactive Start/Stop

The `input` module has no key-state query API. There are two ways to control recording interactively:

- The **GUI Macros page**'s record / stop / playback buttons;
- A **Python script** registering a global hotkey callback via `wingman.hotkey.register("F6", callback)`. Note that the callback fires from a background thread and Lua callables are not thread-safe; passing a Lua function to `hotkey.register` returns 0 and the registration is refused.

## Notes

- Recording captures mouse movement, clicks, and keyboard key presses
- `wingman.macro.playback(speed?, repeat?)`: `speed` is a percentage (100 means original speed), and `repeat` defaults to 1

## API Reference

- `wingman.macro.start()` / `stop()` / `pause()` / `resume()` - Recording control
- `wingman.macro.clear()` - Clear the recorded events
- `wingman.macro.isRecording()` / `isPaused()` / `getEventCount()` - State queries
- `wingman.macro.saveToLua(path)` / `saveToJSON(path)` / `loadFromJSON(path)` - Persistence
- `wingman.macro.playback(speed?, repeat?)` - Playback (`speed` defaults to 100, i.e. original speed; `repeat` defaults to 1)
- `wingman.macro.status()` - Combined state `{recording, paused, eventCount}`
