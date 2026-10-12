# Wingman 模板批量管理示例
# 具名登记模板 + 一次调用批量识别（与 lua_scripts/template_batch.lua 对应）。
# 单张找图见 image_matching。

import wingman

print("=== 模板批量管理示例 ===")

# ========== 登记模板 ==========

# 登记期校验：路径必须存在，重名会被拒绝（先 template_remove 才能重登记）。
TEMPLATES = [
    ("start_button", "templates/start_button.png"),
    ("shop_icon", "templates/shop_icon.png"),
    ("close_x", "templates/close_x.png"),
]

for name, path in TEMPLATES:
    if wingman.vision.template_add(name, path):
        print(f"登记成功: {name} -> {path}")
    else:
        print(f"登记失败（路径不存在或重名）: {name}")

# ========== 查询登记状态 ==========

print("--- 登记表 ---")
for item in wingman.vision.template_list():
    print(f"  {item['name']} => {item['path']}")
print("template_has('shop_icon') =", wingman.vision.template_has("shop_icon"))

# ========== 批量识别 ==========

# 一次调用在屏幕上找全部已登记模板；返回列表每项形如
# {"name", "found", "position"?, "confidence"?, "region"?}，未命中项 found=False。
print("--- 全量批量识别 ---")
for hit in wingman.vision.template_find_all():
    if hit["found"]:
        pos = hit["position"]
        print(f"命中 {hit['name']} @ ({pos['x']}, {pos['y']}) conf={hit['confidence']:.2f}")
    else:
        print(f"未命中 {hit['name']}")

# names 过滤只找指定模板（未知名跳过）；threshold/region 语义同 find_image。
print("--- 过滤批量识别（只找 start_button / close_x，阈值 0.85） ---")
for hit in wingman.vision.template_find_all(["start_button", "close_x"], 0.85):
    print(f"  {hit['name']}: {'命中' if hit['found'] else '未命中'}")

# 典型用法：命中即点击
for hit in wingman.vision.template_find_all():
    if hit["found"] and hit["name"] == "close_x":
        wingman.input.click(hit["position"]["x"], hit["position"]["y"])
        print("已点击 close_x")
        break

# ========== 注销模板 ==========

print("--- 注销 ---")
for name, _ in TEMPLATES:
    wingman.vision.template_remove(name)
print("template_has('shop_icon') =", wingman.vision.template_has("shop_icon"))

print("=== 完成 ===")
