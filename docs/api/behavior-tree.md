# BehaviorTree API

行为树引擎提供灵活的 AI 决策系统。

## Lua API

### bt.create(name)

创建一个新的行为树。

```lua
bt.create("my_tree")
```

---

### bt.sequence(name?)

创建序列节点（所有子节点必须全部成功）。

```lua
local seq = bt.sequence("attack_sequence")
```

---

### bt.selector(name?)

创建选择节点（任一子节点成功即可）。

```lua
local sel = bt.selector("task_selector")
```

---

### bt.condition(name, fn)

创建条件节点。

```lua
local cond = bt.condition("has_enemy", function()
    local enemy = vision.findImage("enemy.png")
    return enemy ~= nil
end)
```

---

### bt.action(name, fn)

创建动作节点。

```lua
local act = bt.action("attack", function()
    input.click(100, 200)
    return "SUCCESS"
end)
```

---

### bt.tick(treeName)

执行行为树一次。

```lua
local status = bt.tick("my_tree")
print("状态:", status)  -- SUCCESS/FAILURE/RUNNING
```

---

### bt.remove(treeName)

移除行为树。

```lua
bt.remove("my_tree")
```

## 节点状态

每个节点执行后返回以下状态之一：

| 状态 | 说明 |
|------|------|
| `SUCCESS` | 成功 |
| `FAILURE` | 失败 |
| `RUNNING` | 运行中 |

## 节点类型

### Sequence（序列）

按顺序执行所有子节点，全部成功才返回成功。

```
┌─ Sequence ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Selector（选择）

按顺序执行子节点，任一成功即返回成功。

```
┌─ Selector ─┐
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
└────────────┘
```

### Parallel（并行）

同时执行所有子节点。

```
┌─ Parallel ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Inverter（反转）

反转子节点的结果（成功变失败，失败变成功）。

```
┌─ Inverter ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Repeat（重复）

重复执行子节点 N 次。

```
┌─ Repeat(N) ─┐
│  ┌─────┐    │
│  │ Node│    │
│  └─────┘    │
└─────────────┘
```

### Wait（等待）

等待指定毫秒数。

```
┌─ Wait(ms) ─┐
└────────────┘
```

## C++ API

### 创建行为树

```cpp
#include "wingman/behavior_tree.hpp"

auto tree = BehaviorTreeManager::instance().createTree("combat");

// 创建序列节点
auto sequence = BehaviorTree::sequence("attack");
sequence->addChild(BehaviorTree::action("find_target", []() {
    // 查找目标逻辑
    return NodeStatus::SUCCESS;
}));
sequence->addChild(BehaviorTree::action("attack_target", []() {
    // 攻击逻辑
    return NodeStatus::SUCCESS;
}));

tree->setRoot(sequence);

// 执行
NodeStatus status = tree->tick();
```

### 复杂行为树示例

```cpp
// 构建复杂行为树
auto root = BehaviorTree::sequence("main");

// 并行执行移动和攻击
auto parallel = BehaviorTree::parallel("combat", ParallelNode::Policy::SUCCEED_ON_ALL);
parallel->addChild(BehaviorTree::action("move", []() {
    Input::move(100, 100);
    return NodeStatus::SUCCESS;
}));
parallel->addChild(BehaviorTree::action("attack", []() {
    Input::click(100, 100);
    return NodeStatus::SUCCESS;
}));

root->addChild(parallel);

// 检查是否完成
auto checkDone = BehaviorTree::condition("is_done", []() {
    return !hasEnemy();
});

root->addChild(checkDone);

tree->setRoot(root);
```
