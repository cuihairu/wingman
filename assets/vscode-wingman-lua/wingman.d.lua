--[[
    Wingman Lua API 类型定义
    用于 EmmyLua VSCode 插件提供代码提示和自动补全

    使用方法：
    1. 将此文件放在你的脚本项目根目录
    2. VSCode 安装 EmmyLua 插件
    3. 享受代码提示！
]]

-- ========== 通用类型 ==========

---@class Point
---@field public x integer @X坐标
---@field public y integer @Y坐标
local Point = {}

---@class Rect
---@field public x integer @起始X坐标
---@field public y integer @起始Y坐标
---@field public width integer @宽度
---@field public height integer @高度
local Rect = {}

---@class Color
---@field public r integer @红色 0-255
---@field public g integer @绿色 0-255
---@field public b integer @蓝色 0-255
---@field public a integer @透明度 0-255
local Color = {}

---@class Size
---@field public width integer @宽度
---@field public height integer @高度
local Size = {}

-- ========== Screen 模块 ==========

---@class Screen
local screen = {}

---截取屏幕区域
---@param x integer @起始X坐标
---@param y integer @起始Y坐标
---@param w integer @宽度
---@param h integer @高度
---@return string @PNG编码的图像数据(base64)
function screen.capture(x, y, w, h) end

---获取指定像素颜色
---@param x integer @X坐标
---@param y integer @Y坐标
---@return Color @颜色值
function screen.getPixel(x, y) end

---查找指定颜色
---@param color integer @颜色值 (0xRRGGBB)
---@param x integer @起始X
---@param y integer @起始Y
---@param w integer @宽度
---@param h integer @高度
---@param tolerance integer @容差 0-255
---@return Point[] @匹配的点列表
function screen.findColor(color, x, y, w, h, tolerance) end

---查找图像
---@param template string @模板图像(base64)
---@param x integer @起始X
---@param y integer @起始Y
---@param w integer @宽度
---@param h integer @高度
---@param threshold number @匹配阈值 0-1
---@return Point|nil @匹配位置
function screen.findImage(template, x, y, w, h, threshold) end

---获取屏幕尺寸
---@return Size @屏幕尺寸
function screen.size() end

-- ========== Input 模块 ==========

---@class Input
local input = {}

---鼠标点击
---@param x integer @X坐标
---@param y integer @Y坐标
---@param button? string @按钮类型 "left"|"right"|"middle"
function input.click(x, y, button) end

---鼠标移动
---@param x integer @目标X坐标
---@param y integer @目标Y坐标
---@param duration? integer @持续时间(毫秒)
function input.move(x, y, duration) end

---鼠标按下
---@param button string @按钮类型
function input.mouseDown(button) end

---鼠标释放
---@param button string @按钮类型
function input.mouseUp(button) end

---按键按下
---@param key string @按键名称或虚拟键码
function input.keyDown(key) end

---按键释放
---@param key string @按键名称或虚拟键码
function input.keyUp(key) end

---按键点击(按下+释放)
---@param key string @按键名称
function input.keyClick(key) end

---输入文本
---@param text string @要输入的文本
function input.type(text) end

-- ========== Window 模块 ==========

---@class WindowInfo
---@field public hwnd number @窗口句柄
---@field public title string @窗口标题
---@field public className string @窗口类名
---@field public rect Rect @窗口位置
---@field public visible boolean @是否可见

---@class Window
local window = {}

---获取所有窗口
---@return WindowInfo[] @窗口列表
function window.list() end

---根据标题查找窗口
---@param title string @窗口标题(支持通配符)
---@return WindowInfo|nil @窗口信息
function window.find(title) end

---激活窗口
---@param hwnd number @窗口句柄
---@return boolean @是否成功
function window.activate(hwnd) end

---获取窗口位置
---@param hwnd number @窗口句柄
---@return Rect @窗口位置
function window.getRect(hwnd) end

---设置窗口位置
---@param hwnd number @窗口句柄
---@param x integer @X坐标
---@param y integer @Y坐标
function window.setPos(hwnd, x, y) end

---设置窗口大小
---@param hwnd number @窗口句柄
---@param w integer @宽度
---@param h integer @高度
function window.setSize(hwnd, w, h) end

-- ========== Process 模块 ==========

---@class ProcessInfo
---@field public pid integer @进程ID
---@field public name string @进程名称
---@field public exePath string @可执行文件路径

---@class Process
local process = {}

