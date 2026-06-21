# API: wingman.transport

Transport 模块提供了 TCP 和 UDP 网络通信功能，基于 asio 实现异步网络操作。

:::warning 条件编译
transport 模块需要 `WINGMAN_HAS_TRANSPORT` 编译宏启用。未启用时，所有函数返回 `{success: false, error: "Transport module not enabled in this build"}`。
:::

## 模块概述

transport 模块在脚本层提供 TCP/UDP 网络能力。它与 runtime 内部用于连接 Go orchestrator 的编排通信层是**两个独立的通道**：

| 用途 | 层级 | 说明 |
|------|------|------|
| 脚本层网络 (本文档) | `wingman.transport` 模块 | 脚本中使用的 TCP 客户端/服务器、UDP socket |
| 编排通信 | `libs/transport` + `remote_client` | runtime 主动 outbound 连接 Go orchestrator，用于 agent 注册、心跳、命令下发 |

脚本层 transport 封装了底层 `libs/transport` 库的 TCP 功能，并直接基于 asio 实现 UDP 功能。主要功能包括：

- **TCP 客户端**：连接到远程 TCP 服务器，发送和接收消息
- **TCP 服务器**：监听端口，接受多个客户端连接
- **UDP Socket**：无连接的数据报通信
- **会话管理**：管理多个 TCP 连接会话，支持点对点和广播通信
- **异步 I/O**：基于 asio 的异步网络操作

### 消息格式

transport 模块使用自定义的消息协议：

```
MessageHeader (16 bytes) + Body
- length:     4 bytes (消息体长度)
- sequence:   4 bytes (序列号，用于请求-响应匹配)
- type:       1 byte  (消息类型：Request/Response/Notify/Error)
- reserved:   4 bytes (保留字段)
- body:       变长   (消息内容)
```

:::tip 注意
发送的消息会自动封装成上述格式，接收时会自动解析。脚本层只需关注消息体内容。
:::

---

## TCP 客户端

### tcpConnect(id?, host?, port?)

**说明**：创建 TCP 客户端并连接到远程服务器。

**函数签名**：

```python
tcp_connect(id: str = "default", host: str = "127.0.0.1", port: int = 8080) -> dict
```

```lua
tcpConnect(id: string, host: string, port: int) -> table
```

**参数**：
- `id` - 客户端标识符，默认 "default"
- `host` - 服务器地址，默认 "127.0.0.1"
- `port` - 服务器端口，默认 8080

**返回**：连接结果对象
- `success` - 是否连接成功
- `handle` - 客户端句柄（用于后续操作）
- `id` - 客户端 ID
- `error` - 错误信息（失败时）

:::tabs

== Python

```python:line-numbers
from wingman import transport

# 连接到服务器
result = transport.tcp_connect("myclient", "192.168.1.100", 9000)

if result["success"]:
    print(f"连接成功，句柄: {result['handle']}")
    handle = result["handle"]
else:
    print(f"连接失败: {result['error']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 连接到服务器
local result = wingman.transport.tcpConnect("myclient", "192.168.1.100", 9000)

if result.success then
    print("连接成功，句柄:", result.handle)
    handle = result.handle
else
    print("连接失败:", result.error)
end
```

:::

---

### tcpSend(handle, data)

**说明**：通过 TCP 客户端发送数据。

**函数签名**：

```python
tcp_send(handle: int, data: str) -> bool
```

```lua
tcpSend(handle: int, data: string) -> boolean
```

**参数**：
- `handle` - 客户端句柄（由 tcpConnect 返回）
- `data` - 要发送的数据

**返回**：是否发送成功

:::tabs

== Python

```python:line-numbers
from wingman import transport

# 连接
result = transport.tcp_connect("client1", "127.0.0.1", 8080)
handle = result["handle"]

# 发送消息
if transport.tcp_send(handle, "Hello, Server!"):
    print("发送成功")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 连接
local result = wingman.transport.tcpConnect("client1", "127.0.0.1", 8080)
local handle = result.handle

-- 发送消息
if wingman.transport.tcpSend(handle, "Hello, Server!") then
    print("发送成功")
end
```

:::

---

### tcpIsConnected(handle)

**说明**：检查 TCP 客户端是否仍连接。

**函数签名**：

```python
tcp_is_connected(handle: int) -> bool
```

```lua
tcpIsConnected(handle: int) -> boolean
```

**返回**：是否已连接

---

### tcpDisconnect(handle)

**说明**：断开 TCP 客户端连接并释放资源。

**函数签名**：

```python
tcp_disconnect(handle: int) -> bool
```

