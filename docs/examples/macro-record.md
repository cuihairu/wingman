# 宏录制

演示如何录制并回放鼠标键盘操作。

> macOS 录制依赖 CGEventTap，需在「系统设置 → 隐私与安全 → 辅助功能」中授予运行进程权限。

## 代码

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

## 运行

```bash
wingman-agent.exe script examples/lua_scripts/macro_record.lua
```

## 交互式开始/停止

`input` 模块没有按键状态查询接口。交互式控制有两种方式：

- **GUI Macros 页面**的录制/停止/回放按钮；
- **Python 脚本**用 `wingman.hotkey.register("F6", callback)` 注册全局热键回调。注意回调从后台线程触发，Lua callable 非线程安全，`hotkey.register` 传入 Lua 函数会返回 0 并拒绝注册。

## 说明

- 录制包含鼠标移动、点击和键盘按键
- `wingman.macro.playback(speed?, repeat?)`：`speed` 为百分比（100 即原速），`repeat` 默认 1

## API 参考

- `wingman.macro.start()` / `stop()` / `pause()` / `resume()` — 录制控制
- `wingman.macro.clear()` — 清空已录制事件
- `wingman.macro.isRecording()` / `isPaused()` / `getEventCount()` — 状态查询
- `wingman.macro.saveToLua(path)` / `saveToJSON(path)` / `loadFromJSON(path)` — 持久化
- `wingman.macro.playback(speed?, repeat?)` — 回放（`speed` 默认 100 即原速，`repeat` 默认 1）
- `wingman.macro.status()` — 综合状态 `{recording, paused, eventCount}`
