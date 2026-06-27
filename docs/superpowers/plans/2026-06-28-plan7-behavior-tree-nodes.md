# Plan 7: behavior-tree 节点构造脚本桥接 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `docs/api/behavior-tree.md` 声明但脚本层缺失的 8 个节点构造函数（sequence/selector/parallel/inverter/repeat/wait/condition/action）桥接到 `bt` 脚本模块，并补必要的组装能力（addChild/setRoot），使文档↔代码一致、行为树可在脚本端完整构建并 tick。

**Architecture:** 纯脚本层桥接（后端 `behavior_tree.hpp` 节点类与 `BehaviorTree` 工厂方法已完备，无需改后端）。引入 **NodeRegistry**（`map<int, shared_ptr<BehaviorNode>>` + 自增 id）作为脚本↔C++ 的节点句柄机制；复合/装饰/叶子节点构造返回 handle，`addChild` 组装、`setRoot` 挂根。condition/action 用**脚本 callable 回调**（拷贝 `ScriptValue::callableVal` 进 C++ 闭包，复用 event/fsm/task 已验证模式），对接后端 `BehaviorTree::condition(name, std::function<bool()>)` / `action(name, std::function<NodeStatus()>)` 无参工厂方法。

**Tech Stack:** C++17、GoogleTest、ScriptValue tagged union（Callable 标签）、BehaviorTree 节点体系（shared_ptr）、`misc_modules.cpp` 的 `createBehaviorTreeModule()`。

---

## 关键设计决策（已与用户确认，勿改）

| 决策 | 结论 | 依据 |
|------|------|------|
| **节点组装模型** | **句柄(handle) + 独立 addChild/setRoot 函数** | 最灵活，可动态构建；后端复合节点已有 `addChild`，`BehaviorTree` 已有 `setRoot` |
| **condition/action 逻辑** | **脚本 callable 回调**（condition→bool、action→"SUCCESS"/"FAILURE"/"RUNNING"） | 复用 event/fsm/task 的 callable 机制，节点完全可脚本定制，符合文档语义；后端 `BehaviorTree::condition/action` 无参工厂方法直接对接 |
| **节点句柄机制** | **NodeRegistry**（`map<int, shared_ptr<BehaviorNode>>`，自增 id 从 1 起，0=无效） | shared_ptr 管理生命周期，避免 uia 裸指针(`misc_modules.cpp:248`)的悬空风险 |
| **后端是否改动** | **不改**（后端节点类 + 工厂方法 + tick 全部已实现，见 `behavior_tree.cpp:101-347`） | 交接文档"后端缺 condition/action/inverter/wait"为过时信息 |

> ⚠️ **不要**重新讨论"改后端 vs 纯桥接"——后端已完备，本 plan 是纯脚本层桥接。

---

## File Structure

- **Modify:** `lib/wingman/src/script/modules/misc_modules.cpp`
  - `createBehaviorTreeModule()`（bt 段，当前 169-197 行仅 create/tick/remove）
  - 在 `createBehaviorTreeModule()` **之前**（namespace 顶层）新增 NodeRegistry 三个 static helper：`btNodeRegistry()` / `btStoreNode()` / `btGetNode()`
  - 在 `createBehaviorTreeModule()` 内、`return mod;` 之前新增 10 个函数：sequence/selector/parallel/wait/inverter/repeat/condition/action/addChild/setRoot
- **Test:** `lib/wingman/tests/script_function_test.cpp`
  - 已 `#include "wingman/behavior_tree.hpp"`（第 5 行），BehaviorNode/SequenceNode 等类型可见
  - 已有 `findFunction(module, func)` helper（16-26 行）
  - 新增 `BtModuleFunctionsTest` fixture 测试组（追加到文件末尾）
- **Docs:** `docs/api/behavior-tree.md`（Task 6 把"节点构造待实现"改为已实现 + 补函数表）

---

## Build & Test Commands（所有 Task 通用）

