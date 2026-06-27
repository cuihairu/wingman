# API 文档 ↔ 代码实现 一致性审查报告

> 审查日期：2026-06-27
> 审查范围：`docs/api/*.md`（API 文档）↔ `lib/wingman/src/script/modules/`（C++ 脚本模块）+ `libs/python/`（Python 引擎）
> 审查方法：38 个注册模块逐函数核对（文档声明 vs 代码 `mod.functions` 注册）
> 代码注册名（`mod.name`）为权威 namespace 来源。

## 0. 健康度概览

| 指标 | 数据 |
|------|------|
| 核对模块 | 38 |
| 发现不一致 | ~100 条 |
| 完全一致 ✓ | 7 个：`db` `verification` `notify` `orchestration` `inbox` `team` `security` |
| 严重偏差模块 | ~15 个 |
| 核心结论 | 文档**系统性超前于/偏离于**代码；命名约定混乱；多个模块文档与代码是两套不同 API |

## 1. 关键查证结论（影响分类判断）

### 1.1 Python 绑定层不做 snake_case 转换 ⚠️
- 证据：`libs/python/src/python_script_engine.cpp:301` — `moduleObject.attr(fn.name.c_str())`，直接用代码注册名（如 `getForeground`），无任何转换。
- 结论：文档中**所有 Python `snake_case` 函数名**（`get_foreground`/`tcp_connect`/`move_mouse`/`get_gpu_info`/`find_color`…）在 Python 中 `AttributeError` 失效。Python/Lua 均须用代码注册的精确名。
- 影响模块：`window` `transport` `process` `input` `screen` `system` `perf` `kv` `config` `http` 等几乎所有含 Python 示例的文档。

### 1.2 Python 引擎默认关闭且 CI 不验证 ⚠️
- 证据：`CMakeLists.txt:37` — `option(WINGMAN_ENABLE_PYTHON ... OFF)`；CI workflows 无 `WINGMAN_ENABLE_PYTHON`；无 Python 测试。
- 现状：Python 引擎实现存在（`libs/python`，pybind11），但默认不编译、CI 不构建、无测试。
- 矛盾：`index.md`/`ROADMAP.md` 声称"Python 支持已完成"，与"默认 OFF + 无 CI 验证"严重不符。

## 2. 分类标记说明

- **[修正]**：实现正确，文档描述错 → 改文档
- **[补文档]**：代码有、文档无 → 补文档
- **[删文档]**：文档超前且功能已废弃/不打算做 → 删除文档声明
- **[标TODO]**：计划要做的功能 → 保留文档并明确标注"未实现/规划中"
- **[决策]**：需产品定夺（删文档 vs 补代码）

## 3. P0 严重问题

### 3.1 文档描述的能力代码未实现（文档有/代码无）

| 模块 | 文档声明但代码没有 | 建议 | 证据 |
|------|-------------------|------|------|
| human | 9 函数（`random_delay`/`move_mouse`/`natural_click`/`natural_type`/`set_config` 等）全无 | [决策] | human.md:390-402 |
| behavior-tree | `sequence`/`selector`/`condition`/`action` 节点构造全无（代码仅 create/tick/remove） | [决策] | misc_modules.cpp:169-197 |
| util | `format_time`/`random`/`random_int`/`shell_exec`/`get_system_info`/`get_script_path`/`get_script_dir`（7个） | [决策] | util_module.cpp 仅 3 函数 |
| debugger | `debugger.md`+`debugging.md` 合计 ~19 函数，代码仅 4 个 stub | [标TODO] stub 返回 false | debugger_module.cpp:13-30 |
| config | `get_server`/`set_tray`/`get_auto_run` 等 6 组强类型 getter/setter | [决策] | config_module.cpp 仅 5 函数 |
| ml | `detect`/`classify`（目标检测/分类） | [标TODO] 文档已自注待补充 | ml_module.cpp |
| kv | `save`/`load`/`enable_auto_save`/`hexists`/`hkeys`（后端有，脚本层未桥接） | [决策] | kv_module.cpp |
| vision | `detect_edges`/`detect_contours`/`detect_circles`/`capture_region` | [决策] | vision_module.cpp |
| ocr | `set_language`/`set_data_path` | [决策] | misc_modules.cpp:17-39 |
| http | `post_form`/`download` | [决策] | http_module.cpp 仅 4 函数 |
| fsm | `is(machineId,state)`/`can(machineId,event)` | [决策] | fsm_module.cpp |
| screen | `wait_for_image`/`find_pixel` | [决策] | screen_module.cpp |
| input | `drag`/`get_mouse_pos`/`send_keys` | [决策] | input_module.cpp |

### 3.2 代码已实现但完全无文档（代码有/文档无）

