# API 文档纠错 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修正 `docs/api/` 现有文档与代码实现的不一致（标题/签名/删除未实现函数/合并重复文档），使文档准确反映当前代码。

**Architecture:** 纯文档修改，不改代码。依据 `docs/api/AUDIT-REPORT.md` 的逐条证据，按模块纠错。Python `snake_case` 命名风格本 plan 不动（待 Plan 3 绑层转换后自动正确）。代码注册名（`mod.name`）为权威 namespace。

**Tech Stack:** Markdown 文档；代码事实来源 `lib/wingman/src/script/modules/`。

**范围边界：**
- ✅ 修正 namespace 标题、签名错误、删除"文档有/代码无"的零散函数、合并重复文档、stub 标 TODO
- ❌ 不补新模块文档（Plan 2）、不改 Python 命名风格（Plan 3）、不补功能代码（Plan 5-7）

---

### Task 1: 修正 namespace 标题（behavior-tree / smart-trigger）

**Files:**
- Modify: `docs/api/behavior-tree.md`（第 1 行标题、概述）
- Modify: `docs/api/smart-trigger.md`（第 1 行标题、概述）

**代码事实：** `misc_modules.cpp:171` 注册名 `bt`；`misc_modules.cpp:103` 注册名 `smarttrigger`（无下划线）。

- [ ] **Step 1: 修正 behavior-tree.md 标题**

将第 1 行 `# API: wingman.behavior_tree` 改为 `# API: wingman.bt`。
将概述中所有 `behavior_tree 模块` 改为 `bt 模块`。
**保留**正文示例中已有的 `wingman.bt` / `from wingman import bt`（这些本就正确）。

- [ ] **Step 2: 修正 smart-trigger.md 标题**

将第 1 行 `# API: wingman.smart_trigger` 改为 `# API: wingman.smarttrigger`。
将概述中所有 `smart_trigger 模块` 改为 `smarttrigger 模块`。
将全文残留的 `wingman.smart_trigger`（约 1 处）改为 `wingman.smarttrigger`。

- [ ] **Step 3: 验证**

Run: `grep -nE "behavior_tree|smart_trigger" docs/api/behavior-tree.md docs/api/smart-trigger.md`
Expected: 无输出（标题/概述已全部用注册名）。

- [ ] **Step 4: Commit**

```bash
git add docs/api/behavior-tree.md docs/api/smart-trigger.md
git commit -m "docs(api): fix namespace titles for bt and smarttrigger modules"
```

---

### Task 2: util.md 纠错

**Files:**
- Modify: `docs/api/util.md`

**代码事实：** `util_module.cpp` 仅注册 3 个函数：`sleep(ms)`、`getTime() -> int`、`log(message) -> nil`。

- [ ] **Step 1: 删除 7 个未实现函数的章节**

删除以下函数的全部章节（标题+正文+示例）：`format_time`/`formatTime`、`random`、`random_int`/`randomInt`、`shell_exec`/`shellExec`、`get_system_info`/`getSystemInfo`、`get_script_path`/`getScriptPath`、`get_script_dir`/`getScriptDir`。
同步删除"可用接口"汇总表中对应行。

- [ ] **Step 2: 修正 time → getTime**

将时间戳函数文档从 `time()` 改为 `getTime()`（代码注册名）。所有 `util.time(` 调用示例改为 `util.getTime(`。

- [ ] **Step 3: 修正 log 签名**

将 `log(level: str, message: str)` 改为 `log(message: str) -> nil`（代码单参数，直接输出消息）。删除 level 相关示例。

- [ ] **Step 4: 验证**

Run: `grep -niE "format_time|random|shell_exec|get_system_info|get_script_path|get_script_dir|util\.time\(" docs/api/util.md`
Expected: 无输出。

- [ ] **Step 5: Commit**

```bash
git add docs/api/util.md
git commit -m "docs(api): correct util module to match implementation"
```

---

### Task 3: config.md 纠错

**Files:**
- Modify: `docs/api/config.md`

**代码事实：** `config_module.cpp` 仅注册 `get`/`set`/`remove`/`save`/`load`。`ConfigManager::get/set` 用 `config.contains(key)` 顶层扁平查找，**不支持点号嵌套键**。

