# API: wingman.remote

远程控制模块，支持 TCP Server/Client 模式。

## 模块概述

remote 模块提供远程控制功能：
- **Server 模式** - 启动 TCP 服务器、注册事件处理器、广播消息
- **Client 模式** - 连接服务器、发送请求、断开连接

---

## Server 模式

### start(port) / start(port)

**说明**：启动 TCP 服务器。

**函数签名**：

```python
start(port: int) -> bool
```

```lua
start(port: number) -> boolean
```

**参数**：
- `port` - 监听端口

**返回**：
- 是否启动成功

:::tabs

== Python

```python:line-numbers
from wingman import remote

ok = remote.start(8888)
if ok:
    print("Server started on port 8888")
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

local ok = remote.start(8888)
if ok then
    print("Server started on port 8888")
end
```

:::

---

## 注册事件处理器

### on(event, handler) / on(event, handler)

**说明**：注册事件处理器。

**函数签名**：

```python
on(event: str, handler: Callable) -> None
```

```lua
on(event: string, handler: function) -> nil
```

**参数**：
- `event` - 事件名称
- `handler` - 处理函数，接收请求对象，返回响应对象

**返回**：
- 无

**事件类型**：
- `execute_script` / `executeScript` - 执行脚本
- `stop_script` / `stopScript` - 停止脚本
- `get_status` / `getStatus` - 获取状态
- `ping` - 心跳
- `screenshot` - 截图
- `mouse_move` / `mouseMove` - 鼠标移动
- `mouse_click` / `mouseClick` - 鼠标点击
- `key_press` / `keyPress` - 按键

:::tabs

== Python

```python:line-numbers
from wingman import remote

def on_execute(req):
    script = req['data']['script']
    try:
        exec(script)
        return {"status": "ok", "message": "Executed"}
    except Exception as e:
        return {"status": "error", "message": str(e)}

remote.on("execute_script", on_execute)
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

remote.on("execute_script", function(req)
    local script = req.data.script
    local fn, err = load(script)
    if fn then
        fn()
        return { status = "ok", message = "Executed" }
    else
        return { status = "error", message = err }
    end
end)
```

:::

---

## 停止服务器

### stop() / stop()

**说明**：停止服务器。

**函数签名**：

```python
stop() -> None
```

```lua
stop() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import remote

remote.stop()
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

remote.stop()
```

:::

---

## 广播消息

### broadcast(data) / broadcast(data)

**说明**：向所有连接的客户端广播消息。

**函数签名**：

```python
broadcast(data: dict) -> None
```

```lua
broadcast(data: table) -> nil
```

**参数**：
- `data` - 要广播的数据

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import remote

remote.broadcast({
    "status": "ok",
    "message": "Broadcast message"
})
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

remote.broadcast({
    status = "ok",
    message = "Broadcast message"
})
```

:::

---

## Client 模式

### connect(host, port) / connect(host, port)

**说明**：连接到远程服务器。

**函数签名**：

```python
connect(host: str, port: int) -> Client
```

```lua
connect(host: string, port: number) -> table
```

**参数**：
- `host` - 服务器地址
- `port` - 服务器端口

**返回**：
- Python: 客户端对象
- Lua: 客户端表格

:::tabs

== Python

```python:line-numbers
from wingman import remote

client = remote.connect("192.168.1.100", 8888)
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

local client = remote.connect("192.168.1.100", 8888)
```

:::

---

## Client: 发送请求

### client.send(request) / client:send(request)

**说明**：发送请求到服务器。

**函数签名**：

```python
client.send(request: dict) -> dict
```

```lua
client:send(request: table) -> table
```

**参数**：
- `request` - 请求对象

**返回**：
- 响应对象

:::tabs

== Python

```python:line-numbers
from wingman import remote

client = remote.connect("192.168.1.100", 8888)

response = client.send({
    "type": "ping",
    "id": "1",
    "data": {}
})

print(response['message'])
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

local client = remote.connect("192.168.1.100", 8888)

local response = client:send({
    type = "ping",
    id = "1",
    data = {}
})

print(response.message)
```

:::

---

## Client: 断开连接

### client.disconnect() / client:disconnect()

**说明**：断开与服务器的连接。

**函数签名**：

```python
client.disconnect() -> None
```

```lua
client:disconnect() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import remote

client.disconnect()
```

== Lua

```lua:line-numbers
local remote = require("wingman.remote")

client:disconnect()
```

:::

---

## 可用接口

### Server 模式

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `start(port)` | `start(port)` | 启动服务器 | port: 监听端口<br>返回: 是否成功 |
| `on(event, handler)` | `on(event, handler)` | 注册事件处理器 | event: 事件名称<br>handler: 处理函数 |
| `stop()` | `stop()` | 停止服务器 | 无返回值 |
| `broadcast(data)` | `broadcast(data)` | 广播消息 | data: 广播数据 |

### Client 模式

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `connect(host, port)` | `connect(host, port)` | 连接服务器 | host: 服务器地址<br>port: 服务器端口<br>返回: 客户端对象 |
| `client.send(request)` | `client:send(request)` | 发送请求 | request: 请求对象<br>返回: 响应对象 |
| `client.disconnect()` | `client:disconnect()` | 断开连接 | 无返回值 |

---

## 数据格式

### 请求格式

```json
{
  "type": "execute_script|stop_script|get_status|ping|...",
  "id": "唯一ID",
  "data": {
    "请求数据"
  }
}
```

### 响应格式

```json
{
  "requestId": "对应的请求ID",
  "status": "ok|error|not_found",
  "message": "状态消息",
  "data": {
    "响应数据"
  }
}
```
