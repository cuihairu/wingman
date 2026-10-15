from __future__ import annotations

# 工作流在 runtime 内本地执行：submit_workflow 立即返回 workflowId，单调度
# 线程做调度决策，就绪任务派发到独立 worker 线程执行。工作流内并发度受
# maxParallel 约束（默认 1 = 串行；<1 提交即拒）。run/when 必须是线程安全
# 可调用体（Python 函数；Lua 可调用体提交即拒绝并发出 orchestration.error
# 事件）。拒绝路径（坏定义/依赖环/非法 maxParallel/非线程安全可调用体）
# 一律返回 None 并发 orchestration.error 事件（负载 {"error": 原因}）。
#
# workflow 定义：{name?: str, maxParallel?: int, tasks: [{id: str,
#   run: callable, dependsOn?: [str], when?: callable, timeoutMs?: int,
#   maxRetries?: int, backoffMs?: int, backoffFactor?: float}]}
# —— dependsOn 为前置任务 id 集合（前置全部 succeeded 才调度；重复 id、
#    未知前置、自依赖、依赖环提交即拒）。timeoutMs/maxRetries/backoffMs/
#    backoffFactor 与 task.submit 同词汇表同默认值（30000/0/500/2.0）。
# —— when 为分支条件，在前置满足后的调度点求值一次（锁外，谓词可重入
#    get_workflow/cancel_workflow）：真值口径 Bool 按值、Int/Float 非 0、
#    String/Array/Object 非空、Null 假；不成立落条件链 skipped（分支过滤，
#    不判工作流失败），后置沿依赖图连锁跳过；混合前置时失败链优先。谓词
#    求值异常按 task 模块失败语义落 failed（error "when predicate threw: ..."）。
#
# 任务状态：blocked（前置未满足）/ pending（就绪待调度）/ running（执行中，
# 协作取消即时反映 canceled 不等工作体返回）/ skipped（失败链或条件链传播，
# 快照不区分根因）/ canceled（cancel_workflow 取消，含未开工任务）/
# succeeded / failed / timeout（终态，task 模块语义）。
# 工作流状态：running / succeeded（全部任务 succeeded 或条件链 skipped）/
# failed（存在失败链任务：failed/timeout 及其传播的 skipped）/ canceled
# （cancel_workflow 请求后定稿，执行中任务协作取消、worker 后台收尾）。
# get_workflow/get_all_workflows 返回快照：
# {id: str, name: str, status: str, tasks: [{id, status, dependsOn, error?, result?}]}，
# error 仅失败任务携带（异常消息，task 模块语义）；result 仅落账成功任务
# 携带（run() 返回值原样聚合，返回 null 则字段省略），下游任务可经
# get_workflow 读取上游产出实现数据流。
# 终局工作流驻留注册表直至进程退出（v1 不自动清理）。
#
# 历史账本：提交与终局落 app 数据目录 JSONL（路径可用环境变量
# WINGMAN_WORKFLOW_LEDGER 重定向，app 数据目录不可得时账本关闭）。任务体是
# 脚本闭包不可序列化，故只落账不自动恢复——重启把上次遗留的在途条目补一条
# interrupted 终局（进程内一次，首次 submit_workflow/history 访问时执行，并发
# orchestration.interrupted 事件），不重跑工作流。history 返回视图，最新在前：
# {id, name, run, status, startedAt, finishedAt?, interrupted?, tasks?}，
# 进行中（仅提交）报 running 且不带 finishedAt 并置顶；run 区分跨进程重号 id。

def submit_workflow(workflow: dict) -> str | None: ...
def cancel_workflow(workflowId: str) -> bool: ...
def get_workflow(workflowId: str) -> dict | None: ...
def get_all_workflows() -> list[dict]: ...
def history(limit: int = 20) -> list[dict]: ...