```lua
tcpDisconnect(handle: int) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import transport

handle = transport.tcp_connect("client1")["handle"]

# 使用完毕后断开
transport.tcp_disconnect(handle)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local handle = wingman.transport.tcpConnect("client1").handle

-- 使用完毕后断开
wingman.transport.tcpDisconnect(handle)
```

:::

---

## TCP 服务器

### tcpListen(id?, host?, port?)

**说明**：创建 TCP 服务器并开始监听端口。

**函数签名**：

```python
tcp_listen(id: str = "default", host: str = "0.0.0.0", port: int = 8080) -> dict
```

```lua
tcpListen(id: string, host: string, port: int) -> table
```

**参数**：
- `id` - 服务器标识符，默认 "default"
- `host` - 监听地址，默认 "0.0.0.0"（所有接口）
- `port` - 监听端口，默认 8080

**返回**：服务器结果对象
- `success` - 是否启动成功
- `handle` - 服务器句柄（用于后续操作）
- `id` - 服务器 ID
- `error` - 错误信息（失败时）

:::tabs

== Python

```python:line-numbers
from wingman import transport

# 启动 TCP 服务器
server = transport.tcp_listen("myserver", "0.0.0.0", 9000)

if server["success"]:
    print(f"服务器启动成功，句柄: {server['handle']}")
    server_handle = server["handle"]
else:
    print(f"启动失败: {server['error']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 启动 TCP 服务器
local server = wingman.transport.tcpListen("myserver", "0.0.0.0", 9000)

if server.success then
    print("服务器启动成功，句柄:", server.handle)
    server_handle = server.handle
else
    print("启动失败:", server.error)
end
```

:::

---

### tcpGetSessions(handle)

**说明**：获取服务器当前所有连接的会话 ID 列表。

**函数签名**：

```python
tcp_get_sessions(handle: int) -> list[int]
```

```lua
tcpGetSessions(handle: int) -> table
```

**返回**：会话 ID 数组

:::tabs

== Python

```python:line-numbers
from wingman import transport

server_handle = transport.tcp_listen("server1", "0.0.0.0", 9000)["handle"]

# 获取所有会话
sessions = transport.tcp_get_sessions(server_handle)
print(f"当前连接数: {len(sessions)}")

for session_id in sessions:
    print(f"会话 ID: {session_id}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server_handle = wingman.transport.tcpListen("server1", "0.0.0.0", 9000).handle

-- 获取所有会话
local sessions = wingman.transport.tcpGetSessions(server_handle)
print("当前连接数:", #sessions)

for i, session_id in ipairs(sessions) do
    print("会话 ID:", session_id)
end
```

:::

---

### tcpSendTo(handle, session_id, data)

**说明**：向指定的客户端会话发送数据。

**函数签名**：

```python
tcp_send_to(handle: int, session_id: int, data: str) -> bool
```

```lua
tcpSendTo(handle: int, sessionId: int, data: string) -> boolean
```

**参数**：
- `handle` - 服务器句柄
- `session_id` / `sessionId` - 目标会话 ID
- `data` - 要发送的数据

**返回**：是否发送成功

:::tabs

== Python

```python:line-numbers
from wingman import transport

server_handle = transport.tcp_listen("server1")["handle"]
sessions = transport.tcp_get_sessions(server_handle)

if sessions:
    # 向第一个客户端发送消息
    transport.tcp_send_to(server_handle, sessions[0], "Welcome!")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server_handle = wingman.transport.tcpListen("server1").handle
local sessions = wingman.transport.tcpGetSessions(server_handle)

if #sessions > 0 then
    -- 向第一个客户端发送消息
    wingman.transport.tcpSendTo(server_handle, sessions[1], "Welcome!")
end
```

:::

---

### tcpBroadcast(handle, data)

**说明**：向所有连接的客户端广播消息。

**函数签名**：

```python
tcp_broadcast(handle: int, data: str) -> bool
```

```lua
tcpBroadcast(handle: int, data: string) -> boolean
```

**参数**：
- `handle` - 服务器句柄
- `data` - 要广播的数据

**返回**：是否发送成功

:::tabs

== Python

```python:line-numbers
from wingman import transport

server_handle = transport.tcp_listen("server1")["handle"]

# 广播消息给所有客户端
transport.tcp_broadcast(server_handle, "Server announcement!")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server_handle = wingman.transport.tcpListen("server1").handle

-- 广播消息给所有客户端
wingman.transport.tcpBroadcast(server_handle, "Server announcement!")
```

:::

---

### tcpCloseSession(handle, session_id)

