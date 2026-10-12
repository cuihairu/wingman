--[=[
    Wingman Lua API 类型定义（生成文件，勿手改）

    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中
    mod.functions.push_back 注册面）生成，覆盖全部脚本模块与函数签名；
    CI 的 api-typing-audit job 重生成后 git diff --exit-code 保持同步。

    使用方法：
    1. 将此文件放在你的脚本项目根目录
    2. VSCode 安装 EmmyLua 插件
    3. 享受代码提示！（保留字函数如 bt.repeat 以 ["repeat"] 形式补全）
]=]

-- ========== bt 模块 ==========

---@class bt
local bt = {}

---@param name string
---@param callback function
---@return any
function bt.action(name, callback) end

---@param parentHandle integer
---@param childHandle integer
---@return boolean
function bt.addChild(parentHandle, childHandle) end

---@param name string
---@param callback function
---@return any
function bt.condition(name, callback) end

---@param name string
---@return boolean
function bt.create(name) end

---@param childHandle integer
---@return any
function bt.inverter(childHandle) end

---@param name string
---@param policy string
---@return any
function bt.parallel(name, policy) end

---@param name string
function bt.remove(name) end

---@param childHandle integer
---@param count integer
---@return any
bt["repeat"] = function(childHandle, count) end

---@param name string
---@return any
function bt.selector(name) end

---@param name string
---@return any
function bt.sequence(name) end

---@param treeName string
---@param nodeHandle integer
---@return boolean
function bt.setRoot(treeName, nodeHandle) end

---@param name string
---@return string
function bt.tick(name) end

---@param milliseconds integer
---@return any
function bt.wait(milliseconds) end


-- ========== clipboard 模块 ==========

---@class clipboard
local clipboard = {}

---@return any
function clipboard.clear() end

---@return any
function clipboard.getFiles() end

---@return any
function clipboard.getHTML() end

---@return any
function clipboard.getImage() end

---@return any
function clipboard.getText() end

---@return any
function clipboard.hasFiles() end

---@return any
function clipboard.hasHTML() end

---@return any
function clipboard.hasImage() end

---@return any
function clipboard.hasText() end

---@return any
function clipboard.isEmpty() end

---@param files any
---@return boolean
function clipboard.setFiles(files) end

---@param html string
---@return boolean
function clipboard.setHTML(html) end

---@param imageData string
---@param width integer
---@param height integer
---@return boolean
function clipboard.setImage(imageData, width, height) end

---@param text string
---@return boolean
function clipboard.setText(text) end


-- ========== config 模块 ==========

---@class config
local config = {}

---@param key string
---@return string
function config.get(key) end

---@return any
function config.load() end

---@param key string
---@return boolean
function config.remove(key) end

---@return any
function config.save() end

---@param key string
---@param value string
function config.set(key, value) end


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


-- ========== debugger 模块 ==========

---@class debugger
local debugger = {}

---@return any
function debugger.breakHere() end

---@param file string
---@param line integer
---@return string
function debugger.breakpoint(file, line) end

---@return any
function debugger.start() end

---@return any
function debugger.stop() end


-- ========== event 模块 ==========

---@class event
local event = {}

---@param type? string
function event.clear(type) end

---@param type string
---@param payload? table
---@param meta? any
---@return boolean
function event.emit(type, payload, meta) end

---@param id any
---@return any
function event.listener(id) end

---@param type string
---@return any
function event.listeners(type) end

---@param type string
---@param payload? table
---@param meta? table
---@return any
function event.message(type, payload, meta) end

---@param subscription any
---@return boolean
function event.off(subscription) end

---@param type string
---@param callback function
---@param name? string
---@return any
function event.on(type, callback, name) end

---@param type string
---@param callback function
---@return any
function event.once(type, callback) end


-- ========== file 模块 ==========

---@class file
local file = {}

---@param path string
---@param content string
---@return boolean
function file.append(path, content) end

---@param src string
---@param dst string
---@return boolean
function file.copy(src, dst) end

---@param path string
---@return boolean
function file.exists(path) end

---@param path string
---@return boolean
function file.isDir(path) end

---@param path string
---@return boolean
function file.isFile(path) end

