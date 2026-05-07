#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>

namespace wingman {

// 节点状态
enum class NodeStatus {
    SUCCESS,    // 成功
    FAILURE,    // 失败
    RUNNING     // 运行中
};

// 行为树节点基类
class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus tick() = 0;
    virtual std::string getName() const = 0;
};

// 行为树上下文
class BehaviorContext {
public:
    std::map<std::string, std::string> variables;
    std::map<std::string, double> numbers;
    std::map<std::string, bool> flags;

    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;

    void set(const std::string& key, const std::string& value) { variables[key] = value; }
    void set(const std::string& key, double value) { numbers[key] = value; }
    void set(const std::string& key, bool value) { flags[key] = value; }
};

// ========== 复合节点 ==========

// Sequence: 顺序执行所有子节点，全部成功才返回 SUCCESS
class SequenceNode : public BehaviorNode {
public:
    SequenceNode(const std::string& name = "Sequence");
    void addChild(std::shared_ptr<BehaviorNode> child);
    NodeStatus tick() override;
    std::string getName() const override { return name_; }

private:
    std::string name_;
    std::vector<std::shared_ptr<BehaviorNode>> children_;
    size_t currentChild_ = 0;
};

// Selector: 选择执行子节点，任一成功就返回 SUCCESS
class SelectorNode : public BehaviorNode {
public:
    SelectorNode(const std::string& name = "Selector");
    void addChild(std::shared_ptr<BehaviorNode> child);
    NodeStatus tick() override;
    std::string getName() const override { return name_; }

private:
    std::string name_;
    std::vector<std::shared_ptr<BehaviorNode>> children_;
    size_t currentChild_ = 0;
};

// Parallel: 并行执行所有子节点
class ParallelNode : public BehaviorNode {
public:
    enum class Policy {
        SUCCEED_ON_ALL,      // 全部成功
        SUCCEED_ON_ONE,      // 任一成功
        FAIL_ON_ALL,         // 全部失败
        FAIL_ON_ONE          // 任一失败
    };

    ParallelNode(const std::string& name = "Parallel", Policy successPolicy = Policy::SUCCEED_ON_ALL);
    void addChild(std::shared_ptr<BehaviorNode> child);
    NodeStatus tick() override;
    std::string getName() const override { return name_; }

private:
    std::string name_;
    std::vector<std::shared_ptr<BehaviorNode>> children_;
    Policy successPolicy_;
};

// Repeat: 重复执行子节点
class RepeatNode : public BehaviorNode {
public:
    RepeatNode(std::shared_ptr<BehaviorNode> child, int count = -1);  // -1 = 无限
    NodeStatus tick() override;
    std::string getName() const override { return "Repeat"; }

private:
    std::shared_ptr<BehaviorNode> child_;
    int count_;
    int current_ = 0;
};

// Retry: 失败时重试
class RetryNode : public BehaviorNode {
public:
    RetryNode(std::shared_ptr<BehaviorNode> child, int maxRetries = 3);
    NodeStatus tick() override;
    std::string getName() const override { return "Retry"; }

private:
    std::shared_ptr<BehaviorNode> child_;
    int maxRetries_;
    int currentRetries_ = 0;
};

// Inverter: 反转子节点结果
class InverterNode : public BehaviorNode {
public:
    InverterNode(std::shared_ptr<BehaviorNode> child);
    NodeStatus tick() override;
    std::string getName() const override { return "Inverter"; }

private:
    std::shared_ptr<BehaviorNode> child_;
};

// ========== 条件节点 ==========

// Condition: 条件判断
class ConditionNode : public BehaviorNode {
public:
    using ConditionFunc = std::function<bool(BehaviorContext&)>;

    ConditionNode(const std::string& name, ConditionFunc condition);
    NodeStatus tick() override;
    std::string getName() const override { return name_; }

private:
    std::string name_;
    ConditionFunc condition_;
};

// Check: 检查变量值
class CheckNode : public BehaviorNode {
public:
    enum class Op { EQUAL, NOT_EQUAL, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL };

    CheckNode(const std::string& variable, double value, Op op = Op::EQUAL);
    NodeStatus tick() override;
    std::string getName() const override { return "Check"; }

private:
    std::string variable_;
    double value_;
    Op op_;
};

// ========== 动作节点 ==========

// Action: 执行动作
class ActionNode : public BehaviorNode {
public:
    using ActionFunc = std::function<NodeStatus(BehaviorContext&)>;

    ActionNode(const std::string& name, ActionFunc action);
    NodeStatus tick() override;
    std::string getName() const override { return name_; }

private:
    std::string name_;
    ActionFunc action_;
};

// Wait: 等待指定时间
class WaitNode : public BehaviorNode {
public:
    WaitNode(int milliseconds);
    NodeStatus tick() override;
    std::string getName() const override { return "Wait"; }

private:
    int milliseconds_;
    std::chrono::steady_clock::time_point startTime_;
};

// Delay: 延迟执行
class DelayNode : public BehaviorNode {
public:
    DelayNode(std::shared_ptr<BehaviorNode> child, int milliseconds);
    NodeStatus tick() override;
    std::string getName() const override { return "Delay"; }

private:
    std::shared_ptr<BehaviorNode> child_;
    int milliseconds_;
    std::chrono::steady_clock::time_point startTime_;
};

// ========== 行为树 ==========

class BehaviorTree {
public:
    BehaviorTree(const std::string& name);
    BehaviorTree(const std::string& name, std::shared_ptr<BehaviorNode> root);

    void setRoot(std::shared_ptr<BehaviorNode> root);
    NodeStatus tick();
    void reset();

    const std::string& getName() const { return name_; }
    BehaviorContext& getContext() { return context_; }

    // 创建常用节点的工厂方法
    static std::shared_ptr<SequenceNode> sequence(const std::string& name = "");
    static std::shared_ptr<SelectorNode> selector(const std::string& name = "");
    static std::shared_ptr<ParallelNode> parallel(const std::string& name = "", ParallelNode::Policy policy = ParallelNode::Policy::SUCCEED_ON_ALL);
    static std::shared_ptr<ConditionNode> condition(const std::string& name, std::function<bool()> fn);
    static std::shared_ptr<ActionNode> action(const std::string& name, std::function<NodeStatus()> fn);

private:
    std::string name_;
    std::shared_ptr<BehaviorNode> root_;
    BehaviorContext context_;
};

// ========== 行为树管理器 ==========

class BehaviorTreeManager {
public:
    static BehaviorTreeManager& instance();

    std::shared_ptr<BehaviorTree> createTree(const std::string& name);
    std::shared_ptr<BehaviorTree> getTree(const std::string& name);
    void removeTree(const std::string& name);
    std::vector<std::shared_ptr<BehaviorTree>> getAllTrees() const;

    // 启动/停止所有树
    void startAll();
    void stopAll();

private:
    std::map<std::string, std::shared_ptr<BehaviorTree>> trees_;
    mutable std::mutex mutex_;
};

} // namespace wingman
