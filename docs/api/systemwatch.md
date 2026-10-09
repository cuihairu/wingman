# API: wingman.systemwatch

统一系统事件源（进程/窗口两腿），轮询式监听进程与窗口快照变化，命中注册目标时触发回调。

## 模块概述

systemwatch 模块提供以下能力：

- **processWatch** - 观察进程启动/退出（按名称精确匹配或全部进程）
- **windowWatch** - 观察窗口打开/关闭/标题变化（按标题子串匹配或全部窗口）
- **unwatch** - 按注册 ID 注销观察
- **clearAll** - 清空全部观察
- **watchCount** - 查询当前观察数量（可按类别）

**实现要点**：

- **轮询式 v1**：后台线程按固定间隔（默认 500ms）快照 `Process::enumerate()` 与 `Window::enumerate()`，与上一拍做 diff → 首拍只建立基线不触发。
- **线程约束**：回调从后台轮询线程触发，**必须是线程安全 callable**（Python 函数可以；Lua callable 非线程安全，会被拒绝并发出 `systemwatch.error` 事件——Lua 侧请改用事件/触发器）。
- **事件面**：命中事件同时以 `systemwatch.process` / `systemwatch.window` 进 `wingman.event`（source `"systemwatch"`，载荷含 action/pid/name 或 action/handle/title）——同样从后台线程同步分发，`event.on` 脚本订阅同受线程安全约束。
- **v1 已知限制**：进程/窗口在两次轮询间隔内出现又消失（< 轮询间隔）可能漏检（与 hotkey 同型）；窗口事件按句柄归并，标题变化触发 `changed`。
- **生命周期**：首个注册自动启动轮询线程；注销清空后线程自查退出（下次注册自动重启）。
- **文件变化**：由 `wingman.filewatcher` 提供，不在本模块内。

---

## processWatch(name, callback)

### processWatch(name, callback) / process_watch(name, callback)

**说明**：观察进程。`name` 为空时观察全部进程；非空按进程名精确匹配（与 `process.find` 一致）。回调为空或非线程安全时返回 `0`。

**函数签名**：

```python
processWatch(name: str, callback: Callable[[dict], None]) -> int
```

**参数**：
- `name` - 进程名（如 `"sleep"`）；空串 = 全部进程
- `callback` - 变化回调，参数为载荷对象 `{ "action": "started"|"exited", "pid": int, "name": str }`

**返回**：
- `int` - 注册 ID（`0` = 失败）

:::tabs

== Python

```python:line-numbers
from wingman import systemwatch

def on_proc(ev):
    print(ev["action"], ev["pid"], ev["name"])

wid = systemwatch.processWatch("mygame", on_proc)
if wid == 0:
    print("注册失败（回调非线程安全）")
```

== Lua

```lua:line-numbers
-- Lua callable 非线程安全，systemwatch.processWatch 在 Lua 侧会被拒绝
-- （返回 0）并发出 systemwatch.error 事件；系统观察请用 Python 脚本。
```

:::

---

## windowWatch(title, callback)

### windowWatch(title, callback) / window_watch(title, callback)

**说明**：观察窗口。`title` 为空时观察全部窗口；非空按标题子串匹配（与 `window.find` 一致）。

**函数签名**：

```python
windowWatch(title: str, callback: Callable[[dict], None]) -> int
```

**参数**：
- `title` - 窗口标题子串；空串 = 全部窗口
- `callback` - 变化回调，参数为 `{ "action": "opened"|"closed"|"changed", "handle": int, "title": str }`（`changed` = 同句柄标题变化）

**返回**：
- `int` - 注册 ID（`0` = 失败）

---

## unwatch(id)

### unwatch(id) / unwatch(id)

**说明**：按注册 ID 注销观察（进程/窗口类别通用）。ID 不存在返回 `false`。

**函数签名**：

```python
unwatch(id: int) -> bool
```

---

## clearAll()

### clearAll() / clear_all()

**说明**：清空全部观察（幂等，空表时线程自查退出）。

**函数签名**：

```python
clearAll() -> None
```

---

## watchCount(kind?)

### watchCount(kind?) / watch_count(kind?)

**说明**：查询当前观察数量。`kind` 可选 `"process"` / `"window"`；缺省返回总数；未知类别返回 `0`。

**函数签名**：

```python
watchCount(kind: str | None = None) -> int
```

---

## 完整示例

```python
from wingman import systemwatch

def on_window(ev):
    if ev["action"] == "closed":
        print("目标窗口已关闭")

wid = systemwatch.windowWatch("记事本", on_window)

# 脚本收尾时清理
systemwatch.clearAll()
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `processWatch(name, callback)` | 同名 | 观察进程启动/退出（Lua 侧拒绝，见模块概述） | name: 进程名（空=全部）<br>callback: 回调（须线程安全） |
| `windowWatch(title, callback)` | 同名 | 观察窗口打开/关闭/标题变化 | title: 标题子串（空=全部）<br>callback: 回调（须线程安全） |
| `unwatch(id)` | `unwatch(id)` | 按 ID 注销 | id: 注册 ID |
| `clearAll()` | `clearAll()` | 清空全部观察 | - |
| `watchCount(kind?)` | `watchCount(kind?)` | 查询观察数量 | kind: "process"/"window" 可选 |

**事件**：

| 事件 | 载荷 | 说明 |
|-----|------|-----|
| `systemwatch.process` | `{action, pid, name}` | 进程 started/exited |
| `systemwatch.window` | `{action, handle, title}` | 窗口 opened/closed/changed |
| `systemwatch.error` | `{error}` | 注册被拒（非线程安全 callable） |
