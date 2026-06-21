-- Wingman MMORPG 游戏自动化示例
-- 演示游戏中的常见自动化场景

local wingman = require("wingman")

print("=== MMORPG 游戏自动化示例 ===")
print("警告: 请确保遵守游戏服务条款，自动化可能导致封号")
print()

-- ========== 配置 ==========

local Config = {
    -- HP/MP 监控区域 (根据实际游戏调整)
    hpBar = {x = 100, y = 100, width = 200, height = 20},
    mpBar = {x = 100, y = 130, width = 200, height = 20},

    -- 颜色定义 (根据实际游戏调整)
    colors = {
        hpLow = 0xFF0000,      -- 红色表示血量低
        hpMedium = 0xFFFF00,   -- 黄色表示血量中等
        hpHigh = 0x00FF00,     -- 绿色表示血量高
        mpLow = 0x0000FF,      -- 蓝色表示魔法低
        enemy = 0xFF0000,      -- 敌人标记颜色
        item = 0xFFFF00,       -- 物品拾取颜色
        skillReady = 0x00FF00  -- 技能就绪颜色
    },

    -- 按键配置
    keys = {
        hpPotion = "1",
        mpPotion = "2",
        skill1 = "3",
        skill2 = "4",
        skill3 = "5",
        autoAttack = "0"
    },

    -- 延迟配置 (毫秒)
    delays = {
        check = 100,        -- 检查间隔
        potion = 2000,      -- 喝药冷却
        skill = 500,        -- 技能施放间隔
        pickup = 300,       -- 拾取物品间隔
        human = 50          -- 人性化随机延迟范围
    }
}

-- ========== 工具函数 ==========

-- 人性化随机延迟
local function humanDelay(baseMs)
    local variation = Config.delays.human
    local delay = baseMs + math.random(-variation, variation)
    wingman.util.sleep(delay)
end

-- 发送按键
local function pressKey(key)
    wingman.input.keyDown(key)
    wingman.util.sleep(50)
    wingman.input.keyUp(key)
end

-- 检测血量百分比
local function detectHP()
    -- 在血条区域检测颜色
    local x = Config.hpBar.x + Config.hpBar.width / 2
    local y = Config.hpBar.y + Config.hpBar.height / 2
    local color = wingman.screen.getPixel(x, y)

    -- 根据颜色判断血量等级
    if color.r == 255 and color.g == 0 and color.b == 0 then
        return "low"  -- 红色，血量低
    elseif color.r == 255 and color.g == 255 and color.b == 0 then
        return "medium"  -- 黄色，血量中等
    else
        return "high"  -- 绿色或其他，血量高
    end
end

-- ========== 自动喝药 ==========

local AutoPotion = {
    lastHPTime = 0,
    lastMPTime = 0,
    enabled = true
}

function AutoPotion.check()
    if not AutoPotion.enabled then
        return
    end

    local now = wingman.util.getTime()
    local hpStatus = detectHP()

    -- HP 低且冷却完成
    if hpStatus == "low" and (now - AutoPotion.lastHPTime) > Config.delays.potion then
        print(string.format("[%s] HP低! 使用血药", os.date("%H:%M:%S")))
        pressKey(Config.keys.hpPotion)
        AutoPotion.lastHPTime = now
        humanDelay(200)
    end

    -- MP 检测 (简化)
    local x = Config.mpBar.x + 10
    local y = Config.mpBar.y + Config.mpBar.height / 2
    local mpColor = wingman.screen.getPixel(x, y)

    if mpColor.b > 200 and (now - AutoPotion.lastMPTime) > Config.delays.potion then
        print(string.format("[%s] MP低! 使用蓝药", os.date("%H:%M:%S")))
        pressKey(Config.keys.mpPotion)
        AutoPotion.lastMPTime = now
        humanDelay(200)
    end
end

-- ========== 自动战斗 ==========

local AutoCombat = {
    enabled = false,
    lastSkillTime = {},
    currentTarget = false
}

function AutoCombat.findTarget()
    -- 在屏幕中心区域查找敌人颜色
    local centerX = wingman.screen.getScreenWidth() / 2
    local centerY = wingman.screen.getScreenHeight() / 2
    local searchRadius = 200

    local found, x, y = wingman.screen.findColor(
        Config.colors.enemy,
        centerX - searchRadius,
        centerY - searchRadius,
        searchRadius * 2,
        searchRadius * 2,
        10
    )

    if found then
        -- 使用人性化移动点击敌人
        wingman.input.move(x, y, 200)
        humanDelay(100)
        wingman.input.click(x, y)
        AutoCombat.currentTarget = true
        print(string.format("[%s] 选中目标", os.date("%H:%M:%S")))
    end

    return found
end

function AutoCombat.castSkills()
    local now = wingman.util.getTime()

    -- 技能 1
    if (now - (AutoCombat.lastSkillTime[1] or 0)) > Config.delays.skill then
        pressKey(Config.keys.skill1)
        AutoCombat.lastSkillTime[1] = now
        humanDelay(100)
    end

    -- 技能 2
    if (now - (AutoCombat.lastSkillTime[2] or 0)) > Config.delays.skill * 2 then
        pressKey(Config.keys.skill2)
        AutoCombat.lastSkillTime[2] = now
        humanDelay(100)
    end

    -- 技能 3
    if (now - (AutoCombat.lastSkillTime[3] or 0)) > Config.delays.skill * 3 then
        pressKey(Config.keys.skill3)
        AutoCombat.lastSkillTime[3] = now
        humanDelay(100)
    end