**说明**：关闭指定的客户端会话。

**函数签名**：

```python
tcp_close_session(handle: int, session_id: int) -> bool
```

```lua
tcpCloseSession(handle: int, sessionId: int) -> boolean
```

**参数**：
- `handle` - 服务器句柄
- `session_id` / `sessionId` - 要关闭的会话 ID

:::tabs

== Python

```python:line-numbers
from wingman import transport

server_handle = transport.tcp_listen("server1")["handle"]
sessions = transport.tcp_get_sessions(server_handle)

if sessions:
    # 关闭第一个客户端连接
    transport.tcp_close_session(server_handle, sessions[0])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server_handle = wingman.transport.tcpListen("server1").handle
local sessions = wingman.transport.tcpGetSessions(server_handle)

if #sessions > 0 then
    -- 关闭第一个客户端连接
    wingman.transport.tcpCloseSession(server_handle, sessions[1])
end
```

:::

---

### tcpStop(handle)

**说明**：停止 TCP 服务器，关闭所有客户端连接。

**函数签名**：

```python
tcp_stop(handle: int) -> bool
```

```lua
tcpStop(handle: int) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import transport

server_handle = transport.tcp_listen("server1")["handle"]

# 停止服务器
transport.tcp_stop(server_handle)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server_handle = wingman.transport.tcpListen("server1").handle

-- 停止服务器
wingman.transport.tcpStop(server_handle)
```

:::

---

## UDP Socket

UDP 提供无连接的数据报通信，适合低延迟、广播等场景。

### udpSocket(id?)

**说明**：创建 UDP socket。

**函数签名**：

```python
udp_socket(id: str = "default") -> dict
```

```lua
udpSocket(id: string) -> table
```

**返回**：socket 对象
- `success` - 是否创建成功
- `handle` - socket 句柄
- `id` - socket ID

---

### udpBind(handle, host, port)

**说明**：绑定 UDP socket 到本地地址和端口（接收数据前必须先绑定）。

**函数签名**：

```python
udp_bind(handle: int, host: str, port: int) -> dict
```

```lua
udpBind(handle: int, host: string, port: int) -> table
```

**参数**：
- `handle` - socket 句柄
- `host` - 绑定地址，通常 "0.0.0.0"（所有接口）
- `port` - 绑定端口

**返回**：绑定结果对象

---

### udpSendTo(handle, host, port, data)

**说明**：向指定地址和端口发送 UDP 数据报。

**函数签名**：

```python
udp_send_to(handle: int, host: str, port: int, data: str) -> bool
```

```lua
udpSendTo(handle: int, host: string, port: int, data: string) -> boolean
```

**参数**：
- `handle` - socket 句柄
- `host` - 目标主机地址
- `port` - 目标端口
- `data` - 要发送的数据

**返回**：是否发送成功

:::tabs

== Python

```python:line-numbers
from wingman import transport

# 创建 socket
sock = transport.udp_socket("myudp")
handle = sock["handle"]

# 发送数据到远程服务器
transport.udp_send_to(handle, "192.168.1.100", 53, "Hello UDP!")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建 socket
local sock = wingman.transport.udpSocket("myudp")
local handle = sock.handle

-- 发送数据到远程服务器
wingman.transport.udpSendTo(handle, "192.168.1.100", 53, "Hello UDP!")
```

:::

---

### udpRecvFrom(handle, timeout?)

**说明**：从 UDP socket 接收数据。

**函数签名**：

```python
udp_recv_from(handle: int, timeout: int = 5000) -> dict
```

```lua
udpRecvFrom(handle: int, timeout: int) -> table
```

**参数**：
- `handle` - socket 句柄
- `timeout` - 超时时间（毫秒），默认 5000

**返回**：
- `success` - 是否接收到数据
- `data` - 接收到的数据
- `error` - 错误信息（失败时）

---

### udpClose(handle)

**说明**：关闭 UDP socket。

**函数签名**：

```python
udp_close(handle: int) -> bool
```

```lua
udpClose(handle: int) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import transport

handle = transport.udp_socket("myudp")["handle"]

# 使用完毕
transport.udp_close(handle)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local handle = wingman.transport.udpSocket("myudp").handle

-- 使用完毕
wingman.transport.udpClose(handle)
```

:::

---

## 完整示例

### TCP Echo 服务器

这个示例展示了一个简单的 Echo 服务器：

:::tabs

== Python