| 模块 | 缺失函数数 | 建议 | 证据 |
|------|-----------|------|------|
| clipboard | 15（全无文档，index.md 未列） | [补文档] | clipboard_module.cpp:9-122 |
| macro | 12（仅 examples 有示例） | [补文档] | macro_module.cpp:44-128 |
| filewatcher | 5（全无文档） | [补文档] | filewatcher_module.cpp:9-62 |
| crypto | 11（overview/index **错误指向 security.md**） | [补文档]+[修正指向] | crypto_module.cpp:14-110 |
| script | 13（script.md 描述的是别的模块！） | [补文档]+重写 script.md | script_module.cpp:62-173 |
| system | 7（`getNetworkAdapters`/`getUptime`/`getDateTime` 等） | [补文档] | system_module.cpp:95-142 |
| perf | 5（`getCacheSize`/`fastFindImage`/`parallelFindColors` 等） | [补文档] | performance_module.cpp:55-108 |
| input | 6（`type`/`keyDown`/`keyUp`/`key`/`delay`/`randomDelay`） | [补文档] | input_module.cpp:62-101 |
| screen | 4（`captureRegion`/`findColors`/`getScreenWidth`/`getScreenHeight`） | [补文档] | screen_module.cpp |
| kv | 1（`expire`） | [补文档] | kv_module.cpp:54-58 |
| smarttrigger | 1（`isRunning`） | [补文档] | misc_modules.cpp:151-155 |
| node | 3（无正式 API 文档，仅在 guide/config.md 示例；`getWindows` 仅 Windows） | [补文档] | misc_modules.cpp:202-237 |

### 3.3 文档与代码是两套完全不同的 API（最严重）

| 模块 | 文档 API | 代码 API | 建议 |
|------|---------|---------|------|
| human | 拟人化（`natural_click`/`set_typing_variance`） | 裸输入（`mouse_click`/`keyboard_type`） | [决策] 零交集 |
| ml | OOP（`model.detect()`） | ID 句柄（`ml.inputs(id)`） | [决策] 形态不同 |
| uia | 15+ 函数 OO（`from_foreground`/`find_button`） | 3 函数句柄（`findByName`/`findById`/`find`） | [决策] |

## 4. P1 命名/签名问题

### 4.1 Namespace 标题错误（文档内部自相矛盾）

| 文档 | 标题 | 代码注册名 | 建议 |
|------|------|-----------|------|
| behavior-tree.md | `wingman.behavior_tree` | `bt` | [修正] 标题改 `bt` |
| smart-trigger.md | `wingman.smart_trigger` | `smarttrigger` | [修正] 标题改 `smarttrigger` |

> 两文档正文示例已用正确名，仅标题/概述是旧名。

### 4.2 Python snake_case 别名系统性失效（§1.1）

- 建议：**[修正]** 全部 Python 示例改用代码注册名（camelCase 或代码实际名）。涉及几乎所有含 Python 示例的文档。
- 前置：需先定 Python SDK 的产品定位（见 §6 决策 D1）。

### 4.3 签名严重不符（按文档调用会出错）

| 模块.函数 | 问题 | 建议 |
|-----------|------|------|
| perf.getCacheStats | 返回字段全错（文档 `cached_count/total_size/hit_rate` vs 代码 `size/hitRate`） | [修正] |
| smarttrigger.addCondition/addAction | 文档位置参数 vs 代码对象表 `{type:...}` | [修正] |
| debugger.breakpoint | 文档无参 vs 代码 `file,line` 两参 | [修正] |
| screen.capture | 文档返回 Image vs 代码返回 bool | [修正] |
| screen.getPixel | 文档返回 int vs 代码返回 `{r,g,b,a}` | [修正] |
| input.click | button 文档 str vs 代码 int | [修正] |
| input.move | 第3参 文档 smooth:bool vs 代码 duration:int | [修正] |
| kv.delete/lpush/hdel | 文档返回 int/bool vs 代码 nil | [修正] |
| kv.set | nx 文档示例当 success bool 用，代码恒返回 nil | [修正] |
| config.get | 文档支持点号嵌套键 vs 代码扁平查找 | [修正] |
| util.log | 文档 `(level,msg)` vs 代码 `(msg)` | [修正] |
| util.time/getTime | 文档 `time()` vs 代码 `getTime` | [修正] |
| system.getGpuInfo | 文档返回 dict vs 代码返回数组 | [修正] |
| event.on/once | 文档返回 str vs 代码返回 int 订阅 ID | [修正] |
| json.encode | serialization.md 用 bool indent vs 代码 int | [修正]（合并重复文档） |

## 5. P2 文档组织问题

| 问题 | 建议 |
|------|------|
| `debugger.md`↔`debugging.md` 重复且均与代码不符 | [修正] 合并为一，以代码 4 函数为准 |
| `serialize.md`↔`serialization.md` 重复且矛盾（encode 参数 bool vs int） | [修正] 合并，统一 int |
| `storage.md` 严重失真（OOP 语法/虚构 `kv.has/keys/clear`/错误 data 目录） | [删文档]或[修正]重写 |
| `data.md` 聚合对比，少量链式/通配符示例失真 | [修正] |
| `core.md` 跨模块聚合索引，命名风格需单独核对 | [决策] 是否保留 |
| macro 历史命名（development-todo.md 的 `startRecording` vs 代码 `start`） | [修正] 同步命名 |

## 6. 待决策项（影响多条，需产品定夺）

- **D1 Python SDK 定位**：默认 OFF + 无 CI + snake_case 全错。选项：(a) 文档降级 Python 为"实验性/需手动启用"并修正命名；(b) 投入修复 Python（补 CI、修绑定/文档）。
- **D2 "文档有/代码无"的功能归属**：human/uia/ml 两套 API + util/config/kv/vision/ocr/http/fsm/screen/input 的大量未实现函数。统一判断为 (a) 废弃→删文档，还是 (b) 计划中→标 TODO 保留。
- **D3 三个无文档真实模块**（clipboard/macro/filewatcher，合计 32 函数）：补完整 API 文档。

## 7. 一致性良好的模块（无需改动）

`db` `verification` `notify` `orchestration` `inbox` `team` `security` —— 文档与代码函数名/签名/返回值均匹配。
