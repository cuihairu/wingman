# HTTP 模块

HTTP 客户端，支持 GET/POST/PUT/DELETE 请求和表单提交。

## Lua API

### http.get(url, options)

发送 GET 请求。

**参数：**
- `url` (string) - 请求 URL
- `options` (table, 可选) - 请求选项
  - `timeout` (number) - 超时时间（秒），默认 30
  - `headers` (table) - 请求头
  - `followRedirects` (boolean) - 是否跟随重定向，默认 true
  - `maxRedirects` (number) - 最大重定向次数，默认 5

**返回：**
- `response` (table) - 响应对象
  - `status` (number) - HTTP 状态码
  - `body` (string) - 响应体
  - `headers` (table) - 响应头
  - `elapsed` (number) - 请求耗时（秒）
  - `success` (boolean) - 是否成功 (2xx 状态码)

**示例：**
```lua
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

### http.post(url, body, options)

发送 POST 请求（JSON）。

**参数：**
- `url` (string) - 请求 URL
- `body` (string) - JSON 请求体
- `options` (table, 可选) - 请求选项

**返回：**
- `response` (table) - 响应对象

**示例：**
```lua
local resp = http.post("https://api.example.com/submit", json.encode({
    name = "Player1",
    score = 100
}), {
    headers = {["Content-Type"] = "application/json"}
})
```

### http.postForm(url, fields, options)

发送表单 POST 请求。

**参数：**
- `url` (string) - 请求 URL
- `fields` (table) - 表单字段
- `options` (table, 可选) - 请求选项

**示例：**
```lua
local resp = http.postForm("https://example.com/login", {
    username = "user",
    password = "pass"
})
```

### http.put(url, body, options)

发送 PUT 请求。

### http.del(url, options)

发送 DELETE 请求。
