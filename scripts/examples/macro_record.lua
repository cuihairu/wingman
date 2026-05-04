-- Wingman 宏录制示例
-- 演示如何录制和回放操作序列

print("=== 宏录制示例 ===")

-- 创建录制器实例
local recorder = MacroRecorder()

print("录制将在 3 秒后开始...")
print("请执行您想录制的操作")
util.sleep(3000)

-- 开始录制
recorder:start()
print("录制中... 按 F10 键停止")

-- 等待用户停止
-- 注意: 需要实现热键检测
util.sleep(10000)  -- 录制 10 秒

-- 停止录制
recorder:stop()
print(string.format("录制完成! 共录制 %d 个事件", recorder:getEventCount()))

-- 保存为 Lua 脚本
local luaPath = "scripts/recorded/auto_playback.lua"
if recorder:saveToLua(luaPath) then
    print(string.format("已保存到: %s", luaPath))
end

-- 保存为 JSON
local jsonPath = "scripts/recorded/macro.json"
if recorder:saveToJSON(jsonPath) then
    print(string.format("已保存到: %s", jsonPath))
end

-- 回放录制的宏
print("\n准备回放...")
util.sleep(2000)

print("回放中...")
recorder:playback(100, 1)  -- 100% 速度，播放 1 次

print("回放完成!")

print("=== 完成 ===")
