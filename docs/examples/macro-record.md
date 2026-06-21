# 宏录制

演示如何录制并回放鼠标键盘操作。

> ℹ️ macOS 录制依赖 CGEventTap，需在「系统设置 → 隐私与安全 → 辅助功能」中授予运行进程权限。

## 代码

```lua
local wingman = require("wingman")

-- scripts/examples/macro-record.lua

print("Macro Recording Example")
print("Press F6 to start recording")
print("Press F7 to stop recording")
print("Press F8 to playback")

-- 热键处理（单例函数式 API，全局只有一个录制器）
while true do
  if wingman.input.isKeyPressed("VK_F6") then
    print("Recording started...")
    wingman.macro.start()
  elseif wingman.input.isKeyPressed("VK_F7") then
    wingman.macro.stop()
    print(string.format("Recording stopped. Captured %d events", wingman.macro.getEventCount()))
  elseif wingman.input.isKeyPressed("VK_F8") then
    print("Playing back recorded events...")
    wingman.macro.playback()
  end

  wingman.util.sleep(50)
end
```

## 运行

```bash
wingman-runtime.exe script scripts/examples/macro-record.lua
```

## 说明

- **F6** - 开始录制
- **F7** - 停止录制
- **F8** - 回放录制的操作

录制的操作包括鼠标移动、点击和键盘按键。

## API 参考

- `wingman.macro.start()` / `stop()` / `pause()` / `resume()` — 录制控制
- `wingman.macro.clear()` — 清空已录制事件
- `wingman.macro.isRecording()` / `isPaused()` / `getEventCount()` — 状态查询
- `wingman.macro.saveToLua(path)` / `saveToJSON(path)` / `loadFromJSON(path)` — 持久化
- `wingman.macro.playback(speed?, repeat?)` — 回放（`speed` 默认 100 即原速，`repeat` 默认 1）
- `wingman.macro.status()` — 综合状态 `{recording, paused, eventCount}`
