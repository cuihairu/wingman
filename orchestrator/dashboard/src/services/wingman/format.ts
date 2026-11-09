import { AgentStatus, StepStatus, WorkflowStatus } from './types';

// ========== 工具函数 ==========

export function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
}

export function formatDuration(ms: number): string {
  if (ms < 1000) return `${ms}ms`;
  if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
  if (ms < 3600000) return `${(ms / 60000).toFixed(1)}m`;
  return `${(ms / 3600000).toFixed(1)}h`;
}

export function getAgentStatusColor(status: AgentStatus): string {
  switch (status) {
    case AgentStatus.Online:
      return 'green';
    case AgentStatus.Idle:
      return 'blue';
    case AgentStatus.Busy:
      return 'orange';
    case AgentStatus.Error:
      return 'red';
    case AgentStatus.Offline:
    default:
      return 'default';
  }
}

export function getWorkflowStatusColor(status: WorkflowStatus): string {
  switch (status) {
    case WorkflowStatus.Completed:
      return 'success';
    case WorkflowStatus.Running:
      return 'processing';
    case WorkflowStatus.Failed:
      return 'error';
    case WorkflowStatus.Cancelled:
      return 'default';
    case WorkflowStatus.Pending:
    default:
      return 'default';
  }
}

export function getStepStatusColor(status: StepStatus): string {
  switch (status) {
    case StepStatus.Completed:
      return 'success';
    case StepStatus.Running:
      return 'processing';
    case StepStatus.Failed:
      return 'error';
    case StepStatus.Skipped:
      return 'default';
    case StepStatus.Pending:
    default:
      return 'default';
  }
}