---@param path string
---@return any
function file.listDir(path) end

---@param path string
---@return boolean
function file.mkdir(path) end

---@param src string
---@param dst string
---@return boolean
function file.move(src, dst) end

---@param path string
---@return any
function file.read(path) end

---@param path string
---@return boolean
function file.remove(path) end

---@param path string
---@return boolean
function file.removeAll(path) end

---@param path string
---@return any
function file.size(path) end

---@param path string
---@param content string
---@return boolean
function file.write(path, content) end


-- ========== filewatcher 模块 ==========

---@class filewatcher
local filewatcher = {}

---@return any
function filewatcher.getWatchedPaths() end

---@param path string
---@return boolean
function filewatcher.isWatching(path) end

---@param path string
---@return boolean
function filewatcher.unwatch(path) end

---@return any
function filewatcher.unwatchAll() end

---@param path string
---@param callback function
---@param recursive? boolean
---@return any
function filewatcher.watch(path, callback, recursive) end


-- ========== fsm 模块 ==========

---@class fsm
local fsm = {}

---@param name string
---@param initial string
---@return any
function fsm.create(name, initial) end

---@param machineId string
---@return any
function fsm.current(machineId) end

---@param machineId string
---@param event string
---@param payload? table
---@return boolean
function fsm.dispatch(machineId, event, payload) end

---@param machineId string
---@return boolean
function fsm.reset(machineId) end

---@param machineId string
---@param stateName string
---@param onEnter? function
---@param onExit? function
---@return boolean
function fsm.state(machineId, stateName, onEnter, onExit) end

---@param machineId string
---@param from string
---@param to string
---@param on? string
---@param guard? function
---@param action? function
---@return boolean
function fsm.transition(machineId, from, to, on, guard, action) end


-- ========== gameprofile 模块 ==========

---@class gameprofile
local gameprofile = {}

---@param gameName string
---@return any
function gameprofile.createTemplate(gameName) end

---@param id string
---@return boolean
function gameprofile.delete(id) end

---@param id string
---@return any
function gameprofile.exportJson(id) end

---@param id string
---@param outputPath string
---@return boolean
function gameprofile.exportPackage(id, outputPath) end

---@param title string
---@return any
function gameprofile.findByWindow(title) end

---@param id string
---@return any
function gameprofile.get(id) end

---@return any
function gameprofile.getActive() end

---@return any
function gameprofile.getProfilesDirectory() end

---@param json string
---@param id? string
---@return boolean
function gameprofile.importJson(json, id) end

---@param packagePath string
---@return boolean
function gameprofile.importPackage(packagePath) end

---@return any
function gameprofile.list() end

---@param id string
---@return boolean
function gameprofile.load(id) end

---@param id string
---@return boolean
function gameprofile.save(id) end

---@return any
function gameprofile.scan() end

---@param id string
---@return boolean
function gameprofile.setActive(id) end

---@param dir string
---@return boolean
function gameprofile.setProfilesDirectory(dir) end


-- ========== hotkey 模块 ==========

---@class hotkey
local hotkey = {}

---@param combo string
---@param callback function
---@return any
function hotkey.register(combo, callback) end

---@param id integer
---@return boolean
function hotkey.unregister(id) end


-- ========== http 模块 ==========

---@class http
local http = {}

---@param url string
---@param options? any
---@return any
function http.delete(url, options) end

---@param url string
---@param options? any
---@return any
function http.get(url, options) end

---@param url string
---@param body string
---@param options? any
---@return any
function http.post(url, body, options) end

---@param url string
---@param body string
---@param options? any
---@return any
function http.put(url, body, options) end


-- ========== human 模块 ==========

---@class human
local human = {}

---@return any
function human.getConfig() end

---@param vkCode integer
function human.keyboard_down(vkCode) end

---@param vkCode integer
function human.keyboard_press(vkCode) end

---@param text string
---@param randomCase boolean
function human.keyboard_type(text, randomCase) end

---@param vkCode integer
function human.keyboard_up(vkCode) end

---@param x integer
---@param y integer
function human.mouse_click(x, y) end

---@param x integer
---@param y integer
function human.mouse_doubleClick(x, y) end

