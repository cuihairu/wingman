# API: wingman.http

HTTP 客户端，支持 GET/POST/PUT/DELETE 请求和表单提交。

## GET 请求

:::tabs

== Python

```python
from wingman import http

resp = http.get("https://api.example.com/data", {
    "timeout": 10,
    "headers": {"Authorization": "Bearer token"}
})

if resp["success"]:
    print(resp["body"])
else:
    print(f"Error: {resp['status']}")
```

== Lua

```lua
local http = require("wingman.http")

local resp = http.get("https://api.example.com/data", {
    timeout = 10,
    headers = {["Authorization"] = "Bearer token"}
})

if resp.success then
    print(resp.body)
else
    print("Error:", resp.status)
end
```

:::

## POST 请求（JSON）

:::tabs

== Python

```python
from wingman import http

resp = http.post("https://api.example.com/submit", {
    "name": "Player1",
    "score": 100
}, {
    "headers": {"Content-Type": "application/json"}
})
```

== Lua

```lua
local http = require("wingman.http")
local json = require("wingman.json")

local resp = http.post("https://api.example.com/submit", json.encode({
    name = "Player1",
    score = 100
}), {
    headers = {["Content-Type"] = "application/json"}
})
```

:::

## POST 表单

:::tabs

== Python

```python
from wingman import http

resp = http.post_form("https://example.com/login", {
    "username": "user",
    "password": "pass"
})
```

== Lua

```lua
local http = require("wingman.http")

local resp = http.postForm("https://example.com/login", {
    username = "user",
    password = "pass"
})
```

:::

## PUT 请求

:::tabs

== Python

```python
from wingman import http

resp = http.put("https://api.example.com/update", {
    "id": 123,
    "status": "active"
})
```

== Lua

```lua
local http = require("wingman.http")

local resp = http.put("https://api.example.com/update", json.encode({
    id = 123,
    status = "active"
}))
```

:::

## DELETE 请求

:::tabs

== Python

```python
from wingman import http

resp = http.delete("https://api.example.com/items/123")
```

== Lua

```lua
local http = require("wingman.http")

local resp = http.delete("https://api.example.com/items/123")
```

:::

---

## 可用接口

### `get(url, options?)`

发送 GET 请求。

**参数：**
- `url` (string) - 请求 URL
- `options` (dict/table, 可选) - 请求选项
  - `timeout` (number) - 超时时间（秒），默认 30
  - `headers` (dict/table) - 请求头
  - `followRedirects` (boolean) - 是否跟随重定向，默认 true
  - `maxRedirects` (number) - 最大重定向次数，默认 5

**返回：**
- `response` (dict/table) - 响应对象
  - `status` (number) - HTTP 状态码
  - `body` (string) - 响应体
  - `headers` (dict/table) - 响应头
  - `elapsed` (number) - 请求耗时（秒）
  - `success` (boolean) - 是否成功 (2xx 状态码)

### `post(url, body, options?)`

发送 POST 请求（JSON）。

**参数：**
- `url` (string) - 请求 URL
- `body` (dict/string) - JSON 请求体（Python 自动序列化，Lua 需用 json.encode）
- `options` (dict/table, 可选) - 请求选项

**返回：**
- `response` (dict/table) - 响应对象

### `post_form(url, fields, options?)`

发送表单 POST 请求。

**参数：**
- `url` (string) - 请求 URL
- `fields` (dict/table) - 表单字段
- `options` (dict/table, 可选) - 请求选项

### `put(url, body, options?)`

发送 PUT 请求。

### `delete(url, options?)`

发送 DELETE 请求。