end

function AutoCombat.check()
    if not AutoCombat.enabled then
        return
    end

    -- 如果没有目标，寻找目标
    if not AutoCombat.currentTarget then
        AutoCombat.findTarget()
    else
        -- 有目标，施放技能
        AutoCombat.castSkills()
    end
end

function AutoCombat.clearTarget()
    AutoCombat.currentTarget = false
end

-- ========== 自动拾取 ==========

local AutoLoot = {
    enabled = true,
    lastLootTime = 0
}

function AutoLoot.check()
    if not AutoLoot.enabled then
        return
    end

    local now = wingman.util.getTime()
    if (now - AutoLoot.lastLootTime) < Config.delays.pickup then
        return
    end

    -- 查找地面物品
    local screenWidth = wingman.screen.getScreenWidth()
    local screenHeight = wingman.screen.getScreenHeight()

    -- 从底部向上扫描
    for y = screenHeight - 100, screenHeight - 300, -30 do
        for x = 100, screenWidth - 100, 50 do
            local color = wingman.screen.getPixel(x, y)

            -- 检测物品颜色 (黄色)
            if color.r == 255 and color.g == 255 and color.b == 0 then
                print(string.format("[%s] 发现物品在 (%d, %d)", os.date("%H:%M:%S"), x, y))

                -- 人性化移动并点击
                wingman.input.move(x, y, 150)
                humanDelay(50)
                wingman.input.click(x, y)

                AutoLoot.lastLootTime = now
                return
            end
        end
    end
end

-- ========== 主循环 ==========

local Automation = {
    running = true
}

function Automation.start()
    print("开始自动化循环...")
    print("按 F10 停止")

    while Automation.running do
        -- 检测停止键 (F10 = 121)
        if wingman.input.isKeyDown(121) then
            print("检测到停止信号...")
            Automation.running = false
            break
        end

        -- 执行各模块
        AutoPotion.check()
        AutoCombat.check()
        AutoLoot.check()

        -- 检查间隔
        wingman.util.sleep(Config.delays.check)
    end

    print("自动化已停止")
end

-- ========== 状态显示 ==========

function Automation.showStatus()
    print("\n=== 当前状态 ===")
    print(string.format("自动喝药: %s", AutoPotion.enabled and "启用" or "禁用"))
    print(string.format("自动战斗: %s", AutoCombat.enabled and "启用" or "禁用"))
    print(string.format("自动拾取: %s", AutoLoot.enabled and "启用" or "禁用"))
    print(string.format("检测到 HP 状态: %s", detectHP()))
    print("==================\n")
end

-- ========== 快捷键切换 ==========

function Automation.toggleModule(moduleName)
    if moduleName == "potion" then
        AutoPotion.enabled = not AutoPotion.enabled
        print(string.format("自动喝药: %s", AutoPotion.enabled and "启用" or "禁用"))
    elseif moduleName == "combat" then
        AutoCombat.enabled = not AutoCombat.enabled
        print(string.format("自动战斗: %s", AutoCombat.enabled and "启用" or "禁用"))
    elseif moduleName == "loot" then
        AutoLoot.enabled = not AutoLoot.enabled
        print(string.format("自动拾取: %s", AutoLoot.enabled and "启用" or "禁用"))
    end
end

-- ========== 测试模式 ==========

function Automation.test()
    print("\n=== 测试模式 ===")

    print("测试 HP 检测...")
    local hp = detectHP()
    print(string.format("HP 状态: %s", hp))

    print("\n测试鼠标移动...")
    local pos = wingman.input.getMousePosition()
    print(string.format("当前位置: (%d, %d)", pos.x, pos.y))

    print("\n测试按键...")
    pressKey(Config.keys.hpPotion)
    print("已按键: " .. Config.keys.hpPotion)

    print("\n=== 测试完成 ===")
    Automation.showStatus()
end

-- ========== 命令行接口 ==========

local commands = {
    ["start"] = function()
        Automation.start()
    end,
    ["stop"] = function()
        Automation.running = false
    end,
    ["status"] = function()
        Automation.showStatus()
    end,
    ["test"] = function()
        Automation.test()
    end,
    ["potion"] = function()
        Automation.toggleModule("potion")
    end,
    ["combat"] = function()
        Automation.toggleModule("combat")
    end,
    ["loot"] = function()
        Automation.toggleModule("loot")
    end,
    ["help"] = function()
        print("\n=== 命令列表 ===")
        print("start   - 开始自动化")
        print("stop    - 停止自动化")
        print("status  - 显示状态")
        print("test    - 测试模式")
        print("potion  - 切换自动喝药")
        print("combat  - 切换自动战斗")
        print("loot    - 切换自动拾取")
        print("help    - 显示帮助")
        print("按 F10 键可随时停止自动化")
        print("==================\n")
    end
}

-- ========== 主入口 ==========

if arg and arg[1] then
    local cmd = arg[1]
    if commands[cmd] then
        commands[cmd]()
    else
        print("未知命令: " .. cmd)
        commands["help"]()
    end
else
    -- 默认运行测试
    commands["test"]()
end

print("\n提示: 运行 'wingman scripts/examples/mmorpg_automation.lua start' 开始自动化")
print("=== 完成 ===")