- [ ] **Step 1: 删除 6 组未实现的强类型 getter/setter**

删除：`get_server`/`set_server`、`get_tray`/`set_tray`、`get_auto_run`/`set_auto_run`（共 6 组）的全部章节与汇总表行。

- [ ] **Step 2: 修正嵌套键描述**

将"支持点号分隔的嵌套键 如 game.character"的声明改为："键为顶层字符串，不支持点号路径嵌套（传入 `game.character` 会查找字面名为 `game.character` 的顶层键）"。

- [ ] **Step 3: 验证**

Run: `grep -niE "get_server|set_tray|get_auto_run|点号|嵌套键" docs/api/config.md`
Expected: 无输出（或仅剩 Step 2 改后的扁平键说明）。

- [ ] **Step 4: Commit**

```bash
git add docs/api/config.md
git commit -m "docs(api): correct config module to match implementation"
```

---

### Task 4: screen.md 纠错

**Files:**
- Modify: `docs/api/screen.md`

**代码事实（screen_module.cpp）：**
- `capture() -> bool`（无参，返回是否成功，非 Image）
- `captureRegion(region:{x,y,width,height}) -> bool`
- `getPixel(x,y) -> {r,g,b,a}`（非 int）
- `findColor(color, region对象, tolerance) -> {point, found}`（tolerance 默认 10）
- `findColors(color, region对象, tolerance) -> 数组`
- `findImage(imagePath, region对象, threshold) -> {point, found}`（threshold 默认 0.9，返回无 confidence）
- `getScreenWidth()` / `getScreenHeight()`

- [ ] **Step 1: 删除未实现函数**

删除 `wait_for_image`/`waitForImage`、`find_pixel`/`findPixel` 章节。

- [ ] **Step 2: 修正 capture 签名**

`capture(x,y,w,h) -> Image` → `capture() -> bool`（全屏捕获，返回成功与否）。区域捕获用 `captureRegion(region)`。

- [ ] **Step 3: 修正 getPixel 返回类型**

`getPixel(x,y) -> int (0xRRGGBB)` → `getPixel(x,y) -> {r,g,b,a}`。

- [ ] **Step 4: 修正 findImage 签名与默认值**

`findImage(path, x,y,w,h, threshold=0.8) -> {x,y,confidence}` → `findImage(imagePath, region:{x,y,width,height}, threshold=0.9) -> {point, found}`。删除 confidence 字段描述。

- [ ] **Step 5: 修正 findColor 签名与默认值**

`findColor(color, x,y,w,h, tolerance) -> 所有点列表` → `findColor(color, region, tolerance=10) -> {point, found}`（返回首个匹配点）。

- [ ] **Step 6: 补 findColors / captureRegion / getScreenWidth / getScreenHeight**

新增这 4 个函数的章节（代码已实现但文档无）：`findColors(color, region, tolerance) -> 点数组`、`captureRegion(region) -> bool`、`getScreenWidth() -> int`、`getScreenHeight() -> int`。

- [ ] **Step 7: 验证**

Run: `grep -niE "wait_for_image|find_pixel|-> Image|0xRRGGBB|confidence" docs/api/screen.md`
Expected: 无输出。

- [ ] **Step 8: Commit**

```bash
git add docs/api/screen.md
git commit -m "docs(api): correct screen module signatures to match implementation"
```

---

### Task 5: input.md 纠错

**Files:**
- Modify: `docs/api/input.md`

**代码事实（input_module.cpp）：**
- `click(x, y, button:int)`（button 为 0/1/2，非 "left"/"right" 字符串）
- `move(x, y, duration:int)`（第 3 参为移动耗时 ms，非 smooth:bool）
- `type(text, delay)`、`key(vkCode)`、`keyDown(vkCode)`、`keyUp(vkCode)`、`delay(ms)`、`randomDelay(min,max)`

- [ ] **Step 1: 删除未实现函数**

删除 `drag`、`get_mouse_pos`/`getMousePos`、`send_keys`/`sendKeys` 章节。

- [ ] **Step 2: 修正 click button 类型**

