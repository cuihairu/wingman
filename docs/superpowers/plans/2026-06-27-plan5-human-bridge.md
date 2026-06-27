# Plan 5 — human 拟人化 API 补全 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在脚本层补齐 `docs/api/human.md` 声明但代码缺失的 9 个拟人化高层 API（`random_delay`/`move_mouse`/`natural_click`/`natural_type`/`set_delay_range`/`set_move_speed`/`set_typing_variance`/`set_config`/`get_config`），并补充其所需的后端能力，使文档↔代码一致。

**Architecture:** 文档是权威 API（handoff §1 已锁定「以文档为准补代码，不删文档」）。改造分两层：
1. **后端增强（`human.hpp`/`human.cpp`）**：新增 `HumanMouse::moveTo(from, to, duration)` 起点重载与 `HumanMouse::middleClick`，填补文档语义缺口。
2. **脚本桥接（`human_module.cpp`）**：注册 9 个 camelCase 函数，桥接到 `Human`/`HumanMouse`/`HumanKeyboard`；配置类函数在脚本层维护扁平高层模型（`move_speed`/`typing_variance`/`delay_min`/`delay_max`），并映射应用到后端分散的 `HumanMouseConfig`/`HumanKeyboardConfig`。

**Tech Stack:** C++17、GoogleTest、Wingman 自研脚本引擎（`ScriptValue` tagged union）、Python 绑定层自动 camelCase→snake_case 双注册。

---

## 关键设计决策（已与用户确认，勿改）

| # | 决策点 | 选定方案 |
|---|--------|----------|
| D1 | `move_mouse(x1,y1,x2,y2,duration)` 起点 | **后端新增** `HumanMouse::moveTo(const Point& from, const Point& to, int duration)`，忠实实现「从起点贝塞尔移到终点」 |
| D2 | 配置函数语义映射 | **高层模型 + 映射应用**：脚本层维护 `HumanScriptConfig`，`set_*` 修改后映射写入后端 mouse/keyboard config |
| D3 | `natural_click` 的 `middle` 按钮 | **后端新增** `HumanMouse::middleClick(int x, int y)`（`platform::MouseButton::Middle` 已存在） |

### 既有机制（无需重复实现，直接利用）

- **Python snake_case 双注册**：`libs/python/src/python_script_engine.cpp:99-376` 在注册模块函数时，自动为每个 camelCase 名额外注册 snake_case 别名（`camelToSnake`）。因此 **human_module.cpp 只需注册 camelCase 名**，文档的 `random_delay`/`randomDelay` 双命名天然满足（Lua 用 camelCase，Python 两者皆可）。
- **`Human`/`HumanMouse`/`HumanKeyboard` 单例**：`Human::mouse()`/`Human::keyboard()` 返回单例引用；`Human::setMouseConfig(cfg)`/`Human::setKeyboardConfig(cfg)` 已存在，配置映射直接复用。
- **现有 10 个底层函数（`mouse_*`/`keyboard_*`）保留不动**：它们有测试覆盖（`script_function_test.cpp:1324-1383`），是另一套底层裸操作 API，与新增高层 API 并存（审计 §3.3 已确认两套零交集，共存合理）。

### 配置映射公式（D2 具体化）

脚本层 `HumanScriptConfig` 字段 → 后端 config 字段映射：

| 高层字段 | 范围 | 后端映射 | 公式 |
|----------|------|----------|------|
| `delay_min`/`delay_max` (int ms) | 任意 | `HumanMouseConfig.clickDelayMin/Max` | 直接赋值 |
| `move_speed` (double) | 0.1–2.0 | `HumanMouseConfig.minMoveDuration/MaxMoveDuration` | `speed` clamp 到 [0.1,2.0]；`min=round(100/speed)`，`max=round(300/speed)`（speed=1.0 → 100/300，与后端默认一致） |
| `typing_variance` (double) | 0.0–1.0 | `HumanKeyboardConfig.typeDelayMin/Max` | `v` clamp 到 [0,1]；`min=50`，`max=50+round(100*v)`（v=0 → 50/50 固定节奏；v=1 → 50/150 最大随机） |

`get_config()` 返回上述 4 个高层字段（非后端原始字段），与 `human.md:359` 文档承诺一致。

---

## File Structure

