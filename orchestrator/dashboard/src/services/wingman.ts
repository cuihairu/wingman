/**
 * @name Wingman API 服务
 * @description 连接到 Wingman 后端服务器
 */

import { request } from '@umijs/max';

// ========== 类型定义 ==========

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

type RawRecord = Record<string, any>;

function asRecord(value: unknown): RawRecord {
  return value && typeof value === 'object' ? (value as RawRecord) : {};
}

function asArray<T = any>(value: unknown): T[] {
  return Array.isArray(value) ? value : [];
}

function toNumber(value: unknown, fallback = 0): number {
  if (value === undefined || value === null || value === '') return fallback;
  if (value instanceof Date) return value.getTime();
  if (typeof value === 'string') {
    const parsedDate = Date.parse(value);
    if (!Number.isNaN(parsedDate)) return parsedDate;
  }
  const numberValue = Number(value);
  return Number.isFinite(numberValue) ? numberValue : fallback;
}

function parseMaybeJson<T>(value: unknown, fallback: T): T {
  if (Array.isArray(value) || (value && typeof value === 'object')) {
    return value as T;
  }
  if (typeof value === 'string' && value.trim()) {
    try {
      return JSON.parse(value) as T;
    } catch {
      return fallback;
    }
  }
  return fallback;
}

function unwrapResponseData<T>(response: ApiResponse<T> | T): T | undefined {
  if (response && typeof response === 'object' && 'success' in (response as RawRecord)) {
    return (response as ApiResponse<T>).data;
  }
  return response as T;
}

function withData<T, R>(
  response: ApiResponse<T>,
  normalize: (data: T | undefined) => R,
): ApiResponse<R> {
  return {
    ...response,
    data: normalize(unwrapResponseData(response)),
  };
}

function normalizeApiResponse<T, R>(
  response: ApiResponse<T>,
  normalize: (data: T | undefined) => R,
): ApiResponse<R> {
  return withData(response, normalize);
}

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
  tags?: string[];
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

function normalizeWorkflowStatus(value: unknown): WorkflowStatus {
  const status = String(value || WorkflowStatus.Pending).toLowerCase();
  return Object.values(WorkflowStatus).includes(status as WorkflowStatus)
    ? (status as WorkflowStatus)
    : WorkflowStatus.Pending;
}

function normalizeStepStatusValue(value: unknown): StepStatus {
  const status = String(value || StepStatus.Pending).toLowerCase();
  return Object.values(StepStatus).includes(status as StepStatus)
    ? (status as StepStatus)
    : StepStatus.Pending;
}

function stringList(value: unknown): string[] {
  if (Array.isArray(value)) {
    return value.map(String).map((item) => item.trim()).filter(Boolean);
  }
  if (typeof value === 'string') {
    return value.split(',').map((item) => item.trim()).filter(Boolean);
  }
  return [];
}

function normalizeTaskStep(value: unknown): TaskStep {
  const item = asRecord(value);
  return {
    id: String(item.id ?? item.ID ?? ''),
    name: String(item.name ?? item.Name ?? ''),
    script: String(item.script ?? item.Script ?? ''),
    workers: stringList(item.workers ?? item.Workers),
    dependsOn: stringList(item.dependsOn ?? item.DependsOn ?? item.depends_on),
    timeoutSeconds: toNumber(item.timeoutSeconds ?? item.TimeoutSeconds ?? item.timeout_seconds, 300),
    parameters: asRecord(item.parameters ?? item.Parameters),
  };
}

export function normalizeWorkflow(value: unknown): Workflow {
  const item = asRecord(value);
  const steps = parseMaybeJson<unknown[]>(item.steps ?? item.StepsJSON ?? item.stepsJson, []);
  const sharedContext = parseMaybeJson<Record<string, any>>(
    item.sharedContext ?? item.context ?? item.ContextJSON ?? item.contextJson,
    {},
  );
  return {
    id: String(item.id ?? item.ID ?? item.workflowId ?? ''),
    name: String(item.name ?? item.Name ?? ''),
    description: String(item.description ?? item.Description ?? ''),
    steps: asArray(steps).map(normalizeTaskStep),
    sharedContext,
    status: normalizeWorkflowStatus(item.status ?? item.Status),
    createdTime: toNumber(item.createdTime ?? item.createdAt ?? item.CreatedAt),
    startTime: toNumber(item.startTime ?? item.StartTime),
    endTime: toNumber(item.endTime ?? item.EndTime),
  };
}

