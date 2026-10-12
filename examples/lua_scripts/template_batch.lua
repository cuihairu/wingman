-- Wingman 模板批量管理示例
-- 演示具名模板登记（templateAdd/templateRemove/templateHas/templateList）
-- 与批量识别（templateFindAll）。单张找图见 image_matching.lua。

local wingman = require("wingman")

print("=== 模板批量管理示例 ===")

-- ========== 登记模板 ==========

-- 登记期校验：路径必须存在，重名会被拒绝（先 templateRemove 才能重登记）。
-- 模板文件建议与脚本放在同一资源目录，用相对或绝对路径均可。
local TEMPLATES = {
    { name = "start_button", path = "templates/start_button.png" },
    { name = "shop_icon",    path = "templates/shop_icon.png" },
    { name = "close_x",      path = "templates/close_x.png" },
}

local function registerAll()
    for _, tpl in ipairs(TEMPLATES) do
        if wingman.vision.templateAdd(tpl.name, tpl.path) then
            print(string.format("登记成功: %s -> %s", tpl.name, tpl.path))
        else
            print(string.format("登记失败（路径不存在或重名）: %s", tpl.name))
        end
    end
end

registerAll()

-- ========== 查询登记状态 ==========

print("\n--- 登记表 ---")
for _, item in ipairs(wingman.vision.templateList()) do
    print(string.format("  %s => %s", item.name, item.path))
end
print("templateHas('shop_icon'): " .. tostring(wingman.vision.templateHas("shop_icon")))

-- ========== 批量识别 ==========

-- 一次调用在屏幕上找全部已登记模板；返回列表每项形如
-- { name, found, position?, confidence?, region? }，未命中项 found=false。
-- names 过滤只找指定模板（未知名跳过）；threshold/region 语义同 findImage。
print("\n--- 全量批量识别 ---")
for _, hit in ipairs(wingman.vision.templateFindAll()) do
    if hit.found then
        print(string.format("命中 %s @ (%d, %d) conf=%.2f",
            hit.name, hit.position.x, hit.position.y, hit.confidence))
    else
        print(string.format("未命中 %s", hit.name))
    end
end

print("\n--- 过滤批量识别（只找 start_button / close_x，阈值 0.85） ---")
local hits = wingman.vision.templateFindAll({ "start_button", "close_x" }, 0.85)
for _, hit in ipairs(hits) do
    print(string.format("  %s: %s", hit.name, hit.found and "命中" or "未命中"))
end

-- 典型用法：命中即点击
for _, hit in ipairs(wingman.vision.templateFindAll()) do
    if hit.found and hit.name == "close_x" then
        wingman.input.click(hit.position.x, hit.position.y)
        print("已点击 close_x")
        break
    end
end

-- ========== 注销模板 ==========

print("\n--- 注销 ---")
for _, tpl in ipairs(TEMPLATES) do
    wingman.vision.templateRemove(tpl.name)
end
print("templateHas('shop_icon'): " .. tostring(wingman.vision.templateHas("shop_icon")))

print("\n=== 完成 ===")