| 文件 | 职责 | 改动 |
|------|------|------|
| `lib/wingman/include/wingman/human.hpp` | `HumanMouse`/`HumanKeyboard` 类声明 | 新增 2 个 public 方法声明 |
| `lib/wingman/src/human.cpp` | 后端实现 | 新增 2 个方法实现 |
| `lib/wingman/src/script/modules/human_module.cpp` | 脚本层桥接（`createHumanModule`） | 新增 `HumanScriptConfig` + `applyHumanConfigToBackend` 辅助 + 9 个函数注册 |
| `lib/wingman/tests/human_test.cpp` | 后端单测 | 新增 2 个测试 |
| `lib/wingman/tests/script_function_test.cpp` | 脚本层函数测试 | 新增 human 高层 API 测试段 |

---

## Build & Test Commands（所有 Task 通用）

```bash
# 构建（macOS single-config；build-core-only-tests 已配置 BUILD_CORE_TESTS=ON）
cmake --build build-core-only-tests -j 2>&1 | tail -20

# 运行 human 相关测试（后端 + 脚本层）
ctest --test-dir build-core-only-tests -R "Human" --output-on-failure

# 备选：直接用 gtest filter
./build-core-only-tests/lib/wingman/tests/core_tests --gtest_filter="Human*"
```

> 若 `build-core-only-tests` 缺失或损坏，重新配置：
> ```bash
> cmake -B build-core-only-tests -DBUILD_CORE_TESTS=ON \
>   -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" \
>   -DVCPKG_TARGET_TRIPLET=x64-osx
> ```
> （triplet 按本机调整；遵守 CLAUDE.md：依赖统一走 vcpkg，禁止系统库兜底。）

---

## Task 0: 建立测试基线

**Files:** 无（只读验证）

- [ ] **Step 1: 构建并运行现有 human 测试，确认全绿**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Human" --output-on-failure
```
Expected: 全部 `HumanMouse*`/`HumanKeyboard*`/`HumanTest*`/`HumanModule*` 测试 PASS（0 failures）。

> 这是后续 TDD 的红/绿判定的基线。若此处已红，先修复再开始。

---

## Task 1: 后端 — `HumanMouse::moveTo(from, to, duration)` 起点重载

**Files:**
- Modify: `lib/wingman/include/wingman/human.hpp`（在 `moveTo(int x, int y, int approximateDurationMs);` 即第 60 行后新增声明）
- Modify: `lib/wingman/src/human.cpp`（在 `moveTo(int x, int y, int approximateDurationMs)` 实现即第 269 行后新增实现）
- Test: `lib/wingman/tests/human_test.cpp`（文件末尾追加）

- [ ] **Step 1: 写失败测试（`human_test.cpp` 末尾追加）**

```cpp
// ========== Plan 5: 起点贝塞尔移动重载 ==========

TEST(HumanMouseTest, MoveFromToDoesNotCrash) {
    HumanMouse mouse;
    // 短距离 + 小 duration，避免单测中长时间 sleep（moveAlongPath 内含 sleep_for）
    EXPECT_NO_THROW(mouse.moveTo(Point(0, 0), Point(5, 5), 1));
}

TEST(HumanMouseTest, MoveFromToSamePointDoesNotCrash) {
    HumanMouse mouse;
    // 起点与终点相同：应早退仅定位，不崩溃
    EXPECT_NO_THROW(mouse.moveTo(Point(50, 50), Point(50, 50), 200));
}
```

- [ ] **Step 2: 运行测试，验证编译失败（方法未定义）**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -15
```
Expected: 编译/链接错误 — `no matching function for call to 'wingman::HumanMouse::moveTo(wingman::Point, wingman::Point, int)'`（方法尚未声明）。

- [ ] **Step 3: 在 `human.hpp` 声明新重载**

在 `lib/wingman/include/wingman/human.hpp` 中，定位到：

```cpp
    // Move to target position, specifying approximate duration
    void moveTo(int x, int y, int approximateDurationMs);
```

在其**下方**插入：

```cpp

    // Move from an explicit start point to target along a Bezier path (Plan 5)
    void moveTo(const Point& from, const Point& to, int durationMs);
```

- [ ] **Step 4: 在 `human.cpp` 实现新重载**

