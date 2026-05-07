/**
 * @name Wingman API 服务
 * @description 连接到 Wingman 后端服务器
 */

import { request } from '@umijs/max';

// ========== 类型定义 ==========

// Agent 状态
export enum AgentStatus {
  Offline = 'offline',
  Online = 'online',
  Idle = 'idle',
  Busy = 'busy',
  Error = 'error',
}

// 资源状态
export interface ResourceStats {
  cpu: {
    usage: number;
    cores: number;
    model: string;
  };
  memory: {
    total: number;
    available: number;
    usage: number;
  };
  disk: {
    total: number;
    available: number;
    usage: number;
  };
  network: {
    up: number;
    down: number;
    localIp: string;
    publicIp: string;
  };
  system: {
    temperature: number;
    os: string;
    arch: string;
  };
  timestamp: number;
}

// Agent 信息
export interface AgentInfo {
  agentId: string;
  hostname: string;
  ip: string;
  status: AgentStatus;
  currentTask: string;
  resources: ResourceStats;
  lastSeen: number;
}

// 工作流状态
export enum WorkflowStatus {
  Pending = 'pending',
  Running = 'running',
  Completed = 'completed',
  Failed = 'failed',
  Cancelled = 'cancelled',
}

// 步骤状态
export enum StepStatus {
  Pending = 'pending',
  Running = 'running',
  Completed = 'completed',
  Failed = 'failed',
  Skipped = 'skipped',
}

// 任务步骤
export interface TaskStep {
  id: string;
  name: string;
  script: string;
  workers: string[];
  dependsOn: string[];
  timeoutSeconds: number;
  parameters: Record<string, any>;
}

// 工作流
export interface Workflow {
  id: string;
  name: string;
  description: string;
  steps: TaskStep[];
  sharedContext: Record<string, any>;
  status: WorkflowStatus;
  createdTime: number;
  startTime: number;
  endTime: number;
}

// Worker 状态
export interface WorkerStatus {
  workerId: string;
  stepId: string;
  status: StepStatus;
  progress: Record<string, any>;
  message: string;
  startTime: number;
  endTime: number;
}

// 工作流实例详情
export interface WorkflowInstance extends Workflow {
  stepStatus: Record<string, StepStatus>;
  workerStatus: Record<string, WorkerStatus>;
  currentStepId: string;
}

// ========== API 接口 ==========

// Agent 管理
export async function getAgents() {
  return request<API.Response<AgentInfo[]>>('/api/agents', {
    method: 'GET',
  });
}

export async function getAgent(agentId: string) {
  return request<API.Response<AgentInfo>>(`/api/agents/${agentId}`, {
    method: 'GET',
  });
}

export async function shutdownAgent(agentId: string) {
  return request<API.Response<void>>(`/api/agents/${agentId}/shutdown`, {
    method: 'POST',
  });
}

// 工作流管理
export async function getWorkflows() {
  return request<API.Response<Workflow[]>>('/api/workflows', {
    method: 'GET',
  });
}

export async function getWorkflow(workflowId: string) {
  return request<API.Response<WorkflowInstance>>(`/api/workflows/${workflowId}`, {
    method: 'GET',
  });
}

export async function submitWorkflow(workflow: Omit<Workflow, 'id' | 'status' | 'createdTime' | 'startTime' | 'endTime'>) {
  return request<API.Response<{ workflowId: string }>>('/api/workflows', {
    method: 'POST',
    data: workflow,
  });
}

export async function cancelWorkflow(workflowId: string) {
  return request<API.Response<void>>(`/api/workflows/${workflowId}/cancel`, {
    method: 'POST',
  });
}

// 任务管理
export async function getWorkerStatuses(workflowId: string) {
  return request<API.Response<WorkerStatus[]>>(`/api/workflows/${workflowId}/workers`, {
    method: 'GET',
  });
}

export async function getStepStatus(workflowId: string, stepId: string) {
  return request<API.Response<StepStatus>>(`/api/workflows/${workflowId}/steps/${stepId}/status`, {
    method: 'GET',
  });
}

// ========== 脚本管理 ==========

// 脚本信息
export interface ScriptInfo {
  id: string;
  name: string;
  path: string;
  description?: string;
  size: number;
  modifiedTime: number;
  isRunning: boolean;
}

// 脚本执行日志
export interface ScriptLog {
  timestamp: number;
  level: 'info' | 'warn' | 'error' | 'debug';
  message: string;
}

// 获取脚本列表
export async function getScripts() {
  return request<API.Response<ScriptInfo[]>>('/api/scripts', {
    method: 'GET',
  });
}

// 获取脚本内容
export async function getScriptContent(path: string) {
  return request<API.Response<string>>(`/api/scripts/content`, {
    method: 'POST',
    data: { path },
  });
}

// 保存脚本内容
export async function saveScriptContent(path: string, content: string) {
  return request<API.Response<void>>(`/api/scripts/save`, {
    method: 'POST',
    data: { path, content },
  });
}

// 创建新脚本
export async function createScript(name: string, description?: string) {
  return request<API.Response<ScriptInfo>>('/api/scripts', {
    method: 'POST',
    data: { name, description },
  });
}

// 删除脚本
export async function deleteScript(path: string) {
  return request<API.Response<void>>(`/api/scripts/delete`, {
    method: 'POST',
    data: { path },
  });
}

// 运行脚本
export async function runScript(path: string, args?: string[]) {
  return request<API.Response<{ executionId: string }>>('/api/scripts/run', {
    method: 'POST',
    data: { path, args },
  });
}

// 停止脚本
export async function stopScript(executionId: string) {
  return request<API.Response<void>>(`/api/scripts/stop`, {
    method: 'POST',
    data: { executionId },
  });
}

// 获取脚本执行日志
export async function getScriptLogs(executionId: string, offset = 0, limit = 100) {
  return request<API.Response<ScriptLog[]>>(`/api/scripts/logs`, {
    method: 'POST',
    data: { executionId, offset, limit },
  });
}

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

export default {
  // Agent
  getAgents,
  getAgent,
  shutdownAgent,
  // Workflow
  getWorkflows,
  getWorkflow,
  submitWorkflow,
  cancelWorkflow,
  // Task
  getWorkerStatuses,
  getStepStatus,
  // Script
  getScripts,
  getScriptContent,
  saveScriptContent,
  createScript,
  deleteScript,
  runScript,
  stopScript,
  getScriptLogs,
  // Utils
  formatBytes,
  formatDuration,
  getAgentStatusColor,
  getWorkflowStatusColor,
  getStepStatusColor,
};
