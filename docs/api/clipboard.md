# API: wingman.clipboard

剪贴板模块，提供系统剪贴板的读写能力，支持文本、HTML、图像和文件列表。

## 模块概述

clipboard 模块提供以下能力：

- **文本剪贴板** - 设置/读取/检测纯文本内容
- **HTML 剪贴板** - 设置/读取/检测富文本 HTML 内容
- **图像剪贴板** - 设置/读取/检测位图图像
- **文件剪贴板** - 设置/读取/检测文件列表（拖拽场景）
- **通用操作** - 清空剪贴板、判断是否为空

---

## 文本操作

### setText(text) / setText(text)

**说明**：设置剪贴板的纯文本内容。

**函数签名**：

```python
setText(text: str) -> bool
```

```lua
setText(text: string) -> boolean
```

**参数**：
- `text` - 要写入剪贴板的文本

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

clipboard.setText("hello world")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.clipboard.setText("hello world")
```

:::

---

### getText() / getText()

**说明**：读取剪贴板的纯文本内容。

**函数签名**：

```python
getText() -> str
```

```lua
getText() -> string
```

**参数**：
- 无

**返回**：
- `str`/`string` - 剪贴板中的文本内容

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

text = clipboard.getText()
print(text)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local text = wingman.clipboard.getText()
print(text)
```

:::

---

### hasText() / hasText()

**说明**：检测剪贴板中是否包含文本内容。

**函数签名**：

```python
hasText() -> bool
```

```lua
hasText() -> boolean
```

**参数**：
- 无

**返回**：
- `bool`/`boolean` - 是否包含文本

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

if clipboard.hasText():
    print(clipboard.getText())
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.clipboard.hasText() then
    print(wingman.clipboard.getText())
end
```

:::

---

## HTML 操作

### setHTML(html) / setHTML(html)

**说明**：设置剪贴板的 HTML 富文本内容。

**函数签名**：

```python
setHTML(html: str) -> bool
```

```lua
setHTML(html: string) -> boolean
```

**参数**：
- `html` - 要写入剪贴板的 HTML 内容

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

clipboard.setHTML("<b>bold text</b>")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.clipboard.setHTML("<b>bold text</b>")
```

:::

---

### getHTML() / getHTML()

**说明**：读取剪贴板的 HTML 富文本内容。

**函数签名**：

```python
getHTML() -> str
```

```lua
getHTML() -> string
```

**参数**：
- 无

**返回**：
- `str`/`string` - 剪贴板中的 HTML 内容

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

html = clipboard.getHTML()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local html = wingman.clipboard.getHTML()
```

:::

---

### hasHTML() / hasHTML()

**说明**：检测剪贴板中是否包含 HTML 内容。

**函数签名**：

```python
hasHTML() -> bool
```

```lua
hasHTML() -> boolean
```

**参数**：
- 无

**返回**：
- `bool`/`boolean` - 是否包含 HTML

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

if clipboard.hasHTML():
    print(clipboard.getHTML())
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.clipboard.hasHTML() then
    print(wingman.clipboard.getHTML())
end
```

:::

---

## 图像操作

### setImage(imageData, width, height) / setImage(imageData, width, height)

**说明**：设置剪贴板中的位图图像。要求原始像素数据（字节流）以及图像的宽高，模块内部按 RGB/BGR 像素缓冲写入系统剪贴板。

**函数签名**：

```python
setImage(imageData: str, width: int, height: int) -> bool
```

```lua
setImage(imageData: string, width: number, height: number) -> boolean
```

**参数**：
- `imageData` - 图像数据（字节字符串）
- `width` - 图像宽度（像素）
- `height` - 图像高度（像素）

**返回**：
- `bool`/`boolean` - 是否成功；当任一参数类型不匹配（非字符串/非整数）时直接返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

# 写入 2x2 像素图像数据
data = b"\xff\x00\x00" * 4  # 像素字节流
clipboard.setImage(data, 2, 2)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 写入 2x2 像素图像数据
local data = string.rep("\255\0\0", 4)  -- 像素字节流
wingman.clipboard.setImage(data, 2, 2)
```

:::

---

### getImage() / getImage()

**说明**：读取剪贴板中的位图图像。

**函数签名**：

```python
getImage() -> dict | None
```

```lua
getImage() -> table | nil
```

**参数**：
- 无

**返回**：
- 成功时返回对象 `{ data: str, width: int, height: int }`：
  - `data` - 图像像素字节流
  - `width` - 图像宽度（像素）
  - `height` - 图像高度（像素）
- 当剪贴板无图像时返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

img = clipboard.getImage()
if img is not None:
    print(img["width"], img["height"], len(img["data"]))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local img = wingman.clipboard.getImage()
if img ~= nil then
    print(img.width, img.height, #img.data)
end
```

:::

---

### hasImage() / hasImage()

**说明**：检测剪贴板中是否包含图像。

**函数签名**：

```python
hasImage() -> bool
```

```lua
hasImage() -> boolean
```

**参数**：
- 无

**返回**：
- `bool`/`boolean` - 是否包含图像

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

