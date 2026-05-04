# Hello World

最简单的 Wingman 脚本示例。

## 代码

```lua
-- scripts/examples/hello-world.lua

local wingman = require("wingman")

print("Wingman Hello World!")

-- 获取屏幕尺寸
local width, height = screen.getSize()
print(string.format("Screen size: %dx%d", width, height))

-- 等待 2 秒
util.sleep(2000)

print("Done!")
```

## 运行

```bash
wingman.exe scripts/examples/hello-world.lua
```

## 输出

```
Wingman Hello World!
Screen size: 1920x1080
Done!
```
