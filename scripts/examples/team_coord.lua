-- Wingman 组队协调示例
-- 演示如何使用 HTTP/JSON/KV 模块进行多 Client 协同

local http = require("http")
local json = require("json")
local kv = require("kv")

print("=== 组队协调示例 ===")

-- 配置
local serverUrl = "http://localhost:8080"
local clientId = kv.get("client_id")
local username = "Player" .. math.random(1000, 9999)

-- 注册 Client
if clientId == "" then
    print("注册新 Client...")
    local resp = http.post(serverUrl .. "/api/client/register", "")
    if resp.success then
        local result = json.decode(resp.body)
        clientId = result.clientId
        kv.set("client_id", clientId)
        print(string.format("已注册: %s", clientId))
    else
        print("注册失败")
        return
    end
else
    print(string.format("使用已注册 Client: %s", clientId))
end

-- 模拟登录游戏
print(string.format("以用户名 %s 登录游戏...", username))

-- 汇报状态
local function reportStatus(status, gameId)
    local resp = http.post(serverUrl .. "/api/client/heartbeat", json.encode({
        clientId = clientId,
        status = status,
        gameId = gameId or "",
        username = username
    }))
    return resp.success
end

-- 登录后汇报
reportStatus("logged_in", "game_account_1")
print("已汇报登录状态")

-- 请求组队分配
print("请求组队分配...")
local resp = http.post(serverUrl .. "/api/team/allocate", json.encode({
    clientId = clientId,
    username = username,
    preferredSize = 3
}))

if resp.success then
    local result = json.decode(resp.body)
    print(string.format("队伍分配成功!"))
    print(string.format("  队伍ID: %s", result.teamId))
    print(string.format("  是否队长: %s", result.isLeader and "是" or "否"))

    -- 保存队伍信息
    kv.set("team_id", result.teamId)

    if #result.teammates > 0 then
        print("  队友:")
        for i, mate in ipairs(result.teammates) do
            print(string.format("    [%d] %s", i, mate))
        end
    end

    -- 游戏内组队逻辑
    if result.isLeader then
        print("\n我是队长，邀请队友...")
        for _, mate in ipairs(result.teammates) do
            if mate ~= username then
                print(string.format("  邀请 %s", mate))
                -- 调用游戏内邀请函数
                -- inviteToTeam(mate)
            end
        end
    end
else
    print("队伍分配失败: " .. resp.body)
end

-- 投票协调
local function checkVotes()
    local resp = http.get(serverUrl .. "/api/vote/pending?clientId=" .. clientId)
    if resp.success then
        local actions = json.decode(resp.body)
        if #actions > 0 then
            print(string.format("收到 %d 个待处理投票", #actions))
            for _, action in ipairs(actions) do
                if action.agree then
                    print("  Server 建议同意")
                    -- 点击"同意"按钮
                else
                    print("  Server 建议拒绝")
                    -- 点击"拒绝"按钮
                end
            end
        end
    end
end

-- 汇报投票事件
local function reportVote(voteType, target, initiator)
    local resp = http.post(serverUrl .. "/api/vote/report", json.encode({
        teamId = kv.get("team_id"),
        type = voteType,
        target = target,
        initiator = initiator
    }))
    if resp.success then
        local result = json.decode(resp.body)
        print(string.format("投票已汇报: %s, 建议: %s",
                           result.voteId,
                           result.recommendAction))
    end
end

print("\n运行中... (模拟投票检查)")
for i = 1, 3 do
    checkVotes()
    util.sleep(1000)
end

print("=== 完成 ===")