`click(x, y, button: str = "left")` → `click(x, y, button: int)`，说明 button 取值 0=左/1=右/2=中。

- [ ] **Step 3: 修正 move 第 3 参**

`move(x, y, smooth: bool)` → `move(x, y, duration: int)`，说明 duration 为移动耗时（毫秒）。

- [ ] **Step 4: 补 type / key / keyDown / keyUp / delay / randomDelay**

新增这 6 个已实现函数的章节：`type(text, delay)`、`key(vkCode:int)`、`keyDown(vkCode)`、`keyUp(vkCode)`、`delay(ms)`、`randomDelay(min,max)`。

- [ ] **Step 5: 验证**

Run: `grep -niE "drag|get_mouse_pos|send_keys|button.*str|smooth.*bool" docs/api/input.md`
Expected: 无输出。

- [ ] **Step 6: Commit**

```bash
git add docs/api/input.md
git commit -m "docs(api): correct input module signatures to match implementation"
```

---

### Task 6: vision.md 纠错

**Files:**
- Modify: `docs/api/vision.md`

**代码事实（vision_module.cpp）：** 注册 `findColor`/`findAllColors`/`hasColor`/`getDominantColor`/`findImage`。findColor tolerance 默认 **10**；findImage threshold 默认 **0.9**；findImage 返回含 `region` 字段。

- [ ] **Step 1: 删除 4 个未实现函数**

删除 `detect_edges`/`detectEdges`、`detect_contours`/`detectContours`、`detect_circles`/`detectCircles`、`capture_region`/`captureRegion` 章节。

- [ ] **Step 2: 修正默认值**

findColor tolerance "默认 0" → "默认 10"。findImage threshold "默认 0.8" → "默认 0.9"。

- [ ] **Step 3: 补 findImage 返回 region 字段**

在 findImage 返回说明中加入 `region` 字段（`{x,y,width,height}`）。

- [ ] **Step 4: 验证**

Run: `grep -niE "detect_edges|detect_contours|detect_circles|capture_region|默认 0|默认 0.8" docs/api/vision.md`
Expected: 无输出。

- [ ] **Step 5: Commit**

```bash
git add docs/api/vision.md
git commit -m "docs(api): correct vision module defaults and remove unimplemented fns"
```

---

### Task 7: ocr.md 纠错

**Files:**
- Modify: `docs/api/ocr.md`

**代码事实（misc_modules.cpp ocr 部分）：** 仅 `recognize(region)`、`recognizeText(region)`，region 为必传（`toRect(args[0])`，无默认全屏）。无 set_language/set_data_path。

- [ ] **Step 1: 删除未实现函数**

删除 `set_language`/`setLanguage`、`set_data_path`/`setDataPath` 章节。

- [ ] **Step 2: 修正 region 可选性**

recognize / recognizeText 的 region 参数从"可选，默认全屏"改为"必传，指定识别区域"。

- [ ] **Step 3: 验证**

Run: `grep -niE "set_language|set_data_path|默认全屏" docs/api/ocr.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/api/ocr.md
git commit -m "docs(api): correct ocr module to match implementation"
```

---

### Task 8: http.md 纠错

**Files:**
- Modify: `docs/api/http.md`

**代码事实（http_module.cpp）：** 仅 `get`/`post`/`put`/`delete`。options 仅解析 `timeout`/`followRedirects`/`maxRedirects`/`headers`（无 params/proxy）。响应对象额外含 `elapsed`(float) 与出错时 `error`。

- [ ] **Step 1: 删除未实现函数**

删除 `post_form`/`postForm`、`download` 章节。

- [ ] **Step 2: 修正 options 字段**

options 从 `timeout/headers/params/proxy` 改为 `timeout/headers/followRedirects/maxRedirects`。删除 params、proxy 说明。

- [ ] **Step 3: 补响应字段**

响应格式表加入 `elapsed`(float，耗时秒) 与 `error`(string，失败时) 字段。

- [ ] **Step 4: 验证**

Run: `grep -niE "post_form|download|params|proxy" docs/api/http.md`
Expected: 无输出（params/proxy 已删）。

- [ ] **Step 5: Commit**

```bash
git add docs/api/http.md
git commit -m "docs(api): correct http module to match implementation"
```

