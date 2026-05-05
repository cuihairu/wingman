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
