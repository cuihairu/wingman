-- ONNX 模型加载与推理示例
-- 当前 wingman.ml 暴露的是 ID 句柄式 API：模型加载、IO 元数据查询与 run() 推理。
-- YOLO detect/classify 高层封装尚未接入脚本层，因此此示例演示加载 + 一次零输入推理。

local wingman = require("wingman")

print("=== Wingman ONNX 模型加载示例 ===")

local MODEL_PATH = "scripts/models/yolov8n.onnx"

local function checkModelExists()
    local f = io.open(MODEL_PATH, "r")
    if f then
        f:close()
        return true
    end
    return false
end

local function printIoInfo(title, items)
    print(title .. ":")
    if #items == 0 then
        print("  (none)")
        return
    end

    for _, item in ipairs(items) do
        local shape = item.shape and table.concat(item.shape, "x") or "unknown"
        print(string.format("  %s [%s]", item.name or "(unnamed)", shape))
    end
end

local function main()
    if not checkModelExists() then
        print("错误: 找不到模型文件: " .. MODEL_PATH)
        print("转换命令: yolo export model=yolov8n.pt format=onnx")
        return
    end

    print("可用执行提供器:")
    for _, provider in ipairs(wingman.ml.providers()) do
        print("  " .. provider)
    end

    print("正在加载模型: " .. MODEL_PATH)
    local modelId = wingman.ml.loadModel(MODEL_PATH, "cpu")
    if not modelId then
        print("错误: 加载模型失败")
        print("请检查 WINGMAN_ENABLE_ML、ONNX Runtime 依赖和模型路径")
        return
    end

    print("模型加载成功: " .. modelId)
    print("加载状态: " .. tostring(wingman.ml.isLoaded(modelId)))

    printIoInfo("输入", wingman.ml.inputs(modelId))
    printIoInfo("输出", wingman.ml.outputs(modelId))

    -- run() 演示：所有输入维度已知时，构造全零输入跑一次推理
    local modelInputs = wingman.ml.inputs(modelId)
    local totalElements = 1
    local shapeKnown = #modelInputs > 0
    for _, dim in ipairs(shapeKnown and modelInputs[1].shape or {}) do
        if dim <= 0 then
            shapeKnown = false  -- 动态维（-1 等），无法预分配输入
        else
            totalElements = totalElements * dim
        end
    end

    if shapeKnown then
        local zeros = {}
        for _ = 1, totalElements do
            zeros[#zeros + 1] = 0.0
        end
        local result = wingman.ml.run(modelId, {
            { name = modelInputs[1].name, data = zeros }
        })
        if result.success then
            for _, item in ipairs(result.outputs) do
                print(string.format("推理输出: %s [%s] %d 元素, 耗时 %.1fms",
                    item.name,
                    item.shape and table.concat(item.shape, "x") or "unknown",
                    #item.data, result.timeMs))
            end
        else
            print("推理失败: " .. tostring(result.error))
        end
    else
        print("输入含动态维度，跳过 run() 演示")
    end

    if wingman.ml.unload(modelId) then
        print("模型已卸载")
    end

    print("提示: YOLO detect/classify 高层封装尚未接入脚本层。")
end

main()
