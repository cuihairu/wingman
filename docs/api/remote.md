# wingman.remote

远程控制模块，支持 TCP Server/Client 模式。

## Server 模式

### wingman.remote.start(port)

启动 TCP 服务器。

**参数：**
- `port` (number) - 监听端口

**返回：**
- `success` (boolean) - 是否启动成功

**示例：**
```lua
local ok = wingman.remote.start(8888)
if ok then
    print("Server started on port 8888")
end
```

### wingman.remote.on(event, handler)

注册事件处理器。

**参数：**
- `event` (string) - 事件名称
- `handler` (function) - 处理函数

**事件类型：**
- `"execute_script"` - 执行脚本
- `"stop_script"` - 停止脚本
- `"get_status"` - 获取状态
- `"ping"` - 心跳
- `"screenshot"` - 截图
- `"mouse_move"` - 鼠标移动
- `"mouse_click"` - 鼠标点击
- `"key_press"` - 按键

**示例：**
```lua
wingman.remote.on("execute_script", function(req)
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

### wingman.remote.stop()

停止服务器。

**示例：**
```lua
wingman.remote.stop()
```

### wingman.remote.broadcast(data)

向所有连接的客户端广播消息。

**参数：**
- `data` (table) - 要广播的数据

**示例：**
```lua
wingman.remote.broadcast({
    status = "ok",
    message = "Broadcast message"
})
```

## Client 模式

### wingman.remote.connect(host, port)

连接到远程服务器。

**参数：**
- `host` (string) - 服务器地址
- `port` (number) - 服务器端口

**返回：**
- `client` (table) - 客户端对象

**示例：**
```lua
local client = wingman.remote.connect("192.168.1.100", 8888)
```

### client:send(request)

发送请求到服务器。

**参数：**
- `request` (table) - 请求对象

**返回：**
- `response` (table) - 响应对象

**示例：**
```lua
local response = client:send({
    type = "ping",
    id = "1",
    data = {}
})

print(response.message)
```

### client:disconnect()

断开连接。

**示例：**
```lua
client:disconnect()
```

## 数据格式

### 请求格式

```lua
{
    type = "execute_script|stop_script|get_status|ping|...",
    id = "唯一ID",
    data = {
        -- 请求数据
    }
}
```

### 响应格式

```lua
{
    requestId = "对应的请求ID",
    status = "ok|error|not_found",
    message = "状态消息",
    data = {
        -- 响应数据
    }
}
```
