-- Wingman 游戏配置示例
-- 演示多游戏配置管理功能

local wingman = require("wingman")

print("=== Wingman 游戏配置管理示例 ===\n")

-- 1. 设置配置目录
print("1. 设置配置目录")
wingman.gameprofile.setProfilesDirectory("./profiles")
print("配置目录: ./profiles\n")

-- 2. 扫描配置目录
print("2. 扫描配置目录")
wingman.gameprofile.scan()
print()

-- 3. 创建新游戏配置模板
print("3. 创建游戏配置模板")
local profileId, profileName = wingman.gameprofile.createTemplate("示例游戏")
print(string.format("配置 ID: %s", profileId))
print(string.format("配置名称: %s\n", profileName))

-- 4. 列出所有配置
print("4. 列出所有配置")
local profiles = wingman.gameprofile.list()
print("可用配置:")
for i, id in ipairs(profiles) do
    print(string.format("  %d. %s", i, id))
end
print()

-- 5. 获取配置详情
print("5. 获取配置详情")
local id, name = wingman.gameprofile.get(profileId)
if id then
    print(string.format("配置: %s (%s)", name, id))
end
print()

-- 6. 根据窗口查找配置
print("6. 根据窗口查找配置")
local foundId, foundName = wingman.gameprofile.findByWindow("游戏")
if foundId then
    print(string.format("找到配置: %s (%s)", foundName, foundId))
else
    print("未找到匹配的配置")
end
print()

-- 7. 设置活动配置
print("7. 设置活动配置")
local success = wingman.gameprofile.setActive(profileId)
print(string.format("设置活动配置: %s\n", tostring(success)))

-- 8. 获取活动配置
print("8. 获取活动配置")
local activeId, activeName = wingman.gameprofile.getActive()
if activeId then
    print(string.format("当前活动: %s (%s)", activeName, activeId))
else
    print("没有活动配置")
end
print()

-- 9. 配置文件格式示例
print("9. 配置文件格式 (INI)")
print([[
    [profile]
    id=my_game
    name=我的游戏
    version=1.0.0
    description=游戏自动化配置

    [window]
    title=游戏窗口标题
    process=game.exe
    exact_match=false

    [colors]
    hp_bar=255,0,0
    mp_bar=0,0,255
    enemy_target=255,255,0

    [images]
    btn_attack=images/attack.png
    btn_skill=images/skill.png
    icon_boss=images/boss_icon.png

    [triggers]
    low_hp=.pixel:100,100,255,0,0:onHpLow
    enemy_appeared=image:enemy.png:0.9:onEnemyFound
]])
print()

-- 10. 实际应用示例
print("10. 实际应用 - 自动选择游戏配置")
print([[
    -- 根据当前窗口自动选择配置
    local hwnd = wingman.window.getForeground()
    local title = wingman.window.getTitle(hwnd)

    local profileId, profileName = wingman.gameprofile.findByWindow(title)
    if profileId then
        wingman.gameprofile.setActive(profileId)
        print("已加载配置: " .. profileName)
    else
        print("未找到匹配的游戏配置")
    end
]])
print()

print("=== 示例完成 ===")
print()
print("使用说明:")
print("- 将游戏配置放在 profiles/目录下")
print("- 每个游戏一个子目录，包含 profile.json 文件")
print("- 图像文件放在同一目录下的 images/ 文件夹")
