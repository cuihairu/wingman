#include "wingman/behavior_tree.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace wingman {

// ========== BehaviorContext 实现 ==========

template<>
std::string BehaviorContext::get<std::string>(const std::string& key, const std::string& defaultValue) const {
    auto it = variables.find(key);
    return it != variables.end() ? it->second : defaultValue;
}

template<>
double BehaviorContext::get<double>(const std::string& key, const double& defaultValue) const {
    auto it = numbers.find(key);
    return it != numbers.end() ? it->second : defaultValue;
}

template<>
bool BehaviorContext::get<bool>(const std::string& key, const bool& defaultValue) const {
    auto it = flags.find(key);
    return it != flags.end() ? it->second : defaultValue;
}

// ========== SequenceNode 实现 ==========

SequenceNode::SequenceNode(const std::string& name) : name_(name) {}

void SequenceNode::addChild(std::shared_ptr<BehaviorNode> child) {
    children_.push_back(child);
}

NodeStatus SequenceNode::tick() {
    for (size_t i = currentChild_; i < children_.size(); i++) {
        NodeStatus status = children_[i]->tick();

        if (status == NodeStatus::FAILURE) {
            currentChild_ = 0;  // 重置
            return NodeStatus::FAILURE;
        }

        if (status == NodeStatus::RUNNING) {
            currentChild_ = i;  // 记住当前位置
            return NodeStatus::RUNNING;
        }
    }

    currentChild_ = 0;  // 完成，重置
    return NodeStatus::SUCCESS;
}

// ========== SelectorNode 实现 ==========

SelectorNode::SelectorNode(const std::string& name) : name_(name) {}

void SelectorNode::addChild(std::shared_ptr<BehaviorNode> child) {
    children_.push_back(child);
}

NodeStatus SelectorNode::tick() {
    for (size_t i = currentChild_; i < children_.size(); i++) {
        NodeStatus status = children_[i]->tick();

        if (status == NodeStatus::SUCCESS) {
            currentChild_ = 0;  // 重置
            return NodeStatus::SUCCESS;
        }

        if (status == NodeStatus::RUNNING) {
            currentChild_ = i;
            return NodeStatus::RUNNING;
        }
    }

    currentChild_ = 0;
    return NodeStatus::FAILURE;
}

// ========== ParallelNode 实现 ==========

ParallelNode::ParallelNode(const std::string& name, Policy successPolicy)
    : name_(name), successPolicy_(successPolicy) {}

void ParallelNode::addChild(std::shared_ptr<BehaviorNode> child) {
    children_.push_back(child);
}

NodeStatus ParallelNode::tick() {
    int successCount = 0;
    int failureCount = 0;
    bool hasRunning = false;

    for (const auto& child : children_) {
        NodeStatus status = child->tick();

        if (status == NodeStatus::SUCCESS) successCount++;
        else if (status == NodeStatus::FAILURE) failureCount++;
        else hasRunning = true;
    }

    // 根据策略决定结果
    switch (successPolicy_) {
        case Policy::SUCCEED_ON_ALL:
            return (successCount == (int)children_.size()) ? NodeStatus::SUCCESS :
                   (hasRunning ? NodeStatus::RUNNING : NodeStatus::FAILURE);

        case Policy::SUCCEED_ON_ONE:
            return (successCount > 0) ? NodeStatus::SUCCESS :
                   (hasRunning ? NodeStatus::RUNNING : NodeStatus::FAILURE);

        case Policy::FAIL_ON_ALL:
            return (failureCount == (int)children_.size()) ? NodeStatus::FAILURE :
                   (hasRunning ? NodeStatus::RUNNING : NodeStatus::SUCCESS);

        case Policy::FAIL_ON_ONE:
            return (failureCount > 0) ? NodeStatus::FAILURE :
                   (hasRunning ? NodeStatus::RUNNING : NodeStatus::SUCCESS);
    }

    return NodeStatus::RUNNING;
}

// ========== RepeatNode 实现 ==========

RepeatNode::RepeatNode(std::shared_ptr<BehaviorNode> child, int count)
    : child_(child), count_(count) {}

