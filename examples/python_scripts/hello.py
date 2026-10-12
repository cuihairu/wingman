# Wingman Python 入门示例
# 与 hello_world.lua 对应的最小示例：KV 存储 + 通知。
#
# 命名约定：所有模块函数同时暴露 camelCase 原名与 snake_case 别名
# （如 enableAutoSave 也可写作 enable_auto_save）。推荐统一用
# snake_case 书写，与 libs/python/typing 提供的桩一致。

import wingman

print("=== Python 入门示例 ===")

# ========== KV 存储 ==========

wingman.kv.set("demo:name", "Wingman")
wingman.kv.set("demo:count", "1")

print("name =", wingman.kv.get("demo:name"))
print("exists('demo:name') =", wingman.kv.exists("demo:name"))

# 原生计数器（无需读改写）
wingman.kv.incr("demo:count")
print("count after incr =", wingman.kv.get("demo:count"))

# 过期键：30 秒后自动消失
wingman.kv.set("demo:tmp", "soon-gone")
wingman.kv.expire("demo:tmp", 30)
print("ttl('demo:tmp') =", wingman.kv.ttl("demo:tmp"))

# ========== 通知 ==========

wingman.notify.info("Python 脚本运行中")
wingman.notify.toast("demo", "Python 脚本运行完成")

# ========== 清理 ==========

wingman.kv.delete(["demo:name", "demo:count", "demo:tmp"])

print("=== 完成 ===")