```bash
# 构建（增量）
cmake --build build-core-only-tests -j 2>&1 | tail -5

# 跑 bt 相关测试（后端 behavior_tree_test.cpp 100 个 + 脚本层 BtModuleFunctionsTest）
ctest --test-dir build-core-only-tests -R "BtModule|BehaviorTree|SequenceNode|SelectorNode|ParallelNode|Inverter|Repeat|Condition|Action|Wait" --output-on-failure
```

> 约定：有效节点 handle ≥ 1；构造失败（如 inverter 无效 child、condition 无效 callable）返回 `0`。

---

## Task 0: 建立测试基线

**Files:** 无改动（确认后端 bt 测试 + 现有 bt 模块 create/tick/remove 不回归）

- [ ] **Step 1: 确认后端 behavior_tree 测试全绿**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -3
ctest --test-dir build-core-only-tests -R "Node|BehaviorTree|Sequence|Selector|Parallel|Inverter|Repeat|Condition|Action|Wait" --output-on-failure 2>&1 | tail -8
```
Expected: 后端 100 个 behavior_tree 测试全部 PASS（已在 `behavior_tree_test.cpp`）。

- [ ] **Step 2: 确认现有 bt 模块（create/tick/remove）可调用**
```bash
ctest --test-dir build-core-only-tests -R "ModulePresence" --output-on-failure 2>&1 | tail -5
```
Expected: `AllExpectedModulesExist` PASS（bt 模块存在）。`ModuleFunctionsAreCallable` 若 SEGFAULT 是**预先存在问题**（经 Plan 5 baseline 实验证明，非本 plan 引入），不阻塞。

---

## Task 1: NodeRegistry + 复合节点 sequence/selector/parallel

**Files:**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（bt 段顶部加 3 个 helper；`createBehaviorTreeModule()` 内加 3 个函数）
- Test: `lib/wingman/tests/script_function_test.cpp`（追加 `BtModuleFunctionsTest` 组）

**依赖:** 无（首个实现 task，建立 registry 模式）。

- [ ] **Step 1: 写失败测试（追加到 `script_function_test.cpp` 末尾）**

```cpp
// ========== Plan 7: bt 节点构造脚本桥接 ==========

TEST(BtModuleFunctionsTest, SequenceReturnsPositiveHandle) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("seq1")});
    ASSERT_TRUE(h.isInt());
    EXPECT_GT(h.asInt(), 0); // 有效句柄 ≥ 1
}

TEST(BtModuleFunctionsTest, SelectorReturnsPositiveHandle) {
    auto fn = findFunction("bt", "selector");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromString("sel1")}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SequenceDefaultNameDoesNotCrash) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认名
}

TEST(BtModuleFunctionsTest, ParallelWithPolicyStringReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("par"), ScriptValue::fromString("FAIL_ON_ONE")});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ParallelDefaultPolicyReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认 SUCCEED_ON_ALL
}
```

- [ ] **Step 2: 运行测试，验证失败**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "BtModule" --output-on-failure
```
Expected: FAIL — `findFunction("bt","sequence")` 返回空名。

- [ ] **Step 3: 在 `misc_modules.cpp` 的 `createBehaviorTreeModule()` **之前**加 NodeRegistry helpers**

定位 `ModuleDescriptor createBehaviorTreeModule() {`（约 169 行），在其**之前**（namespace 顶层，无缩进）插入：
```cpp
// ========== Plan 7: 行为树节点句柄注册表 ==========
// 脚本层通过 int handle 引用构造的 BehaviorNode（shared_ptr 生命周期管理）。
// handle 从 1 自增，0 表示无效。用于 addChild / setRoot 组装行为树。
static std::map<int, std::shared_ptr<BehaviorNode>>& btNodeRegistry() {
	static std::map<int, std::shared_ptr<BehaviorNode>> registry;
	return registry;
}
static int btStoreNode(std::shared_ptr<BehaviorNode> node) {
	static int nextId = 0;
	int id = ++nextId;
	btNodeRegistry()[id] = std::move(node);
	return id;
}
static std::shared_ptr<BehaviorNode> btGetNode(int handle) {
	auto& reg = btNodeRegistry();
	auto it = reg.find(handle);
	return it != reg.end() ? it->second : nullptr;
}
```
> 缩进用 Tab（与该文件一致）。`<map>` / `<memory>` 经 `behavior_tree.hpp` 传递可见。

