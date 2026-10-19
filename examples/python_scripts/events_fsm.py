# Wingman 事件订阅 + FSM 状态机示例
# event 模块（on/once/emit/off）与 fsm 模块（create/state/transition/dispatch）
# 组合的典型用法：状态机迁移事件回灌事件面，事件面驱动状态机。
#
# 系统事件源（进程/窗口/文件变化）见 systemwatch / filewatcher 模块，
# 它们与这里的自定义事件共用同一个订阅面。

import wingman

print("=== 事件订阅 + 状态机示例 ===")

# ========== 事件订阅 ==========

fired = []

def on_login(msg):
    fired.append(("login", msg["payload"]))
    print("收到 login 事件, payload =", msg["payload"])

# on 返回订阅 id，off(id) 注销；第三参 name 按位置传（可选，便于 listeners() 里辨识）
sub_id = wingman.event.on("demo.login", on_login, "login-watcher")

# once 只触发一次
wingman.event.once("demo.boot", lambda msg: print("boot 事件（once，只此一次）"))

# emit 的 meta 可带 source / correlationId / priority
wingman.event.emit("demo.boot", {"v": 1})
wingman.event.emit("demo.login", {"user": "alice"}, {"source": "demo-script"})
wingman.event.emit("demo.login", {"user": "bob"})

print("login 触发次数 =", len(fired))
print("demo.login 订阅数 =", len(wingman.event.listeners("demo.login")))

# ========== 状态机 ==========

# create 返回机器 id；state/transition 挂回调与迁移
machine = wingman.fsm.create("task_flow", "idle")
wingman.fsm.state(machine, "idle")
# 回调均按位置传参（绑定层不收关键字参数）：state(machine, name, onEnter?, onExit?)
wingman.fsm.state(machine, "running", lambda ctx: print("进入 running, 事件 =", ctx["event"]))
wingman.fsm.state(machine, "done")

# transition(machine, from, to, on?, guard?, action?)
wingman.fsm.transition(machine, "idle", "running", "start")
wingman.fsm.transition(machine, "running", "done", "finish", None,
                       lambda ctx: print("迁移动作触发, 到达 =", ctx["to"]))

print("初始状态 =", wingman.fsm.current(machine))
wingman.fsm.dispatch(machine, "start")
print("dispatch start 后 =", wingman.fsm.current(machine))
wingman.fsm.dispatch(machine, "finish")
print("dispatch finish 后 =", wingman.fsm.current(machine))

# 未定义迁移被拒绝（返回 False，状态不变）
print("非法迁移结果 =", wingman.fsm.dispatch(machine, "start"))
print("终态 =", wingman.fsm.current(machine))

# ========== 注销 ==========

print("off(sub_id) =", wingman.event.off(sub_id))
print("=== 完成 ===")
