#pragma once

#include "wingman/server/protocol.hpp"
#include "wingman_protocol.pb.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace wingman::server {

// ========== 工作流定义 ==========

// 步骤状态
enum class StepStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Skipped
};

inline std::string stepStatusToString(StepStatus status) {
    switch (status) {
        case StepStatus::Pending: return "pending";
        case StepStatus::Running: return "running";
        case StepStatus::Completed: return "completed";
        case StepStatus::Failed: return "failed";
        case StepStatus::Skipped: return "skipped";
        default: return "unknown";
    }
}

// 任务步骤
struct TaskStep {
    std::string id;
    std::string name;
    std::string script;                       // Lua 脚本路径或内容
    std::vector<std::string> workers;         // 分配给哪些 agent
    std::vector<std::string> dependsOn;       // 依赖的步骤 ID
    int timeoutSeconds = 300;
    nlohmann::json parameters;                // 脚本参数

    // 转换到 protobuf
    wingman::protocol::TaskStep toProtobuf() const {
        wingman::protocol::TaskStep proto;
        proto.set_id(id);
        proto.set_name(name);
        proto.set_script(script);
        for (const auto& w : workers) proto.add_workers(w);
        for (const auto& d : dependsOn) proto.add_depends_on(d);
        proto.set_timeout_seconds(timeoutSeconds);
        proto.set_parameters(parameters.dump());
        return proto;
    }

    // 从 protobuf 转换
    static TaskStep fromProtobuf(const wingman::protocol::TaskStep& proto) {
        TaskStep step;
        step.id = proto.id();
        step.name = proto.name();
        step.script = proto.script();
        for (const auto& w : proto.workers()) step.workers.push_back(w);
        for (const auto& d : proto.depends_on()) step.dependsOn.push_back(d);
        step.timeoutSeconds = proto.timeout_seconds();
        if (!proto.parameters().empty()) {
            step.parameters = nlohmann::json::parse(proto.parameters());
        }
        return step;
    }

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["script"] = script;
        j["workers"] = workers;
        j["depends_on"] = dependsOn;
        j["timeout_seconds"] = timeoutSeconds;
        j["parameters"] = parameters;
        return j;
    }

    static TaskStep fromJson(const nlohmann::json& j) {
        TaskStep step;
        if (j.contains("id")) step.id = j["id"];
        if (j.contains("name")) step.name = j["name"];
        if (j.contains("script")) step.script = j["script"];
        if (j.contains("workers")) step.workers = j["workers"].get<std::vector<std::string>>();
        if (j.contains("depends_on")) step.dependsOn = j["depends_on"].get<std::vector<std::string>>();
        if (j.contains("timeout_seconds")) step.timeoutSeconds = j["timeout_seconds"];
        if (j.contains("parameters")) step.parameters = j["parameters"];
        return step;
    }
};

// Worker 执行状态
struct WorkerStatus {
    std::string workerId;
    std::string stepId;
    StepStatus status = StepStatus::Pending;
    nlohmann::json progress;                   // 进度数据
    std::string message;                       // 状态消息
    uint64_t startTime = 0;
    uint64_t endTime = 0;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["worker_id"] = workerId;
        j["step_id"] = stepId;
        j["status"] = stepStatusToString(status);
        j["progress"] = progress;
        j["message"] = message;
        j["start_time"] = startTime;
        j["end_time"] = endTime;
        return j;
    }
};

// 工作流状态
enum class WorkflowStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

inline std::string workflowStatusToString(WorkflowStatus status) {
    switch (status) {
        case WorkflowStatus::Pending: return "pending";
        case WorkflowStatus::Running: return "running";
        case WorkflowStatus::Completed: return "completed";
        case WorkflowStatus::Failed: return "failed";
        case WorkflowStatus::Cancelled: return "cancelled";
        default: return "unknown";
    }
}

// 工作流状态枚举转换
inline wingman::protocol::Workflow::Status workflowStatusToProtobuf(WorkflowStatus status) {
    switch (status) {
        case WorkflowStatus::Pending: return wingman::protocol::Workflow::PENDING;
        case WorkflowStatus::Running: return wingman::protocol::Workflow::RUNNING;
        case WorkflowStatus::Completed: return wingman::protocol::Workflow::COMPLETED;
        case WorkflowStatus::Failed: return wingman::protocol::Workflow::FAILED;
        case WorkflowStatus::Cancelled: return wingman::protocol::Workflow::CANCELLED;
        default: return wingman::protocol::Workflow::PENDING;
    }
}

inline WorkflowStatus protobufToWorkflowStatus(wingman::protocol::Workflow::Status status) {
    switch (status) {
        case wingman::protocol::Workflow::PENDING: return WorkflowStatus::Pending;
        case wingman::protocol::Workflow::RUNNING: return WorkflowStatus::Running;
        case wingman::protocol::Workflow::COMPLETED: return WorkflowStatus::Completed;
        case wingman::protocol::Workflow::FAILED: return WorkflowStatus::Failed;
        case wingman::protocol::Workflow::CANCELLED: return WorkflowStatus::Cancelled;
        default: return WorkflowStatus::Pending;
    }
}

// 工作流定义
struct Workflow {
    std::string id;
    std::string name;
    std::string description;
    std::vector<TaskStep> steps;
    nlohmann::json sharedContext;              // 步骤间共享数据
    WorkflowStatus status = WorkflowStatus::Pending;
    uint64_t createdTime = 0;
    uint64_t startTime = 0;
    uint64_t endTime = 0;

