> [中文](ocr.md) | **English**

# API: wingman.ocr

OCR module providing text recognition based on the Tesseract engine.

::: warning Note
This module requires Tesseract to be installed. When it is not installed, a stub implementation is returned.
:::

## Module Overview

The ocr module provides text recognition:
- **Text recognition** - recognize text in a specified region
- **Simplified recognition** - return the recognized text directly

---

## Recognize Text

### recognize(region) / recognize(region)

**Description**: Recognizes text in the specified region and returns a detailed result.

**Function signature**:

```python
recognize(region: dict) -> dict
```

```lua
recognize(region: table) -> table
```

**Parameters**:
- `region` - required, the recognition region `{x: number, y: number, width: number, height: number}`

**Returns**:
- Recognition result object:
  - `success` / `success` - whether the call succeeded
  - `text` / `text` - the recognized text
  - `confidence` / `confidence` - confidence (0-100 integer percentage)

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

## Simplified Recognition

### recognize_text(region) / recognizeText(region)

**Description**: Simplified text recognition that returns the text directly.

**Function signature**:

```python
recognize_text(region: dict) -> str | None
```

```lua
recognizeText(region: table) -> string | nil
```

**Parameters**:
- `region` - required, the recognition region `{x: number, y: number, width: number, height: number}`

**Returns**:
- The recognized text, or `None`/`nil` on failure

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

## Available Interfaces

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `recognize(region)` | `recognize(region)` | Recognize text | region: recognition region (required)<br>Return: result object |
| `recognize_text(region)` | `recognizeText(region)` | Simplified recognition | region: recognition region (required)<br>Return: text or None/nil |

---

## Language Pack Configuration

Tesseract language packs must be downloaded separately.

### Download Language Packs

Download the required `.traineddata` files from [tessdata](https://github.com/tesseract-ocr/tessdata):

| Language | File name |
|------|--------|
| English | `eng.traineddata` |
| Simplified Chinese | `chi_sim.traineddata` |
| Traditional Chinese | `chi_tra.traineddata` |
| Japanese | `jpn.traineddata` |
| Korean | `kor.traineddata` |

### Install Language Packs

Place the downloaded `.traineddata` files into Tesseract's tessdata directory:

**Windows (vcpkg installation):**
```
<vcpkg-root>\installed\x64-windows\share\tesseract\tessdata\
```

---

## Build Options

The OCR feature requires Tesseract to be installed:

```bash
vcpkg install tesseract
```

If you do not need the OCR feature, you can disable it at build time:

```bash
cmake -DWINGMAN_ENABLE_OCR=OFF ..
```

When disabled, a stub implementation is used and `recognize()` always returns an empty result.