---@param fromX integer
---@param fromY integer
---@param toX integer
---@param toY integer
function human.mouse_drag(fromX, fromY, toX, toY) end

---@param x integer
---@param y integer
---@param duration integer
function human.mouse_move(x, y, duration) end

---@param x integer
---@param y integer
function human.mouse_rightClick(x, y) end

---@param x integer
---@param y integer
---@param delta integer
function human.mouse_scroll(x, y, delta) end

---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param duration integer
function human.moveMouse(x1, y1, x2, y2, duration) end

---@param x integer
---@param y integer
---@param button string
function human.naturalClick(x, y, button) end

---@param text string
function human.naturalType(text) end

---@param min integer
---@param max integer
function human.randomDelay(min, max) end

---@param key string
---@param value any
function human.setConfig(key, value) end

---@param min integer
---@param max integer
function human.setDelayRange(min, max) end

---@param speed number
function human.setMoveSpeed(speed) end

---@param variance number
function human.setTypingVariance(variance) end


-- ========== inbox 模块 ==========

---@class inbox
local inbox = {}

---@param handle integer
---@param msgId string
---@return boolean
function inbox.ack(handle, msgId) end

---@param url string
---@param config? table
---@return any
function inbox.connect(url, config) end

---@param handle? integer
---@param timeout? integer
---@return any
function inbox.consume(handle, timeout) end

---@param handle? integer
---@return boolean
function inbox.disconnect(handle) end

---@param handle? integer
---@return boolean
function inbox.heartbeat(handle) end

---@param handle? integer
---@return boolean
function inbox.isConnected(handle) end

---@param handle integer
---@param msgId string
---@param result? table
---@return boolean
function inbox.report(handle, msgId, result) end


-- ========== ini 模块 ==========

---@class ini
local ini = {}

---@param content string
---@return any
function ini.decode(content) end

---@param data table
---@param section string
---@param key? string
---@return boolean
function ini.delete(data, section, key) end

---@param data table
---@return string
function ini.encode(data) end

---@param data table
---@param section string
---@param key? string
---@return any
function ini.get(data, section, key) end

---@param data table
---@param section string
---@param key string
---@return boolean
function ini.has_key(data, section, key) end

---@param data table
---@param section string
---@return boolean
function ini.has_section(data, section) end

---@param data table
---@param section string
---@return any
function ini.keys(data, section) end

---@param base table
---@param override table
---@return table
function ini.merge(base, override) end

---@param data table
---@return any
function ini.sections(data) end

---@param data table
---@param section string
---@param key string
---@param value string
---@return any
function ini.set(data, section, key, value) end


-- ========== input 模块 ==========

---@class input
local input = {}

---@param x integer
---@param y integer
---@param button integer
function input.click(x, y, button) end

---@param ms integer
function input.delay(ms) end

---@param vkCode integer
function input.key(vkCode) end

---@param vkCode integer
function input.keyDown(vkCode) end

---@param vkCode integer
function input.keyUp(vkCode) end

---@param x integer
---@param y integer
---@param duration integer
function input.move(x, y, duration) end

---@param min integer
---@param max integer
function input.randomDelay(min, max) end

---@param x integer
---@param y integer
---@param delta integer
function input.scroll(x, y, delta) end

---@param text string
---@param delay integer
function input.type(text, delay) end


-- ========== json 模块 ==========

---@class json
local json = {}

---@param jsonString string
---@return any
function json.decode(jsonString) end

---@param value any
---@param indent integer
---@return string
function json.encode(value, indent) end


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


-- ========== macro 模块 ==========

---@class macro
local macro = {}

---@return any
function macro.clear() end

---@return any
function macro.getEventCount() end

---@return any
function macro.isPaused() end

---@return any
function macro.isRecording() end

---@param path string
---@return boolean
function macro.loadFromJSON(path) end

---@return any
function macro.pause() end

---@param speed? integer
---@param repeat_? integer
---@return boolean
function macro.playback(speed, repeat_) end

---@return any
function macro.resume() end

---@param path string
---@return boolean
function macro.saveToJSON(path) end

---@param path string
---@return boolean
function macro.saveToLua(path) end

---@return any
function macro.start() end

---@return any
function macro.status() end

