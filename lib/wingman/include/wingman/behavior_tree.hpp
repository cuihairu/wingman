#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>

namespace wingman {

// Node status
enum class NodeStatus {
    SUCCESS,    // Success
    FAILURE,    // Failure
    RUNNING     // Running
};

// Behavior tree node base class
class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus tick() = 0;
    virtual std::string getName() const = 0;
};

// Behavior tree context
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

// ========== Composite nodes ==========

// Sequence: execute all children in order, returns SUCCESS only if all succeed
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

// Selector: execute children in order, returns SUCCESS if any succeeds
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

// Parallel: execute all children in parallel
class ParallelNode : public BehaviorNode {
public:
    enum class Policy {
        SUCCEED_ON_ALL,      // All succeed
        SUCCEED_ON_ONE,      // Any succeeds
        FAIL_ON_ALL,         // All fail
        FAIL_ON_ONE          // Any fails
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

// Repeat: repeat child execution
class RepeatNode : public BehaviorNode {
public:
    RepeatNode(std::shared_ptr<BehaviorNode> child, int count = -1);  // -1 = infinite
    NodeStatus tick() override;
    std::string getName() const override { return "Repeat"; }

private:
    std::shared_ptr<BehaviorNode> child_;
    int count_;
    int current_ = 0;
};

// Retry: retry on failure
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

// Inverter: invert child result
class InverterNode : public BehaviorNode {
public:
    InverterNode(std::shared_ptr<BehaviorNode> child);
    NodeStatus tick() override;
    std::string getName() const override { return "Inverter"; }

private:
    std::shared_ptr<BehaviorNode> child_;
};

// ========== Condition nodes ==========

// Condition: conditional check
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

// Check: check variable value
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

// ========== Action nodes ==========

// Action: execute action
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

// Wait: wait for specified duration
class WaitNode : public BehaviorNode {
public:
    WaitNode(int milliseconds);
    NodeStatus tick() override;
    std::string getName() const override { return "Wait"; }

private:
    int milliseconds_;
    std::chrono::steady_clock::time_point startTime_;
};

// Delay: delayed execution
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

// ========== Behavior tree ==========

class BehaviorTree {
public:
    BehaviorTree(const std::string& name);
    BehaviorTree(const std::string& name, std::shared_ptr<BehaviorNode> root);

    void setRoot(std::shared_ptr<BehaviorNode> root);
    NodeStatus tick();
    void reset();

    const std::string& getName() const { return name_; }
    BehaviorContext& getContext() { return context_; }

    // Factory methods for common nodes
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

// ========== Behavior tree manager ==========

class BehaviorTreeManager {
public:
    static BehaviorTreeManager& instance();

    std::shared_ptr<BehaviorTree> createTree(const std::string& name);
    std::shared_ptr<BehaviorTree> getTree(const std::string& name);
    void removeTree(const std::string& name);
    std::vector<std::shared_ptr<BehaviorTree>> getAllTrees() const;

    // Start/stop all trees
    void startAll();
    void stopAll();

private:
    std::map<std::string, std::shared_ptr<BehaviorTree>> trees_;
    mutable std::mutex mutex_;
};

} // namespace wingman
