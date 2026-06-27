# Plan 6: uia OO API 全量实现 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `docs/api/uia/index.md` + `docs/api/types.md` 声明的完整 uia OO API（根元素 3 + 通用查找 4 + 专用查找 3 + 事件监听 3 + UIElement 12 方法）补到脚本层，并补齐后端缺口（getChildren/expand/collapse/doubleClick/fromWindow/findAll/wait + 事件监听），Windows + Mac 双平台对齐，使文档↔代码一致、UIElement 可作为 OO 对象在脚本端操作。

**Architecture:** 三层：(1) **后端补齐** —— 扩展 `IUIAElement`/`IUIAManager` 接口 + Windows（UIA pattern/TreeWalker/event）+ Mac（AX API/AXObserver）实现；(2) **脚本层 OO 绑定** —— UIElement 句柄机制（`shared_ptr` registry，仿 Plan 7 NodeRegistry）+ UIElement 作 `ScriptValue::Object`（含 handle 字段 + 各方法 Callable）；(3) **脚本函数** —— 13 个模块函数 + 事件监听 callable 回调（仿 Plan 7 condition/action）。

**Tech Stack:** C++17、GoogleTest、Windows UIA（IUIAutomation/ExpandCollapsePattern/TreeWalker/Add*EventHandler）、macOS Accessibility（AXUIElement/kAXChildrenAttribute/AXObserver）、ScriptValue tagged union（Object + Callable）。

---

## 关键设计决策（已与用户确认）

| 决策 | 结论 | 依据 |
|------|------|------|
| **范围** | **全做**（后端补齐 + 完整 OO + 事件 + 双平台） | 用户 AskUserQuestion |
| **测试策略** | **Mac 真实 UIA 烟雾测试**（Mac 是完整实现，非 stub） | 调研修正 + 用户选择；Mac 可经 AX API 测真实行为，但本 plan 用烟雾测试（函数注册 + OO 对象结构 + 句柄机制 + stub/空返回防御），真实 UI 交互依赖 CI/手动 |
| **UIElement OO 表达** | **ScriptValue::Object**（含 `"_handle": int` + 各方法为 Callable 闭包捕获 handle） | ScriptValue 已支持 Object + Callable；无需改脚本引擎架构 |
| **句柄机制** | **UIAElementRegistry**（`map<int, shared_ptr<IUIAElement>>`，自增 id，0=无效） | 仿 Plan 7 NodeRegistry；替换 misc_modules.cpp 现有裸指针 handle（悬空风险） |
| **事件回调** | **脚本 callable**（拷贝 callableVal 进 C++ 事件处理器闭包） | 仿 Plan 7 condition/action + event_module 模式 |
| **跨平台** | **Windows + Mac 双平台实现**（Linux 暂不支持，文档标注） | win_automation.cpp + mac_automation.cpp 都已存在 |

> ⚠️ **不要**重新讨论范围（全做已定）或测试策略（Mac 烟雾测试已定）。

---

## File Structure

**后端（接口 + 双平台实现）：**
- Modify: `lib/wingman/include/wingman/ui_automation.hpp`（IUIAElement 加 getChildren/expand/collapse/isExpanded/doubleClick；IUIAManager 加 fromWindow/findAllByControlType/waitForName + 事件监听接口）
- Modify: `lib/wingman/src/platform/win/win_automation.cpp`（Windows UIA 实现）
- Modify: `lib/wingman/src/platform/mac/mac_automation.cpp`（Mac AX 实现）

**脚本层：**
- Modify: `lib/wingman/src/script/modules/misc_modules.cpp`（`createUIAutomationModule()`：替换裸指针 handle 为 UIAElementRegistry + OO UIElement 对象构造 + 13 函数 + 事件）

**测试：**
- Modify: `lib/wingman/tests/ui_automation_test.cpp`（后端接口测试，Mac 上烟雾）
- Modify: `lib/wingman/tests/script_function_test.cpp`（脚本层 `UiaModuleFunctionsTest`）

**文档：**
- Modify: `docs/api/uia/index.md`（标注已实现 vs 规划中）
- Modify: `docs/api/types.md`（UIElement 方法表）

---

## 阶段划分（跨会话执行）