---@return any
function macro.stop() end


-- ========== ml 模块 ==========

---@class ml
local ml = {}

---@param modelId string
---@return table
function ml.inputs(modelId) end

---@param modelId string
---@return boolean
function ml.isLoaded(modelId) end

---@param path string
---@param ep? string
---@return any
function ml.loadModel(path, ep) end

---@param modelId string
---@return table
function ml.outputs(modelId) end

---@return any
function ml.providers() end

---@param modelId string
---@param inputs any
---@return any
function ml.run(modelId, inputs) end

---@param modelId string
---@return boolean
function ml.unload(modelId) end


-- ========== node 模块 ==========

---@class node
local node = {}

---@return any
function node.createHeartbeat() end

---@return any
function node.getWindows() end

---@param heartbeat any
function node.sendHeartbeat(heartbeat) end


-- ========== notify 模块 ==========

---@class notify
local notify = {}

---@param eventName string
---@param target string
---@param options? table
function notify.bridge(eventName, target, options) end

---@param message string
---@param meta? table
function notify.debug(message, meta) end

---@param message string
---@param meta? table
function notify.error(message, meta) end

---@param message string
---@param meta? table
function notify.info(message, meta) end

---@param title string
---@param message string
---@param level? string
function notify.toast(title, message, level) end

---@return any
function notify.trayHide() end

---@param text? string
---@return boolean
function notify.traySetBadge(text) end

---@param text string
---@return boolean
function notify.traySetTooltip(text) end

---@return any
function notify.trayShow() end

---@param message string
---@param meta? table
function notify.warn(message, meta) end

---@param url string
---@param payload? table
---@param options? table
function notify.webhook(url, payload, options) end


-- ========== ocr 模块 ==========

---@class ocr
local ocr = {}

---@param region any
---@return any
function ocr.recognize(region) end

---@param region any
---@return string
function ocr.recognizeText(region) end


-- ========== orchestration 模块 ==========

---@class orchestration
local orchestration = {}

---@param workflowId string
---@return boolean
function orchestration.cancel_workflow(workflowId) end

---@return any
function orchestration.get_all_workflows() end

---@param workflowId string
---@return any
function orchestration.get_workflow(workflowId) end

---@param workflow any
---@return any
function orchestration.submit_workflow(workflow) end


-- ========== perf 模块 ==========

---@class perf
local perf = {}

---@return any
function perf.clearCache() end

---@param path string
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param threshold number
---@return any
function perf.fastFindImage(path, x, y, w, h, threshold) end

---@return any
function perf.getCacheSize() end

---@return any
function perf.getCacheStats() end

---@return any
function perf.getConfig() end

---@return any
function perf.getStats() end

---@param color integer
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param tolerance integer
---@param maxCount integer
---@return any
function perf.parallelFindColors(color, x, y, w, h, tolerance, maxCount) end

---@param path string
function perf.preloadImage(path) end

---@return any
function perf.resetStats() end

---@param config any
function perf.setConfig(config) end


-- ========== process 模块 ==========

---@class process
local process = {}

---@param pid integer
---@return boolean
function process.exists(pid) end

---@param name string
---@return any
function process.find(name) end

---@param path string
---@param args string
---@param workdir string
---@return any
function process.start(path, args, workdir) end

---@param pid integer
---@param force boolean
---@return boolean
function process.terminate(pid, force) end

---@param pid integer
---@param timeout integer
---@return boolean
function process.wait(pid, timeout) end

---@param name string
---@param timeout integer
---@return boolean
function process.waitFor(name, timeout) end


-- ========== screen 模块 ==========

---@class screen
local screen = {}

---@return any
function screen.capture() end

---@param region any
---@return boolean
function screen.captureRegion(region) end

---@param color any
---@param region any
---@param tolerance integer
---@return any
function screen.findColor(color, region, tolerance) end

---@param color any
---@param region any
---@param tolerance integer
---@param maxCount integer
---@return any
function screen.findColors(color, region, tolerance, maxCount) end

---@param imagePath string
---@param region any
---@param threshold number
---@return any
function screen.findImage(imagePath, region, threshold) end

---@param x integer
---@param y integer
---@return any
function screen.getPixel(x, y) end