```python:line-numbers
from wingman import transport
import time

# 启动服务器
server = transport.tcp_listen("echo_server", "0.0.0.0", 9000)
if not server["success"]:
    print("启动服务器失败")
    exit()

server_handle = server["handle"]
print("Echo 服务器启动在端口 9000")

# 运行 10 秒
for i in range(10):
    time.sleep(1)
    sessions = transport.tcp_get_sessions(server_handle)
    if sessions:
        print(f"当前有 {len(sessions)} 个连接")

# 清理
transport.tcp_stop(server_handle)
print("服务器已停止")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 启动服务器
local server = wingman.transport.tcpListen("echo_server", "0.0.0.0", 9000)
if not server.success then
    print("启动服务器失败")
    return
end

local server_handle = server.handle
print("Echo 服务器启动在端口 9000")

-- 运行 10 秒
for i = 1, 10 do
    os.execute("sleep 1")
    local sessions = wingman.transport.tcpGetSessions(server_handle)
    if #sessions > 0 then
        print("当前有", #sessions, "个连接")
    end
end

-- 清理
wingman.transport.tcpStop(server_handle)
print("服务器已停止")
```

:::

---

### TCP 客户端

:::tabs

== Python

```python:line-numbers
from wingman import transport

# 连接到服务器
client = transport.tcp_connect("myclient", "127.0.0.1", 9000)

if client["success"]:
    handle = client["handle"]
    print("连接成功")

    # 发送消息
    if transport.tcp_send(handle, "Hello from client"):
        print("消息已发送")

    # 检查连接状态
    if transport.tcp_is_connected(handle):
        print("仍然连接")

    # 断开连接
    transport.tcp_disconnect(handle)
else:
    print(f"连接失败: {client['error']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 连接到服务器
local client = wingman.transport.tcpConnect("myclient", "127.0.0.1", 9000)

if client.success then
    local handle = client.handle
    print("连接成功")

    -- 发送消息
    if wingman.transport.tcpSend(handle, "Hello from client") then
        print("消息已发送")
    end

    -- 检查连接状态
    if wingman.transport.tcpIsConnected(handle) then
        print("仍然连接")
    end

    -- 断开连接
    wingman.transport.tcpDisconnect(handle)
else
    print("连接失败:", client.error)
end
```

:::

---

## 可用接口

### 客户端函数

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `tcp_connect(id?, host?, port?)` | `tcpConnect(id?, host?, port?)` | 创建 TCP 客户端并连接 |
| `tcp_send(handle, data)` | `tcpSend(handle, data)` | 发送数据 |
| `tcp_is_connected(handle)` | `tcpIsConnected(handle)` | 检查连接状态 |
| `tcp_disconnect(handle)` | `tcpDisconnect(handle)` | 断开连接 |

### 服务器函数

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `tcp_listen(id?, host?, port?)` | `tcpListen(id?, host?, port?)` | 启动 TCP 服务器 |
| `tcp_get_sessions(handle)` | `tcpGetSessions(handle)` | 获取所有会话 |
| `tcp_send_to(handle, sid, data)` | `tcpSendTo(handle, sessionId, data)` | 发送给指定会话 |
| `tcp_broadcast(handle, data)` | `tcpBroadcast(handle, data)` | 广播给所有会话 |
| `tcp_close_session(handle, sid)` | `tcpCloseSession(handle, sessionId)` | 关闭指定会话 |
| `tcp_stop(handle)` | `tcpStop(handle)` | 停止服务器 |

### UDP 函数

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `udp_socket(id?)` | `udpSocket(id?)` | 创建 UDP socket |
| `udp_bind(handle, host, port)` | `udpBind(handle, host, port)` | 绑定到本地地址 |
| `udp_send_to(handle, host, port, data)` | `udpSendTo(handle, host, port, data)` | 发送数据报 |
| `udp_recv_from(handle, timeout?)` | `udpRecvFrom(handle, timeout?)` | 接收数据报 |
| `udp_close(handle)` | `udpClose(handle)` | 关闭 socket |

---

## 注意事项

1. **句柄管理**：
   - TCP 客户端：从 1 开始
   - TCP 服务器：从 1000 开始
   - UDP socket：从 10000 开始
2. **异步操作**：底层使用 asio 异步 I/O，脚本接口提供简化的同步 API
3. **消息格式**：TCP 数据会自动封装成 transport 协议格式；UDP 为原始数据报
4. **资源清理**：使用完毕后务必调用相应的 close 函数释放资源
5. **UDP 特性**：
   - 无连接协议，不需要预先建立连接
   - 可能丢包，不保证顺序
   - 接收前必须先调用 `udp_bind()` 绑定本地端口
   - 适合低延迟、广播、DNS 查询等场景
5. **线程安全**：所有函数都是线程安全的