function normalizeWorkerStatus(value: unknown): WorkerStatus {
  const item = asRecord(value);
  return {
    workerId: String(item.workerId ?? item.WorkerID ?? item.worker_id ?? ''),
    stepId: String(item.stepId ?? item.StepID ?? item.step_id ?? ''),
    status: normalizeStepStatusValue(item.status ?? item.Status),
    progress: asRecord(item.progress ?? item.Progress),
    message: String(item.message ?? item.Message ?? ''),
    startTime: toNumber(item.startTime ?? item.StartTime),
    endTime: toNumber(item.endTime ?? item.EndTime),
  };
}

function normalizeStepStatusMap(value: unknown): Record<string, StepStatus> {
  if (Array.isArray(value)) {
    return value.reduce<Record<string, StepStatus>>((result, raw) => {
      const item = asRecord(raw);
      const stepId = String(item.stepId ?? item.StepID ?? item.step_id ?? '');
      if (stepId) result[stepId] = normalizeStepStatusValue(item.status ?? item.Status);
      return result;
    }, {});
  }

  const record = asRecord(value);
  return Object.entries(record).reduce<Record<string, StepStatus>>((result, [stepId, raw]) => {
    const item = asRecord(raw);
    result[stepId] = normalizeStepStatusValue(item.status ?? item.Status ?? raw);
    return result;
  }, {});
}

function normalizeWorkerStatusMap(value: unknown): Record<string, WorkerStatus> {
  if (Array.isArray(value)) {
    return value.reduce<Record<string, WorkerStatus>>((result, raw) => {
      const worker = normalizeWorkerStatus(raw);
      if (worker.workerId) result[worker.workerId] = worker;
      return result;
    }, {});
  }

  const record = asRecord(value);
  return Object.entries(record).reduce<Record<string, WorkerStatus>>((result, [workerId, raw]) => {
    const worker = normalizeWorkerStatus({ workerId, ...asRecord(raw) });
    if (worker.workerId) result[worker.workerId] = worker;
    return result;
  }, {});
}

export function normalizeWorkflowInstance(value: unknown): WorkflowInstance {
  const item = asRecord(value);
  return {
    ...normalizeWorkflow(item),
    stepStatus: normalizeStepStatusMap(item.stepStatus ?? item.StepStatus ?? item.step_status),
    workerStatus: normalizeWorkerStatusMap(item.workerStatus ?? item.WorkerStatus ?? item.workers),
    currentStepId: String(item.currentStepId ?? item.CurrentStepID ?? item.current_step_id ?? ''),
  };
}

// ========== API 接口 ==========

// Agent 管理
export async function getAgents() {
  return request<ApiResponse<AgentInfo[]>>('/api/agents', {
    method: 'GET',
  });
}

export async function getAgent(agentId: string) {
  return request<ApiResponse<AgentInfo>>(`/api/agents/${agentId}`, {
    method: 'GET',
  });
}

export async function shutdownAgent(agentId: string) {
  return request<ApiResponse<void>>(`/api/agents/${agentId}/shutdown`, {
    method: 'POST',
  });
}

// setAgentTags 设置 agent 标签（分组），需 admin
export async function setAgentTags(agentId: string, tags: string[]) {
  return request<ApiResponse<void>>(`/api/agents/${agentId}/tags`, {
    method: 'PUT',
    data: { tags },
  });
}

// 工作流管理
export async function getWorkflows() {
  const response = await request<ApiResponse<unknown[]>>('/api/workflows', {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeWorkflow));
}