### Phase 1: 后端接口扩展 + 双平台补齐（核心，无事件）— 详细 task 见下
扩展接口 + Windows/Mac 实现 getChildren/expand/collapse/isExpanded/doubleClick/fromWindow/findAllByControlType/waitForName。

### Phase 2: 事件监听（难度高，独立子阶段）— 概要 task
Windows（AddPropertyChangedEventHandler/AddStructureChangedEventHandler + 事件线程/消息循环）+ Mac（AXObserver + run loop）+ 脚本 callable 回调 + removeEventListener 生命周期。

### Phase 3: 脚本层 OO 绑定 + 13 函数 — 详细 task 见下
UIAElementRegistry + UIElement Object 构造 + 12 方法 + 13 模块函数。

### Phase 4: 文档对齐 + 全量回归 — 概要 task
index.md/types.md 标注 + Mac 烟雾测试全量回归。

---

## Build & Test Commands（通用）

```bash
cmake --build build-core-only-tests -j 2>&1 | tail -5
ctest --test-dir build-core-only-tests -R "Uia|UIA|Element" --output-on-failure
```

> Mac 烟雾测试：验证函数注册、OO 对象结构（`isObject()` + `_handle` 字段 + 方法 Callable）、句柄机制（≥1 有效，0 无效）、空/无效输入防御。真实 UIA 交互（点击/查找真实元素）需辅助功能权限 + 真实 UI，依赖手动/CI。

---

## Phase 1 详细 Task

### Task 1: 扩展 IUIAElement/IUIAManager 接口

**Files:** Modify `lib/wingman/include/wingman/ui_automation.hpp`

- [ ] **Step 1: IUIAElement 加 5 方法（getChildren/expand/collapse/isExpanded/doubleClick）**
```cpp
// 在 IUIAElement public 内（getInfo 之后）加：
virtual std::vector<std::shared_ptr<IUIAElement>> getChildren() = 0;
virtual bool expand() = 0;
virtual bool collapse() = 0;
virtual bool isExpanded() const = 0;
virtual bool doubleClick() = 0;
```

- [ ] **Step 2: IUIAManager 加 3 方法 + 事件接口**
```cpp
// 在 IUIAManager public 内（findElement 之后）加：
virtual std::shared_ptr<IUIAElement> getElementFromWindow(uint64_t hwnd) = 0;
virtual std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role) = 0;
virtual std::shared_ptr<IUIAElement> waitForElement(const UIASelector& selector, int timeoutMs) = 0;

// 事件监听（返回 listenerId，0=失败）
using UIAEventCallback = std::function<void(std::shared_ptr<IUIAElement>)>;
virtual uint64_t addPropertyChangedListener(const UIASelector& selector, UIAEventCallback cb) = 0;
virtual uint64_t addStructureChangedListener(const UIASelector& selector, UIAEventCallback cb) = 0;
virtual bool removeEventListener(uint64_t listenerId) = 0;
```

- [ ] **Step 3: UIAutomation 门面加对应方法（委托 manager）**
```cpp
// UIAutomation 类加（委托 impl->manager）：
std::shared_ptr<IUIAElement> fromWindow(uint64_t hwnd);
std::vector<std::shared_ptr<IUIAElement>> findAllByRole(UIARole role);
std::shared_ptr<IUIAElement> waitForName(const std::string& name, int timeoutMs);
uint64_t addPropertyChangedListener(const UIASelector& s, UIAEventCallback cb);
uint64_t addStructureChangedListener(const UIASelector& s, UIAEventCallback cb);
bool removeEventListener(uint64_t id);
```

> ⚠️ 接口改动会让 win_automation.cpp / mac_automation.cpp 的具体实现类编译失败（缺新方法）—— Task 2/3 立即补实现。本 task 只改接口 + 门面声明 + ui_automation.cpp 委托实现。

- [ ] **Step 4: ui_automation.cpp 委托实现**
- [ ] **Step 5: Commit**（接口扩展，此时构建会因 Win/Mac 实现未补而失败 —— 预期，Task 2/3 修复）

> 实际操作：Task 1-3 应作为一个连续单元（接口→Win实现→Mac实现→构建通过）。建议 Task 1+2+3 合并执行（接口 + 至少 Mac 实现让 Mac 构建通过），Windows 实现可后续（Win 构建在 Windows CI）。