在 `lib/wingman/src/human.cpp` 中，定位到 `moveTo(int x, int y, int approximateDurationMs)` 实现结尾（约第 269 行 `}`），在其**下方**插入：

```cpp

void HumanMouse::moveTo(const Point& from, const Point& to, int durationMs) {
    // 起点与终点重合：仅定位，避免空路径
    if (from.x == to.x && from.y == to.y) {
        getInput().mouseMove(to.x, to.y);
        return;
    }

    // 临时将移动时长范围约束到指定 duration（复用现有 duration 机制）
    HumanMouseConfig originalConfig = config_;
    config_.minMoveDuration = durationMs - config_.moveVariance;
    config_.maxMoveDuration = durationMs + config_.moveVariance;

    // 从显式起点生成贝塞尔路径并移动（addRandomness/moveAlongPath 为 private 成员，可直接调用）
    std::vector<Point> path = generateBezierPath(from, to);
    addRandomness(path);
    moveAlongPath(path);

    config_ = originalConfig;
}
```

- [ ] **Step 5: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "MoveFromTo" --output-on-failure
```
Expected: `HumanMouseTest.MoveFromToDoesNotCrash` 与 `HumanMouseTest.MoveFromToSamePointDoesNotCrash` 均 PASS。

- [ ] **Step 6: Commit**

```bash
git add lib/wingman/include/wingman/human.hpp lib/wingman/src/human.cpp lib/wingman/tests/human_test.cpp
git commit -m "feat(human): add HumanMouse::moveTo(from, to, duration) Bezier overload"
```

---

## Task 2: 后端 — `HumanMouse::middleClick`

**Files:**
- Modify: `lib/wingman/include/wingman/human.hpp`（在 `void rightClick(int x, int y);` 即第 65 行后新增声明）
- Modify: `lib/wingman/src/human.cpp`（在 `rightClick` 实现即第 301 行后新增实现）
- Test: `lib/wingman/tests/human_test.cpp`（文件末尾追加）

- [ ] **Step 1: 写失败测试（`human_test.cpp` 末尾追加）**

```cpp
// ========== Plan 5: middleClick ==========

TEST(HumanMouseTest, MiddleClickDoesNotCrash) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.middleClick(100, 100));
}
```

- [ ] **Step 2: 运行测试，验证编译失败（方法未定义）**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -15
```
Expected: 编译错误 — `'class wingman::HumanMouse' has no member named 'middleClick'`。

- [ ] **Step 3: 在 `human.hpp` 声明 `middleClick`**

在 `lib/wingman/include/wingman/human.hpp` 中，定位到：

```cpp
    void rightClick(int x, int y);
    void doubleClick(int x, int y);
```

在 `rightClick` 行与 `doubleClick` 行**之间**插入：

```cpp
    void middleClick(int x, int y);  // Plan 5: 中键点击
```

- [ ] **Step 4: 在 `human.cpp` 实现 `middleClick`（仿 `rightClick`）**

在 `lib/wingman/src/human.cpp` 中，定位到 `rightClick` 实现结尾（约第 301 行 `}`），在其**下方**插入：

```cpp

void HumanMouse::middleClick(int x, int y) {
    // Move to target position
    moveTo(x, y);

    // Random delay before click
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // Middle click
    getInput().mouseMove(x, y);
    getInput().mouseClick(platform::MouseButton::Middle);

    spdlog::debug("HumanMouse: middle-clicked at ({}, {})", x, y);
}
```

- [ ] **Step 5: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "MiddleClick" --output-on-failure
```
Expected: `HumanMouseTest.MiddleClickDoesNotCrash` PASS。

- [ ] **Step 6: Commit**

```bash
git add lib/wingman/include/wingman/human.hpp lib/wingman/src/human.cpp lib/wingman/tests/human_test.cpp
git commit -m "feat(human): add HumanMouse::middleClick"
```

---

## Task 3: 脚本层 — 高层配置模型 + `getConfig` / `setConfig`

**Files:**
- Modify: `lib/wingman/src/script/modules/human_module.cpp`（顶部 namespace 内新增 struct/辅助函数；`createHumanModule` 内新增 2 个函数）
- Test: `lib/wingman/tests/script_function_test.cpp`（在 `HumanModuleFunctionsTest` 段即第 1383 行后新增测试）

- [ ] **Step 1: 写失败测试（`script_function_test.cpp`，在 `HumanModuleFunctionsTest, KeyboardTypeDoesNotCrash` 测试之后追加）**

```cpp
// ========== Plan 5: human 高层 API（文档声明） ==========

