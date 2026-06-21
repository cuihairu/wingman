-- Wingman KV 存储示例
-- 演示键值存储的各类操作

local wingman = require("wingman")

print("=== KV 存储示例 ===")

-- ========== 字符串操作 ==========

local function stringExample()
    print("\n--- 字符串操作 ---")

    -- 设置值
    wingman.kv.set("user:name", "Wingman User")
    wingman.kv.set("user:level", "42")

    -- 获取值
    local name = wingman.kv.get("user:name")
    local level = wingman.kv.get("user:level")
    print(string.format("用户: %s, 等级: %s", name, level))

    -- 检查键是否存在
    local exists = wingman.kv.exists("user:name")
    print(string.format("'user:name' 存在: %s", exists and "是" or "否"))

    -- 追加值
    wingman.kv.append("user:log", "登录时间: " .. os.date("%Y-%m-%d %H:%M:%S") .. "\n")
    wingman.kv.append("user:log", "执行操作: 测试\n")
    local log = wingman.kv.get("user:log")
    print("日志内容:")
    print(log)

    -- 获取字符串长度
    local len = wingman.kv.strlen("user:name")
    print(string.format("'user:name' 长度: %d", len))

    -- 清理
    wingman.kv.del("user:name", "user:level", "user:log")
end

-- ========== 数字操作 ==========

local function numberExample()
    print("\n--- 数字操作 ---")

    -- 设置初始值
    wingman.kv.set("counter:hits", 0)
    wingman.kv.set("counter:misses", 0)

    -- 自增
    wingman.kv.incr("counter:hits")
    wingman.kv.incr("counter:hits")
    wingman.kv.incr("counter:hits")
    print("点击次数: " .. wingman.kv.get("counter:hits"))

    -- 自减
    wingman.kv.decr("counter:misses")
    print("未命中次数: " .. wingman.kv.get("counter:misses"))

    -- 按指定值增减
    wingman.kv.incrBy("counter:hits", 10)
    print("增加后点击: " .. wingman.kv.get("counter:hits"))

    wingman.kv.decrBy("counter:misses", 5)
    print("减少后未命中: " .. wingman.kv.get("counter:misses"))

    -- 清理
    wingman.kv.del("counter:hits", "counter:misses")
end

-- ========== Hash 操作 ==========

local function hashExample()
    print("\n--- Hash 操作 ---")

    local hashKey = "session:" .. os.time()

    -- 设置字段
    wingman.kv.hset(hashKey, "user_id", "12345")
    wingman.kv.hset(hashKey, "username", "wingman")
    wingman.kv.hset(hashKey, "login_time", os.time())
    wingman.kv.hset(hashKey, "ip", "192.168.1.100")

    -- 获取单个字段
    local user = wingman.kv.hget(hashKey, "username")
    print(string.format("用户名: %s", user))

    -- 获取多个字段
    local fields = wingman.kv.hgetall(hashKey)
    print("所有字段:")
    for k, v in pairs(fields) do
        print(string.format("  %s: %s", k, v))
    end

    -- 检查字段是否存在
    local hasUser = wingman.kv.hexists(hashKey, "username")
    local hasEmail = wingman.kv.hexists(hashKey, "email")
    print(string.format("有 username: %s, 有 email: %s", hasUser, hasEmail))

    -- 获取所有字段名
    local keys = wingman.kv.hkeys(hashKey)
    print("字段名: " .. table.concat(keys, ", "))

    -- 获取字段数量
    local count = wingman.kv.hlen(hashKey)
    print(string.format("字段数量: %d", count))

    -- 清理
    wingman.kv.del(hashKey)
end

-- ========== List 操作 ==========