---获取所有进程
---@return ProcessInfo[] @进程列表
function process.list() end

---根据名称查找进程
---@param name string @进程名称
---@return ProcessInfo|nil @进程信息
function process.find(name) end

---启动进程
---@param path string @可执行文件路径
---@param args? string[] @命令行参数
---@return integer @进程ID
function process.start(path, args) end

---终止进程
---@param pid integer @进程ID
---@return boolean @是否成功
function process.kill(pid) end

-- ========== Trigger 模块 ==========

---@class TriggerConfig
---@field public name string @触发器名称
---@field public enabled boolean @是否启用
---@field public condition table @条件配置
---@field public action table @动作配置
---@field public cooldown integer @冷却时间(毫秒)

---@class Trigger
local trigger = {}

---添加触发器
---@param config TriggerConfig @触发器配置
---@return string @触发器ID
function trigger.add(config) end

---移除触发器
---@param id string @触发器ID
---@return boolean @是否成功
function trigger.remove(id) end

---启用触发器
---@param id string @触发器ID
function trigger.enable(id) end

---禁用触发器
---@param id string @触发器ID
function trigger.disable(id) end

---获取所有触发器
---@return TriggerConfig[] @触发器列表
function trigger.list() end

-- ========== Human 模块 ==========

---@class Human
local human = {}

---人性化鼠标移动(贝塞尔曲线)
---@param x integer @目标X
---@param y integer @目标Y
---@param duration? integer @持续时间
function human.move(x, y, duration) end

---人性化点击
---@param x integer @X坐标
---@param y integer @Y坐标
function human.click(x, y) end

---设置随机延迟范围
---@param minMs integer @最小延迟(毫秒)
---@param maxMs integer @最大延迟(毫秒)
function human.setDelay(minMs, maxMs) end

---随机延迟
---@param minMs integer @最小延迟
---@param maxMs integer @最大延迟
function human.delay(minMs, maxMs) end

-- ========== Log 模块 ==========

---@class Log
local log = {}

---输出日志
---@param message string @日志消息
function log.info(message) end

---输出警告
---@param message string @警告消息
function log.warn(message) end

---输出错误
---@param message string @错误消息
function log.error(message) end

---输出调试信息
---@param message string @调试消息
function log.debug(message) end

-- ========== Macro 模块 ==========

---@class Macro
local macro = {}

---开始录制
---@param name string @宏名称
function macro.startRecording(name) end

---停止录制
---@return boolean @是否成功
function macro.stopRecording() end

---播放宏
---@param name string @宏名称
---@param repeatCount? integer @重复次数
function macro.play(name, repeatCount) end

---删除宏
---@param name string @宏名称
function macro.delete(name) end

---列出所有宏
---@return string[] @宏名称列表
function macro.list() end

-- ========== Http 模块 ==========

---@class HttpResponse
---@field public status integer @HTTP状态码
---@field public body string @响应体
---@field public headers table @响应头

---@class Http
local http = {}

---GET 请求
---@param url string @URL
---@return HttpResponse @响应
function http.get(url) end

---POST 请求
---@param url string @URL
---@param data string|table @请求数据
---@return HttpResponse @响应
function http.post(url, data) end

---PUT 请求
---@param url string @URL
---@param data string|table @请求数据
---@return HttpResponse @响应
function http.put(url, data) end

---DELETE 请求
---@param url string @URL
---@return HttpResponse @响应
function http.delete(url) end

-- ========== KV 模块 ==========

---@class KV
local kv = {}

---设置键值
---@param key string @键
---@param value string @值
function kv.set(key, value) end

---获取键值
---@param key string @键
---@return string|nil @值
function kv.get(key) end

---删除键
---@param key string @键
function kv.delete(key) end

---列出所有键
---@return string[] @键列表
function kv.keys() end

-- ========== Team 模块 ==========

---@class Team
local team = {}

---获取当前会话ID
---@return string @会话ID
function team.getSessionId() end

---获取节点ID
---@return string @节点ID
function.team.getNodeId() end

---发送消息
---@param channel string @频道
---@param data table @数据
function team.send(channel, data) end

---广播消息
---@param data table @数据
function team.broadcast(data) end

-- ========== 导出 ==========

local wingman = {
    screen = screen,
    input = input,
    window = window,
    process = process,
    trigger = trigger,
    human = human,
    log = log,
    macro = macro,
    http = http,
    kv = kv,
    team = team
}

return wingman