TEST(HumanModuleFunctionsTest, GetConfigReturnsHighLevelFields) {
    auto fn = findFunction("human", "getConfig");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    // 文档承诺的 4 个扁平字段（human.md:359）
    EXPECT_NE(result.get("delay_min"), nullptr);
    EXPECT_NE(result.get("delay_max"), nullptr);
    EXPECT_NE(result.get("move_speed"), nullptr);
    EXPECT_NE(result.get("typing_variance"), nullptr);
}

TEST(HumanModuleFunctionsTest, SetConfigUpdatesDelayRangeRoundTrip) {
    auto setFn = findFunction("human", "setConfig");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    ASSERT_FALSE(getFn.name.empty());

    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_min"), ScriptValue::fromInt(40)}).isNull());
    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_max"), ScriptValue::fromInt(120)}).isNull());

    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 40);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 120);
}

TEST(HumanModuleFunctionsTest, SetConfigUnknownKeyDoesNotCrash) {
    auto fn = findFunction("human", "setConfig");
    ASSERT_FALSE(fn.name.empty());
    // 未知 key 应静默忽略（脚本层惯例），不抛异常
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_key"), ScriptValue::fromInt(1)}));
}
```

- [ ] **Step 2: 运行测试，验证失败（函数未注册）**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "GetConfigReturnsHighLevelFields|SetConfigUpdatesDelayRangeRoundTrip" --output-on-failure
```
Expected: FAIL — `findFunction("human", "getConfig")` 返回空名（`fn.name.empty()`），`ASSERT_FALSE` 失败。

- [ ] **Step 3: 在 `human_module.cpp` 顶部 namespace 内新增高层配置模型与映射辅助**

打开 `lib/wingman/src/script/modules/human_module.cpp`，定位到 namespace 开头：

```cpp
namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createHumanModule() {
```

在 `ModuleDescriptor createHumanModule() {` **之前**插入以下内容：

```cpp

// ========== Plan 5: 高层配置模型 + 后端映射 ==========
// 文档（human.md）暴露扁平高层字段：delay_min/delay_max/move_speed/typing_variance。
// 后端配置分散在 HumanMouseConfig/HumanKeyboardConfig，这里做语义映射。

struct HumanScriptConfig {
    int delayMin = 100;          // 随机延迟下限 ms（-> get_config.delay_min）
    int delayMax = 300;          // 随机延迟上限 ms（-> get_config.delay_max）
    double moveSpeed = 1.0;      // 移动速度系数 0.1-2.0（-> get_config.move_speed）
    double typingVariance = 0.5; // 输入变异度 0.0-1.0（-> get_config.typing_variance）
};

static HumanScriptConfig& humanScriptConfig() {
    static HumanScriptConfig instance;
    return instance;
}

// 把高层配置映射写入后端 mouse/keyboard config（仅改相关字段，保留其余字段）
static void applyHumanConfigToBackend(const HumanScriptConfig& cfg) {
    // mouse: move_speed -> moveDuration；delay -> clickDelay
    HumanMouseConfig mc = Human::mouse().getConfig();
    double speed = cfg.moveSpeed < 0.1 ? 0.1 : (cfg.moveSpeed > 2.0 ? 2.0 : cfg.moveSpeed);
    mc.minMoveDuration = static_cast<int>(100.0 / speed);
    mc.maxMoveDuration = static_cast<int>(300.0 / speed);
    mc.clickDelayMin = cfg.delayMin;
    mc.clickDelayMax = cfg.delayMax;
    Human::setMouseConfig(mc);

    // keyboard: typing_variance -> typeDelay 范围宽度
    HumanKeyboardConfig kc = Human::keyboard().getConfig();
    double v = cfg.typingVariance < 0.0 ? 0.0 : (cfg.typingVariance > 1.0 ? 1.0 : cfg.typingVariance);
    kc.typeDelayMin = 50;                              // 基准下限
    kc.typeDelayMax = 50 + static_cast<int>(100.0 * v); // v=0 -> 50/50 固定；v=1 -> 50/150
    Human::setKeyboardConfig(kc);
}

```

