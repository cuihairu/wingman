-- Wingman Team Orchestration 示例
-- 演示 wingman.team 模块的组队协同

local wingman = require("wingman")

print("=== Wingman 队伍协同示例 ===")

-- 配置
local TEAM_ID = "team_demo_001"
local MY_NAME = "Player" .. math.random(1000, 9999)

-- 1. 加入队伍（joinTeam 生成/使用成员 ID，返回是否成功）
print(string.format("以成员 %s 加入队伍 %s...", MY_NAME, TEAM_ID))
if wingman.team.joinTeam(TEAM_ID, MY_NAME) then
    print("加入成功")
else
    print("加入失败")
    return
end

-- 2. 获取我的成员 ID
local myId = wingman.team.getMemberId()
print(string.format("我的客户端ID: %s", myId))

-- 3. 获取队伍信息（getTeamStatus 返回 JSON 字符串）
local status = wingman.json.decode(wingman.team.getTeamStatus())
print(string.format("队伍ID: %s", status.teamId))
print(string.format("队长ID: %s", status.leaderId))
print(string.format("队伍状态: %s", status.state))
print(string.format("队员数量: %d", #status.members))

-- 4. 获取队员列表
print("队员列表:")
for i, memberId in ipairs(status.members) do
    local mark = (memberId == status.leaderId) and " [队长]" or ""
    print(string.format("  [%d] %s%s", i, memberId, mark))
end

-- 5. 向队伍广播消息
print("\n向队伍广播消息...")
local sent = wingman.team.broadcast({
    action = "ready",
    position = {x = 100, y = 200},
    status = "ready"
})
print("广播 ready 消息: " .. tostring(sent))

sent = wingman.team.broadcast({
    action = "scan_complete",
    enemies_found = 3,
    position = {x = 150, y = 250}
})
print("广播 scan_complete 消息: " .. tostring(sent))

-- 6. 汇报状态
print("\n汇报状态...")
local reported = wingman.team.reportStatus({
    hp = 100,
    status = "ready"
})
print("状态汇报: " .. tostring(reported))
print("提示: 入站队伍消息（广播/投票）经 wingman.team.on 事件订阅接收，team 模块无同步轮询接口")

-- 7. 协同示例：确认队伍就绪后发起任务
print("\n协同示例：确认队伍就绪...")
local allReady = false
local checkCount = 0

while not allReady and checkCount < 10 do
    local cur = wingman.json.decode(wingman.team.getTeamStatus())
    -- 本地 team 状态随服务器 team.joined 等消息更新，
    -- 这里以“已入队且队内至少 1 名成员”作为就绪条件
    if cur.teamId ~= "" and #cur.members > 0 then
        allReady = true
        print("队伍已就绪！")
    else
        checkCount = checkCount + 1
        wingman.util.sleep(1000)
    end
end

if allReady then
    wingman.team.broadcast({action = "start_mission", timestamp = os.time()})
    print("任务开始！")
end

-- 8. 清理
print("\n离开队伍...")
wingman.team.leaveTeam()

print("=== 示例完成 ===")