if clipboard.hasImage():
    img = clipboard.getImage()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.clipboard.hasImage() then
    local img = wingman.clipboard.getImage()
end
```

:::

---

## 文件列表操作

### setFiles(files) / setFiles(files)

**说明**：设置剪贴板中的文件列表（用于模拟文件拖拽/复制）。参数既支持单个字符串数组，也支持多个字符串参数。

**函数签名**：

```python
setFiles(files: list[str]) -> bool
```

```lua
setFiles(files: string[]) -> boolean
```

**参数**：
- `files` - 文件路径数组；也可传入多个字符串参数（每个字符串会被收集为路径，数组参数中的字符串元素也会被展开）

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

# 传入数组
clipboard.setFiles(["C:/a.txt", "C:/b.txt"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 传入数组
wingman.clipboard.setFiles({ "C:/a.txt", "C:/b.txt" })

-- 也可传入多个字符串参数
wingman.clipboard.setFiles("C:/a.txt", "C:/b.txt")
```

:::

---

### getFiles() / getFiles()

**说明**：读取剪贴板中的文件列表。

**函数签名**：

```python
getFiles() -> list[str]
```

```lua
getFiles() -> string[]
```

**参数**：
- 无

**返回**：
- `list[str]`/`string[]` - 文件路径数组

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

files = clipboard.getFiles()
for f in files:
    print(f)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local files = wingman.clipboard.getFiles()
for _, f in ipairs(files) do
    print(f)
end
```

:::

---

### hasFiles() / hasFiles()

**说明**：检测剪贴板中是否包含文件列表。

**函数签名**：

```python
hasFiles() -> bool
```

```lua
hasFiles() -> boolean
```

**参数**：
- 无

**返回**：
- `bool`/`boolean` - 是否包含文件

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

if clipboard.hasFiles():
    print(clipboard.getFiles())
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.clipboard.hasFiles() then
    print(wingman.clipboard.getFiles())
end
```

:::

---

## 通用操作

### clear() / clear()

**说明**：清空剪贴板中的所有内容。

**函数签名**：

```python
clear() -> None
```

```lua
clear() -> nil
```

**参数**：
- 无

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

clipboard.clear()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.clipboard.clear()
```

:::

---

### isEmpty() / isEmpty()

**说明**：检测剪贴板是否为空。

**函数签名**：

```python
isEmpty() -> bool
```

```lua
isEmpty() -> boolean
```

**参数**：
- 无

**返回**：
- `bool`/`boolean` - 是否为空

:::tabs

== Python

```python:line-numbers
from wingman import clipboard

if clipboard.isEmpty():
    print("clipboard is empty")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.clipboard.isEmpty() then
    print("clipboard is empty")
end
```

:::

---

## 完整示例

### Python

```python
from wingman import clipboard

# 文本读写
clipboard.setText("hello")
assert clipboard.hasText()
print(clipboard.getText())

# HTML 读写
clipboard.setHTML("<i>styled</i>")
assert clipboard.hasHTML()

# 文件列表
clipboard.setFiles(["C:/report.csv", "C:/data.json"])
if clipboard.hasFiles():
    print(clipboard.getFiles())

# 判空与清空
if not clipboard.isEmpty():
    clipboard.clear()
```

### Lua

```lua
local wingman = require("wingman")

-- 文本读写
wingman.clipboard.setText("hello")
assert(wingman.clipboard.hasText())
print(wingman.clipboard.getText())

-- HTML 读写
wingman.clipboard.setHTML("<i>styled</i>")
assert(wingman.clipboard.hasHTML())

-- 文件列表
wingman.clipboard.setFiles({ "C:/report.csv", "C:/data.json" })
if wingman.clipboard.hasFiles() then
    print(wingman.clipboard.getFiles())
end

-- 判空与清空
if not wingman.clipboard.isEmpty() then
    wingman.clipboard.clear()
end
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `setText(text)` | `setText(text)` | 设置文本 | text: 文本内容 |
| `getText()` | `getText()` | 读取文本 | 无 |
| `hasText()` | `hasText()` | 是否含文本 | 无 |
| `setHTML(html)` | `setHTML(html)` | 设置 HTML | html: HTML 内容 |
| `getHTML()` | `getHTML()` | 读取 HTML | 无 |
| `hasHTML()` | `hasHTML()` | 是否含 HTML | 无 |
| `setImage(imageData, width, height)` | `setImage(imageData, width, height)` | 设置图像 | imageData: 像素字节流<br>width: 宽度<br>height: 高度 |
| `getImage()` | `getImage()` | 读取图像 | 无 |
| `hasImage()` | `hasImage()` | 是否含图像 | 无 |
| `setFiles(files)` | `setFiles(files)` | 设置文件列表 | files: 路径数组（也支持多字符串参数） |
| `getFiles()` | `getFiles()` | 读取文件列表 | 无 |
| `hasFiles()` | `hasFiles()` | 是否含文件列表 | 无 |
| `clear()` | `clear()` | 清空剪贴板 | 无 |
| `isEmpty()` | `isEmpty()` | 是否为空 | 无 |