---@return any
function screen.getScreenHeight() end

---@return any
function screen.getScreenWidth() end


-- ========== script 模块 ==========

---@class script
local script = {}

---@param key string
---@return string
function script.getConfig(key) end

---@param name string
---@param key string
---@return string
function script.getEnv(name, key) end

---@param name string
---@return string
function script.getState(name) end

---@param name string
---@return any
function script.getStats(name) end

---@param name string
---@return boolean
function script.has(name) end

---@param name string
---@return boolean
function script.isRunning(name) end

---@return any
function script.list() end

---@param name string
---@param path string
---@param config? any
---@return boolean
function script.load(name, path, config) end

---@param name string
---@return boolean
function script.reload(name) end

---@param name string
---@return boolean
function script.run(name) end

---@param key string
---@param value string
---@return boolean
function script.setConfig(key, value) end

---@param name string
---@param key string
---@param value string
---@return boolean
function script.setEnv(name, key, value) end

---@param enabled boolean
---@return boolean
function script.setHotReload(enabled) end

---@param name string
---@return boolean
function script.stop(name) end


-- ========== security 模块 ==========

---@class security
local security = {}

---@param str string
---@return string
function security.filterSensitive(str) end

---@param length integer
---@return string
function security.generateRandomString(length) end

---@return any
function security.getRandomDelay() end

---@return any
function security.getRandomOffset() end

---@param str string
---@return string
function security.hashString(str) end

---@return any
function security.isDebuggerPresent() end

---@return any
function security.isRunningInVM() end

---@return any
function security.verifyIntegrity() end


-- ========== smarttrigger 模块 ==========

---@class smarttrigger
local smarttrigger = {}

---@param name string
---@param action any
---@return boolean
function smarttrigger.addAction(name, action) end

---@param name string
---@param condition any
---@return boolean
function smarttrigger.addCondition(name, condition) end

---@param name string
---@return boolean
function smarttrigger.create(name) end

---@param name string
---@return integer
function smarttrigger.getTriggerCount(name) end

---@param name string
---@return boolean
function smarttrigger.isRunning(name) end

---@param name string
function smarttrigger.remove(name) end

---@param name string
---@param ms integer
---@return boolean
function smarttrigger.setCheckInterval(name, ms) end

---@param name string
---@return boolean
function smarttrigger.start(name) end

---@param name string
---@return boolean
function smarttrigger.stop(name) end


-- ========== system 模块 ==========

---@class system
local system = {}

---@return any
function system.getCpuInfo() end

---@return any
function system.getCpuUsage() end

---@return any
function system.getDateTime() end

---@param drive string
---@return any
function system.getDiskInfo(drive) end

---@return any
function system.getDisplayInfo() end

---@return any
function system.getGpuInfo() end

---@return any
function system.getMemoryInfo() end

---@return any
function system.getNetworkAdapters() end

---@return any
function system.getOsInfo() end

---@return any
function system.getProcessCount() end

---@return any
function system.getThreadCount() end

---@return any
function system.getTimeZone() end

---@return any
function system.getUptime() end


-- ========== systemwatch 模块 ==========

---@class systemwatch
local systemwatch = {}

---@return any
function systemwatch.clearAll() end

---@param name? string
---@param callback function
---@return any
function systemwatch.processWatch(name, callback) end

---@param id integer
---@return boolean
function systemwatch.unwatch(id) end

---@param kind? string
---@return integer
function systemwatch.watchCount(kind) end

---@param title? string
---@param callback function
---@return any
function systemwatch.windowWatch(title, callback) end


-- ========== task 模块 ==========

---@class task
local task = {}

---@param taskId string
---@return boolean
function task.cancel(taskId) end

---@param taskId string
---@return any
function task.error(taskId) end

---@param taskId string
---@return boolean
function task.pause(taskId) end

---@param taskId string
---@return any
function task.result(taskId) end

---@param taskId string
---@return boolean
function task.resume(taskId) end

---@param taskId string
---@param options? any
---@return boolean
function task.retry(taskId, options) end

---@param taskId string
---@return any
function task.status(taskId) end

---@param work function
---@param options? any
---@return any
function task.submit(work, options) end