- [ ] **Step 4: 在 `createBehaviorTreeModule()` 内、`return mod;` 之前加 3 个复合节点函数**

定位现有 `remove` 注册块之后、`return mod;` 之前，插入：
```cpp
	// ========== Plan 7: 节点构造（复合 / 装饰 / 叶子）==========

	mod.functions.push_back({"sequence", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Sequence") : "Sequence";
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::sequence(name)));
	}, "name:string? -> handle:int"});

	mod.functions.push_back({"selector", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Selector") : "Selector";
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::selector(name)));
	}, "name:string? -> handle:int"});

	mod.functions.push_back({"parallel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Parallel") : "Parallel";
		std::string policyStr = args.size() > 1 ? args[1].asString("SUCCEED_ON_ALL") : "SUCCEED_ON_ALL";
		ParallelNode::Policy policy = ParallelNode::Policy::SUCCEED_ON_ALL;
		if (policyStr == "SUCCEED_ON_ONE") policy = ParallelNode::Policy::SUCCEED_ON_ONE;
		else if (policyStr == "FAIL_ON_ALL") policy = ParallelNode::Policy::FAIL_ON_ALL;
		else if (policyStr == "FAIL_ON_ONE") policy = ParallelNode::Policy::FAIL_ON_ONE;
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::parallel(name, policy)));
	}, "name:string?, policy:string? -> handle:int"});
```