- [ ] **Step 4: 在 `createHumanModule` 内注册 `getConfig` 与 `setConfig`**

在 `lib/wingman/src/script/modules/human_module.cpp` 中，定位到 `keyboard_type` 注册块结尾与 `return mod;` 之间：

```cpp
	mod.functions.push_back({"keyboard_type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		bool randomCase = args.size() > 1 ? args[1].asBool() : false;
		Human::keyboard().type(args[0].asString(), randomCase);
		return ScriptValue::null();
	}, "text:string, randomCase:bool? -> nil"});

	return mod;
```

在 `keyboard_type` 块之后、`return mod;` 之前插入：

```cpp

	// ========== Plan 5: 文档声明的高层拟人化 API（camelCase；Python 自动转 snake_case）==========

	mod.functions.push_back({"getConfig", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& c = humanScriptConfig();
		return ScriptValue::fromObject({
			{"delay_min", ScriptValue::fromInt(c.delayMin)},
			{"delay_max", ScriptValue::fromInt(c.delayMax)},
			{"move_speed", ScriptValue::fromFloat(c.moveSpeed)},
			{"typing_variance", ScriptValue::fromFloat(c.typingVariance)}
		});
	}, "() -> {delay_min,delay_max,move_speed,typing_variance}"});

	mod.functions.push_back({"setConfig", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		std::string key = args.size() > 0 ? args[0].asString() : std::string();
		const ScriptValue& value = args.size() > 1 ? args[1] : ScriptValue::null();
		if (key == "delay_min") {
			c.delayMin = static_cast<int>(value.asInt(c.delayMin));
		} else if (key == "delay_max") {
			c.delayMax = static_cast<int>(value.asInt(c.delayMax));
		} else if (key == "move_speed") {
			c.moveSpeed = value.asFloat(c.moveSpeed);
		} else if (key == "typing_variance") {
			c.typingVariance = value.asFloat(c.typingVariance);
		}
		// 未知 key 静默忽略
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "key:string, value:any -> nil"});
```

- [ ] **Step 5: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "GetConfigReturnsHighLevelFields|SetConfig" --output-on-failure
```
Expected: 3 个新测试全部 PASS。

- [ ] **Step 6: Commit**

```bash
git add lib/wingman/src/script/modules/human_module.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(human): add getConfig/setConfig high-level config bridge"
```

---

## Task 4: 脚本层 — `setDelayRange` / `setMoveSpeed` / `setTypingVariance`

**Files:**
- Modify: `lib/wingman/src/script/modules/human_module.cpp`（`createHumanModule` 内新增 3 个函数）
- Test: `lib/wingman/tests/script_function_test.cpp`（追加测试）

**依赖:** Task 3（复用 `humanScriptConfig()` / `applyHumanConfigToBackend()`）。

- [ ] **Step 1: 写失败测试（`script_function_test.cpp` 追加）**

```cpp
TEST(HumanModuleFunctionsTest, SetDelayRangeRoundTrip) {
    auto setFn = findFunction("human", "setDelayRange");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromInt(50), ScriptValue::fromInt(200)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 50);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 200);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedRoundTrip) {
    auto setFn = findFunction("human", "setMoveSpeed");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(1.5)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("move_speed")->asFloat(), 1.5, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetTypingVarianceRoundTrip) {
    auto setFn = findFunction("human", "setTypingVariance");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(0.3)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("typing_variance")->asFloat(), 0.3, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedAffectsBackendMoveDuration) {
    // 验证映射应用：move_speed=2.0 -> minMoveDuration≈50, maxMoveDuration≈150
    auto setFn = findFunction("human", "setMoveSpeed");
    ASSERT_FALSE(setFn.name.empty());
    setFn({ScriptValue::fromFloat(2.0)});
    auto mc = Human::mouse().getConfig();
    EXPECT_EQ(mc.minMoveDuration, 50); // 100/2
    EXPECT_EQ(mc.maxMoveDuration, 150); // 300/2
}
```

> 注：最后一个测试需要在文件顶部已 `#include "wingman/human.hpp"`（`script_function_test.cpp` 当前未 include；本步骤同时补 include）。

