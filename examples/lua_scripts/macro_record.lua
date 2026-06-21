-- Wingman 宏录制示例
-- 演示如何录制和回放操作序列（单例函数式 API）

local wingman = require("wingman")

print("=== 宏录制示例 ===")

print("录制将在 3 秒后开始...")
print("请执行您想录制的操作")
wingman.util.sleep(3000)

-- 开始录制
wingman.macro.start()
print("录制中... 按 F10 键停止")

-- 等待用户停止
wingman.util.sleep(10000)  -- 录制 10 秒

-- 停止录制
wingman.macro.stop()
print(string.format("录制完成! 共录制 %d 个事件", wingman.macro.getEventCount()))

-- 保存为 Lua 脚本
local luaPath = "scripts/recorded/auto_playback.lua"
if wingman.macro.saveToLua(luaPath) then
    print(string.format("已保存到: %s", luaPath))
end

-- 保存为 JSON
local jsonPath = "scripts/recorded/macro.json"
if wingman.macro.saveToJSON(jsonPath) then
    print(string.format("已保存到: %s", jsonPath))
end

-- 回放录制的宏
print("\n准备回放...")
wingman.util.sleep(2000)

print("回放中...")
wingman.macro.playback(100, 1)  -- 100% 速度，播放 1 次

print("回放完成!")

print("=== 完成 ===")