- [ ] **Step 5: 运行测试，验证通过**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "BtModule" --output-on-failure
```
Expected: 5 个新测试 PASS。

- [ ] **Step 6: Commit**
```bash
git add lib/wingman/src/script/modules/misc_modules.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(bt): add NodeRegistry and sequence/selector/parallel node constructors"
```

---

## Task 2: 叶子 wait + 装饰 inverter/repeat

**Files:**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（紧接 Task 1 parallel 之后）
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 1（NodeRegistry + 复合节点作 child）。

- [ ] **Step 1: 写失败测试（追加）**
```cpp
TEST(BtModuleFunctionsTest, WaitReturnsPositiveHandle) {
    auto fn = findFunction("bt", "wait");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromInt(100)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterWithChildReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto invFn = findFunction("bt", "inverter");
    ASSERT_FALSE(invFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(50)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(invFn({ScriptValue::fromInt(child)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterInvalidChildReturnsZero) {
    auto fn = findFunction("bt", "inverter");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromInt(999999)}).asInt(), 0); // 无效 child handle → 0
}

TEST(BtModuleFunctionsTest, RepeatWithCountReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto repFn = findFunction("bt", "repeat");
    ASSERT_FALSE(repFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(repFn({ScriptValue::fromInt(child), ScriptValue::fromInt(3)}).asInt(), 0);
}
```

- [ ] **Step 2: 运行测试，验证失败**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "BtModule" --output-on-failure
```
Expected: FAIL — wait/inverter/repeat 未注册。

- [ ] **Step 3: 在 Task 1 parallel 之后加 3 个函数**
```cpp
	mod.functions.push_back({"wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int ms = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		return ScriptValue::fromInt(btStoreNode(std::make_shared<WaitNode>(ms)));
	}, "milliseconds:int -> handle:int"});

	mod.functions.push_back({"inverter", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int childHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		auto child = btGetNode(childHandle);
		if (!child) return ScriptValue::fromInt(0); // 无效 child
		return ScriptValue::fromInt(btStoreNode(std::make_shared<InverterNode>(child)));
	}, "childHandle:int -> handle:int"});

	mod.functions.push_back({"repeat", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int childHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		int count = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1); // -1 = 无限
		auto child = btGetNode(childHandle);
		if (!child) return ScriptValue::fromInt(0);
		return ScriptValue::fromInt(btStoreNode(std::make_shared<RepeatNode>(child, count)));
	}, "childHandle:int, count:int? -> handle:int"});
```

- [ ] **Step 4: 运行测试，验证通过**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "BtModule" --output-on-failure
```
Expected: 4 个新测试 PASS（累计 9 个 BtModule 测试）。

- [ ] **Step 5: Commit**
```bash
git add lib/wingman/src/script/modules/misc_modules.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(bt): add wait/inverter/repeat node constructors"
```

---

## Task 3: addChild 组装复合节点

**Files:**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（紧接 Task 2 repeat 之后）
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 1（复合节点）、Task 2（叶子/装饰作 child）。

- [ ] **Step 1: 写失败测试（追加）**
```cpp
TEST(BtModuleFunctionsTest, AddChildToSequenceReturnsTrue) {
    auto seqFn = findFunction("bt", "sequence");
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    ASSERT_FALSE(addFn.name.empty());
    int64_t parent = seqFn({}).asInt();
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToParallelReturnsTrue) {
    auto parFn = findFunction("bt", "parallel");
    auto seqFn = findFunction("bt", "sequence");
    auto addFn = findFunction("bt", "addChild");
    int64_t parent = parFn({}).asInt();
    int64_t child = seqFn({}).asInt(); // 复合节点也可作 child
    EXPECT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildInvalidParentReturnsFalse) {
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(999999), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToLeafReturnsFalse) {
    // 叶子/装饰节点不支持 addChild（仅复合节点 Sequence/Selector/Parallel 支持）
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t leaf = waitFn({ScriptValue::fromInt(10)}).asInt();
    int64_t another = waitFn({ScriptValue::fromInt(20)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(leaf), ScriptValue::fromInt(another)}).asBool());
}
```

- [ ] **Step 2: 运行测试，验证失败**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "AddChild" --output-on-failure
```
Expected: FAIL — addChild 未注册。

- [ ] **Step 3: 在 Task 2 repeat 之后加 addChild**
```cpp
	mod.functions.push_back({"addChild", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int parentHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		int childHandle = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1);
		auto parent = btGetNode(parentHandle);
		auto child = btGetNode(childHandle);
		if (!parent || !child) return ScriptValue::fromBool(false);
		// 仅复合节点支持 addChild
		if (auto seq = std::dynamic_pointer_cast<SequenceNode>(parent)) { seq->addChild(child); return ScriptValue::fromBool(true); }
		if (auto sel = std::dynamic_pointer_cast<SelectorNode>(parent)) { sel->addChild(child); return ScriptValue::fromBool(true); }
		if (auto par = std::dynamic_pointer_cast<ParallelNode>(parent)) { par->addChild(child); return ScriptValue::fromBool(true); }
		return ScriptValue::fromBool(false); // 装饰/叶子节点不支持
	}, "parentHandle:int, childHandle:int -> bool"});
```

- [ ] **Step 4: 运行测试，验证通过**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "AddChild" --output-on-failure
```
Expected: 4 个新测试 PASS（累计 13 个 BtModule 测试）。

- [ ] **Step 5: Commit**
```bash
git add lib/wingman/src/script/modules/misc_modules.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(bt): add addChild to compose composite nodes"
```

---

## Task 4: condition（脚本 callable → bool 回调）

**Files:**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（紧接 Task 3 addChild 之后）
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 1（NodeRegistry）。这是**核心难点**（脚本 callable → C++ std::function<bool()>）。

- [ ] **Step 1: 写失败测试（追加）**

> 用 `ScriptValue::fromCallable` 构造一个 C++ 侧 callable 模拟脚本函数（Lua/Python 绑定层会把脚本函数转成同样的 Callable ScriptValue，见 `lua_marshal.cpp:81` / `python_marshal.cpp:82`）。

```cpp
TEST(BtModuleFunctionsTest, ConditionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // callable 返回 bool true（模拟脚本条件函数）
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true);
    });
    auto h = fn({ScriptValue::fromString("hp_low"), cb});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // 第二参非 callable → 返回 0
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(123)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionMissingCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("nocb")}).asInt(), 0); // 缺 callable
}
```

- [ ] **Step 2: 运行测试，验证失败**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Condition" --output-on-failure
```
Expected: FAIL — condition 未注册。

