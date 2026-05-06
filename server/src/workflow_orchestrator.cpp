#include "wingman/server/workflow_orchestrator.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>
#include <random>

namespace wingman::server {

// ========== 工作流管理 ==========

std::string WorkflowOrchestrator::submitWorkflow(const Workflow& workflow) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 创建工作流副本
    Workflow wf = workflow;
    if (wf.id.empty()) {
        wf.id = generateId("workflow");
    }
    if (wf.createdTime == 0) {
        wf.createdTime = Protocol::now();
    }
    wf.status = WorkflowStatus::Pending;

    // 创建执行实例
    auto instance = std::make_shared<WorkflowInstance>();
    instance->workflow = wf;

    // 初始化步骤状态
    for (const auto& step : wf.steps) {
        instance->stepStatus[step.id] = StepStatus::Pending;
        // 记录步骤分配的 workers
        for (const auto& workerId : step.workers) {
            instance->stepWorkers[step.id].insert(workerId);
        }
    }

    workflows_[wf.id] = instance;

    spdlog::info("Workflow submitted: {} with {} steps", wf.id, wf.steps.size());
    return wf.id;
}

bool WorkflowOrchestrator::cancelWorkflow(const std::string& workflowId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return false;
    }

    auto& instance = it->second;
    if (instance->workflow.status == WorkflowStatus::Completed ||
        instance->workflow.status == WorkflowStatus::Cancelled) {
        return false;
    }

    instance->workflow.status = WorkflowStatus::Cancelled;
    instance->workflow.endTime = Protocol::now();

    // 取消所有运行中的任务
    for (auto& [stepId, status] : instance->stepStatus) {
        if (status == StepStatus::Running) {
            status = StepStatus::Failed;
        }
    }

    spdlog::info("Workflow cancelled: {}", workflowId);
    return true;
}

std::optional<Workflow> WorkflowOrchestrator::getWorkflow(const std::string& workflowId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it != workflows_.end()) {
        return it->second->workflow;
    }
    return std::nullopt;
}

std::vector<Workflow> WorkflowOrchestrator::getAllWorkflows() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Workflow> result;
    result.reserve(workflows_.size());

    for (const auto& [id, instance] : workflows_) {
        result.push_back(instance->workflow);
    }

    return result;
}

// ========== 任务执行 ==========

std::optional<TaskStep> WorkflowOrchestrator::getNextTask(const std::string& workerId,
                                                           const std::string& workflowId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        spdlog::warn("Workflow not found: {}", workflowId);
        return std::nullopt;
    }

    auto& instance = it->second;

    // 检查工作流状态
    if (instance->workflow.status == WorkflowStatus::Cancelled) {
        return std::nullopt;
    }

    // 启动工作流
    if (instance->workflow.status == WorkflowStatus::Pending) {
        instance->workflow.status = WorkflowStatus::Running;
        instance->workflow.startTime = Protocol::now();
    }

    // 查找下一个可执行的任务
    auto nextStep = findNextStep(*instance, workerId);
    if (!nextStep) {
        // 检查是否所有步骤都完成了
        bool allCompleted = true;
        for (const auto& [stepId, status] : instance->stepStatus) {
            if (status != StepStatus::Completed && status != StepStatus::Skipped) {
                allCompleted = false;
                break;
            }
        }

        if (allCompleted) {
            instance->workflow.status = WorkflowStatus::Completed;
            instance->workflow.endTime = Protocol::now();
            spdlog::info("Workflow completed: {}", workflowId);
        }
    }

    return nextStep;
}

bool WorkflowOrchestrator::reportProgress(const std::string& workerId,
                                          const std::string& workflowId,
                                          const std::string& stepId,
                                          const nlohmann::json& progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return false;
    }

    auto& instance = it->second;

    // 更新 Worker 状态
    auto wit = instance->workerStatus.find(workerId);
    if (wit == instance->workerStatus.end()) {
        WorkerStatus status;
        status.workerId = workerId;
        status.stepId = stepId;
        status.status = StepStatus::Running;
        status.startTime = Protocol::now();
        instance->workerStatus[workerId] = status;
    } else {
        wit->second.progress = progress;
    }

    return true;
}