    // 转换到 protobuf
    wingman::protocol::Workflow toProtobuf() const {
        wingman::protocol::Workflow proto;
        proto.set_id(id);
        proto.set_name(name);
        proto.set_description(description);
        for (const auto& step : steps) {
            *proto.add_steps() = step.toProtobuf();
        }
        proto.set_shared_context(sharedContext.dump());
        proto.set_status(workflowStatusToProtobuf(status));
        proto.set_created_time(createdTime);
        proto.set_start_time(startTime);
        proto.set_end_time(endTime);
        return proto;
    }

    // 从 protobuf 转换
    static Workflow fromProtobuf(const wingman::protocol::Workflow& proto) {
        Workflow wf;
        wf.id = proto.id();
        wf.name = proto.name();
        wf.description = proto.description();
        for (const auto& stepProto : proto.steps()) {
            wf.steps.push_back(TaskStep::fromProtobuf(stepProto));
        }
        if (!proto.shared_context().empty()) {
            wf.sharedContext = nlohmann::json::parse(proto.shared_context());
        }
        wf.status = protobufToWorkflowStatus(proto.status());
        wf.createdTime = proto.created_time();
        wf.startTime = proto.start_time();
        wf.endTime = proto.end_time();
        return wf;
    }

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["status"] = workflowStatusToString(status);
        j["created_time"] = createdTime;
        j["start_time"] = startTime;
        j["end_time"] = endTime;
        j["shared_context"] = sharedContext;

        nlohmann::json stepsJson = nlohmann::json::array();
        for (const auto& step : steps) {
            stepsJson.push_back(step.toJson());
        }
        j["steps"] = stepsJson;
        return j;
    }

    static Workflow fromJson(const nlohmann::json& j) {
        Workflow wf;
        if (j.contains("id")) wf.id = j["id"];
        if (j.contains("name")) wf.name = j["name"];
        if (j.contains("description")) wf.description = j["description"];
        if (j.contains("shared_context")) wf.sharedContext = j["shared_context"];
        if (j.contains("steps")) {
            for (const auto& stepJson : j["steps"]) {
                wf.steps.push_back(TaskStep::fromJson(stepJson));
            }
        }
        return wf;
    }
};

// 工作流执行实例
class WorkflowInstance {
public:
    Workflow workflow;
    std::unordered_map<std::string, StepStatus> stepStatus;      // stepId -> status
    std::unordered_map<std::string, WorkerStatus> workerStatus;   // workerId -> status
    std::unordered_map<std::string, std::unordered_set<std::string>> stepWorkers;  // stepId -> workerIds
    std::string currentStepId;                                     // 当前执行的步骤

    bool isStepCompleted(const std::string& stepId) const {
        auto it = stepStatus.find(stepId);
        return it != stepStatus.end() && it->second == StepStatus::Completed;
    }

    bool areAllDependenciesMet(const TaskStep& step) const {
        for (const auto& depId : step.dependsOn) {
            if (!isStepCompleted(depId)) {
                return false;
            }
        }
        return true;
    }

    bool areAllWorkersCompleted(const std::string& stepId) const {
        auto it = stepWorkers.find(stepId);
        if (it == stepWorkers.end()) return false;

        for (const auto& workerId : it->second) {
            auto wit = workerStatus.find(workerId);
            if (wit == workerStatus.end() ||
                (wit->second.status != StepStatus::Completed &&
                 wit->second.status != StepStatus::Failed)) {
                return false;
            }
        }
        return true;
    }
};

// ========== WorkflowOrchestrator 核心类 ==========

class WorkflowOrchestrator {
public:
    WorkflowOrchestrator() = default;
    ~WorkflowOrchestrator() = default;

    // 禁止拷贝
    WorkflowOrchestrator(const WorkflowOrchestrator&) = delete;
    WorkflowOrchestrator& operator=(const WorkflowOrchestrator&) = delete;

    // ========== 工作流管理 ==========

    // 提交工作流
    std::string submitWorkflow(const Workflow& workflow);

    // 取消工作流
    bool cancelWorkflow(const std::string& workflowId);

    // 获取工作流状态
    std::optional<Workflow> getWorkflow(const std::string& workflowId) const;

    // 获取所有工作流
    std::vector<Workflow> getAllWorkflows() const;

    // ========== 任务执行 ==========

    // Worker 请求下一步任务
    std::optional<TaskStep> getNextTask(const std::string& workerId, const std::string& workflowId);

    // Worker 报告进度
    bool reportProgress(const std::string& workerId, const std::string& workflowId,
                       const std::string& stepId, const nlohmann::json& progress);

    // Worker 完成任务
    bool completeTask(const std::string& workerId, const std::string& workflowId,
                     const std::string& stepId, bool success, const nlohmann::json& result);

    // Worker 失败任务
    bool failTask(const std::string& workerId, const std::string& workflowId,
                 const std::string& stepId, const std::string& error);

    // ========== 查询 ==========

    // 获取工作流实例
    std::shared_ptr<WorkflowInstance> getWorkflowInstance(const std::string& workflowId) const;

    // 获取工作流中所有 Worker 的状态
    std::vector<WorkerStatus> getWorkerStatuses(const std::string& workflowId) const;

    // 获取指定步骤的状态
    StepStatus getStepStatus(const std::string& workflowId, const std::string& stepId) const;

private:
    mutable std::mutex mutex_;

    // workflowId -> WorkflowInstance
    std::unordered_map<std::string, std::shared_ptr<WorkflowInstance>> workflows_;

    // 生成唯一 ID
    std::string generateId(const std::string& prefix) const;

    // 查找下一个可执行的步骤
    std::optional<TaskStep> findNextStep(const WorkflowInstance& instance, const std::string& workerId) const;

    // 更新工作流状态
    void updateWorkflowStatus(WorkflowInstance& instance);
};

} // namespace wingman::server
