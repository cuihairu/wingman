# Wingman 工作流编排示例
# orchestration.submit_workflow：任务依赖（dependsOn）、分支条件（when）、
# 并发额度（maxParallel）与产出聚合（上游 result 经 get_workflow 读取）。
#
# 注意：run/when 必须是线程安全可调用体——Python 函数即可；Lua 函数体会
# 被提交即拒（发 orchestration.error 事件），所以工作流编排示例只给 Python。
# 拒绝路径（坏定义/依赖环/非法 maxParallel）返回 None，事件面可见原因。
#
# 沙箱提示：脚本默认沙箱运行，import 仅放行 wingman——轮询等待用
# wingman.util.sleep(毫秒)，不要 import time。

import wingman

print("=== 工作流编排示例 ===")

# ========== 定义任务 ==========

# 上游产出任意 ScriptValue（Int/String/Object...），原样落进快照 result 字段
def fetch():
    return {"items": [1, 2, 3], "source": "demo"}

# 下游在 run 内经 get_workflow 读上游 result，实现数据流组合
def process():
    snap = wingman.orchestration.get_workflow(wf_id)
    upstream = next(t for t in snap["tasks"] if t["id"] == "fetch")
    return sum(upstream["result"]["items"])

# when 守卫：前置满足后的调度点求值一次；False 走条件链 skipped（不算失败）
def should_report():
    return True

def report():
    snap = wingman.orchestration.get_workflow(wf_id)
    for t in snap["tasks"]:
        if t["id"] == "process":
            print(f"上报处理结果: {t['result']}")
    return "reported"

wf_id = wingman.orchestration.submit_workflow({
    "name": "demo-pipeline",
    "maxParallel": 1,  # 默认 1 串行；>1 时依赖边仍优先于并发额度
    "tasks": [
        {"id": "fetch", "run": fetch, "timeoutMs": 5000},
        {"id": "process", "run": process, "dependsOn": ["fetch"]},
        {"id": "report", "run": report, "dependsOn": ["process"], "when": should_report},
        # 分支示例：条件不成立的任务沿依赖图连锁 skipped，工作流仍 succeeded
        {"id": "report_zh", "run": report, "dependsOn": ["process"],
         "when": lambda: False},
    ],
})

if wf_id is None:
    print("工作流被拒绝（查 orchestration.error 事件）")
    raise SystemExit(1)

print("workflowId =", wf_id)

# ========== 轮询到终局 ==========

while True:
    snap = wingman.orchestration.get_workflow(wf_id)
    if snap["status"] in ("succeeded", "failed", "canceled"):
        break
    wingman.util.sleep(100)

print("--- 快照 ---")
print("status =", snap["status"])
for t in snap["tasks"]:
    extra = f" result={t['result']}" if "result" in t else ""
    extra += f" error={t['error']}" if "error" in t else ""
    print(f"  {t['id']}: {t['status']}{extra}")

# ========== 拒绝路径示例 ==========

# 未知前置引用：提交即拒
bad = wingman.orchestration.submit_workflow({
    "tasks": [{"id": "a", "run": fetch, "dependsOn": ["ghost"]}],
})
print("坏定义提交返回:", bad)  # None

print("=== 完成 ===")