bool WorkflowOrchestrator::completeTask(const std::string& workerId,
                                        const std::string& workflowId,
                                        const std::string& stepId,
                                        bool success,
                                        const nlohmann::json& result) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return false;
    }

    auto& instance = it->second;

    // 更新 Worker 状态
    auto wit = instance->workerStatus.find(workerId);
    if (wit != instance->workerStatus.end()) {
        wit->second.status = success ? StepStatus::Completed : StepStatus::Failed;
        wit->second.endTime = Protocol::now();
        wit->second.progress = result;
    }

    // 检查步骤是否所有 worker 都完成了
    if (instance->areAllWorkersCompleted(stepId)) {
        instance->stepStatus[stepId] = StepStatus::Completed;
        spdlog::info("Step completed: {} in workflow {}", stepId, workflowId);

        // 更新工作流状态
        updateWorkflowStatus(*instance);
    }

    return true;
}

bool WorkflowOrchestrator::failTask(const std::string& workerId,
                                    const std::string& workflowId,
                                    const std::string& stepId,
                                    const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return false;
    }

    auto& instance = it->second;

    // 更新 Worker 状态
    auto wit = instance->workerStatus.find(workerId);
    if (wit != instance->workerStatus.end()) {
        wit->second.status = StepStatus::Failed;
        wit->second.endTime = Protocol::now();
        wit->second.message = error;
    }

    spdlog::error("Task failed: worker={}, step={}, error={}", workerId, stepId, error);

    // 步骤失败，标记整个步骤为失败
    instance->stepStatus[stepId] = StepStatus::Failed;
    instance->workflow.status = WorkflowStatus::Failed;
    instance->workflow.endTime = Protocol::now();

    return true;
}

// ========== 查询 ==========

std::shared_ptr<WorkflowInstance> WorkflowOrchestrator::getWorkflowInstance(const std::string& workflowId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it != workflows_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<WorkerStatus> WorkflowOrchestrator::getWorkerStatuses(const std::string& workflowId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return {};
    }

    const auto& instance = it->second;
    std::vector<WorkerStatus> result;
    result.reserve(instance->workerStatus.size());

    for (const auto& [workerId, status] : instance->workerStatus) {
        result.push_back(status);
    }

    return result;
}

StepStatus WorkflowOrchestrator::getStepStatus(const std::string& workflowId,
                                                const std::string& stepId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = workflows_.find(workflowId);
    if (it == workflows_.end()) {
        return StepStatus::Pending;
    }

    const auto& instance = it->second;
    auto sit = instance->stepStatus.find(stepId);
    if (sit != instance->stepStatus.end()) {
        return sit->second;
    }

    return StepStatus::Pending;
}

// ========== 私有方法 ==========

std::string WorkflowOrchestrator::generateId(const std::string& prefix) const {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return prefix + "_" + std::to_string(timestamp) + "_" + std::to_string(counter++);
}

std::optional<TaskStep> WorkflowOrchestrator::findNextStep(const WorkflowInstance& instance,
                                                            const std::string& workerId) const {
    for (const auto& step : instance.workflow.steps) {
        // 跳过已完成的步骤
        if (instance.isStepCompleted(step.id)) {
            continue;
        }

        // 跳过运行中的步骤（除非这个 worker 还没参与）
        auto sit = instance.stepStatus.find(step.id);
        if (sit != instance.stepStatus.end() && sit->second == StepStatus::Running) {
            // 检查这个 worker 是否已经在这个步骤中
            auto wsIt = instance.workerStatus.find(workerId);
            if (wsIt != instance.workerStatus.end() && wsIt->second.stepId == step.id) {
                continue;  // 已经在这个步骤中
            }
        }

        // 检查是否分配给这个 worker
        if (std::find(step.workers.begin(), step.workers.end(), workerId) == step.workers.end()) {
            continue;
        }

        // 检查依赖是否满足
        if (!instance.areAllDependenciesMet(step)) {
            continue;
        }

        // 找到一个可以执行的步骤
        return step;
    }

    return std::nullopt;
}

void WorkflowOrchestrator::updateWorkflowStatus(WorkflowInstance& instance) {
    // 检查是否有失败的步骤
    for (const auto& [stepId, status] : instance.stepStatus) {
        if (status == StepStatus::Failed) {
            instance.workflow.status = WorkflowStatus::Failed;
            instance.workflow.endTime = Protocol::now();
            return;
        }
    }

    // 检查是否所有步骤都完成了
    bool allCompleted = true;
    for (const auto& [stepId, status] : instance.stepStatus) {
        if (status != StepStatus::Completed && status != StepStatus::Skipped) {
            allCompleted = false;
            break;
        }
    }

    if (allCompleted) {
        instance.workflow.status = WorkflowStatus::Completed;
        instance.workflow.endTime = Protocol::now();
    }
}

} // namespace wingman::server
