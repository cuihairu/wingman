# API: wingman.remote

远程控制模块，支持 TCP Server/Client 模式。

## Server 模式

### 启动服务器

::: code-group

```python [Python]
from wingman import remote

ok = remote.start(8888)
if ok:
    print("Server started on port 8888")
```

```lua [Lua]
local remote = require("wingman.remote")

local ok = remote.start(8888)
if ok then
    print("Server started on port 8888")
end
```

:::

### 注册事件处理器

::: code-group

```python [Python]
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

```lua [Lua]
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

### 停止服务器

::: code-group

```python [Python]
from wingman import remote

remote.stop()
```

```lua [Lua]
local remote = require("wingman.remote")

remote.stop()
```

:::

### 广播消息

::: code-group

```python [Python]
from wingman import remote

remote.broadcast({
    "status": "ok",
    "message": "Broadcast message"
})
```

```lua [Lua]
local remote = require("wingman.remote")

remote.broadcast({
    status = "ok",
    message = "Broadcast message"
})
```

:::

## Client 模式

### 连接服务器

::: code-group

```python [Python]
from wingman import remote

client = remote.connect("192.168.1.100", 8888)
```

```lua [Lua]
local remote = require("wingman.remote")

local client = remote.connect("192.168.1.100", 8888)
```

:::

### 发送请求

::: code-group

```python [Python]
from wingman import remote

client = remote.connect("192.168.1.100", 8888)

response = client.send({
    "type": "ping",
    "id": "1",
    "data": {}
})

print(response['message'])
```

```lua [Lua]
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

### 断开连接

::: code-group

```python [Python]
from wingman import remote

client.disconnect()
```

```lua [Lua]
local remote = require("wingman.remote")

client:disconnect()
```

:::

---

## 可用接口

### Server 模式

#### `start(port)`

启动 TCP 服务器。

**参数：**
- `port` (number) - 监听端口

**返回：**
- `success` (boolean) - 是否启动成功

#### `on(event, handler)`

注册事件处理器。

**参数：**
- `event` (string) - 事件名称
- `handler` (function) - 处理函数

**事件类型：**
- `"execute_script"` / `"executeScript"` - 执行脚本
- `"stop_script"` / `"stopScript"` - 停止脚本
- `"get_status"` / `"getStatus"` - 获取状态
- `"ping"` - 心跳
- `"screenshot"` - 截图
- `"mouse_move"` / `"mouseMove"` - 鼠标移动
- `"mouse_click"` / `"mouseClick"` - 鼠标点击
- `"key_press"` / `"keyPress"` - 按键

#### `stop()`

停止服务器。

#### `broadcast(data)`

向所有连接的客户端广播消息。

### Client 模式

#### `connect(host, port)`

连接到远程服务器。

**参数：**
- `host` (string) - 服务器地址
- `port` (number) - 服务器端口

**返回：**
- `client` (object/table) - 客户端对象

#### `client.send(request)` / `client:send(request)`

发送请求到服务器。

#### `client.disconnect()` / `client:disconnect()`

断开连接。

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