---

### Task 9: fsm.md 纠错

**Files:**
- Modify: `docs/api/fsm.md`

**代码事实：** `fsm_module.cpp` 注册 `create`/`state`/`transition`/`dispatch`/`current`/`reset`，无 `is`/`can`。

- [ ] **Step 1: 删除未实现函数**

删除 `is(machineId, state)`、`can(machineId, event)` 章节。

- [ ] **Step 2: 验证**

Run: `grep -niE "fsm\.is\(|fsm\.can\(" docs/api/fsm.md`
Expected: 无输出。

- [ ] **Step 3: Commit**

```bash
git add docs/api/fsm.md
git commit -m "docs(api): remove unimplemented fsm.is/can from docs"
```

---

### Task 10: perf.md 纠错

**Files:**
- Modify: `docs/api/perf.md`

**代码事实（performance_module.cpp）：**
- `getCacheStats` 返回 `{size, hits, misses, hitRate}`（非文档的 cached_count/total_size/hit_rate）
- 额外函数：`getCacheSize`/`fastFindImage`/`parallelFindColors`/`getStats`/`resetStats`
- OpenCV 不可用时为 stub

- [ ] **Step 1: 修正 getCacheStats 返回字段**

返回字段从 `{cached_count, total_size, hits, misses, hit_rate}` 改为 `{size, hits, misses, hitRate}`。

- [ ] **Step 2: 补 5 个已实现函数**

新增章节：`getCacheSize() -> int`、`fastFindImage(...)`、`parallelFindColors(...)`、`getStats() -> dict`、`resetStats()`（签名以代码 performance_module.cpp:55-108 为准）。

- [ ] **Step 3: 顶部标注 stub 条件**

在文档顶部加注："本模块依赖 OpenCV；当构建未启用 OpenCV 时为 stub 实现（函数存在但返回空/默认值）。"

- [ ] **Step 4: 验证**

Run: `grep -niE "cached_count|total_size|hit_rate" docs/api/perf.md`
Expected: 无输出。

- [ ] **Step 5: Commit**

```bash
git add docs/api/perf.md
git commit -m "docs(api): correct perf cache stats fields and document stub"
```

---

### Task 11: smart-trigger.md 纠错

**Files:**
- Modify: `docs/api/smart-trigger.md`

**代码事实（misc_modules.cpp smarttrigger 部分）：**
- 注册：`create`/`start`/`stop`/`remove`/`addCondition`/`addAction`/`setCheckInterval`/`isRunning`/`getTriggerCount`
- `addCondition(name, condition:{type,color,tolerance,threshold,region,text,template})`（对象表，非位置参数）
- `addAction(name, action:{type,x,y,key,waitMs,script,message})`（对象表）
- 无 `set_max_triggers`；parseAction 仅识别 `lua_script`（无 python_script）

- [ ] **Step 1: 删除未实现函数**

删除 `set_max_triggers`/`setMaxTriggers` 章节。

- [ ] **Step 2: 修正 addCondition 签名**

从位置参数 `add_condition(triggerName, conditionType, *args)` 改为对象表：`addCondition(triggerName, condition:{type:"COLOR_FOUND"|"IMAGE_FOUND"|..., color?, tolerance?, threshold?, region?, text?, template?}) -> bool`。

- [ ] **Step 3: 修正 addAction 签名**

从位置参数改为对象表：`addAction(triggerName, action:{type:"CLICK"|"WAIT"|"KEY"|"LUA_SCRIPT", x?, y?, key?, waitMs?, script?, message?}) -> bool`。

- [ ] **Step 4: 删除 PYTHON_SCRIPT 动作类型**

动作类型参考中删除 `PYTHON_SCRIPT`（代码仅支持 `LUA_SCRIPT`）。

- [ ] **Step 5: 补 isRunning**

新增 `isRunning(triggerName) -> bool` 章节。

- [ ] **Step 6: 修正返回值描述**

`stop`/`setCheckInterval` 返回值从 nil 改为 bool；`addCondition`/`addAction` 返回 bool（成功 true）。

- [ ] **Step 7: 验证**