- [ ] **Step 2: 运行测试，验证失败（函数未注册）**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "SetDelayRange|SetMoveSpeed|SetTypingVariance" --output-on-failure
```
Expected: FAIL — `findFunction` 返回空名 / `Human::mouse` 未声明（缺 include）。

- [ ] **Step 3: 在 `script_function_test.cpp` 顶部补 include**

在 `lib/wingman/tests/script_function_test.cpp` 现有 `#include` 区（约第 1-8 行），追加：

```cpp
#include "wingman/human.hpp"
```

- [ ] **Step 4: 在 `human_module.cpp` 注册 3 个 setter（紧接 Task 3 的 `setConfig` 之后）**

在 `lib/wingman/src/script/modules/human_module.cpp` 的 `setConfig` 注册块之后插入：

```cpp

	mod.functions.push_back({"setDelayRange", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.delayMin = static_cast<int>(args.size() > 0 ? args[0].asInt(c.delayMin) : c.delayMin);
		c.delayMax = static_cast<int>(args.size() > 1 ? args[1].asInt(c.delayMax) : c.delayMax);
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "min:int, max:int -> nil"});

	mod.functions.push_back({"setMoveSpeed", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.moveSpeed = args.size() > 0 ? args[0].asFloat(c.moveSpeed) : c.moveSpeed;
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "speed:float -> nil"});

	mod.functions.push_back({"setTypingVariance", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.typingVariance = args.size() > 0 ? args[0].asFloat(c.typingVariance) : c.typingVariance;
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "variance:float -> nil"});
```

- [ ] **Step 5: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "SetDelayRange|SetMoveSpeed|SetTypingVariance|SetMoveSpeedAffects" --output-on-failure
```
Expected: 4 个新测试全部 PASS。

- [ ] **Step 6: Commit**

```bash
git add lib/wingman/src/script/modules/human_module.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(human): add setDelayRange/setMoveSpeed/setTypingVariance with backend mapping"
```

---

## Task 5: 脚本层 — `randomDelay` + `naturalType`

**Files:**
- Modify: `lib/wingman/src/script/modules/human_module.cpp`
- Test: `lib/wingman/tests/script_function_test.cpp`

- [ ] **Step 1: 写失败测试（`script_function_test.cpp` 追加）**

```cpp
TEST(HumanModuleFunctionsTest, RandomDelayExplicitRangeDoesNotCrash) {
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    // 显式传 0-1ms，避免测试耗时（默认 100-300ms 会真实 sleep）
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(0), ScriptValue::fromInt(1)}));
}

TEST(HumanModuleFunctionsTest, RandomDelayNoArgsUsesDefaultRange) {
    // 无参调用走默认 100-300ms 路径（仅验证可调用，接受单次 ~300ms sleep）
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({}));
}

TEST(HumanModuleFunctionsTest, NaturalTypeDoesNotCrash) {
    auto fn = findFunction("human", "naturalType");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hi")}));
}
```

- [ ] **Step 2: 运行测试，验证失败（函数未注册）**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "RandomDelay|NaturalType" --output-on-failure
```
Expected: FAIL — `findFunction("human", "randomDelay")` 返回空名。

- [ ] **Step 3: 在 `human_module.cpp` 注册 `randomDelay` 与 `naturalType`（紧接 Task 4 之后）**

```cpp

	mod.functions.push_back({"randomDelay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int minMs = args.size() > 0 ? static_cast<int>(args[0].asInt(100)) : 100;
		int maxMs = args.size() > 1 ? static_cast<int>(args[1].asInt(300)) : 300;
		Human::mouse().randomDelay(minMs, maxMs);
		return ScriptValue::null();
	}, "min:int?, max:int? -> nil"});

	mod.functions.push_back({"naturalType", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().type(args.size() > 0 ? args[0].asString() : std::string());
		return ScriptValue::null();
	}, "text:string -> nil"});
```

- [ ] **Step 4: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "RandomDelay|NaturalType" --output-on-failure
```
Expected: 3 个新测试全部 PASS。

- [ ] **Step 5: Commit**

```bash
git add lib/wingman/src/script/modules/human_module.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(human): add randomDelay/naturalType script bridge"
```

---

## Task 6: 脚本层 — `moveMouse`（起点贝塞尔移动）

**Files:**
- Modify: `lib/wingman/src/script/modules/human_module.cpp`
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 1（后端 `moveTo(from, to, duration)` 重载）。

- [ ] **Step 1: 写失败测试（`script_function_test.cpp` 追加）**

```cpp
TEST(HumanModuleFunctionsTest, MoveMouseWithDefaultDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(50), ScriptValue::fromInt(50)
    }));
}

