from __future__ import annotations

# 工作流在 runtime 内本地执行：submit_workflow 立即返回 workflowId，任务在
# 独立调度线程上按依赖序串行执行（v1 不并发，并发控制为后续批次项）。run
# 必须是线程安全可调用体（Python 函数；Lua 可调用体提交即拒绝并发出
# orchestration.error 事件）。拒绝路径（坏定义/依赖环/非线程安全可调用体）
# 一律返回 None 并发 orchestration.error 事件（负载 {"error": 原因}）。
#
# workflow 定义：{name?: str, tasks: [{id: str, run: callable,
#   dependsOn?: [str], timeoutMs?: int, maxRetries?: int,
#   backoffMs?: int, backoffFactor?: float}]}
# —— dependsOn 为前置任务 id 集合（前置全部 succeeded 才调度；重复 id、
#    未知前置、自依赖、依赖环提交即拒）。timeoutMs/maxRetries/backoffMs/
#    backoffFactor 与 task.submit 同词汇表同默认值（30000/0/500/2.0）。
#
# 任务状态：blocked（前置未满足）/ pending（就绪待调度）/ running /
# skipped（前置 failed/canceled/timeout 连锁跳过）/ canceled（cancel_workflow
# 取消，含未开工任务）/ succeeded / failed / timeout（终态，task 模块语义）。
# 工作流状态：running / succeeded（全部任务 succeeded）/ failed（存在失败或
# 被跳过任务）/ canceled（cancel_workflow 请求后定稿）。
# get_workflow/get_all_workflows 返回快照：
# {id: str, name: str, status: str, tasks: [{id, status, dependsOn, error?}]}，
# error 仅失败任务携带（异常消息，task 模块语义）。
# 终局工作流驻留注册表直至进程退出（v1 不自动清理）。

def submit_workflow(workflow: dict) -> str | None: ...
def cancel_workflow(workflowId: str) -> bool: ...
def get_workflow(workflowId: str) -> dict | None: ...
def get_all_workflows() -> list[dict]: ...