- [ ] **Step 3: 在 Task 3 addChild 之后加 condition**

```cpp
	mod.functions.push_back({"condition", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Condition") : "Condition";
		if (args.size() < 2 || !args[1].isCallable()) return ScriptValue::fromInt(0); // 无效 callable
		ScriptValue::CallableFunc cb = args[1].callableVal; // 拷贝；闭包持有，节点存活期间有效
		auto node = BehaviorTree::condition(name, [cb]() -> bool {
			return cb({}).asBool(false); // 无参回调脚本，返回 bool
		});
		return ScriptValue::fromInt(btStoreNode(node));
	}, "name:string, callback:callable -> handle:int"});
```
> 参考 `event_module.cpp:94`（`ScriptValue::CallableFunc callback = args[1].callableVal;` 后在闭包调用）。

- [ ] **Step 4: 运行测试，验证通过**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Condition" --output-on-failure
```
Expected: 3 个新测试 PASS（累计 16 个 BtModule 测试）。

- [ ] **Step 5: Commit**
```bash
git add lib/wingman/src/script/modules/misc_modules.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(bt): add condition node with script callable callback"
```

---

## Task 5: action（callable → NodeStatus）+ setRoot + 端到端组装 tick

**Files:**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（紧接 Task 4 condition 之后）
- Test: `lib/wingman/tests/script_function_test.cpp`

**依赖:** Task 1-4（全部节点 + addChild）。本 task 闭环：组装树 + tick，验证 callable 回调端到端工作。

- [ ] **Step 1: 写失败测试（追加）**
```cpp
TEST(BtModuleFunctionsTest, ActionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    EXPECT_GT(fn({ScriptValue::fromString("attack"), cb}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ActionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(1)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SetRootOnExistingTreeReturnsTrue) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    ASSERT_FALSE(setRootFn.name.empty());
    std::string tree = "plan7_setroot_test";
    createFn({ScriptValue::fromString(tree)});
    int64_t node = seqFn({}).asInt();
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(node)}).asBool());
    // 清理
    findFunction("bt", "remove")({ScriptValue::fromString(tree)});
}

TEST(BtModuleFunctionsTest, SetRootInvalidTreeReturnsFalse) {
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    int64_t node = seqFn({}).asInt();
    EXPECT_FALSE(setRootFn({ScriptValue::fromString("no_such_tree_xyz"), ScriptValue::fromInt(node)}).asBool());
}

// 端到端：condition(true) + action(SUCCESS) 组装成 sequence，setRoot，tick → SUCCESS
TEST(BtModuleFunctionsTest, EndToEndConditionActionTickReturnsSuccess) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_test";
    ASSERT_TRUE(createFn({ScriptValue::fromString(tree)}).asBool());

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true); // 条件成立
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS"); // 动作成功
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    ASSERT_GT(cond, 0); ASSERT_GT(act, 0); ASSERT_GT(seq, 0);

    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)}).asBool());
    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)}).asBool());
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)}).asBool());

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "SUCCESS");
    removeFn({ScriptValue::fromString(tree)});
}

// 端到端：condition(false) → sequence 失败 → action 不执行，tick → FAILURE
TEST(BtModuleFunctionsTest, EndToEndFalseConditionTickReturnsFailure) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_fail_test";
    createFn({ScriptValue::fromString(tree)});

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(false); // 条件不成立
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)});
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)});
    setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)});

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "FAILURE");
    removeFn({ScriptValue::fromString(tree)});
}
```

- [ ] **Step 2: 运行测试，验证失败**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Action|SetRoot|EndToEnd" --output-on-failure
```
Expected: FAIL — action/setRoot 未注册。

