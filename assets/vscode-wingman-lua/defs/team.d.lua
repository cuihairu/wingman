--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）—— team 模块

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成；CI 的 api-typing-audit job
    重生成后 git diff --exit-code 保持同步。

    与 wingman.d.lua 聚合面配套：将 wingman.d.lua 和 defs/ 目录一起
    放进你的脚本项目根目录，VSCode 安装 EmmyLua 插件即可获得代码提示
    （保留字函数如 bt.repeat 以 ["repeat"] 形式补全）。
]=]

-- ========== team 模块 ==========

---@class team
local team = {}

---@param message any
---@return boolean
function team.broadcast(message) end

---@param voteId string
---@param response string
---@return boolean
function team.castVote(voteId, response) end

---@param subject string
---@param timeout? number
---@return boolean
function team.createVote(subject, timeout) end

---@return any
function team.getMemberId() end

---@return any
function team.getTeamStatus() end

---@param voteId string
---@return string
function team.getVoteResult(voteId) end

---@return any
function team.isJoined() end

---@param teamId string
---@param memberId? string
---@return boolean
function team.joinTeam(teamId, memberId) end

---@return any
function team.leaveTeam() end

---@param event string
---@param callback function
---@return boolean
function team.on(event, callback) end

---@param status any
---@return boolean
function team.reportStatus(status) end

return team
