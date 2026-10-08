# API: wingman.vision

视觉模块，提供屏幕检测、图像匹配、形状识别等功能。

## 模块概述

vision 模块提供屏幕视觉检测功能：
- **颜色检测** - 查找颜色、检查颜色存在、获取主要颜色
- **图像匹配** - 查找图像模板

---

## 查找颜色

### find_color(color, tolerance?, region?) / findColor(color, tolerance?, region?)

**说明**：查找指定颜色在屏幕中的第一个位置。

**函数签名**：

```python
find_color(color: dict | int, tolerance: int = 10, region: dict = None) -> dict | None
```

```lua
findColor(color: table | number, tolerance: number = 10, region: table = nil) -> table | nil
```

**参数**：
- `color` - 颜色值，支持 `{r: number, g: number, b: number}` 或整数 `0xRRGGBB`
- `tolerance` - 可选，容差值（0-255），默认 10
- `region` - 可选，搜索区域 `{x: number, y: number, width: number, height: number}`，默认全屏

**返回**：
- 找到时返回 `{x: number, y: number}`
- 未找到时返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import vision

# 查找红色
pos = vision.find_color({"r": 255, "g": 0, "b": 0}, 10)
if pos:
    print(f"找到颜色: {pos['x']}, {pos['y']}")

# 使用十六进制颜色
pos = vision.find_color(0xFF0000, 10)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找红色
local pos = wingman.vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    print("找到颜色:", pos.x, pos.y)
end

-- 使用十六进制颜色
local pos = wingman.vision.findColor(0xFF0000, 10)
```

:::

---

## 查找所有颜色

### find_all_colors(color, tolerance?, region?) / findAllColors(color, tolerance?, region?)

**说明**：查找所有匹配颜色的位置。

**函数签名**：

```python
find_all_colors(color: dict | int, tolerance: int = 10, region: dict = None) -> list[dict]
```

```lua
findAllColors(color: table | number, tolerance: number = 10, region: table = nil) -> table
```

**参数**：
- `color` - 颜色值，支持字典 `{r, g, b}` 或整数
- `tolerance` - 可选，容差值（0-255），默认 10
- `region` - 可选，搜索区域，默认全屏

**返回**：
- Python: 列表 `[{x: number, y: number}, ...]`
- Lua: 数组 `[{x: number, y: number}, ...]`

:::tabs

== Python

```python:line-numbers
from wingman import vision