NodeStatus RepeatNode::tick() {
    if (count_ > 0 && current_ >= count_) {
        current_ = 0;
        return NodeStatus::SUCCESS;
    }

    NodeStatus status = child_->tick();

    if (status == NodeStatus::SUCCESS || status == NodeStatus::FAILURE) {
        current_++;
        if (count_ > 0 && current_ >= count_) {
            current_ = 0;
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::RUNNING;
    }

    return NodeStatus::RUNNING;
}

// ========== RetryNode 实现 ==========

RetryNode::RetryNode(std::shared_ptr<BehaviorNode> child, int maxRetries)
    : child_(child), maxRetries_(maxRetries) {}

NodeStatus RetryNode::tick() {
    while (currentRetries_ <= maxRetries_) {
        NodeStatus status = child_->tick();

        if (status == NodeStatus::SUCCESS) {
            currentRetries_ = 0;
            return NodeStatus::SUCCESS;
        }

        if (status == NodeStatus::RUNNING) {
            return NodeStatus::RUNNING;
        }

        currentRetries_++;
    }

    currentRetries_ = 0;
    return NodeStatus::FAILURE;
}

// ========== InverterNode 实现 ==========

InverterNode::InverterNode(std::shared_ptr<BehaviorNode> child) : child_(child) {}

NodeStatus InverterNode::tick() {
    NodeStatus status = child_->tick();

    if (status == NodeStatus::SUCCESS) return NodeStatus::FAILURE;
    if (status == NodeStatus::FAILURE) return NodeStatus::SUCCESS;
    return NodeStatus::RUNNING;
}

// ========== ConditionNode 实现 ==========

ConditionNode::ConditionNode(const std::string& name, ConditionFunc condition)
    : name_(name), condition_(condition) {}

NodeStatus ConditionNode::tick() {
    static BehaviorContext dummyContext;
    return condition_(dummyContext) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
}

// ========== CheckNode 实现 ==========

CheckNode::CheckNode(const std::string& variable, double value, Op op)
    : variable_(variable), value_(value), op_(op) {}

NodeStatus CheckNode::tick() {
    static BehaviorContext ctx;
    double actual = ctx.get<double>(variable_, 0.0);

    bool result = false;
    switch (op_) {
        case Op::EQUAL: result = (actual == value_); break;
        case Op::NOT_EQUAL: result = (actual != value_); break;
        case Op::GREATER: result = (actual > value_); break;
        case Op::LESS: result = (actual < value_); break;
        case Op::GREATER_EQUAL: result = (actual >= value_); break;
        case Op::LESS_EQUAL: result = (actual <= value_); break;
    }

    return result ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
}

// ========== ActionNode 实现 ==========

ActionNode::ActionNode(const std::string& name, ActionFunc action)
    : name_(name), action_(action) {}

NodeStatus ActionNode::tick() {
    static BehaviorContext dummyContext;
    return action_(dummyContext);
}

// ========== WaitNode 实现 ==========

WaitNode::WaitNode(int milliseconds) : milliseconds_(milliseconds) {}

NodeStatus WaitNode::tick() {
    auto now = std::chrono::steady_clock::now();

    if (startTime_.time_since_epoch().count() == 0) {
        startTime_ = now;
        return NodeStatus::RUNNING;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();

    if (elapsed >= milliseconds_) {
        startTime_ = std::chrono::steady_clock::time_point();
        return NodeStatus::SUCCESS;
    }

    return NodeStatus::RUNNING;
}

// ========== DelayNode 实现 ==========

DelayNode::DelayNode(std::shared_ptr<BehaviorNode> child, int milliseconds)
    : child_(child), milliseconds_(milliseconds) {}

NodeStatus DelayNode::tick() {
    auto now = std::chrono::steady_clock::now();

    if (startTime_.time_since_epoch().count() == 0) {
        startTime_ = now;
        return NodeStatus::RUNNING;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();

    if (elapsed >= milliseconds_) {
        startTime_ = std::chrono::steady_clock::time_point();
        return child_->tick();
    }

    return NodeStatus::RUNNING;
}

// ========== BehaviorTree 实现 ==========

BehaviorTree::BehaviorTree(const std::string& name)
    : name_(name), root_(nullptr) {}

BehaviorTree::BehaviorTree(const std::string& name, std::shared_ptr<BehaviorNode> root)
    : name_(name), root_(root) {}

void BehaviorTree::setRoot(std::shared_ptr<BehaviorNode> root) {
    root_ = root;
}

NodeStatus BehaviorTree::tick() {
    if (!root_) {
        spdlog::warn("Tree '{}' has no root node", name_);
        return NodeStatus::FAILURE;
    }

    return root_->tick();
}

void BehaviorTree::reset() {
    // TODO: 重置所有节点状态
}

std::shared_ptr<SequenceNode> BehaviorTree::sequence(const std::string& name) {
    return std::make_shared<SequenceNode>(name.empty() ? "Sequence" : name);
}

std::shared_ptr<SelectorNode> BehaviorTree::selector(const std::string& name) {
    return std::make_shared<SelectorNode>(name.empty() ? "Selector" : name);
}

std::shared_ptr<ParallelNode> BehaviorTree::parallel(const std::string& name, ParallelNode::Policy policy) {
    return std::make_shared<ParallelNode>(name.empty() ? "Parallel" : name, policy);
}

std::shared_ptr<ConditionNode> BehaviorTree::condition(const std::string& name, std::function<bool()> fn) {
    return std::make_shared<ConditionNode>(name, [fn](BehaviorContext&) { return fn(); });
}

std::shared_ptr<ActionNode> BehaviorTree::action(const std::string& name, std::function<NodeStatus()> fn) {
    return std::make_shared<ActionNode>(name, [fn](BehaviorContext&) { return fn(); });
}

// ========== BehaviorTreeManager 实现 ==========

BehaviorTreeManager& BehaviorTreeManager::instance() {
    static BehaviorTreeManager instance;
    return instance;
}

std::shared_ptr<BehaviorTree> BehaviorTreeManager::createTree(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (trees_.find(name) != trees_.end()) {
        spdlog::warn("Tree '{}' already exists", name);
        return trees_[name];
    }

    auto tree = std::make_shared<BehaviorTree>(name);
    trees_[name] = tree;

    spdlog::info("Created behavior tree '{}'", name);
    return tree;
}

std::shared_ptr<BehaviorTree> BehaviorTreeManager::getTree(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = trees_.find(name);
    if (it != trees_.end()) {
        return it->second;
    }

    return nullptr;
}

void BehaviorTreeManager::removeTree(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = trees_.find(name);
    if (it != trees_.end()) {
        trees_.erase(it);
        spdlog::info("Removed behavior tree '{}'", name);
    }
}

std::vector<std::shared_ptr<BehaviorTree>> BehaviorTreeManager::getAllTrees() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<BehaviorTree>> result;
    for (const auto& [name, tree] : trees_) {
        result.push_back(tree);
    }

    return result;
}

void BehaviorTreeManager::startAll() {
    spdlog::info("Starting {} behavior trees", trees_.size());
}

void BehaviorTreeManager::stopAll() {
    spdlog::info("Stopping {} behavior trees", trees_.size());
}

} // namespace wingman
