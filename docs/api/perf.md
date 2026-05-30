# Performance API

`wingman.perf` 提供性能优化配置，包括图像缓存、并行处理等功能。

## 配置性能选项

:::tabs

== Python

```python
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

```lua
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

## 获取当前配置

:::tabs

== Python

```python
from wingman import perf

# 获取配置
config = perf.get_config()
print(f"图像缓存: {config['enableImageCache']}")
print(f"最大缓存: {config['maxCacheSize']} MB")
```

== Lua

```lua
local perf = require("wingman.perf")

-- 获取配置
local config = perf.getConfig()
print("图像缓存: " .. tostring(config.enableImageCache))
print("最大缓存: " .. config.maxCacheSize .. " MB")
```

:::

## 预加载图像

:::tabs

== Python

```python
from wingman import perf

# 预加载图像到缓存
perf.preload_image("assets/target.png")
perf.preload_image("assets/button.png")
```

== Lua

```lua
local perf = require("wingman.perf")

-- 预加载图像到缓存
perf.preloadImage("assets/target.png")
perf.preloadImage("assets/button.png")
```

:::

## 清理缓存

:::tabs

== Python

```python
from wingman import perf

# 清理图像缓存
perf.clear_cache()
```

== Lua

```lua
local perf = require("wingman.perf")

-- 清理图像缓存
perf.clearCache()
```

:::

## 获取缓存统计

:::tabs

== Python

```python
from wingman import perf

# 获取缓存统计
stats = perf.get_cache_stats()
print(f"已缓存: {stats['cached_count']} 张")
print(f"命中次数: {stats['hits']}")
print(f"未命中次数: {stats['misses']}")
```

== Lua

```lua
local perf = require("wingman.perf")

-- 获取缓存统计
local stats = perf.getCacheStats()
print("已缓存: " .. stats.cached_count .. " 张")
print("命中次数: " .. stats.hits)
print("未命中次数: " .. stats.misses)
```

:::

---

## 完整示例

### 游戏脚本性能优化

:::tabs

== Python

```python
from wingman import perf, screen, vision

# 启用图像缓存和并行处理
perf.set_config({
    "enableImageCache": True,
    "maxCacheSize": 200,
    "enableParallelProcessing": True,
    "numThreads": 8
})

# 预加载常用图像
perf.preload_image("ui/enemy.png")
perf.preload_image("ui/ally.png")
perf.preload_image("ui/loot.png")

# 主循环
while True:
    img = screen.capture(0, 0, 1920, 1080)
    enemies = vision.find_all_images(img, "ui/enemy.png")
    # 使用缓存的图像匹配会更快
```

== Lua

```lua
local perf = require("wingman.perf")
local screen = require("wingman.screen")
local vision = require("wingman.vision")

-- 启用图像缓存和并行处理
perf.setConfig({
    enableImageCache = true,
    maxCacheSize = 200,
    enableParallelProcessing = true,
    numThreads = 8
})

-- 预加载常用图像
perf.preloadImage("ui/enemy.png")
perf.preloadImage("ui/ally.png")
perf.preloadImage("ui/loot.png")

-- 主循环
while true do
    local img = screen.capture(0, 0, 1920, 1080)
    local enemies = vision.findAllImages(img, "ui/enemy.png")
    -- 使用缓存的图像匹配会更快
end
```

:::

---

## 可用接口

### `set_config(config)` / `setConfig(config)`

设置性能配置。

**参数：**
- `config` - 配置对象
  - `enableImageCache` - 是否启用图像缓存，默认 `true`
  - `maxCacheSize` - 最大缓存大小（MB），默认 `100`
  - `enableParallelProcessing` - 是否启用并行处理，默认 `true`
  - `numThreads` - 线程数，默认 `4`

**返回：**
- `nil`

### `get_config()` / `getConfig()`

获取当前性能配置。

**返回：**
- `dict/table` - 配置对象

### `preload_image(path)` / `preloadImage(path)`

预加载图像到缓存。

**参数：**
- `path` - 图像文件路径

**返回：**
- `nil`

### `clear_cache()` / `clearCache()`

清理图像缓存。

**返回：**
- `nil`

### `get_cache_stats()` / `getCacheStats()`

获取缓存统计信息。

**返回：**
- `dict/table` - 统计对象
  - `cached_count` - 已缓存图像数量
  - `total_size` - 总缓存大小（字节）
  - `hits` - 缓存命中次数
  - `misses` - 缓存未命中次数
  - `hit_rate` - 命中率（0-1）