### Task 2: Windows 实现（win_automation.cpp）

**Files:** Modify `lib/wingman/src/platform/win/win_automation.cpp`

- [ ] **getChildren**：`IUIAutomationTreeWalker::GetFirstChildElement` + `GetNextSiblingElement` 遍历，返回 vector
- [ ] **expand/collapse/isExpanded**：`GetCurrentPatternAs(IID_IUIAutomationExpandCollapsePattern)` → `Expand()`/`Collapse()`/`get_CurrentExpandCollapseState`；不支持 pattern 返回 false
- [ ] **doubleClick**：`click()` 两次（简单版）或 `SendInput` 双击
- [ ] **getElementFromWindow**：`automation_->ElementFromHandle((UIA_HWND)hwnd, &element)`
- [ ] **findAllByRole**：`CreatePropertyCondition(UIA_ControlTypeIdPropertyId, controlType)` + `root->FindAll(TreeScope_Descendants, cond)`
- [ ] **waitForElement**：轮询 `findElement` + `std::this_thread::sleep_for(100ms)` 直到 timeout
- [ ] **事件监听**（Phase 2 详细）：实现 `IUIAutomationPropertyChangedEventHandler`/`StructureChangedEventHandler` + 事件线程 + 回调映射

> Windows 实现无法在 Mac 测，依赖 Windows CI。Mac 构建不编译 win_automation.cpp（#ifdef _WIN32）。

### Task 3: Mac 实现（mac_automation.cpp）

**Files:** Modify `lib/wingman/src/platform/mac/mac_automation.cpp`

- [ ] **getChildren**：`AXUIElementCopyAttributeValue(kAXChildrenAttribute)` → 数组 → 转 vector<IUIAElement>
- [ ] **expand/collapse/isExpanded**：AX 无统一 expand action，用 `kAXExpandedAttribute` 读；expand/collapse 可能需 `kAXPressAction` 或属性设置（按控件类型）
- [ ] **doubleClick**：`click()` 两次
- [ ] **getElementFromWindow**：Mac 无 hwnd 概念；用 PID + AXUIElementCreateApplication(pid)（hwnd 参数作 pid 解释，文档注明）
- [ ] **findAllByRole**：递归遍历 children + role 过滤（复用 findElement 的递归）
- [ ] **waitForElement**：轮询 + sleep
- [ ] **事件监听**（Phase 2 详细）：`AXObserverCreate` + `AXObserverAddNotification` + run loop 线程

> Mac 实现可在 Mac 测（烟雾）。这是 Phase 1 在 Mac 可验证的部分。

### Task 4: 后端接口 Mac 烟雾测试

**Files:** Modify `lib/wingman/tests/ui_automation_test.cpp`

- [ ] 加测试验证新接口可调用（getChildren/expand/doubleClick 等在 Mac 上对 AX 元素或空防御不崩溃）
- [ ] waitForElement timeout 不崩溃
- [ ] findAllByRole 返回 vector（可能空）

---

## Phase 3 详细 Task（脚本层 OO）

### Task 5: UIAElementRegistry + UIElement OO 对象构造

**Files:** Modify `lib/wingman/src/script/modules/misc_modules.cpp`（uia 段）

- [ ] **Step 1: UIAElementRegistry helpers**（仿 Plan 7 NodeRegistry）
```cpp
static std::map<int, std::shared_ptr<IUIAElement>>& uiaElementRegistry() {
	static std::map<int, std::shared_ptr<IUIAElement>> registry;
	return registry;
}
static int uiaStoreElement(std::shared_ptr<IUIAElement> e) {
	static int nextId = 0; int id = ++nextId;
	uiaElementRegistry()[id] = std::move(e); return id;
}
static std::shared_ptr<IUIAElement> uiaGetElement(int h) {
	auto& r = uiaElementRegistry(); auto it = r.find(h);
	return it != r.end() ? it->second : nullptr;
}
```