---@param taskId string
---@param timeoutMs? integer
---@return boolean
function task.wait(taskId, timeoutMs) end


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


-- ========== timer 模块 ==========

---@class timer
local timer = {}

---@param ms integer
---@param callback function
---@return any
function timer.after(ms, callback) end

---@param timerId integer
---@return boolean
function timer.cancel(timerId) end

---@return any
function timer.clearAll() end

---@param timerId integer
---@return boolean
function timer.clearInterval(timerId) end

---@param timerId integer
---@return boolean
function timer.clearTimeout(timerId) end

---@param timerId integer
---@return boolean
function timer.clearTimer(timerId) end

---@return any
function timer.count() end

---@param ms integer
---@param callback function
---@return any
function timer.every(ms, callback) end

---@param timerId integer
---@return boolean
function timer.exists(timerId) end

---@param ms integer
---@param callback function
---@return any
function timer.setInterval(ms, callback) end

---@param ms integer
---@param callback function
---@return any
function timer.setTimeout(ms, callback) end

---@param ms integer
function timer.sleep(ms) end


-- ========== transport 模块 ==========

---@class transport
local transport = {}

---@param handle integer
---@param data string
---@return boolean
function transport.tcpBroadcast(handle, data) end

---@param handle integer
---@param sessionId integer
---@return boolean
function transport.tcpCloseSession(handle, sessionId) end

---@param id string
---@param host string
---@param port integer
---@return any
function transport.tcpConnect(id, host, port) end

---@param handle integer
---@return boolean
function transport.tcpDisconnect(handle) end

---@param handle integer
---@return any
function transport.tcpGetSessions(handle) end

---@param handle integer
---@return boolean
function transport.tcpIsConnected(handle) end

---@param id string
---@param host string
---@param port integer
---@return any
function transport.tcpListen(id, host, port) end

---@param handle integer
---@param data string
---@return boolean
function transport.tcpSend(handle, data) end

---@param handle integer
---@param sessionId integer
---@param data string
---@return boolean
function transport.tcpSendTo(handle, sessionId, data) end

---@param handle integer
---@return boolean
function transport.tcpStop(handle) end

---@param handle integer
---@param host string
---@param port integer
---@return any
function transport.udpBind(handle, host, port) end

---@param handle integer
---@return boolean
function transport.udpClose(handle) end

---@param handle integer
---@param timeout integer
---@return any
function transport.udpRecvFrom(handle, timeout) end

---@param handle integer
---@param host string
---@param port integer
---@param data string
---@return boolean
function transport.udpSendTo(handle, host, port, data) end

---@param id string
---@return any
function transport.udpSocket(id) end


-- ========== uia 模块 ==========

---@class uia
local uia = {}

---@param controlType integer
---@return any
function uia.find_all_by_control_type(controlType) end

---@param name string
---@return any
function uia.find_button(name) end

---@param id string
---@return any
function uia.find_by_id(id) end

---@param name string
---@return any
function uia.find_by_name(name) end

---@param name string
---@return any
function uia.find_check_box(name) end

---@param name string
---@return any
function uia.find_combo_box(name) end

---@param name string
---@return any
function uia.find_edit(name) end

---@param name string
---@return any
function uia.find_hyperlink(name) end

---@param name string
---@return any
function uia.find_image(name) end

---@param name string
---@return any
function uia.find_list(name) end

---@param name string
---@return any
function uia.find_list_item(name) end

---@param name string
---@return any
function uia.find_menu_item(name) end

---@param name string
---@return any
function uia.find_progress_bar(name) end

---@param name string
---@return any
function uia.find_radio_button(name) end

---@param name string
---@return any
function uia.find_slider(name) end

---@param name string
---@return any
function uia.find_spinner(name) end

---@param name string
---@return any
function uia.find_tab(name) end

---@param name string
---@return any
function uia.find_tab_item(name) end

---@param name string
---@return any
function uia.find_text(name) end

---@param name string
---@return any
function uia.find_tree(name) end

---@param name string
---@return any
function uia.find_tree_item(name) end

---@return any
function uia.from_foreground() end

---@param x integer
---@param y integer
---@return any
function uia.from_point(x, y) end

