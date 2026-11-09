--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— crypto 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== crypto 模块 ==========

---@class crypto
local crypto = {}

---@param base64_string string
---@return any
function crypto.base64Decode(base64_string) end

---@param data string
---@return any
function crypto.base64Encode(data) end

---@param ciphertext any
---@param password string
---@return any
function crypto.decryptAES(ciphertext, password) end

---@param password string
---@param salt any
---@param iterations? integer
---@param keyLen? integer
---@return any
function crypto.deriveKey(password, salt, iterations, keyLen) end

---@param plaintext string
---@param password string
---@param salt string
---@return any
function crypto.encryptAES(plaintext, password, salt) end

---@param length? integer
---@return any
function crypto.generateSalt(length) end

---@param hex_string string
---@return any
function crypto.hexDecode(hex_string) end

---@param data string
---@return any
function crypto.hexEncode(data) end

---@param length? integer
---@return any
function crypto.randomBytes(length) end

---@param data string
---@return any
function crypto.sha256(data) end

---@param data string
---@return any
function crypto.sha512(data) end

return crypto
