# Lua HTTP Routes

Wingman 支持 Lua 定义 HTTP 路由，使用类似 Go Gin 框架的 context 模式。

## Context API

### 请求方法
```lua
ctx:getBody()           -- 获取请求体
ctx:getHeader(key)      -- 获取请求头
ctx:getMethod()         -- 获取 HTTP 方法
ctx:getURL()            -- 获取请求路径
ctx:getQuery(key)       -- 获取查询参数
ctx:getJSON()           -- 解析 JSON 请求体
ctx:getParam(key)       -- 获取路径参数
```

### 响应方法
```lua
ctx:setStatus(code)           -- 设置状态码
ctx:setHeader(key, value)     -- 设置响应头
ctx:setString(body)           -- 设置文本响应
ctx:setJSON(data)             -- 设置 JSON 响应
```

### 快捷响应
```lua
ctx:OK(data)                  -- 返回成功响应 (200)
ctx:Error(message, code)      -- 返回错误响应 (默认 400)
ctx:JSON(data)                -- 返回 JSON 响应
```

### Session & Storage
```lua
-- Session (会话数据)
ctx:setSession(key, value)
ctx:getSession(key)

-- Storage (请求作用域存储，用于中间件传递数据)
ctx:set(key, value)
ctx:get(key)
```

### 用户信息
```lua
ctx:isAuthenticated()    -- 是否已认证
ctx:getUsername()        -- 获取用户名
ctx:getUserRole()        -- 获取用户角色
```

## 路由注册

```lua
-- GET 路由
http_get("/api/users", function(ctx)
    ctx:OK({
        users = {{id = 1, name = "Alice"}}
    })
end)

-- POST 路由
http_post("/api/users", function(ctx)
    local data = ctx:getJSON()
    ctx:OK({created = true, id = 123})
end)

-- PUT 路由
http_put("/api/users/:id", function(ctx)
    local id = ctx:getParam("id")
    local data = ctx:getJSON()
    ctx:OK({updated = true})
end)

-- DELETE 路由
http_delete("/api/users/:id", function(ctx)
    ctx:OK({deleted = true})
end)
```

## 中间件

```lua
-- 日志中间件
http_use(function(ctx)
    print(string.format("[HTTP] %s %s", ctx:getMethod(), ctx:getURL()))
    return true  -- 继续处理链
end)

-- 认证中间件
http_use(function(ctx)
    local authHeader = ctx:getHeader("Authorization")

    -- 公开路由跳过认证
    if ctx:getURL() == "/api/public" then
        return true
    end

    -- 验证 token
    if authHeader and authHeader:sub(1, 7) == "Bearer " then
        local token = authHeader:sub(8)
        if validate_token(token) then
            ctx:set("userId", "123")
            return true
        end
    end

    -- 认证失败
    ctx:setStatus(401)
    ctx:Error("Unauthorized")
    return false  -- 停止处理链
end)
```

## 完整示例

```lua
-- scripts/my_routes.lua

-- 中间件：记录请求时间
http_use(function(ctx)
    ctx:set("startTime", os.time())
    return true
end)

-- 中间件：认证
http_use(function(ctx)
    local path = ctx:getURL()
    if path == "/api/public" or path == "/api/health" then
        return true
    end

    local token = ctx:getHeader("Authorization"):sub(8)
    if not valid_token(token) then
        ctx:setStatus(401)
        ctx:Error("Unauthorized")
        return false
    end
    return true
end)

-- 健康检查
http_get("/api/health", function(ctx)
    ctx:OK({status = "ok"})
end)

-- 公开接口
http_get("/api/public", function(ctx)
    ctx:OK({message = "Hello!"})
end)

-- 需要认证的接口
http_get("/api/user", function(ctx)
    local userId = ctx:get("userId")
    ctx:OK({userId = userId})
end)

-- 处理 JSON 请求
http_post("/api/data", function(ctx)
    local ok, data = pcall(function()
        return ctx:getJSON()
    end)

    if not ok then
        ctx:Error("Invalid JSON", 400)
        return
    end

    ctx:OK({received = data})
end)

print("[HTTP] Routes loaded")
```

## 与 Gin 的对比

| Go Gin | Lua Wingman |
|--------|-------------|
| `ctx.Request` | `ctx` (方法封装) |
| `ctx.GetHeader(key)` | `ctx:getHeader(key)` |
| `ctx.Query(key)` | `ctx:getQuery(key)` |
| `ctx.Param(key)` | `ctx:getParam(key)` |
| `ctx.JSON(code, data)` | `ctx:OK(data)` / `ctx:JSON(data)` |
| `ctx.Abort()` | `return false` (中间件) |
