# OCR API

OCR 模块提供文字识别功能，基于 Tesseract 引擎。

## Lua API

### ocr.recognize(region?)

识别指定区域的文字。

```lua
local result = ocr.recognize({x=100, y=100, width=200, height=50})
if result.success then
    print("识别成功:", result.text)
    print("置信度:", result.confidence)
end
```

**参数:**
- `region` - 识别区域 `{x, y, width, height}`，默认全屏

**返回:** `{success: boolean, text: string, confidence: number}`

---

### ocr.recognizeText(region?)

简化版文字识别，直接返回文本。

```lua
local text = ocr.recognizeText({x=0, y=0, width=300, height=100})
if text then
    print("识别到:", text)
end
```

**返回:** `string` 或 `nil`

## C++ API

### OCR::recognize()

```cpp
#include "wingman/ocr.hpp"

// 识别全屏
auto result = OCR::recognize();

// 识别指定区域
Rect region{100, 100, 200, 50};
auto result = OCR::recognize(region);

if (result.success) {
    std::cout << "识别结果: " << result.text << std::endl;
}
```

### OcrResult 结构

```cpp
struct OcrResult {
    bool success;              // 是否成功
    std::string text;          // 识别文本
    double confidence;         // 置信度 0.0-1.0
    std::vector<Rect> charRegions;  // 每个字符的位置
};
```

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

或者通过代码指定语言包路径：
```lua
-- 设置语言包目录
ocr.setDataPath("C:/path/to/tessdata")

-- 指定使用的中英文语言包
ocr.setLanguage("chi_sim+eng")
```

### 指定识别语言

```lua
-- 单语言
ocr.setLanguage("eng")      -- 仅英文
ocr.setLanguage("chi_sim")  -- 仅简体中文

-- 多语言（用 + 连接）
ocr.setLanguage("chi_sim+eng")  -- 中英文混合
```

## 编译选项

OCR 功能默认启用，需要安装 Tesseract：

```bash
vcpkg install tesseract
```

如不需要 OCR 功能，可在编译时禁用：

```bash
cmake -DWINGMAN_ENABLE_OCR=OFF ..
```

禁用后将使用 stub 实现，`recognize()` 始终返回空结果。
