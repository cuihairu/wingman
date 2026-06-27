# API: wingman.ocr

OCR 模块，提供基于 Tesseract 引擎的文字识别功能。

::: warning 注意
此模块需要安装 Tesseract。未安装时返回存根实现。
:::

## 模块概述

ocr 模块提供文字识别功能：
- **文字识别** - 识别指定区域的文字
- **简化识别** - 直接返回识别文本

---

## 识别文字

### recognize(region) / recognize(region)

**说明**：识别指定区域的文字，返回详细结果。

**函数签名**：

```python
recognize(region: dict) -> dict
```

```lua
recognize(region: table) -> table
```

**参数**：
- `region` - 必传，识别区域 `{x: number, y: number, width: number, height: number}`

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

### recognize_text(region) / recognizeText(region)

**说明**：简化版文字识别，直接返回文本。

**函数签名**：

```python
recognize_text(region: dict) -> str | None
```

```lua
recognizeText(region: table) -> string | nil
```

**参数**：
- `region` - 必传，识别区域 `{x: number, y: number, width: number, height: number}`

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

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `recognize(region)` | `recognize(region)` | 识别文字 | region: 识别区域(必传)<br>返回: 结果对象 |
| `recognize_text(region)` | `recognizeText(region)` | 简化识别 | region: 识别区域(必传)<br>返回: 文本或None/nil |

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
