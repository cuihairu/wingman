-- Wingman HTTP API 示例
-- 演示 HTTP 请求和 JSON 处理

local http = require("http")
local json = require("json")

print("=== HTTP API 示例 ===")

-- GET 请求示例
local function getRequest(url, headers)
    headers = headers or {}
    print(string.format("GET: %s", url))

    local resp = http.get(url, headers)
    print(string.format("状态码: %d", resp.status))
    print(string.format("成功: %s", resp.success and "是" or "否"))

    if resp.success then
        print(string.format("响应长度: %d 字节", #resp.body))
        return resp.body
    else
        print(string.format("错误: %s", resp.body))
        return nil
    end
end

-- POST 请求示例
local function postRequest(url, data, headers)
    headers = headers or {}
    print(string.format("POST: %s", url))

    local body = json.encode(data)
    print(string.format("请求体: %s", body))

    local resp = http.post(url, body, headers)
    print(string.format("状态码: %d", resp.status))
    print(string.format("成功: %s", resp.success and "是" or "否"))

    if resp.success then
        local result = json.decode(resp.body)
        return result
    else
        print(string.format("错误: %s", resp.body))
        return nil
    end
end

-- 示例：调用 httpbin.org
local function httpbinExample()
    print("\n--- httpbin.org 示例 ---")

    -- GET 请求
    local resp = getRequest("https://httpbin.org/get")
    if resp then
        local data = json.decode(resp)
        print("响应数据:")
        print(json.encode(data, {indent = true}))
    end

    util.sleep(1000)

    -- POST 请求
    print("\nPOST 请求:")
    local result = postRequest("https://httpbin.org/post", {
        message = "Hello from Wingman",
        timestamp = os.time()
    })
    if result then
        print("响应数据:")
        print(json.encode(result.json, {indent = true}))
    end
end

-- 示例：调用本地 Wingman Server
local function wingmanServerExample()
    print("\n--- Wingman Server API 示例 ---")

    local baseUrl = "http://localhost:8080/api"

    -- 获取服务器状态
    print("获取服务器状态...")
    local resp = http.get(baseUrl .. "/status")
    if resp.success then
        local data = json.decode(resp)
        print(string.format("服务器状态: %s", data.status or "unknown"))
    else
        print("服务器未响应，请先启动 wingman server")
    end

    -- 执行 Lua 脚本
    print("\n执行远程脚本...")
    local script = [[
        return {
            screen = {
                width = screen.getScreenWidth(),
                height = screen.getScreenHeight()
            },
            mouse = input.getMousePosition()
        }
    ]]

    resp = http.post(baseUrl .. "/execute", json.encode({
        script = script
    }))

    if resp.success then
        local data = json.decode(resp)
        print("执行结果:")
        print(json.encode(data, {indent = true}))
    end
end

-- 示例：带自定义 headers 的请求
local function customHeadersExample()
    print("\n--- 自定义 Headers 示例 ---")

    local headers = {
        ["User-Agent"] = "Wingman/1.0",
        ["Accept"] = "application/json",
        ["X-Custom-Header"] = "test-value"
    }

    local resp = http.get("https://httpbin.org/headers", headers)
    if resp then
        local data = json.decode(resp)
        print("请求的 Headers:")
        print(json.encode(data.headers, {indent = true}))
    end
end

-- 示例：错误处理
local function errorHandlingExample()
    print("\n--- 错误处理示例 ---")

    -- 无效的 URL
    print("测试无效 URL:")
    local resp = http.get("http://this-domain-does-not-exist-12345.com")
    print(string.format("成功: %s", resp.success and "是" or "否"))
    if not resp.success then
        print("错误处理成功!")
    end

    -- 超时处理
    print("\n测试超时:")
    resp = http.get("https://httpbin.org/delay/10", {}, {timeout = 2000})
    print(string.format("成功: %s", resp.success and "是" or "否"))
end

-- 运行示例
-- 取消注释以运行特定示例
-- httpbinExample()
-- wingmanServerExample()
-- customHeadersExample()
errorHandlingExample()

print("=== 完成 ===")
