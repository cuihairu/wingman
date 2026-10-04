# API: wingman.hotkey

全局热键模块，轮询式监听全局键态，主键与修饰键同时按下（上升沿）时触发回调。

## 模块概述

hotkey 模块提供以下能力：

- **注册全局热键** - 组合键文本 + 回调，返回注册 ID
- **注销热键** - 按 ID 注销

**实现要点**：

- **轮询式 v1**：后台线程按固定间隔（默认 30ms）读全局键盘状态，按上升沿触发一次回调（`Ctrl+Shift+A` 风格组合键，大小写不敏感、修饰键顺序不限）。
- **线程约束**：回调从后台轮询线程触发，**必须是线程安全 callable**（Python 函数可以；Lua callable 非线程安全，会被拒绝并发出 `hotkey.error` 事件——Lua 侧请改用事件/触发器）。
- **平台支持**：Windows（`GetAsyncKeyState`）/ Linux X11（`XQueryKeymap`）/ macOS（`CGEventSourceKeyState`，需辅助功能权限，真实行为待真机验证）。
- **v1 已知限制**：按键在两次轮询间隔内按下又弹起（<30ms）可能漏检。
- **生命周期**：首个注册自动启动轮询线程；注销清空后线程自查退出（下次注册自动重启）。

---

## register(combo, callback)

### register(combo, callback) / register(combo, callback)

**说明**：注册全局热键。组合键解析失败、回调为空或回调非线程安全时返回 `0`。

**函数签名**：

```python
register(combo: str, callback: Callable[[], None]) -> int
```

```lua
-- Lua callable 非线程安全，注册会被拒绝返回 0 并发出 hotkey.error 事件
register(combo: string, callback: function) -> int
```

**参数**：
- `combo` - 组合键文本：`修饰键+…+主键`，如 `"Ctrl+Shift+A"`、`"Alt+F12"`、`"F9"`（仅主键也允许）。修饰键 `Ctrl`/`Shift`/`Alt` 可任意顺序、大小写不敏感；主键支持字母 `A-Z`、数字 `0-9`、`F1-F12`、`SPACE`/`ENTER`/`ESC`/`TAB`/`BACKSPACE`/`DELETE`/`INSERT` 等命名键。
- `callback` - 触发回调，无参数。

**返回**：
- `int` - 注册 ID（`0` = 失败）

:::tabs

== Python

```python:line-numbers
from wingman import hotkey

def on_combo():
    print("hotkey pressed")

hid = hotkey.register("Ctrl+Shift+A", on_combo)
if hid == 0:
    print("注册失败（组合键非法或回调不可用）")
```

== Lua

```lua:line-numbers
-- Lua callable 非线程安全，hotkey.register 在 Lua 侧会被拒绝（返回 0）
-- 并发出 hotkey.error 事件；全局热键请用 Python 脚本注册。
```

:::

**事件**：注册被拒绝（非线程安全 callable）时发出 `hotkey.error`，载荷 `{ "error": "..." }`。

---

## unregister(id)

### unregister(id) / unregister(id)

**说明**：按注册 ID 注销热键。ID 不存在返回 `false`。

**函数签名**：

```python
unregister(id: int) -> bool
```

```lua
unregister(id: int) -> boolean
```

**参数**：
- `id` - `register` 返回的注册 ID

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import hotkey

hid = hotkey.register("Ctrl+Shift+A", lambda: print("hi"))
hotkey.unregister(hid)
```

== Lua

```lua:line-numbers
-- 见 register 说明：Lua 侧无法注册，故 unregister 通常无用武之地
```

:::

---

## 完整示例

### Python

```python
from wingman import hotkey

def toggle_farm():
    # 切换任务状态（示例）
    print("toggle")

hid = hotkey.register("F9", toggle_farm)
if hid == 0:
    print("hotkey.error: 组合键非法或回调非线程安全")

# 脚本收尾时注销
hotkey.unregister(hid)
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `register(combo, callback)` | `register(combo, callback)` | 注册全局热键（Lua 侧拒绝，见模块概述） | combo: 组合键文本<br>callback: 触发回调（须线程安全） |
| `unregister(id)` | `unregister(id)` | 按 ID 注销 | id: register 返回的 ID |
