-- ONNX 目标检测示例
-- 用 vision.aiSetupLocal 加载本地 YOLOv5/v8 ONNX 检测模型（需 WINGMAN_ENABLE_ML 构建），
-- aiLocate/aiElements 入口自动分流到本地推理，返回形状与 HTTP provider 一致：
-- aiLocate 命中时返回 {found, x, y, w, h, confidence, label}。

local wingman = require("wingman")

print("=== Wingman ONNX 目标检测示例 ===")

local MODEL_PATH = "scripts/models/yolov8n.onnx"

local function checkModelExists()
    local f = io.open(MODEL_PATH, "r")
    if f then
        f:close()
        return true
    end
    return false
end

local function main()
    if not checkModelExists() then
        print("错误: 找不到模型文件: " .. MODEL_PATH)
        print("转换命令: yolo export model=yolov8n.pt format=onnx")
        return
    end

    -- 切换到本地 ONNX 检测 provider（与 aiSetup 的 HTTP provider 互斥；未构建 ML 时恒返回 false）
    print("正在加载本地检测模型: " .. MODEL_PATH)
    local ok = wingman.vision.aiSetupLocal({
        modelPath = MODEL_PATH,
        labels = { "person", "button", "input" },  -- classId→名称，按模型实际类别调整
        minConfidence = 0.5
    })
    if not ok then
        print("错误: 加载模型失败")
        print("请检查 WINGMAN_ENABLE_ML、onnxruntime 依赖和模型路径")
        return
    end

    print("本地检测 provider 就绪")
    print("provider 状态: " .. wingman.json.encode(wingman.vision.aiSetupStatus(), 2))

    -- aiLocate：按描述定位单个目标，命中返回 {found, x, y, w, h, confidence, label}
    local box = wingman.vision.aiLocate("person")
    if box.found then
        print(string.format("定位到 %s: (%d, %d) %dx%d, 置信度 %.2f",
            box.label, box.x, box.y, box.w, box.h, box.confidence))

        -- 点击目标中心
        wingman.input.click(box.x + box.w / 2, box.y + box.h / 2)
    else
        print("未定位到目标: " .. tostring(box.error))
    end

    -- aiElements：批量识别可交互元素，返回 {found, elements:[{label,x,y,w,h,confidence}]}
    local page = wingman.vision.aiElements("")
    if page.found then
        print(string.format("识别到 %d 个元素:", #page.elements))
        for i, el in ipairs(page.elements) do
            print(string.format("  [%d] %s (%d, %d) %dx%d, 置信度 %.2f",
                i, el.label, el.x, el.y, el.w, el.h, el.confidence))
        end
    else
        print("未识别到元素: " .. tostring(page.error))
    end

    print("提示: 本地推理用简单 resize 预处理（无 letterbox），极端长宽比画面检测框会有系统性偏移。")
end

main()
