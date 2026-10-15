-- Wingman 触发器配置示例（可运行脚本）
-- 用 wingman.smarttrigger 编程式构建触发器：create -> addCondition/addAction
-- -> setCheckInterval -> start。
--
-- 与「配置文件」的差别（旧版本此文件写成全局 triggers 表，运行时无加载器消费）：
-- 触发器没有独立的配置文件格式，条件与动作必须经 addCondition/addAction 传入；
-- 没有「冷却」字段（冷却 = WAIT 动作 + setCheckInterval 检查间隔）；
-- 没有「启用」字段（禁用 = 已 create 但未 start）。

local wingman = require("wingman")

-- 1. 技能冷却检测：技能图标变绿 (0x00FF00) -> 按 1 键，随后等 500ms 冷却
if not wingman.smarttrigger.create("技能冷却检测") then
    error("创建触发器失败: 技能冷却检测")
end
wingman.smarttrigger.addCondition("技能冷却检测", {
    type = "COLOR_FOUND",
    color = 0x00FF00,
    tolerance = 10,
    region = { x = 100, y = 100, width = 50, height = 50 }
})
wingman.smarttrigger.addAction("技能冷却检测", { type = "KEY_PRESS", key = 49 })
wingman.smarttrigger.addAction("技能冷却检测", { type = "WAIT", waitMs = 500 })
wingman.smarttrigger.setCheckInterval("技能冷却检测", 500)

-- 2. 血量监控：血量条区域变红 (0xFF0000) -> 按 2 键喝药
wingman.smarttrigger.create("血量监控")
wingman.smarttrigger.addCondition("血量监控", {
    type = "COLOR_FOUND",
    color = 0xFF0000,
    tolerance = 15,
    region = { x = 50, y = 50, width = 20, height = 20 }
})
wingman.smarttrigger.addAction("血量监控", { type = "KEY_PRESS", key = 50 })
wingman.smarttrigger.setCheckInterval("血量监控", 2000)

-- 3. 蓝量监控：仅构建不启动（等价于旧版的「默认禁用」）
wingman.smarttrigger.create("蓝量监控")
wingman.smarttrigger.addCondition("蓝量监控", {
    type = "COLOR_FOUND",
    color = 0x0000FF,
    tolerance = 15,
    region = { x = 50, y = 80, width = 20, height = 20 }
})
wingman.smarttrigger.addAction("蓝量监控", { type = "KEY_PRESS", key = 51 })
wingman.smarttrigger.setCheckInterval("蓝量监控", 5000)

-- 启动已配置完成的触发器；start 透传真实结果（无条件/动作时返回 false）
for _, name in ipairs({ "技能冷却检测", "血量监控" }) do
    print(("启动 %s: %s（运行中: %s）")
        :format(name, tostring(wingman.smarttrigger.start(name)),
                tostring(wingman.smarttrigger.isRunning(name))))
end