TEST(HumanModuleFunctionsTest, MoveMouseExplicitDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromInt(1) // 1ms，避免测试耗时
    }));
}
```

- [ ] **Step 2: 运行测试，验证失败**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "MoveMouse" --output-on-failure
```
Expected: FAIL — `findFunction("human", "moveMouse")` 返回空名。

- [ ] **Step 3: 在 `human_module.cpp` 注册 `moveMouse`（紧接 Task 5 之后）**

```cpp

	mod.functions.push_back({"moveMouse", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x1 = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		int y1 = static_cast<int>(args.size() > 1 ? args[1].asInt(0) : 0);
		int x2 = static_cast<int>(args.size() > 2 ? args[2].asInt(0) : 0);
		int y2 = static_cast<int>(args.size() > 3 ? args[3].asInt(0) : 0);
		int duration = args.size() > 4 ? static_cast<int>(args[4].asInt(500)) : 500;
		Human::mouse().moveTo(Point(x1, y1), Point(x2, y2), duration);
		return ScriptValue::null();
	}, "x1:int, y1:int, x2:int, y2:int, duration:int? -> nil"});
```

- [ ] **Step 4: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "MoveMouse" --output-on-failure
```
Expected: 2 个新测试 PASS。

- [ ] **Step 5: Commit**

```bash
git add lib/wingman/src/script/modules/human_module.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(human): add moveMouse script bridge with start point"
```

---

## Task 7: 脚本层 — `naturalClick`（含 left/right/middle 分发）

**Files:**
- Modify: `lib/wingman/src/script/modules/human_module.cpp`
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 2（后端 `middleClick`）。

- [ ] **Step 1: 写失败测试（`script_function_test.cpp` 追加）**

```cpp
TEST(HumanModuleFunctionsTest, NaturalClickLeftDefaultDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)})); // 默认 left
}

TEST(HumanModuleFunctionsTest, NaturalClickRightButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("right")}));
}

TEST(HumanModuleFunctionsTest, NaturalClickMiddleButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("middle")}));
}
```

- [ ] **Step 2: 运行测试，验证失败**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "NaturalClick" --output-on-failure
```
Expected: FAIL — `findFunction("human", "naturalClick")` 返回空名。

- [ ] **Step 3: 在 `human_module.cpp` 注册 `naturalClick`（紧接 Task 6 之后）**

```cpp

	mod.functions.push_back({"naturalClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		int y = static_cast<int>(args.size() > 1 ? args[1].asInt(0) : 0);
		std::string button = args.size() > 2 ? args[2].asString("left") : "left";
		if (button == "right") {
			Human::mouse().rightClick(x, y);
		} else if (button == "middle") {
			Human::mouse().middleClick(x, y);
		} else {
			Human::mouse().click(x, y); // left 及未知值默认左键
		}
		return ScriptValue::null();
	}, "x:int, y:int, button:string? -> nil"});
```

- [ ] **Step 4: 运行测试，验证通过**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "NaturalClick" --output-on-failure
```
Expected: 3 个新测试 PASS。

- [ ] **Step 5: Commit**

```bash
git add lib/wingman/src/script/modules/human_module.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(human): add naturalClick script bridge with button dispatch"
```

---

## Task 8: 文档对齐 + 全量回归

**Files:**
- 无代码改动（验证 `human.md` 已声明函数与代码全对齐，并跑全量 human 测试 + ModulePresence smoke test）

- [ ] **Step 1: 核对 `docs/api/human.md` 函数清单与脚本层注册一一对应**

Run:
```bash
# 列出文档声明的 9 个函数
grep -oE '\`(random_delay|move_mouse|natural_click|natural_type|set_delay_range|set_move_speed|set_typing_variance|set_config|get_config)' docs/api/human.md | sort -u
```
Expected: 输出全部 9 个 snake_case 函数名。

```bash
# 列出 human 模块注册的函数（camelCase + 自动 snake 别名）
grep -oE 'mod.functions.push_back\(\{"[a-zA-Z]+"' lib/wingman/src/script/modules/human_module.cpp
```
Expected: 包含 `randomDelay`/`moveMouse`/`naturalClick`/`naturalType`/`setDelayRange`/`setMoveSpeed`/`setTypingVariance`/`setConfig`/`getConfig` 9 个新函数 + 原有 10 个 `mouse_*`/`keyboard_*`。Python 绑定层会自动为这 9 个生成 snake_case 别名（`getConfig`→`get_config` 等），与文档 snake_case 对齐。

- [ ] **Step 2: 全量 human 测试回归**

Run:
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Human" --output-on-failure
```
Expected: 所有 `Human*` 测试 PASS（后端 + 脚本层 + 原有底层函数测试均不回归）。

