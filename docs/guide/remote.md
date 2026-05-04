# 远程控制

Wingman 提供了内置的网络层，支持远程控制和统一调度。

## 工作模式

### Server 模式

Server 模式下，Wingman 监听指定端口，等待外部连接：

```cpp
#include "wingman/server/server.hpp"

asio::io_context ioContext;
wingman::server::Server server(ioContext, 8888);

// 注册处理器
server.setHandler(RequestType::kExecuteScript, [](const Request& req) {
    // 执行脚本
    Response response;
    response.status = ResponseStatus::kOk;
    response.message = "Script executed";
    return response;
});

server.start();
ioContext.run();
```

### Client 模式

Client 模式下，Wingman 主动连接到远程服务器：

```cpp
#include "wingman/server/client.hpp"

asio::io_context ioContext;
wingman::server::Client client(ioContext);

// 连接到服务器
auto connected = client.connect("192.168.1.100", 8888).get();

if (connected) {
    // 发送请求
    Request request;
    request.type = RequestType::kPing;
    request.id = "1";

    auto response = client.sendSync(request).get();
    std::cout << response.message << std::endl;
}
```

## 协议格式

通信协议使用 JSON 格式，带有长度前缀：

```
<length_hex>\n
<json>\n
```

### 请求格式

```json
{
  "type": "execute_script|stop_script|get_status|ping|screenshot|mouse_move|mouse_click|key_press",
  "id": "唯一请求ID",
  "data": {}
}
```

### 响应格式

```json
{
  "requestId": "对应的请求ID",
  "status": "ok|error|not_found",
  "message": "状态消息",
  "data": {}
}
```

## API 类型

| 类型 | 说明 |
|-----|------|
| `execute_script` | 执行 Lua 脚本 |
| `stop_script` | 停止当前脚本 |
| `get_status` | 获取运行状态 |
| `ping` | 心跳检测 |
| `screenshot` | 截取屏幕 |
| `mouse_move` | 移动鼠标 |
| `mouse_click` | 点击鼠标 |
| `key_press` | 按下按键 |

## Lua 脚本控制

### 启动 Server

```lua
local wingman = require("wingman")

-- 启动服务器
wingman.server.start(8888)

-- 注册处理器
wingman.server.on("execute_script", function(req)
    local success, err = pcall(function()
        load(req.data.script)()
    end)

    if success then
        return { status = "ok", message = "Script executed" }
    else
        return { status = "error", message = err }
    end
end)
```

### 连接到远程

```lua
local wingman = require("wingman")

-- 连接到服务器
local client = wingman.client.connect("192.168.1.100", 8888)

-- 发送请求
local response = client:send({
    type = "ping",
    id = "1",
    data = {}
})

print(response.message)
```

## 安全性

- 通信基于 TCP，建议在内网或 VPN 环境使用
- 不内置加密，如需安全传输建议使用 SSL/TLS 代理
- 可以通过防火墙限制访问来源