positions = vision.find_all_colors({"r": 255, "g": 0, "b": 0}, 5)
for i, pos in enumerate(positions):
    print(f"位置 {i}: {pos['x']}, {pos['y']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local positions = wingman.vision.findAllColors({r=255, g=0, b=0}, 5)
for i, pos in ipairs(positions) do
    print("位置", i, ":", pos.x, pos.y)
end
```

:::

---

## 检查颜色存在

### has_color(color, tolerance?, region?) / hasColor(color, tolerance?, region?)

**说明**：检查区域内是否包含指定颜色。

**函数签名**：

```python
has_color(color: dict | int, tolerance: int = 10, region: dict = None) -> bool
```

```lua
hasColor(color: table | number, tolerance: number = 10, region: table = nil) -> boolean
```

**参数**：
- `color` - 颜色值
- `tolerance` - 可选，容差值（0-255），默认 10
- `region` - 可选，搜索区域，默认全屏

**返回**：
- 是否找到颜色

:::tabs

== Python

```python:line-numbers
from wingman import vision

if vision.has_color({"r": 0, "g": 255, "b": 0}, 10):
    print("找到了绿色")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.vision.hasColor({r=0, g=255, b=0}, 10) then
    print("找到了绿色")
end
```

:::

---

## 获取主要颜色

### get_dominant_color(region?) / getDominantColor(region?)

**说明**：获取区域内主要颜色（出现次数最多的颜色）。

**函数签名**：

```python
get_dominant_color(region: dict = None) -> dict
```

```lua
getDominantColor(region: table = nil) -> table
```

**参数**：
- `region` - 可选，分析区域，默认全屏

**返回**：
- 颜色对象 `{r: number, g: number, b: number, a: number}`

:::tabs

== Python

```python:line-numbers
from wingman import vision

color = vision.get_dominant_color({"x": 0, "y": 0, "width": 200, "height": 200})
print(f"主要颜色: {color['r']}, {color['g']}, {color['b']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local color = wingman.vision.getDominantColor({x=0, y=0, width=200, height=200})
print("主要颜色:", color.r, color.g, color.b)
```

:::

---

## 查找图像

### find_image(template_path, threshold?, region?) / findImage(templatePath, threshold?, region?)

**说明**：查找图像模板在屏幕中的位置。

**函数签名**：

```python
find_image(template_path: str, threshold: float = 0.9, region: dict = None) -> dict
```

```lua
findImage(templatePath: string, threshold: number = 0.9, region: table = nil) -> table
```

**参数**：
- `template_path` / `templatePath` - 模板图片路径
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.9
- `region` - 可选，搜索区域，默认全屏

**返回**：
- 匹配结果对象：
  - `found` / `found` - 是否找到
  - `position` / `position` - 位置 `{x: number, y: number}`
  - `confidence` / `confidence` - 置信度（0-1）
  - `region` / `region` - 匹配区域 `{x: number, y: number, width: number, height: number}`

:::tabs

== Python

```python:line-numbers
from wingman import vision

result = vision.find_image("target.png", 0.9)
if result['found']:
    print(f"找到图像: {result['position']['x']}, {result['position']['y']}")
    print(f"置信度: {result['confidence']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local result = wingman.vision.findImage("target.png", 0.9)
if result.found then
    print("找到图像:", result.position.x, result.position.y)
    print("置信度:", result.confidence)
end
```

:::

---

---

## AI 视觉识别（aiSetup / aiLocate / aiElements）

AI 语义定位：截取当前屏幕（或指定区域）→ JPEG → OpenAI 兼容视觉模型 →
结构化包围盒（像素坐标），配合 `input.click` 完成「识别目标并点击」。
需 `WINGMAN_ENABLE_VISION` 构建（与 findImage 同门禁）。

> **Android**：`wingman.vision` 五函数同形可用（截帧走 captureFrame 注入，
> 参数与返回一致；`aiSetupLocal` 例外——onnxruntime NDK 依赖未收口，
> Android 侧恒返回 `false`，不静默切 HTTP）；无 OpenCV 构建下 `aiSetup`
> 注册桌面 stub 同语义假体（`aiSetup` 恒 `true`、
> `aiSetupStatus.configured` 恒 `false`）。

**端点约定**：OpenAI 兼容 `chat.completions`（`baseUrl` 填到 `/v1` 为止）——
云端 API 与本地推理服务（Ollama `http://127.0.0.1:11434/v1`、vLLM 等）同形，
模型需支持图像输入与坐标定位（如 Qwen2.5-VL 系列）。模型被要求只回
`{"found": bool, "label": str, "bbox_2d": [x1,y1,x2,y2], "confidence": 0-1}`，
坐标为 0–1000 归一值，按帧尺寸换算像素。

### ai_setup(cfg) / aiSetup(cfg)

**说明**：配置 provider（进程级驻内存，重复调用整体覆盖）。

**参数**（`cfg` 表）：
- `baseUrl`（string，必填）— 到 `/v1` 为止的 OpenAI 兼容端点
- `model`（string，必填）— 视觉模型名
- `apiKey`（string）— 明文凭据，仅推荐本地/测试环境
- `apiKeyEnc` + `passphrase`（string，成对）— **凭据加密面**：`apiKeyEnc`
  为 `crypto.encryptAES(apiKey, passphrase)` 的密文（AES-256-GCM，与密钥
  保险箱同型原语），配置文件里只存密文，运行时解密后仅驻内存，不落盘、
  不进日志；`apiKey` 与 `apiKeyEnc` 二选一
- `timeoutSeconds`（int，默认 60）

**返回**：`bool`（baseUrl/model 缺失或密文解密失败返回 `false`）

### ai_setup_status() / aiSetupStatus()

**返回**：`{configured, localMode, baseUrl, model, hasKey, lastError}`——凭据
明文不出查询接口（只有 `hasKey` 标志位）；`localMode` 标识当前走本地
ONNX 检测（见下节）。

### ai_locate(desc, region?) / aiLocate(desc, region?)

**说明**：定位屏幕上符合自然语言描述的目标。

**参数**：
- `desc`（string）— 目标描述，如「登录按钮」「左上角红色关闭叉」
- `region`（表，可选）— 只在该区域内截帧识别；带 `region` 时返回的坐标**相对 region 左上角**（不回加偏移，如需全屏坐标请自行加上 region.x/region.y）

**返回**：
- `{found=true, x, y, w, h, confidence, label}` — 像素包围盒（左上角+尺寸）
- `{found=false, error}` — 未配置 / 截帧失败 / HTTP 失败 / 模型未找到目标，
  `error` 可区分原因

:::tabs

== Python

```python:line-numbers
from wingman import vision, input, crypto

# 配置侧：密文预先用 crypto 生成后写进配置（明文不落盘）
cipher = crypto.encryptAES("sk-xxx", "machine-passphrase")

assert vision.aiSetup({
    "baseUrl": "http://127.0.0.1:11434/v1",   # 本地 Ollama
    "model": "qwen2.5-vl:7b",
    "apiKeyEnc": cipher,
    "passphrase": "machine-passphrase",
})

box = vision.aiLocate("设置图标")
if box["found"]:
    input.click(box["x"] + box["w"] // 2, box["y"] + box["h"] // 2)
```

== Lua

```lua:line-numbers
local box = wingman.vision.aiLocate("登录按钮")
if box.found then
    wingman.input.click(box.x + box.w / 2, box.y + box.h / 2)
end
```

:::

---

## 批量元素识别（aiElements）

一次 provider 调用取回多个元素（坐标语义同 `aiLocate`），供批量断言、
遍历点击等场景。截帧/凭据/容错口径与 `aiLocate` 共用（0–1000 归一按帧
换算像素；协议约定模型回
`{"found": bool, "elements": [{"label": str, "bbox_2d": [x1,y1,x2,y2], "confidence": 0-1}]}`）。

### ai_elements(desc, region?) / aiElements(desc, region?)

**参数**：
- `desc`（string）— 元素筛选描述；**空串 = 列出屏幕上全部可交互元素**
- `region`（表，可选）— 只在该区域内截帧识别；带 `region` 时返回的坐标**相对 region 左上角**（不回加偏移，如需全屏坐标请自行加上 region.x/region.y）

**返回**：
- `{found=true, elements=[{label, x, y, w, h, confidence}, ...]}` — 像素
  包围盒列表（顺序即模型返回顺序）
- `{found=false, elements=[]}` — 屏幕无匹配元素（正常空批）
- `{found=false, error}` — 未配置 / 截帧失败 / HTTP 失败，`error` 可区分原因
  （失败与正常空批靠 `error` 字段区分）

单项 bbox 缺失或退化只跳过该项，不废整批。

:::tabs

== Python

```python:line-numbers
page = vision.aiElements("")   # 全部可交互元素
if page["found"]:
    for el in page["elements"]:
        print(el["label"], el["x"], el["y"], el["w"], el["h"])
```

== Lua

```lua:line-numbers
local page = wingman.vision.aiElements("确定类按钮")
if page.found then
    for _, el in ipairs(page.elements) do
        wingman.input.click(el.x + el.w / 2, el.y + el.h / 2)
    end
end
```

:::

---

## 本地 ONNX 检测 provider（aiSetupLocal）

不依赖外部推理服务的本地视觉定位：加载本地 ONNX 检测模型（YOLOv5/v8
导出约定）直接推理，`aiLocate`/`aiElements` 入口自动分流，返回形状与
HTTP provider 完全一致。需 `WINGMAN_ENABLE_ML` 构建（onnxruntime，
Windows/Linux 档收口，Linux 为开发档验证）；未构建 ML 时 `aiSetupLocal`
恒返回 `false`，不静默降级到 HTTP provider。

### ai_setup_local(cfg) / aiSetupLocal(cfg)

**说明**：切换到本地检测 provider。与 `aiSetup` 的 HTTP provider 互斥，
后调用者生效；`reset` 双清。模型加载失败返回 `false` 且不影响既有配置。

**参数**（`cfg` 表）：
- `modelPath`（string，必填）— ONNX 模型文件路径
- `labels`（string 列表，可选）— classId→名称表；`aiLocate` 的 `desc`
  与 label 按双向大小写不敏感子串匹配，命中者中置信最高者胜出
- `minConfidence`（float，默认 0.5）— 检测置信阈值

**返回**：`bool`

**模型约定**：YOLOv5 布局 `[1,N,5+C]`（conf=obj×cls）与 YOLOv8 布局
`[1,4+C,N]`（无 obj）按输出形状自动判别；输入按模型 NCHW 尺寸 resize 到
0–1 归一，输出框按比例还原原图像素。

**限制**：预处理为简单 resize、无 letterbox（极端长宽比画面检测框会有
系统性偏移）；分割（segment）未实现，诚实报错返回空位图。

:::tabs

== Python

```python:line-numbers
assert vision.aiSetupLocal({
    "modelPath": "models/ui-detector.onnx",
    "labels": ["button", "input", "checkbox"],
    "minConfidence": 0.6,
})

box = vision.aiLocate("button")   # 本地推理，返回形状同 HTTP provider
if box["found"]:
    input.click(box["x"] + box["w"] // 2, box["y"] + box["h"] // 2)
```

== Lua

```lua:line-numbers
assert(wingman.vision.aiSetupLocal({
    modelPath = "models/ui-detector.onnx",
    labels = { "button", "input", "checkbox" },
    minConfidence = 0.6,
}))

local box = wingman.vision.aiLocate("button")
if box.found then
    wingman.input.click(box.x + box.w / 2, box.y + box.h / 2)
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_color(color, tolerance?, region?)` | `findColor(color, tolerance?, region?)` | 查找颜色 | color: 颜色值<br>tolerance: 容差(默认10)<br>region: 搜索区域(可选)<br>返回: 位置对象或None/nil |
| `find_all_colors(color, tolerance?, region?)` | `findAllColors(color, tolerance?, region?)` | 查找所有颜色 | 返回: 位置数组 |
| `has_color(color, tolerance?, region?)` | `hasColor(color, tolerance?, region?)` | 检查颜色存在 | 返回: 是否存在 |
| `get_dominant_color(region?)` | `getDominantColor(region?)` | 获取主要颜色 | region: 分析区域(可选)<br>返回: 颜色对象RGBA |
| `find_image(path, threshold?, region?)` | `findImage(path, threshold?, region?)` | 查找图像 | path: 模板路径<br>threshold: 匹配阈值(默认0.9)<br>region: 搜索区域(可选)<br>返回: 匹配结果对象 |
| `ai_setup(cfg)` | `aiSetup(cfg)` | 配置 AI 视觉 provider | cfg: 配置表（baseUrl/model/凭据）<br>返回: 是否成功 |
| `ai_setup_status()` | `aiSetupStatus()` | 查询 provider 配置状态 | 返回: {configured, localMode, baseUrl, model, hasKey, lastError} |
| `ai_setup_local(cfg)` | `aiSetupLocal(cfg)` | 切换本地 ONNX 检测 provider（需 ML 构建） | cfg: 配置表（modelPath/labels/minConfidence）<br>返回: 是否成功 |
| `ai_locate(desc, region?)` | `aiLocate(desc, region?)` | AI 语义定位屏幕目标 | desc: 目标描述<br>region: 区域(可选)<br>返回: {found,x,y,w,h,confidence,label} 或 {found:false,error} |
| `ai_elements(desc, region?)` | `aiElements(desc, region?)` | AI 批量元素识别 | desc: 筛选描述（空串=全部可交互元素）<br>region: 区域(可选)<br>返回: {found,elements:[{label,x,y,w,h,confidence}]} 或 {found:false,error} |
