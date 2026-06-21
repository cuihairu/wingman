-- ONNX 对象检测示例
-- 使用 YOLOv8 模型检测游戏中的目标并自动点击
--
-- 准备工作:
-- 1. 下载 YOLOv8n 模型并转换为 ONNX 格式
-- 2. 将模型文件放在 scripts/models/ 目录下
-- 3. 运行此脚本

local wingman = require("wingman")

print("=== Wingman ONNX 目标检测示例 ===")

-- 配置
local MODEL_PATH = "scripts/models/yolov8n.onnx"
local CONFIDENCE_THRESHOLD = 0.5
local TARGET_CLASS = "enemy"  -- 根据你的模型类别调整

-- 检查模型文件
local function checkModelExists()
    local f = io.open(MODEL_PATH, "r")
    if f then
        f:close()
        return true
    end
    return false
end

-- 主检测循环
local function main()
    -- 检查模型
    if not checkModelExists() then
        print("错误: 找不到模型文件: " .. MODEL_PATH)
        print("请先下载 YOLOv8n ONNX 模型并放在正确位置")
        print("转换命令: ultralytics yolo export model=yolov8n.pt format=onnx")
        return
    end

    -- 加载模型
    print("正在加载模型: " .. MODEL_PATH)
    local model, err = wingman.ml.loadModel(MODEL_PATH)
    if not model then
        print("错误: 加载模型失败 - " .. tostring(err))
        return
    end
    print("模型加载成功")

    -- 获取前台窗口
    local hwnd = wingman.window.getForeground()
    if not hwnd then
        print("错误: 无法获取前台窗口")
        return
    end
    print("前台窗口: " .. wingman.window.getTitle(hwnd))

    print("\n开始检测循环 (按 Ctrl+C 退出)...")
    print("置信度阈值: " .. CONFIDENCE_THRESHOLD)
    print("目标类别: " .. TARGET_CLASS)

    local count = 0
    while count < 10 do  -- 限制循环次数用于测试
        -- 截图
        local screenshot = wingman.screen.captureWindow(hwnd)

        -- 运行推理
        local results = model.detect(screenshot, CONFIDENCE_THRESHOLD)

        -- 处理结果
        for _, obj in ipairs(results) do
            if obj.class == TARGET_CLASS or obj.label == TARGET_CLASS then
                print(string.format("检测到 %s: 置信度 %.2f, 位置 (%d, %d)",
                    obj.class or obj.label, obj.confidence, obj.center.x, obj.center.y))

                -- 点击目标中心
                wingman.input.click(obj.center.x, obj.center.y, "left")
            end
        end

        count = count + 1
        wingman.util.sleep(500)  -- 休眠 500ms
    end

    print("\n检测完成")
end

-- 运行
main()