local function listExample()
    print("\n--- List 操作 ---")

    local listKey = "queue:tasks"

    -- 清空现有列表
    wingman.kv.del(listKey)

    -- 从左侧推入
    wingman.kv.lpush(listKey, "task1")
    wingman.kv.lpush(listKey, "task2")
    wingman.kv.lpush(listKey, "task3")

    -- 从右侧推入
    wingman.kv.rpush(listKey, "task4")

    -- 获取列表长度
    local len = wingman.kv.llen(listKey)
    print(string.format("队列长度: %d", len))

    -- 获取范围
    local items = wingman.kv.lrange(listKey, 0, -1)
    print("所有任务:")
    for i, item in ipairs(items) do
        print(string.format("  [%d] %s", i, item))
    end

    -- 从左侧弹出
    local task = wingman.kv.lpop(listKey)
    print(string.format("处理任务: %s", task))

    -- 从右侧弹出
    task = wingman.kv.rpop(listKey)
    print(string.format("从右侧取出: %s", task))

    -- 索引访问
    task = wingman.kv.lindex(listKey, 0)
    print(string.format("第一个任务: %s", task))

    -- 清理
    wingman.kv.del(listKey)
end

-- ========== Set 操作 ==========

local function setExample()
    print("\n--- Set 操作 ---")

    local setKey = "online:users"

    -- 添加成员
    wingman.kv.sadd(setKey, "user1", "user2", "user3")
    wingman.kv.sadd(setKey, "user2", "user4")  -- user2 重复

    -- 获取成员数
    local count = wingman.kv.scard(setKey)
    print(string.format("在线用户数: %d", count))

    -- 获取所有成员
    local members = wingman.kv.smembers(setKey)
    print("在线用户: " .. table.concat(members, ", "))

    -- 检查成员是否存在
    local isOnline = wingman.kv.sismember(setKey, "user1")
    print(string.format("user1 在线: %s", isOnline and "是" or "否"))

    -- 集合运算
    local setKey2 = "vip:users"
    wingman.kv.sadd(setKey2, "user2", "user3", "user5")

    -- 交集
    local common = wingman.kv.sinter(setKey, setKey2)
    print("既是在线又是VIP: " .. table.concat(common, ", "))

    -- 并集
    local all = wingman.kv.sunion(setKey, setKey2)
    print("所有用户: " .. table.concat(all, ", "))

    -- 差集
    local diff = wingman.kv.sdiff(setKey, setKey2)
    print("在线但非VIP: " .. table.concat(diff, ", "))

    -- 清理
    wingman.kv.del(setKey, setKey2)
end

-- ========== 过期时间 ==========

local function expiryExample()
    print("\n--- 过期时间示例 ---")

    -- 设置带过期时间的键
    wingman.kv.set("temp:data", "This will expire soon")
    wingman.kv.expire("temp:data", 10)  -- 10 秒后过期
    print("设置了 10 秒过期的键")

    -- 查看剩余时间
    local ttl = wingman.kv.ttl("temp:data")
    print(string.format("剩余时间: %d 秒", ttl))

    wingman.util.sleep(2000)

    ttl = wingman.kv.ttl("temp:data")
    print(string.format("2 秒后剩余时间: %d 秒", ttl))

    -- 设置时同时指定过期时间
    wingman.kv.set("temp:data2", "With TTL", 5)
    print("设置了 5 秒过期的键")

    -- 清理
    wingman.kv.del("temp:data", "temp:data2")
end

-- ========== 实用示例：游戏配置缓存 ==========

local function gameConfigExample()
    print("\n--- 游戏配置缓存示例 ---")

    local configKey = "game:config:default"

    -- 检查配置是否存在
    if wingman.kv.exists(configKey) then
        print("从缓存加载配置")
        local configJson = wingman.kv.get(configKey)
        local config = wingman.json.decode(configJson)
        print("配置:")
        print(wingman.json.encode(config, {indent = true}))
    else
        print("创建新配置")
        local config = {
            autoHP = true,
            hpThreshold = 30,
            autoMP = true,
            mpThreshold = 20,
            skills = {
                {key = "1", cooldown = 5000},
                {key = "2", cooldown = 8000},
                {key = "3", cooldown = 10000}
            }
        }

        wingman.kv.set(configKey, wingman.json.encode(config))
        wingman.kv.expire(configKey, 3600)  -- 1 小时
        print("配置已缓存")
    end

    -- 清理
    wingman.kv.del(configKey)
end

-- 运行所有示例
stringExample()
numberExample()
hashExample()
listExample()
setExample()
expiryExample()
gameConfigExample()

print("\n=== 完成 ===")
