--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— kv 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== kv 模块 ==========

---@class kv
local kv = {}

---@param key string
function kv.delete(key) end

---@param path string
---@param intervalSeconds integer
function kv.enableAutoSave(path, intervalSeconds) end

---@param key string
---@return boolean
function kv.exists(key) end

---@param key string
---@param seconds integer
function kv.expire(key, seconds) end

---@param key string
---@return string
function kv.get(key) end

---@param hash string
---@param field string
function kv.hdel(hash, field) end

---@param hash string
---@param field string
---@return boolean
function kv.hexists(hash, field) end

---@param hash string
---@param field string
---@return string
function kv.hget(hash, field) end

---@param hash string
---@return any
function kv.hgetall(hash) end

---@param hash string
---@return any
function kv.hkeys(hash) end

---@param hash string
---@param field string
---@param value string
function kv.hset(hash, field, value) end

---@param key string
---@param delta integer
---@return integer
function kv.incr(key, delta) end

---@param list string
---@return integer
function kv.llen(list) end

---@param path string
---@return boolean
function kv.load(path) end

---@param list string
---@return string
function kv.lpop(list) end

---@param list string
---@param value string
function kv.lpush(list, value) end

---@param list string
---@param start integer
---@param stop integer
---@return any
function kv.lrange(list, start, stop) end

---@param list string
---@return string
function kv.rpop(list) end

---@param list string
---@param value string
function kv.rpush(list, value) end

---@param path string
---@return boolean
function kv.save(path) end

---@param key string
---@param value string
---@param options? any
function kv.set(key, value, options) end

---@param key string
---@return integer
function kv.ttl(key) end

return kv