- [ ] **Step 3: 在 Task 4 condition 之后加 action + setRoot**
```cpp
	mod.functions.push_back({"action", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Action") : "Action";
		if (args.size() < 2 || !args[1].isCallable()) return ScriptValue::fromInt(0);
		ScriptValue::CallableFunc cb = args[1].callableVal;
		auto node = BehaviorTree::action(name, [cb]() -> NodeStatus {
			std::string s = cb({}).asString("FAILURE");
			if (s == "SUCCESS") return NodeStatus::SUCCESS;
			if (s == "RUNNING") return NodeStatus::RUNNING;
			return NodeStatus::FAILURE;
		});
		return ScriptValue::fromInt(btStoreNode(node));
	}, "name:string, callback:callable -> handle:int"});

	mod.functions.push_back({"setRoot", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string treeName = args.size() > 0 ? args[0].asString() : std::string();
		int nodeHandle = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1);
		auto tree = BehaviorTreeManager::instance().getTree(treeName);
		auto node = btGetNode(nodeHandle);
		if (!tree || !node) return ScriptValue::fromBool(false);
		tree->setRoot(node);
		return ScriptValue::fromBool(true);
	}, "treeName:string, nodeHandle:int -> bool"});
```

- [ ] **Step 4: 运行测试，验证通过**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Action|SetRoot|EndToEnd" --output-on-failure
```
Expected: 6 个新测试 PASS（累计 22 个 BtModule 测试）。两个端到端测试验证 callable 回调在 tick 时真正被调用（condition bool / action status 正确传播）。

- [ ] **Step 5: Commit**
```bash
git add lib/wingman/src/script/modules/misc_modules.cpp lib/wingman/tests/script_function_test.cpp
git commit -m "feat(bt): add action node, setRoot, and end-to-end tree tick"
```

---

## Task 6: 文档对齐 + 全量回归

**Files:**
- Modify: `docs/api/behavior-tree.md`（把"节点构造待实现"改为已实现 + 补节点构造 API 文档）
- 无代码改动（验证全量 bt 测试 + ModulePresence 不回归）

- [ ] **Step 1: 全量 bt 测试回归**
```bash
cmake --build build-core-only-tests -j 2>&1 | tail -3
ctest --test-dir build-core-only-tests -R "BtModule|Node|BehaviorTree|Sequence|Selector|Parallel|Inverter|Repeat|Condition|Action|Wait" --output-on-failure 2>&1 | tail -8
```
Expected: 后端 100 + 脚本层 22 个 bt 相关测试全 PASS，无回归。

- [ ] **Step 2: 核对文档函数清单与脚本层一一对应**
```bash
# 文档声明的节点构造函数
grep -oE '`(sequence|selector|parallel|inverter|repeat|wait|condition|action)' docs/api/behavior-tree.md | sort -u
# 代码注册的 bt 函数（create/tick/remove + 10 新增）
grep -oE 'mod\.functions\.push_back\(\{"[a-zA-Z]+"' lib/wingman/src/script/modules/misc_modules.cpp | grep -A999 createBehaviorTreeModule
```
Expected: 文档含 8 个节点构造函数名；代码 bt 段含 create/tick/remove + sequence/selector/parallel/wait/inverter/repeat/condition/action/addChild/setRoot（13 个）。

- [ ] **Step 3: 更新 `docs/api/behavior-tree.md`**

把顶部警告（第 5 行）与"节点构造待实现"段（179-187 行）改为已实现，并补节点构造 API 文档：

1. 删除/改写第 5 行的 `> ⚠️ 当前仅实现 create/tick/remove ...` 警告为：
   > ✅ 已实现 create/tick/remove + 节点构造（sequence/selector/parallel/inverter/repeat/wait/condition/action）+ 组装（addChild/setRoot）。

2. 把"## 节点构造待实现"段替换为"## 节点构造"已实现文档，含函数签名表：

```markdown
## 节点构造

