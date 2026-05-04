# 宏录制

演示如何录制并回放鼠标键盘操作。

## 代码

```lua
-- scripts/examples/macro-record.lua

local macro = require("wingman.macro")
local input = require("wingman.input")
local util = require("wingman.util")

local recorder = macro.newRecorder()

print("Macro Recording Example")
print("Press F6 to start recording")
print("Press F7 to stop recording")
print("Press F8 to playback")

-- 热键处理
while true do
  if input.isKeyPressed("VK_F6") then
    print("Recording started...")
    recorder:start()
  elseif input.isKeyPressed("VK_F7") then
    local events = recorder:stop()
    print(string.format("Recording stopped. Captured %d events", #events))
  elseif input.isKeyPressed("VK_F8") then
    print("Playing back recorded events...")
    recorder:playback()
  end
  
  util.sleep(50)
end
```

## 运行

```bash
wingman.exe scripts/examples/macro-record.lua
```

## 说明

- **F6** - 开始录制
- **F7** - 停止录制
- **F8** - 回放录制的操作

录制的操作包括鼠标移动、点击和键盘按键。
