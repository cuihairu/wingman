-- Simple HTTP Routes for Testing
-- Basic routes to verify Lua HTTP functionality

-- Health check endpoint
http_get("/test/health", function(ctx)
    ctx:OK({
        status = "ok",
        message = "Lua HTTP is working"
    })
end)

-- Echo endpoint - returns what you send
http_post("/test/echo", function(ctx)
    local body = ctx:getBody()
    ctx:OK({
        echo = body
    })
end)

-- JSON test endpoint
http_post("/test/json", function(ctx)
    local ok, data = pcall(function()
        return ctx:getJSON()
    end)

    if not ok then
        ctx:Error("Invalid JSON", 400)
        return
    end

    ctx:OK({
        received = data,
        success = true
    })
end)

-- Query parameter test
http_get("/test/query", function(ctx)
    local name = ctx:getQuery("name") or "world"
    ctx:OK({
        greeting = "Hello, " .. name .. "!"
    })
end)

-- Header test
http_get("/test/headers", function(ctx)
    local userAgent = ctx:getHeader("User-Agent") or "unknown"
    local contentType = ctx:getHeader("Content-Type") or "none"
    ctx:OK({
        userAgent = userAgent,
        contentType = contentType
    })
end)

-- Session test
http_get("/test/session", function(ctx)
    ctx:setSession("visitTime", os.time())
    local visitTime = ctx:getSession("visitTime")
    ctx:OK({
        visitTime = visitTime
    })
end)

-- Middleware test - log all /test/* requests
http_use(function(ctx)
    local path = ctx:getURL()
    if path:find("^/test/") then
        print("[Test] " .. ctx:getMethod() .. " " .. path)
    end
    return true
end)

print("[HTTP] Test routes loaded")
