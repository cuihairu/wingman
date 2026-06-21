# API: wingman.ocr

OCR 模块，提供基于 Tesseract 引擎的文字识别功能。

::: warning 注意
此模块需要安装 Tesseract。未安装时返回存根实现。
:::

## 模块概述

ocr 模块提供文字识别功能：
- **文字识别** - 识别指定区域的文字
- **简化识别** - 直接返回识别文本
- **语言设置** - 设置识别语言
- **路径配置** - 设置语言包目录

---

## 识别文字

### recognize(region?) / recognize(region?)

**说明**：识别指定区域的文字，返回详细结果。

**函数签名**：

```python
recognize(region: dict = None) -> dict
```

```lua
recognize(region: table = nil) -> table
```

**参数**：
- `region` - 可选，识别区域 `{x: number, y: number, width: number, height: number}`，默认全屏

**返回**：
- 识别结果对象：
  - `success` / `success` - 是否成功
  - `text` / `text` - 识别的文本
  - `confidence` / `confidence` - 置信度（0-1）

:::tabs

== Python

```python:line-numbers
from wingman import ocr

# 识别指定区域
result = ocr.recognize({"x": 100, "y": 100, "width": 200, "height": 50})
if result['success']:
    print(f"识别成功: {result['text']}")
    print(f"置信度: {result['confidence']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 识别指定区域
local result = wingman.ocr.recognize({x=100, y=100, width=200, height=50})
if result.success then
    print("识别成功:", result.text)
    print("置信度:", result.confidence)
end
```

:::

---

## 简化版识别

### recognize_text(region?) / recognizeText(region?)

**说明**：简化版文字识别，直接返回文本。

**函数签名**：

```python
recognize_text(region: dict = None) -> str | None
```

```lua
recognizeText(region: table = nil) -> string | nil
```

**参数**：
- `region` - 可选，识别区域，默认全屏

**返回**：
- 识别的文本，失败返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import ocr

text = ocr.recognize_text({"x": 0, "y": 0, "width": 300, "height": 100})
if text:
    print(f"识别到: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local text = wingman.ocr.recognizeText({x=0, y=0, width=300, height=100})
if text then
    print("识别到:", text)
end
```

:::

---

## 设置识别语言

### set_language(lang) / setLanguage(lang)

**说明**：指定识别语言。

**函数签名**：

```python
set_language(lang: str) -> None
```

```lua
setLanguage(lang: string) -> nil
```

**参数**：
- `lang` - 语言代码，支持多语言用 `+` 连接

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import ocr

# 单语言
ocr.set_language("eng")      # 仅英文
ocr.set_language("chi_sim")  # 仅简体中文

# 多语言（用 + 连接）
ocr.set_language("chi_sim+eng")  # 中英文混合
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 单语言
wingman.ocr.setLanguage("eng")      -- 仅英文
wingman.ocr.setLanguage("chi_sim")  -- 仅简体中文

-- 多语言（用 + 连接）
wingman.ocr.setLanguage("chi_sim+eng")  -- 中英文混合
```

:::

---

## 设置语言包路径

### set_data_path(path) / setDataPath(path)

**说明**：设置 Tesseract 语言包目录。

**函数签名**：

```python
set_data_path(path: str) -> None
```

```lua
setDataPath(path: string) -> nil
```

**参数**：
- `path` - 语言包目录路径

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import ocr

ocr.set_data_path("C:/path/to/tessdata")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.ocr.setDataPath("C:/path/to/tessdata")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `recognize(region?)` | `recognize(region?)` | 识别文字 | region: 识别区域(可选)<br>返回: 结果对象 |
| `recognize_text(region?)` | `recognizeText(region?)` | 简化识别 | region: 识别区域(可选)<br>返回: 文本或None/nil |
| `set_language(lang)` | `setLanguage(lang)` | 设置语言 | lang: 语言代码 |
| `set_data_path(path)` | `setDataPath(path)` | 设置语言包路径 | path: 目录路径 |

---

## 语言包配置

Tesseract 语言包需要单独下载。

### 下载语言包

从 [tessdata](https://github.com/tesseract-ocr/tessdata) 下载所需的 `.traineddata` 文件：

| 语言 | 文件名 |
|------|--------|
| 英文 | `eng.traineddata` |
| 简体中文 | `chi_sim.traineddata` |
| 繁体中文 | `chi_tra.traineddata` |
| 日文 | `jpn.traineddata` |
| 韩文 | `kor.traineddata` |

### 安装语言包

将下载的 `.traineddata` 文件放到 Tesseract 的 tessdata 目录：

**Windows (vcpkg 安装):**
```
<vcpkg-root>\installed\x64-windows\share\tesseract\tessdata\
```

---

## 编译选项

OCR 功能需要安装 Tesseract：

```bash
vcpkg install tesseract
```

如不需要 OCR 功能，可在编译时禁用：

```bash
cmake -DWINGMAN_ENABLE_OCR=OFF ..
```

禁用后将使用 stub 实现，`recognize()` 始终返回空结果。
