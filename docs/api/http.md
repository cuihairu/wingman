# API: wingman.http

HTTP 客户端模块，支持 GET、POST、PUT、DELETE 等常见的 HTTP 请求方法。

## 模块概述

http 模块提供了完整的 HTTP 客户端功能，用于发送网络请求。主要功能包括：

- **HTTP 方法**：支持 GET、POST、PUT、DELETE、PATCH 等请求方法
- **请求选项**：支持设置超时时间、请求头、代理等
- **响应处理**：自动解析响应，返回统一格式的响应对象
- **文件下载**：支持下载文件到本地

### 响应格式

所有请求方法都返回一个响应对象（字典/表格），包含以下字段：

| 字段 | 类型 | 说明 |
|-----|------|-----|
| `success` | bool | 请求是否成功（状态码 2xx） |
| `status` | int | HTTP 状态码（如 200、404、500） |
| `body` | string | 响应体内容 |
| `headers` | dict/table | 响应头 |

---

## GET 请求

### get(url, options?)

**说明**：发送 GET 请求，用于获取资源。

**函数签名**：

```python
get(url: str, options: dict = None) -> dict
```

```lua
get(url: string, options: table | nil) -> table
```

**参数**：
- `url` - 请求的 URL 地址
- `options` - 可选，请求选项：
  - `timeout` - 超时时间（秒），默认 30
  - `headers` - 请求头字典/表格
  - `params` - URL 查询参数（字典/表格）
  - `proxy` - 代理服务器地址

**返回**：响应对象（包含 success、status、body、headers 字段）

:::tabs

== Python

```python:line-numbers
from wingman import http

# 基本请求
resp = http.get("https://api.example.com/data")
if resp["success"]:
    print(resp["body"])

# 带选项的请求
resp = http.get("https://api.example.com/data", {
    "timeout": 10,
    "headers": {"Authorization": "Bearer token"}
})

# 带查询参数
resp = http.get("https://api.example.com/search", {
    "params": {"q": "wingman", "page": 1}
})

if resp["success"]:
    print(f"状态码: {resp['status']}")
    print(f"响应内容: {resp['body']}")
else:
    print(f"请求失败: {resp['status']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 基本请求
local resp = wingman.http.get("https://api.example.com/data")
if resp.success then
    print(resp.body)
end

-- 带选项的请求
local resp = wingman.http.get("https://api.example.com/data", {
    timeout = 10,
    headers = {["Authorization"] = "Bearer token"}
})

-- 带查询参数
local resp = wingman.http.get("https://api.example.com/search", {
    params = {q = "wingman", page = 1}
})

if resp.success then
    print("状态码:", resp.status)
    print("响应内容:", resp.body)
else
    print("请求失败:", resp.status)
end
```

:::

---

## POST 请求（JSON）

### post(url, data, options?)

**说明**：发送 POST 请求，通常用于提交数据。数据默认以 JSON 格式发送。

**函数签名**：

```python
post(url: str, data: dict, options: dict = None) -> dict
```

```lua
post(url: string, data: string | table, options: table | nil) -> table
```

**参数**：
- `url` - 请求的 URL 地址
- `data` - 要发送的数据（Python 使用字典，Lua 使用 JSON 字符串）
- `options` - 可选，请求选项：
  - `timeout` - 超时时间（秒），默认 30
  - `headers` - 请求头字典/表格
  - `json` - 是否使用 JSON 序列化（Python 默认 true）

**返回**：响应对象

:::tabs

== Python

```python:line-numbers
from wingman import http

# 发送 JSON 数据
resp = http.post("https://api.example.com/submit", {
    "name": "Player1",
    "score": 100,
    "level": 5
})

if resp["success"]:
    print("提交成功")
    print(f"响应: {resp['body']}")

# 带自定义请求头
resp = http.post("https://api.example.com/submit", {
    "name": "Player1"
}, {
    "headers": {
        "Content-Type": "application/json",
        "X-Custom-Header": "value"
    }
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 发送 JSON 数据（需要手动序列化）
local resp = wingman.http.post("https://api.example.com/submit", wingman.json.encode({
    name = "Player1",
    score = 100,
    level = 5
}), {
    headers = {["Content-Type"] = "application/json"}
})

if resp.success then
    print("提交成功")
    print("响应:", resp.body)
end
```

:::

---

## POST 表单

### post_form(url, form_data, options?) / postForm(url, formData, options?)

**说明**：发送表单格式的 POST 请求（Content-Type: application/x-www-form-urlencoded）。

**函数签名**：

```python
post_form(url: str, form_data: dict, options: dict = None) -> dict
```

```lua
postForm(url: string, formData: table, options: table | nil) -> table
```

**参数**：
- `url` - 请求的 URL 地址
- `form_data` / `formData` - 表单数据字典/表格
- `options` - 可选，请求选项

**返回**：响应对象

**使用场景**：
- 登录表单提交
- 搜索表单提交

:::tabs

== Python

```python:line-numbers
from wingman import http

# 提交登录表单
resp = http.post_form("https://example.com/login", {
    "username": "user",
    "password": "pass",
    "remember": "1"
})

if resp["success"]:
    print("登录成功")
else:
    print(f"登录失败: {resp['status']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 提交登录表单
local resp = wingman.http.postForm("https://example.com/login", {
    username = "user",
    password = "pass",
    remember = "1"
})

if resp.success then
    print("登录成功")
else
    print("登录失败:", resp.status)
end
```

:::

---

## PUT 请求

### put(url, data, options?)

**说明**：发送 PUT 请求，通常用于更新资源。

**函数签名**：

```python
put(url: str, data: dict, options: dict = None) -> dict
```

