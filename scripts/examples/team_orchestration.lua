-- Wingman Team Orchestration 示例
-- 演示多脚本协同工作

print("=== Wingman 队伍协同示例 ===")

-- 配置
local TEAM_ID = "team_demo_001"
local MY_NAME = "Player" .. math.random(1000, 9999)

-- 加入队伍
print("加入队伍 " .. TEAM_ID ...)
team.join(TEAM_ID, "", MY_NAME)

-- 获取队伍信息
local info = team.info()
print(string.format("队伍ID: %s", info.team_id))
print(string.format("我的ID: %s", info.my_id))
print(string.format("我的昵称: %s", info.my_username))
print(string.format("队员数量: %d", info.member_count))

-- 获取队员列表
local members = team.members()
print("队员列表:")
for i, m in ipairs(members) do
    print(string.format("  [%d] %s (%s)", i, m.username, m.id))
end

-- 发送消息到队伍
print("\n发送消息到队伍...")
team.send("ready", {
    position = {x = 100, y = 200},
    status = "ready"
})

team.send("scan_complete", {
    enemies_found = 3,
    position = {x = 150, y = 250}
})

-- 轮询队伍消息
print("\n轮询队伍消息 (模拟其他脚本发送的消息)...")
for i = 1, 3 do
    local msg = team.poll()
    if msg then
        print(string.format("收到消息: action=%s, from=%s",
            msg.action, msg.from))
        if msg.data then
            print(string.format("  data: %s", msg.data))
        end
    else
        print("没有新消息")
    end
    util.sleep(500)
end

-- 协同示例：等待所有队员准备就绪
print("\n协同示例：等待队员准备...")
local allReady = false
local checkCount = 0

while not allReady and checkCount < 10 do
    local members = team.members()
    local readyCount = 0

    for _, m in ipairs(members) do
        -- 这里可以检查每个成员的状态
        -- 简化示例：假设所有成员都准备好了
        readyCount = readyCount + 1
    end

    if readyCount >= info.member_count then
        allReady = true
        print("所有队员已准备就绪！")
    else
        checkCount = checkCount + 1
        util.sleep(1000)
    end
end

if allReady then
    team.send("start_mission", {timestamp = os.time()})
    print("任务开始！")
end

-- 清理
print("\n离开队伍...")
team.leave()

print("=== 示例完成 ===")
