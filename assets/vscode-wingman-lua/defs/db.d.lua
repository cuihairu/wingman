--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— db 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== db 模块 ==========

---@class db
local db = {}

---@param conn any
---@return integer
function db.changes(conn) end

---@param conn any
---@return boolean
function db.close(conn) end

---@param conn any
---@param sql string
---@param params? table
---@return boolean
function db.execute(conn, sql, params) end

---@param conn any
---@return integer
function db.last_insert_id(conn) end

---@param name string
---@return any
function db.open(name) end

---@param conn any
---@param sql string
---@param params? table
---@param maxRows? integer
---@return any
function db.query(conn, sql, params, maxRows) end

---@param query any
---@return any
function db.query_all(query) end

---@param query any
---@return boolean
function db.query_close(query) end

---@param query any
---@return integer
function db.query_count(query) end

---@param query any
---@return integer
function db.query_delete(query) end

---@param query any
---@return any
function db.query_first(query) end

---@param query any
---@param n integer
---@return any
function db.query_limit(query, n) end

---@param query any
---@param field string
---@param direction string
---@return any
function db.query_order_by(query, field, direction) end

---@param query any
---@param row table
---@return boolean
function db.query_update(query, row) end

---@param conn any
---@param sql string
---@param params? table
---@return string
function db.scalar(conn, sql, params) end

---@param conn any
---@param tableName string
---@return table
function db.table(conn, tableName) end

---@param table table
---@return any
function db.table_all(table) end

---@param table table
---@return boolean
function db.table_close(table) end

---@param table table
---@return integer
function db.table_count(table) end

---@param table table
---@param schema table
---@return boolean
function db.table_create(table, schema) end

---@param table table
---@param id string
---@return any
function db.table_get(table, id) end

---@param table table
---@param row table
---@return boolean
function db.table_insert(table, row) end

---@param table table
---@param field string
---@param op string
---@param value string
---@return any
function db.table_where(table, field, op, value) end

---@param conn any
---@param callback function
---@return boolean
function db.transaction(conn, callback) end

return db
