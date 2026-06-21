-- Wingman 安全模块示例
-- 演示安全防护和防检测功能

local wingman = require("wingman")

print("=== Wingman 安全模块示例 ===\n")

-- 1. 防检测 - 随机延迟
print("1. 随机延迟 (防检测)")
local delay = wingman.security.getRandomDelay()
print(string.format("建议延迟: %d ms", delay))
print()

-- 2. 随机偏移 (点击抖动)
print("2. 随机偏移 (模拟人类操作)")
local offsetX, offsetY = wingman.security.getRandomOffset()
print(string.format("X 偏移: %.2f, Y 偏移: %.2f", offsetX, offsetY))
print()

-- 3. 反调试检测
print("3. 反调试检测")
local hasDebugger = wingman.security.isDebuggerPresent()
print(string.format("调试器检测: %s", hasDebugger and "发现调试器!" or "安全"))
print()

-- 4. 虚拟机检测
print("4. 虚拟机检测")
local inVM = wingman.security.isRunningInVM()
print(string.format("虚拟机环境: %s", inVM and "在虚拟机中运行" or "物理机"))
print()

-- 5. 完整性验证
print("5. 程序完整性验证")
local integrity = wingman.security.verifyIntegrity()
print(string.format("完整性: %s", integrity and "通过" else "失败"))
print()

-- 6. 字符串哈希
print("6. 字符串哈希")
local hash = wingman.security.hashString("Hello, Wingman!")
print(string.format("SHA256: %s", hash))
print()

-- 7. 字符串加密/解密
print("7. 字符串加密 (XOR)")
local secret = "my_secret_key"
local message = "Sensitive data"
local encrypted = wingman.security.encryptString(message, secret)
print(string.format("原文: %s", message))
print(string.format("加密: %s", encrypted))
local decrypted = wingman.security.decryptString(encrypted, secret)
print(string.format("解密: %s", decrypted))
print()

-- 8. 随机字符串生成
print("8. 随机字符串生成")
local randomStr = wingman.security.generateRandomString(16)
print(string.format("随机字符串: %s", randomStr))
print()

-- 9. 敏感信息过滤
print("9. 敏感信息过滤")
local log = "User logged in with password=secret123 and token=abc456"
local filtered = wingman.security.filterSensitive(log)
print(string.format("原文: %s", log))
print(string.format("过滤: %s", filtered))
print()

-- 10. 实际应用示例
print("10. 实际应用 - 安全的鼠标点击")
print([[
    -- 获取带抖动的坐标
    local baseX, baseY = 100, 100
    local offsetX, offsetY = wingman.security.getRandomOffset()
    local clickX, clickY = baseX + offsetX, baseY + offsetY

    -- 添加随机延迟
    local delay = wingman.security.getRandomDelay()
    wingman.util.sleep(delay)

    -- 执行点击
    wingman.input.click(clickX, clickY, "left")
]])
print()

print("=== 示例完成 ===")
print()
print("注意事项:")
print("- 防检测功能可帮助避免被游戏反作弊系统检测")
print("- 哈希函数可用于校验文件和数据完整性")
print("- 加密功能用于保护脚本中的敏感数据")
print("- 日志过滤防止敏感信息泄露")
