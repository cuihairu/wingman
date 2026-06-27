# API: wingman.perf

性能优化模块，提供图像缓存、并行处理等性能优化配置。

> **依赖说明**：本模块依赖 OpenCV。当构建未启用 OpenCV（`HAS_OPENCV` 未定义）时为 **stub 实现**——所有函数仍然存在并可调用，但返回空值/默认值（例如缓存统计恒为 0、`fastFindImage` 恒返回 nil、`parallelFindColors` 恒返回空表）。请确保构建时启用 OpenCV 才能获得真实功能。

## 模块概述

perf 模块提供性能优化功能：
- **配置管理** - 设置和获取性能配置
- **图像缓存** - 预加载和清理图像缓存
- **缓存统计** - 获取缓存使用情况

---

## 设置性能配置

### set_config(config) / setConfig(config)

**说明**：设置性能配置。

**函数签名**：

```python
set_config(config: dict) -> None
```

```lua
setConfig(config: table) -> nil
```

**参数**：
- `config` - 配置对象
  - `enableImageCache` / `enableImageCache` - 是否启用图像缓存，默认 `true`
  - `maxCacheSize` / `maxCacheSize` - 最大缓存大小（MB），默认 `100`
  - `enableParallelProcessing` / `enableParallelProcessing` - 是否启用并行处理，默认 `true`
  - `numThreads` / `numThreads` - 线程数，默认 `4`

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 配置性能选项
perf.set_config({
    "enableImageCache": True,
    "maxCacheSize": 100,
    "enableParallelProcessing": True,
    "numThreads": 4
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 配置性能选项
wingman.perf.setConfig({
    enableImageCache = true,
    maxCacheSize = 100,
    enableParallelProcessing = true,
    numThreads = 4
})
```

:::

---

## 获取性能配置

### get_config() / getConfig()

**说明**：获取当前性能配置。

**函数签名**：

```python
get_config() -> dict
```

```lua
getConfig() -> table
```

**返回**：
- Python: 配置字典
- Lua: 配置表格

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 获取配置
config = perf.get_config()
print(f"图像缓存: {config['enableImageCache']}")
print(f"最大缓存: {config['maxCacheSize']} MB")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取配置
local config = wingman.perf.getConfig()
print("图像缓存: " .. tostring(config.enableImageCache))
print("最大缓存: " .. config.maxCacheSize .. " MB")
```

:::

---

## 预加载图像

### preload_image(path) / preloadImage(path)

**说明**：预加载图像到缓存。

**函数签名**：

```python
preload_image(path: str) -> None
```

```lua
preloadImage(path: string) -> nil
```

**参数**：
- `path` - 图像文件路径

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 预加载图像到缓存
perf.preload_image("assets/target.png")
perf.preload_image("assets/button.png")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 预加载图像到缓存
wingman.perf.preloadImage("assets/target.png")
wingman.perf.preloadImage("assets/button.png")
```

:::

---

## 清理缓存

### clear_cache() / clearCache()

**说明**：清理图像缓存。

**函数签名**：

```python
clear_cache() -> None
```

```lua
clearCache() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 清理图像缓存
perf.clear_cache()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 清理图像缓存
wingman.perf.clearCache()
```

:::

---

## 获取缓存统计

### get_cache_stats() / getCacheStats()

**说明**：获取缓存统计信息。

**函数签名**：

```python
get_cache_stats() -> dict
```

```lua
getCacheStats() -> table
```

**返回**：
- 统计对象：
  - `size` - 已缓存图像数量
  - `hits` - 缓存命中次数
  - `misses` - 缓存未命中次数
  - `hitRate` - 命中率（0–100 的百分比数值，非 0–1）

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 获取缓存统计
stats = perf.get_cache_stats()
print(f"已缓存: {stats['size']} 张")
print(f"命中次数: {stats['hits']}")
print(f"未命中次数: {stats['misses']}")
print(f"命中率: {stats['hitRate']}%")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取缓存统计
local stats = wingman.perf.getCacheStats()
print("已缓存: " .. stats.size .. " 张")
print("命中次数: " .. stats.hits)
print("未命中次数: " .. stats.misses)
print("命中率: " .. stats.hitRate .. "%")
```

:::

---

## 获取缓存大小

### get_cache_size() / getCacheSize()

**说明**：获取当前已缓存的图像数量（与 `get_cache_stats()` 返回的 `size` 字段相同，但仅返回该整数）。

**函数签名**：

```python
get_cache_size() -> int
```

```lua
getCacheSize() -> number
```

**返回**：已缓存图像数量。

:::tabs

== Python

```python:line-numbers
from wingman import perf

count = perf.get_cache_size()
print(f"已缓存 {count} 张图像")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local count = wingman.perf.getCacheSize()
print("已缓存 " .. count .. " 张图像")
```

:::

---

## 快速查找图像

### fast_find_image(...) / fastFindImage(...)

**说明**：在指定矩形区域内快速查找目标图像，返回首个匹配位置。底层使用缓存的图像数据以提升速度。

**函数签名**：

```python
fast_find_image(path: str, x: int, y: int, w: int, h: int, threshold: float = 0.9) -> dict | None
```

```lua
fastFindImage(path: string, x: number, y: number, w: number, h: number, threshold: number = 0.9) -> table | nil
```

**参数**：
- `path` - 目标图像文件路径
- `x`, `y` - 搜索区域左上角坐标
- `w`, `h` - 搜索区域宽高
- `threshold` - 匹配阈值（0–1），默认 `0.9`

**返回**：找到时返回 `{x, y}`（匹配位置），未找到返回 `None`/`nil`。

:::tabs

== Python

```python:line-numbers
from wingman import perf

pos = perf.fast_find_image("assets/target.png", 0, 0, 1920, 1080, 0.95)
if pos:
    print(f"找到目标: {pos['x']}, {pos['y']}")
else:
    print("未找到目标")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local pos = wingman.perf.fastFindImage("assets/target.png", 0, 0, 1920, 1080, 0.95)
if pos then
    print("找到目标:", pos.x, pos.y)
else
    print("未找到目标")
end
```

:::

---

## 并行查找颜色

### parallel_find_colors(...) / parallelFindColors(...)

**说明**：在指定矩形区域内并行查找指定颜色，返回所有匹配点坐标。

**函数签名**：

```python
parallel_find_colors(color: int, x: int, y: int, w: int, h: int, tolerance: int, max_count: int = 0) -> list[dict]
```

```lua
parallelFindColors(color: number, x: number, y: number, w: number, h: number, tolerance: number, maxCount: number = 0) -> table
```

**参数**：
- `color` - 目标颜色的 RGB 整数值（如 `0xFF0000` 表示红色）
- `x`, `y` - 搜索区域左上角坐标
- `w`, `h` - 搜索区域宽高
- `tolerance` - 颜色容差
- `max_count` / `maxCount` - 最大返回点数，`0` 表示不限制（默认 `0`）

**返回**：匹配点列表，每个元素为 `{x, y}`。无匹配时返回空列表。

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 查找屏幕上的红色像素（最多 50 个）
points = perf.parallel_find_colors(0xFF0000, 0, 0, 1920, 1080, 10, 50)
for p in points:
    print(f"({p['x']}, {p['y']})")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找屏幕上的红色像素（最多 50 个）
local points = wingman.perf.parallelFindColors(0xFF0000, 0, 0, 1920, 1080, 10, 50)
for _, p in ipairs(points) do
    print("(" .. p.x .. ", " .. p.y .. ")")
end
```

:::

---

## 获取性能统计

### get_stats() / getStats()

**说明**：获取性能管理器的运行统计信息（捕获与查找的次数及平均耗时）。

**函数签名**：

```python
get_stats() -> dict
```

```lua
getStats() -> table
```

**返回**：统计对象：
- `totalCaptures` - 屏幕捕获总次数
- `totalColorSearches` - 颜色查找总次数
- `totalImageSearches` - 图像查找总次数
- `avgCaptureTime` - 平均捕获耗时（秒）
- `avgColorSearchTime` - 平均颜色查找耗时（秒）
- `avgImageSearchTime` - 平均图像查找耗时（秒）

:::tabs

== Python

```python:line-numbers
from wingman import perf

stats = perf.get_stats()
print(f"捕获次数: {stats['totalCaptures']}")
print(f"平均捕获耗时: {stats['avgCaptureTime']}s")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local stats = wingman.perf.getStats()
print("捕获次数: " .. stats.totalCaptures)
print("平均捕获耗时: " .. stats.avgCaptureTime .. "s")
```

:::

---

## 重置性能统计

### reset_stats() / resetStats()

**说明**：重置性能管理器的运行统计计数器（捕获/查找次数与平均耗时归零）。不影响图像缓存内容。

**函数签名**：

```python
reset_stats() -> None
```

```lua
resetStats() -> nil
```

**返回**：无。

:::tabs

== Python

```python:line-numbers
from wingman import perf

perf.reset_stats()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.perf.resetStats()
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `set_config(config)` | `setConfig(config)` | 设置性能配置 | config: 配置对象 |
| `get_config()` | `getConfig()` | 获取性能配置 | 返回: 配置对象 |
| `preload_image(path)` | `preloadImage(path)` | 预加载图像 | path: 图像文件路径 |
| `clear_cache()` | `clearCache()` | 清理缓存 | 无返回值 |
| `get_cache_size()` | `getCacheSize()` | 获取缓存图像数量 | 返回: int |
| `get_cache_stats()` | `getCacheStats()` | 获取缓存统计 | 返回: {size, hits, misses, hitRate} |
| `fast_find_image(...)` | `fastFindImage(...)` | 快速查找图像 | path, x, y, w, h, threshold=0.9 返回: {x,y}? |
| `parallel_find_colors(...)` | `parallelFindColors(...)` | 并行查找颜色 | color, x, y, w, h, tolerance, maxCount=0 返回: {{x,y}} |
| `get_stats()` | `getStats()` | 获取性能统计 | 返回: {totalCaptures,...} |
| `reset_stats()` | `resetStats()` | 重置性能统计 | 无返回值 |

---

## 配置选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enableImageCache` | boolean | `true` | 是否启用图像缓存 |
| `maxCacheSize` | number | `100` | 最大缓存大小（MB） |
| `enableParallelProcessing` | boolean | `true` | 是否启用并行处理 |
| `numThreads` | number | `4` | 线程数 |