---@param hwnd integer
---@return any
function uia.from_window(hwnd) end

---@param name string
---@param callback function
---@return any
function uia.on_property_changed(name, callback) end

---@param name string
---@param callback function
---@return any
function uia.on_structure_changed(name, callback) end

---@param listenerId integer
---@return boolean
function uia.remove_event_listener(listenerId) end

---@param selector any
---@param timeout integer
---@return any
function uia.wait_for(selector, timeout) end

---@param id string
---@param timeout integer
---@return any
function uia.wait_for_id(id, timeout) end

---@param name string
---@param timeout integer
---@return any
function uia.wait_for_name(name, timeout) end

---@param controlType integer
---@param timeout integer
---@return any
function uia.wait_for_role(controlType, timeout) end


-- ========== util 模块 ==========

---@class util
local util = {}

---@return any
function util.getTime() end

---@param message string
function util.log(message) end

---@param ms integer
function util.sleep(ms) end


-- ========== verification 模块 ==========

---@class verification
local verification = {}

---@param period integer
---@return integer
function verification.remaining(period) end

---@param secret string
---@return string
function verification.steamGuard(secret) end

---@param secret string
---@param digits integer
---@param period integer
---@return string
function verification.totp(secret, digits, period) end

---@param secret string
---@param code string
---@param digits integer
---@param period integer
---@param window integer
---@return boolean
function verification.verify(secret, code, digits, period, window) end


-- ========== vision 模块 ==========

---@class vision
local vision = {}

---@param desc string
---@param region? any
---@return any
function vision.aiElements(desc, region) end

---@param desc string
---@param region? any
---@return any
function vision.aiLocate(desc, region) end

---@param cfg any
---@return boolean
function vision.aiSetup(cfg) end

---@param cfg any
---@return any
function vision.aiSetupLocal(cfg) end

---@return any
function vision.aiSetupStatus() end

---@param color any
---@param tolerance integer
---@param region? any
---@return any
function vision.findAllColors(color, tolerance, region) end

---@param color any
---@param tolerance integer
---@param region? any
---@return any
function vision.findColor(color, tolerance, region) end

---@param templatePath string
---@param threshold number
---@param region? any
---@return any
function vision.findImage(templatePath, threshold, region) end

---@param region? any
---@return any
function vision.getDominantColor(region) end

---@param color any
---@param tolerance integer
---@param region? any
---@return boolean
function vision.hasColor(color, tolerance, region) end

---@param name string
---@param path string
---@return boolean
function vision.templateAdd(name, path) end

---@param names table
---@param threshold number
---@param region? any
---@return any
function vision.templateFindAll(names, threshold, region) end

---@param name string
---@return boolean
function vision.templateHas(name) end

---@return any
function vision.templateList() end

---@param name string
---@return boolean
function vision.templateRemove(name) end


-- ========== window 模块 ==========

---@class window
local window = {}

---@param handle integer
---@return boolean
function window.activate(handle) end

---@param title string
---@return any
function window.find(title) end

---@param handle integer
---@return any
function window.getBounds(handle) end

---@return any
function window.getForeground() end

---@param handle integer
---@return string
function window.getTitle(handle) end

---@param title string
---@param timeout integer
---@return boolean
function window.waitFor(title, timeout) end


-- ========== 导出 ==========

local wingman = {
    bt = bt,
    clipboard = clipboard,
    config = config,
    crypto = crypto,
    db = db,
    debugger = debugger,
    event = event,
    file = file,
    filewatcher = filewatcher,
    fsm = fsm,
    gameprofile = gameprofile,
    hotkey = hotkey,
    http = http,
    human = human,
    inbox = inbox,
    ini = ini,
    input = input,
    json = json,
    kv = kv,
    macro = macro,
    ml = ml,
    node = node,
    notify = notify,
    ocr = ocr,
    orchestration = orchestration,
    perf = perf,
    process = process,
    screen = screen,
    script = script,
    security = security,
    smarttrigger = smarttrigger,
    system = system,
    systemwatch = systemwatch,
    task = task,
    team = team,
    timer = timer,
    transport = transport,
    uia = uia,
    util = util,
    verification = verification,
    vision = vision,
    window = window,
}

return wingman