```lua
put(url: string, data: string | table, options: table | nil) -> table
```

:::tabs

== Python

```python:line-numbers
from wingman import http

# 更新用户信息
resp = http.put("https://api.example.com/user/123", {
    "name": "NewName",
    "email": "new@example.com"
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local resp = wingman.http.put("https://api.example.com/user/123", wingman.json.encode({
    name = "NewName",
    email = "new@example.com"
}), {
    headers = {["Content-Type"] = "application/json"}
})
```

:::

---

## DELETE 请求

### delete(url, options?)

**说明**：发送 DELETE 请求，用于删除资源。

**函数签名**：

```python
delete(url: str, options: dict = None) -> dict
```

```lua
delete(url: string, options: table | nil) -> table
```

:::tabs

== Python

```python:line-numbers
from wingman import http

# 删除资源
resp = http.delete("https://api.example.com/user/123")
if resp["success"]:
    print("删除成功")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 删除资源
local resp = wingman.http.delete("https://api.example.com/user/123")
if resp.success then
    print("删除成功")
end
```

:::

---

## 文件下载

### download(url, file_path, options?) / download(url, filePath, options?)

**说明**：下载文件并保存到本地。

**函数签名**：

```python
download(url: str, file_path: str, options: dict = None) -> bool
```

```lua
download(url: string, filePath: string, options: table | nil) -> boolean
```

**参数**：
- `url` - 文件的 URL 地址
- `file_path` / `filePath` - 本地保存路径
- `options` - 可选，请求选项（timeout、headers 等）

**返回**：下载是否成功

:::tabs

== Python

```python:line-numbers
from wingman import http

# 下载图片
success = http.download(
    "https://example.com/image.png",
    "C:/images/image.png"
)
if success:
    print("下载成功")

# 下载文件（带超时设置）
success = http.download(
    "https://example.com/large-file.zip",
    "C:/downloads/file.zip",
    {"timeout": 300}
)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 下载图片
local success = wingman.http.download(
    "https://example.com/image.png",
    "C:/images/image.png"
)
if success then
    print("下载成功")
end

-- 下载文件（带超时设置）
local success = wingman.http.download(
    "https://example.com/large-file.zip",
    "C:/downloads/file.zip",
    {timeout = 300}
)
```

:::

---

## 完整示例

### API 请求封装

这个示例展示了如何封装一个 API 客户端类：

:::tabs

== Python

```python:line-numbers
from wingman import http

class APIClient:
    """API 客户端示例"""

    def __init__(self, base_url, api_key):
        self.base_url = base_url
        self.headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json"
        }

    def get(self, endpoint):
        """发送 GET 请求"""
        url = f"{self.base_url}/{endpoint}"
        resp = http.get(url, {"headers": self.headers})
        return resp

    def post(self, endpoint, data):
        """发送 POST 请求"""
        url = f"{self.base_url}/{endpoint}"
        resp = http.post(url, data, {"headers": self.headers})
        return resp

# 使用示例
client = APIClient("https://api.example.com/v1", "your-api-key")

# 获取用户信息
user = client.get("user/123")
if user["success"]:
    print(f"用户信息: {user['body']}")

# 创建用户
new_user = client.post("users", {
    "name": "Player1",
    "score": 100
})
if new_user["success"]:
    print(f"用户创建成功: {new_user['body']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local APIClient = {}
APIClient.__index = APIClient

function APIClient.new(baseUrl, apiKey)
    local self = setmetatable({}, APIClient)
    self.baseUrl = baseUrl
    self.headers = {
        ["Authorization"] = "Bearer " .. apiKey,
        ["Content-Type"] = "application/json"
    }
    return self
end

function APIClient:get(endpoint)
    local url = self.baseUrl .. "/" .. endpoint
    return wingman.http.get(url, {headers = self.headers})
end

function APIClient:post(endpoint, data)
    local url = self.baseUrl .. "/" .. endpoint
    return wingman.http.post(url, wingman.json.encode(data), {headers = self.headers})
end

-- 使用示例
local client = APIClient.new("https://api.example.com/v1", "your-api-key")

-- 获取用户信息
local user = client:get("user/123")
if user.success then
    print("用户信息:", user.body)
end

-- 创建用户
local newUser = client:post("users", {
    name = "Player1",
    score = 100
})
if newUser.success then
    print("用户创建成功:", newUser.body)
end
```

:::

---

## 可用接口

### 请求方法

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get(url, opts?)` | `get(url, opts?)` | GET 请求 | url: 请求地址<br>opts: 请求选项 |
| `post(url, data, opts?)` | `post(url, data, opts?)` | POST 请求 | url: 请求地址<br>data: 请求数据<br>opts: 请求选项 |
| `put(url, data, opts?)` | `put(url, data, opts?)` | PUT 请求 | 同 POST |
| `delete(url, opts?)` | `delete(url, opts?)` | DELETE 请求 | url: 请求地址<br>opts: 请求选项 |
| `post_form(url, data, opts?)` | `postForm(url, data, opts?)` | 表单 POST | url: 请求地址<br>data: 表单数据<br>opts: 请求选项 |
| `download(url, path, opts?)` | `download(url, path, opts?)` | 下载文件 | url: 文件地址<br>path: 保存路径<br>opts: 请求选项 |

### 请求选项

| 选项 | 类型 | 说明 | 默认值 |
|-----|------|-----|-------|
| `timeout` | int | 超时时间（秒） | 30 |
| `headers` | dict/table | 请求头 | {} |
| `params` | dict/table | URL 查询参数 | {} |
| `proxy` | string | 代理服务器地址 | nil |