- [ ] **Step 3: ModulePresence smoke test 不回归**

`script_function_test.cpp:1474` 的 `ModuleFunctionsAreCallable` 会遍历所有模块函数用最小参数调用。新增的 9 个函数签名必须能被其参数推断逻辑安全调用（首参非 string 的用 `fromInt(1)` 填充）。

Run:
```bash
ctest --test-dir build-core-only-tests -R "ModuleFunctionsAreCallable|ModulePresence" --output-on-failure
```
Expected: PASS。

> ⚠️ 关注点：`setConfig`/`getConfig`/`setDelayRange` 等的首参。`getConfig()`/`setMoveSpeed`/`setTypingVariance`/`randomDelay` 无 string 首参——smoke test 会传 `fromInt(1)`。
> - `setConfig(key, value)`：首参 `:string` → smoke 传 `fromString("test")`，安全。
> - `naturalType(text)`：首参 `:string` → `fromString("test")`，安全。
> - `moveMouse`/`naturalClick`/`setDelayRange`：首参 `:int` → `fromInt(1)`，安全。
> - `setMoveSpeed`/`setTypingVariance`/`randomDelay`/`getConfig`：`fromInt(1)`，安全。
> 若 smoke test 报某函数抛异常，回到对应 Task 检查参数防御（本 plan 所有函数均用 `args.size() > N ? ... : default` 防御，应无问题）。

- [ ] **Step 4: Python snake_case 双注册验证（若启用 Python 构建）**

若 `build-python-tests` 可用：
```bash
cmake --build build-python-tests -j 2>&1 | tail -5
ctest --test-dir build-python-tests --output-on-failure
```
Expected: `python_name_conversion_test` PASS（验证 `getConfig`→`get_config`、`naturalClick`→`natural_click` 等转换规则覆盖新函数）。若未启用 Python，此步可跳过（双注册机制已由 Plan 3 验证）。

- [ ] **Step 5: Commit（若有文档微调）**

```bash
git add -A
git commit -m "docs(human): align human.md with implemented high-level API"
```
> 仅当 Step 1 发现文档与本 plan 实现有细微出入（如参数描述）时才需要；本 plan 严格按 `human.md` 实现，通常无需改动文档。

---

## 完成判据

- [ ] `docs/api/human.md` 声明的 9 个函数在脚本层全部可调用（Lua 用 camelCase，Python 用 snake_case）。
- [ ] 后端新增 `HumanMouse::moveTo(from, to, duration)` 与 `middleClick`，均有单测。
- [ ] 配置类函数通过 round-trip 测试，且 `set_move_speed` 映射后端 moveDuration 经测试验证。
- [ ] 全量 `Human*` 测试 + `ModulePresence`/`ModuleFunctionsAreCallable` smoke test 不回归。
- [ ] 8 个 Task 各自独立提交，commit message 用 `feat(human):` / `docs(human):` 前缀。

---

## 执行约束（来自 handoff §4 / CLAUDE.md）

- 第三方依赖统一 vcpkg；禁止系统库/FetchContent 兜底。
- 代码注释语言与代码库一致（**中文**）。
- 不主动 git 提交/分支，除非用户明确要求（本 plan 的 commit 步骤遵循 handoff §4 的 TDD 工作流；若用户指示本会话不提交，可改为末尾统一提交或跳过）。
- TDD：先写失败测试 → 实现 → 通过 → 提交。
- Windows 上 `WindowHandle` 是 HWND 强类型，不能与 int 比较（本 plan 不涉及，但执行时遇相关代码需注意）。
