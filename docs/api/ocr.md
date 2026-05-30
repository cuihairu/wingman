# API: wingman.ocr

OCR 模块提供文字识别功能，基于 Tesseract 引擎。

## 识别文字

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
local ocr = require("wingman.ocr")

-- 识别指定区域
local result = ocr.recognize({x=100, y=100, width=200, height=50})
if result.success then
    print("识别成功:", result.text)
    print("置信度:", result.confidence)
end
```

:::

## 简化版识别

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
local ocr = require("wingman.ocr")

local text = ocr.recognizeText({x=0, y=0, width=300, height=100})
if text then
    print("识别到:", text)
end
```

:::

## 设置语言

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
local ocr = require("wingman.ocr")

-- 单语言
ocr.setLanguage("eng")      -- 仅英文
ocr.setLanguage("chi_sim")  -- 仅简体中文

-- 多语言（用 + 连接）
ocr.setLanguage("chi_sim+eng")  -- 中英文混合
```

:::

## 设置语言包路径

:::tabs

== Python

```python:line-numbers
from wingman import ocr

ocr.set_data_path("C:/path/to/tessdata")
```

== Lua

```lua:line-numbers
local ocr = require("wingman.ocr")

ocr.setDataPath("C:/path/to/tessdata")
```

:::

---

## 可用接口

### `recognize(region?)`

识别指定区域的文字。

**参数：**
- `region` - 识别区域 `{x, y, width, height}`，默认全屏

**返回：** `{success: boolean, text: string, confidence: number}`

### `recognize_text(region?)` / `recognizeText(region?)`

简化版文字识别，直接返回文本。

**返回：** `string` 或 `None`/`nil`

### `set_language(lang)` / `setLanguage(lang)`

指定识别语言。

**参数：**
- `lang` - 语言代码（如 `"eng"`, `"chi_sim"`, `"chi_sim+eng"`）

### `set_data_path(path)` / `setDataPath(path)`

设置 Tesseract 语言包目录。

---

## 语言包配置

Tesseract 语言包需要单独下载。

### 下载语言包

从 [tessdata](https://github.com/tesseract-ocr/tessdata) 下载所需的 .traineddata 文件：

| 语言 | 文件名 |
|------|--------|
| 英文 | `eng.traineddata` |
| 简体中文 | `chi_sim.traineddata` |
| 繁体中文 | `chi_tra.traineddata` |
| 日文 | `jpn.traineddata` |
| 韩文 | `kor.traineddata` |

### 安装语言包

将下载的 .traineddata 文件放到 Tesseract 的 tessdata 目录：

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