行为树由节点组成。构造函数返回节点句柄（int，≥1），用 `addChild` 组装、`setRoot` 挂到树。

### 复合节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `sequence(name?)` | `sequence(name?)` | 序列：所有子节点成功才成功 | name: 节点名(可选) → handle |
| `selector(name?)` | `selector(name?)` | 选择：任一子节点成功则成功 | name → handle |
| `parallel(name?, policy?)` | `parallel(name?, policy?)` | 并行 | policy: SUCCEED_ON_ALL/SUCCEED_ON_ONE/FAIL_ON_ALL/FAIL_ON_ONE(默认 SUCCEED_ON_ALL) → handle |

### 装饰节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `inverter(child)` | `inverter(child)` | 反转子节点结果 | childHandle → handle |
| `repeat(child, count?)` | `repeat(child, count?)` | 重复执行子节点 | childHandle, count(默认 -1=无限) → handle |

### 叶子节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `wait(ms)` | `wait(ms)` | 等待指定毫秒 | milliseconds → handle |
| `condition(name, callback)` | `condition(name, callback)` | 条件：回调返回 bool | name, callback(): bool → handle |
| `action(name, callback)` | `action(name, callback)` | 动作：回调返回状态 | name, callback(): "SUCCESS"/"FAILURE"/"RUNNING" → handle |

### 组装

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `addChild(parent, child)` | `addChild(parent, child)` | 把子节点加到复合节点 | parentHandle, childHandle → bool |
| `setRoot(tree, node)` | `setRoot(tree, node)` | 设置行为树根节点 | treeName, nodeHandle → bool |

> condition/action 的 callback 在行为树 tick 时被回调。Lua 回调需在创建脚本的同一线程 tick（Lua state 非线程安全）；Python 回调线程安全（GIL）。
```

3. 更新底部"## 可用接口"表，把节点构造与组装函数补入。

- [ ] **Step 4: ModulePresence 不回归确认**
```bash
ctest --test-dir build-core-only-tests -R "AllExpectedModulesExist" --output-on-failure 2>&1 | tail -5
```
Expected: PASS（bt 模块仍存在）。`ModuleFunctionsAreCallable` 若 SEGFAULT 是预先存在问题（非本 plan 引入），不阻塞。

- [ ] **Step 5: Commit（文档更新）**
```bash
git add docs/api/behavior-tree.md
git commit -m "docs(bt): document implemented node constructors and assembly API"
```

---

## 完成判据

- [ ] `docs/api/behavior-tree.md` 声明的 8 个节点构造函数（sequence/selector/parallel/inverter/repeat/wait/condition/action）在脚本层全部可调用。
- [ ] condition/action 通过脚本 callable 回调工作（端到端 tick 测试验证 bool/NodeStatus 正确传播）。
- [ ] 节点句柄机制（NodeRegistry，shared_ptr 生命周期）+ addChild/setRoot 组装完整可用。
- [ ] 全量 bt 测试（后端 100 + 脚本层 22）不回归。
- [ ] 文档 `behavior-tree.md` 更新为已实现，函数表与代码一致。
- [ ] 7 个 Task 各自独立提交，commit message 用 `feat(bt):` / `docs(bt):` 前缀。

---

## 执行约束（来自 handoff §4 / CLAUDE.md）

- 第三方依赖统一由 vcpkg 管理；禁止回退系统库 / FetchContent（本 plan 无新依赖）
- 代码注释语言与代码库一致（中文）
- 不主动 git 提交/分支，除非用户明确要求（本 plan 的 TDD 提交属已批准的 subagent-driven-development 流程）
- TDD：先写失败测试 → 实现 → 通过 → 提交
- 后端 `behavior_tree.hpp` / `behavior_tree.cpp` **不改**（已完备）
