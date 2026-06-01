# API: wingman.perf

性能优化模块，提供图像缓存、并行处理等性能优化配置。

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
local perf = require("wingman.perf")

-- 配置性能选项
perf.setConfig({
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
local perf = require("wingman.perf")

-- 获取配置
local config = perf.getConfig()
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
local perf = require("wingman.perf")

-- 预加载图像到缓存
perf.preloadImage("assets/target.png")
perf.preloadImage("assets/button.png")
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
local perf = require("wingman.perf")

-- 清理图像缓存
perf.clearCache()
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
  - `cached_count` / `cached_count` - 已缓存图像数量
  - `total_size` / `total_size` - 总缓存大小（字节）
  - `hits` / `hits` - 缓存命中次数
  - `misses` / `misses` - 缓存未命中次数
  - `hit_rate` / `hit_rate` - 命中率（0-1）

:::tabs

== Python

```python:line-numbers
from wingman import perf

# 获取缓存统计
stats = perf.get_cache_stats()
print(f"已缓存: {stats['cached_count']} 张")
print(f"命中次数: {stats['hits']}")
print(f"未命中次数: {stats['misses']}")
```

== Lua

```lua:line-numbers
local perf = require("wingman.perf")

-- 获取缓存统计
local stats = perf.getCacheStats()
print("已缓存: " .. stats.cached_count .. " 张")
print("命中次数: " .. stats.hits)
print("未命中次数: " .. stats.misses)
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
| `get_cache_stats()` | `getCacheStats()` | 获取缓存统计 | 返回: 统计对象 |

---

## 配置选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enableImageCache` | boolean | `true` | 是否启用图像缓存 |
| `maxCacheSize` | number | `100` | 最大缓存大小（MB） |
| `enableParallelProcessing` | boolean | `true` | 是否启用并行处理 |
| `numThreads` | number | `4` | 线程数 |