- [ ] **Step 2: makeUiaElementObject(shared_ptr<IUIAElement>) -> ScriptValue**
构造 `ScriptValue::Object`：`{"_handle": int, "get_info": Callable, "click": Callable, "double_click": Callable, "focus": Callable, "get_value": Callable, "set_value": Callable, "get_children": Callable, "expand": Callable, "collapse": Callable, "is_expanded": Callable, "is_visible": Callable, "is_enabled": Callable}`。每个方法 Callable 捕获 handle，调用 `uiaGetElement(handle)->对应方法()`，转 ScriptValue。无效 handle 返回 null/false。

- [ ] **Step 3: 测试**（UiaModuleFunctionsTest）：构造 UIElement 对象，验证 isObject + _handle + 各方法 isCallable

### Task 6: 13 模块函数

**Files:** Modify `lib/wingman/src/script/modules/misc_modules.cpp`

- [ ] from_foreground/from_point/from_window → uia().xxx → makeUiaElementObject 或 null
- [ ] find_by_name/find_by_id/find_button/find_edit/find_text → UIASelector → find → makeUiaElementObject
- [ ] find_all_by_control_type → findAllByRole → Array of UIElement Object
- [ ] wait_for_name → waitForName → makeUiaElementObject 或 null
- [ ] on_property_changed/on_structure_changed → 拷贝 callableVal → add*Listener → listenerId
- [ ] remove_event_listener → removeEventListener
- [ ] 替换现有 findByName/findById/find（裸指针）为 OO 版（返回 UIElement Object）

- [ ] **测试**：每个函数烟雾（注册 + 返回 Object/null + 句柄）

---

## Phase 2 概要 Task（事件监听，难度高）

### Task 7: Windows 事件监听
- 实现 IUIAutomationPropertyChangedEventHandler / StructureChangedEventHandler COM 接口
- 事件处理线程（UIA 事件需消息循环；创建专用线程 + 隐藏窗口消息泵，或利用 STA）
- listenerId 映射 + removeEventListener（Remove*EventHandler）
- callable 回调（拷贝 callableVal，跨线程调用需 callableThreadSafe 检查）

### Task 8: Mac 事件监听
- AXObserverCreate(pid, callback) + AXObserverAddNotification(kAX*ChangedNotifications)
- 后台 run loop 线程调度 observer
- listenerId 映射 + removeEventListener（AXObserverRemoveNotification + release）
- callable 回调

### Task 9: 脚本事件函数测试
- on_property_changed/on_structure_changed 注册返回 listenerId > 0
- remove_event_listener 返回 bool
- 真实事件触发依赖真实 UI（CI/手动）

---

## Phase 4 概要 Task

### Task 10: 文档对齐
- index.md：标注已实现（Phase 1+3 函数）vs 规划中（事件若未完成）
- types.md：UIElement 方法表核对
- 标注 Windows/Mac 支持，Linux 不支持

### Task 11: 全量回归
- Mac 烟雾测试全量（UiaModuleFunctionsTest + ui_automation_test）
- 确认无回归（bt/human 等不受影响）

---

## 完成判据

- [ ] IUIAElement 加 getChildren/expand/collapse/isExpanded/doubleClick，Windows + Mac 实现
- [ ] IUIAManager 加 fromWindow/findAllByControlType/waitForName + 事件监听，双平台实现
- [ ] 脚本层 UIElement OO 对象（12 方法）+ 13 模块函数，替换裸指针 handle 为 registry
- [ ] 文档 index.md/types.md 与实现一致
- [ ] Mac 烟雾测试全绿（函数注册 + OO 结构 + 句柄 + 防御）
- [ ] Windows 真实 UIA 行为依赖 Windows CI 验证

---

## 执行约束（来自 handoff §4 / CLAUDE.md）

- 第三方依赖统一 vcpkg（Windows UIA 是系统 COM，无需 vcpkg；Mac AX 是系统框架）
- Windows `WindowHandle` 是 HWND 强类型（fromWindow 参数用 uint64_t/handle，不与 int 比较）
- 注释中文
- 不主动 git 提交/分支（TDD 提交属已批准流程）
- TDD：先失败测试 → 实现 → 通过 → 提交
- **跨会话**：本 plan 工程巨大（11 task），建议 Phase 1+3 一个会话、Phase 2（事件）独立会话。每 task 强 SCOPE 纪律（Plan 7 教训：implementer 易顺手改无关代码，需 prompt 强约束 + 自检 + controller 核实 diff）