Run: `grep -niE "set_max_triggers|PYTHON_SCRIPT|add_condition\(triggerName, conditionType" docs/api/smart-trigger.md`
Expected: 无输出。

- [ ] **Step 8: Commit**

```bash
git add docs/api/smart-trigger.md
git commit -m "docs(api): correct smarttrigger signatures to match implementation"
```

---

### Task 12: event.md 纠错

**Files:**
- Modify: `docs/api/event.md`

**代码事实（event_module.cpp）：** `on`/`once` 返回 int 订阅 ID（非 str）；`off` 同时接受 string（名称）与 int（订阅 ID）。

- [ ] **Step 1: 修正 on/once 返回类型**

`on(...) -> str` → `on(...) -> int`（订阅 ID）；`once(...) -> str` → `once(...) -> int`。

- [ ] **Step 2: 修正 off 参数说明**

`off(subscription: str)` → `off(subscription: str|int)`，说明可传订阅 ID（int）或名称（str）。

- [ ] **Step 3: 验证**

Run: `grep -niE "on\(.*-> str|once\(.*-> str|subscription: str$" docs/api/event.md`
Expected: 无输出。

- [ ] **Step 4: Commit**

```bash
git add docs/api/event.md
git commit -m "docs(api): correct event on/once return type to int"
```

---

### Task 13: kv.md 纠错

**Files:**
- Modify: `docs/api/kv.md`

**代码事实（kv_module.cpp）：**
- `delete`/`lpush`/`rpush`/`hdel` 返回 nil（非 int/bool）
- `set` 恒返回 nil（不支持用返回值判断 nx 成功）；解析 nx 和 xx
- 注册 `expire(key, seconds)`（文档无）

- [ ] **Step 1: 修正返回类型**

`delete(...) -> int` → `delete(...) -> nil`；`lpush/rpush(...) -> int` → `-> nil`；`hdel(...) -> bool` → `-> nil`。

- [ ] **Step 2: 修正 set nx 描述**

删除"用返回值判断 nx 成功"的示例（`success = kv.set(...,{nx})`）；说明 set 返回 nil，nx/xx 仅控制写入条件。补 `xx` 选项说明（仅键存在时设置）。

- [ ] **Step 3: 补 expire 函数**

新增 `expire(key, seconds) -> nil`（为已存在键设置过期）章节。

- [ ] **Step 4: 验证**

Run: `grep -niE "delete\(.*-> int|lpush\(.*-> int|hdel\(.*-> bool|success = kv.set" docs/api/kv.md`
Expected: 无输出。

- [ ] **Step 5: Commit**

```bash
git add docs/api/kv.md
git commit -m "docs(api): correct kv return types and add expire"
```

---

### Task 14: 合并 debugger.md 与 debugging.md

**Files:**
- Delete: `docs/api/debugging.md`
- Modify: `docs/api/debugger.md`

**代码事实（debugger_module.cpp）：** 仅 4 个 stub 函数：`start() -> bool`（stub 恒 false）、`stop()`、`breakpoint(file, line) -> string`、`breakHere()`。源码注释明确 "debugger not yet implemented"。

- [ ] **Step 1: 重写 debugger.md**

将 debugger.md 重写为仅描述 4 个 stub 函数，顶部加醒目标注："⚠️ 调试器尚未实现，当前为 stub。`start` 恒返回 false。完整调试 API（断点管理/单步/求值/堆栈）规划中。"
函数清单：`start() -> bool`、`stop()`、`breakpoint(file: str, line: int) -> str`、`breakHere()`。

- [ ] **Step 2: 删除 debugging.md**

`git rm docs/api/debugging.md`（内容已合并/废弃）。

- [ ] **Step 3: 更新 index.md 链接**

移除 index.md 中对 debugging.md 的链接（如有）。

- [ ] **Step 4: 验证**

Run: `test ! -f docs/api/debugging.md && grep -niE "step_into|get_stacktrace|log_point" docs/api/debugger.md`
Expected: 第一条通过（文件已删），第二条无输出（幽灵函数已移除）。

- [ ] **Step 5: Commit**

```bash
git add -A docs/api/debugger.md docs/api/debugging.md docs/api/index.md
git commit -m "docs(api): merge debugger docs, mark as unimplemented stub"
```