export async function getWorkflow(workflowId: string) {
  const response = await request<ApiResponse<unknown>>(`/api/workflows/${workflowId}`, {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => normalizeWorkflowInstance(data));
}

export interface WorkflowTemplateStep {
  id: string;
  name?: string;
  script?: string;
  workers?: string[];
  dependsOn?: string[];
  timeoutSeconds?: number;
  maxRetries?: number;
  retryBackoffSeconds?: number;
  parameters?: Record<string, any>;
}

export interface WorkflowTemplate {
  id: string;
  name: string;
  description?: string;
  category?: string;
  steps: WorkflowTemplateStep[];
}

// getWorkflowTemplates 内置工作流模板目录（只读）
export async function getWorkflowTemplates(): Promise<WorkflowTemplate[]> {
  const response = await request<ApiResponse<unknown>>('/api/workflow-templates', {
    method: 'GET',
  });
  const result = normalizeApiResponse(response, (data) => asArray(data) as WorkflowTemplate[]);
  return result.data ?? [];
}

export async function submitWorkflow(workflow: Omit<Workflow, 'id' | 'status' | 'createdTime' | 'startTime' | 'endTime'>) {
  const response = await request<ApiResponse<RawRecord>>('/api/workflows', {
    method: 'POST',
    data: workflow,
  });
  return normalizeApiResponse(response, (data) => ({
    workflowId: String(data?.workflowId ?? data?.id ?? ''),
  }));
}

export async function cancelWorkflow(workflowId: string) {
  return request<ApiResponse<void>>(`/api/workflows/${workflowId}/cancel`, {
    method: 'POST',
  });
}

// 任务管理
export async function getWorkerStatuses(workflowId: string) {
  const response = await request<ApiResponse<unknown[]>>(`/api/workflows/${workflowId}/workers`, {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeWorkerStatus));
}

export async function getStepStatus(workflowId: string, stepId: string) {
  const response = await request<ApiResponse<unknown>>(`/api/workflows/${workflowId}/steps/${stepId}/status`, {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => normalizeStepStatusValue(asRecord(data).status ?? data));
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
  executionId?: string;
}

// 脚本执行日志
export interface ScriptLog {
  timestamp: number;
  level: 'info' | 'warn' | 'error' | 'debug';
  message: string;
}

export function normalizeScript(value: unknown): ScriptInfo {
  const item = asRecord(value);
  const name = String(item.name ?? item.Name ?? '');
  const path = String(item.path ?? item.Path ?? name);
  const status = String(item.status ?? item.Status ?? '').toLowerCase();
  const isRunning = Boolean(item.isRunning ?? item.is_running ?? item.IsRunning ?? status === 'running');

  return {
    id: String(item.id ?? item.ID ?? path ?? name),
    name,
    path,
    description: String(item.description ?? item.Description ?? ''),
    size: toNumber(item.size ?? item.Size),
    modifiedTime: toNumber(item.modifiedTime ?? item.updatedAt ?? item.UpdatedAt ?? item.CreatedAt),
    isRunning,
    executionId: item.executionId ? String(item.executionId) : undefined,
  };
}

function normalizeScriptLog(value: unknown): ScriptLog {
  const item = asRecord(value);
  const level = String(item.level ?? item.Level ?? 'info').toLowerCase();
  return {
    timestamp: toNumber(item.timestamp ?? item.CreatedAt ?? item.createdAt, Date.now()),
    level: ['info', 'warn', 'error', 'debug'].includes(level)
      ? (level as ScriptLog['level'])
      : 'info',
    message: String(item.message ?? item.output ?? item.Output ?? ''),
  };
}

// 获取脚本列表
export async function getScripts() {
  const response = await request<ApiResponse<unknown[]>>('/api/scripts', {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeScript));
}

// 获取脚本内容
export async function getScriptContent(path: string) {
  return request<ApiResponse<string>>(`/api/scripts/content`, {
    method: 'POST',
    data: { path },
  });
}

// 保存脚本内容
export async function saveScriptContent(path: string, content: string) {
  return request<ApiResponse<void>>(`/api/scripts/save`, {
    method: 'POST',
    data: { path, content },
  });
}

// 创建新脚本
export async function createScript(name: string, description?: string) {
  const response = await request<ApiResponse<unknown>>('/api/scripts', {
    method: 'POST',
    data: { name, description },
  });
  return normalizeApiResponse(response, (data) => normalizeScript(data));
}

// 删除脚本
export async function deleteScript(path: string) {
  return request<ApiResponse<void>>(`/api/scripts/delete`, {
    method: 'POST',
    data: { path },
  });
}

// 运行脚本
export async function runScript(path: string, args?: string[]) {
  const response = await request<ApiResponse<RawRecord>>('/api/scripts/run', {
    method: 'POST',
    data: { path, args },
  });
  return normalizeApiResponse(response, (data) => ({
    executionId: String(data?.executionId ?? data?.scriptId ?? ''),
  }));
}

// 停止脚本
export async function stopScript(executionId: string) {
  return request<ApiResponse<void>>(`/api/scripts/stop`, {
    method: 'POST',
    data: { executionId },
  });
}

// 获取脚本执行日志
export async function getScriptLogs(executionId: string, offset = 0, limit = 100) {
  const response = await request<ApiResponse<unknown[]>>(`/api/scripts/logs`, {
    method: 'POST',
    data: { executionId, offset, limit },
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeScriptLog));
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
