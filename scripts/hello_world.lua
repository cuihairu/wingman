-- Wingman Hello World 示例
-- 这是最基础的示例脚本

print("=== Wingman Hello World ===")

-- 获取屏幕尺寸
local width = screen.getScreenWidth()
local height = screen.getScreenHeight()
print(string.format("屏幕尺寸: %d x %d", width, height))

-- 获取鼠标位置
util.sleep(1000) -- 等待 1 秒
print("脚本执行完成!")
