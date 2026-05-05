-- HTTP Routes Example with Context Pattern
-- This demonstrates how to use the ctx pattern similar to Go web frameworks

-- Middleware: Log all requests
http_use(function(ctx)
    print(string.format("[HTTP] %s %s", ctx:getMethod(), ctx:getURL()))
    -- Set request timestamp for handlers to use
    ctx:set("requestTime", os.time())
    return true  -- Continue to next middleware/handler
end)

-- Middleware: Auth check for protected routes
http_use(function(ctx)
    local authHeader = ctx:getHeader("Authorization")
    local path = ctx:getURL()

    -- Skip auth for public routes
    if path == "/api/public" or path == "/api/health" then
        return true
    end

    -- Simple token check
    if authHeader and authHeader:sub(1, 7) == "Bearer " then
        local token = authHeader:sub(8)
        if token == "valid-token" then
            -- Set user info in context
            ctx:setSession("userId", "123")
            ctx:set("authenticated", "true")
            return true
        end
    end

    ctx:setStatus(401)
    ctx:Error("Unauthorized")
    return false  -- Stop chain
end)

-- GET /api/health - Public health check
http_get("/api/health", function(ctx)
    ctx:OK({
        status = "healthy",
        timestamp = os.time()
    })
end)

-- GET /api/public - Public endpoint
http_get("/api/public", function(ctx)
    ctx:OK({
        message = "This is a public endpoint",
        info = {
            method = ctx:getMethod(),
            url = ctx:getURL(),
            userAgent = ctx:getHeader("User-Agent")
        }
    })
end)

-- GET /api/user - Protected endpoint, returns user info
http_get("/api/user", function(ctx)
    local userId = ctx:getSession("userId")
    local reqTime = ctx:get("requestTime")

    ctx:OK({
        userId = userId,
        isAuthenticated = ctx:isAuthenticated(),
        requestTime = reqTime
    })
end)

-- POST /api/echo - Echo back the request
http_post("/api/echo", function(ctx)
    local body = ctx:getBody()
    local contentType = ctx:getHeader("Content-Type")

    ctx:setHeader("X-Processed-By", "Wingman-Lua")
    ctx:OK({
        receivedBody = body,
        contentType = contentType,
        timestamp = os.time()
    })
end)

-- POST /api/json - Handle JSON request
http_post("/api/json", function(ctx)
    local ok, data = pcall(function()
        return ctx:getJSON()
    end)

    if not ok then
        ctx:Error("Invalid JSON: " .. tostring(data), 400)
        return
    end

    -- Process the JSON data
    ctx:OK({
        received = data,
        processed = true
    })
end)

-- GET /api/query - Demonstrate query parameter parsing
http_get("/api/query", function(ctx)
    local name = ctx:getQuery("name") or "anonymous"
    local limit = tonumber(ctx:getQuery("limit") or "10")

    ctx:OK({
        greeting = "Hello, " .. name,
        limit = limit
    })
end)

print("[HTTP] Routes loaded successfully")