---

### Task 15: 合并 serialize.md 与 serialization.md

**Files:**
- Delete: `docs/api/serialization.md`
- Modify: `docs/api/serialize.md`

**代码事实（json_module.cpp）：** `encode(data, indent:int=-1)`，indent 为缩进空格数（int），-1=压缩。无独立序列化模块——文档是 json+ini 聚合。

- [ ] **Step 1: 统一到 serialize.md**

保留 serialize.md（其 `encode(config, indent=2)` 的 int 风格正确）。删除 serialization.md 中混用 Python 标准库 `import json` 与 `encode(data, true)`（bool）的错误示例。

- [ ] **Step 2: 删除 serialization.md**

`git rm docs/api/serialization.md`。

- [ ] **Step 3: 更新 index.md 链接**

移除对 serialization.md 的链接，保留 serialize.md。

- [ ] **Step 4: 验证**

Run: `test ! -f docs/api/serialization.md && grep -nE "encode\(.*true\)|import json" docs/api/serialize.md`
Expected: 第一条通过，第二条无输出。

- [ ] **Step 5: Commit**

```bash
git add -A docs/api/serialize.md docs/api/serialization.md docs/api/index.md
git commit -m "docs(api): merge serialize docs, fix encode indent type to int"
```

---

### Task 16: 重写 storage.md

**Files:**
- Modify 或 Delete: `docs/api/storage.md`

**代码事实：** storage.md 是 kv+db 聚合文档，但使用了不存在的 OOP 语法（`conn:execute()`）、虚构函数（`kv.has`/`kv.keys`/`kv.clear`）、错误目录（代码用 `scripts` 目录，非 `data`）。

- [ ] **Step 1: 重写 storage.md**

改为扁平 API 风格（`db.execute(conn, sql)` 而非 `conn:execute(sql)`）；删除虚构的 `kv.has`/`kv.keys`/`kv.clear`（指向 `kv.exists`）；修正数据目录为 `scripts`（Windows `%APPDATA%/wingman/scripts`，Unix `~/.local/share/wingman/scripts`）。
**或者**若判定聚合文档价值低，直接 `git rm docs/api/storage.md` 并在 index.md 移除链接（推荐删除，因 kv.md/db.md 已各自完整）。

- [ ] **Step 2: 验证**

Run: `grep -niE "conn:execute|kv\.has\(|kv\.keys\(|kv\.clear\(|\\\\data\\\\|/data/" docs/api/storage.md`
Expected: 无输出（若删除则文件不存在）。

- [ ] **Step 3: Commit**

```bash
git add -A docs/api/storage.md docs/api/index.md
git commit -m "docs(api): fix storage doc to match flat API and correct paths"
```

---

### Task 17: 自检与总结

- [ ] **Step 1: 全量 grep 复查残留错误**

```bash
grep -rniE "behavior_tree|smart_trigger\b" docs/api/*.md
grep -rniE "wait_for_image|find_pixel|detect_edges|set_language|post_form|set_max_triggers" docs/api/*.md
```
Expected: 无输出（或仅 AUDIT-REPORT.md 内的历史记录）。

- [ ] **Step 2: 更新 AUDIT-REPORT.md**

在 AUDIT-REPORT.md 顶部加注："✅ Plan 1（文档纠错）已执行，P0/P1 中标 [修正]/[删文档] 的条目已处理；[标TODO]/[补文档]/[决策] 项见 Plan 2-8。"

- [ ] **Step 3: Commit**

```bash
git add docs/api/AUDIT-REPORT.md
git commit -m "docs(api): mark Plan 1 corrections as done in audit report"
```

---

## 执行说明

- **提交策略**：每个 Task 独立提交（共 ~16 个提交），便于 review 与回退。
- **Python snake_case**：本 plan 不改 Python 示例命名（如 `get_foreground`），待 Plan 3 绑层加转换后自动正确。
- **验证手段**：文档无单测，每个 Task 用 `grep` 确认错误模式已消除。
- **后续 plan**：Plan 2（补缺失模块文档）、Plan 3-4（Python 修复）、Plan 5-7（功能补全）、Plan 8（kv 桥接）。
