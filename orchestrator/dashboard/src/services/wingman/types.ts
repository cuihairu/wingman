// ========== 类型定义 ==========

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

export type RawRecord = Record<string, any>;

export function asRecord(value: unknown): RawRecord {
  return value && typeof value === 'object' ? (value as RawRecord) : {};
}

export function asArray<T = any>(value: unknown): T[] {
  return Array.isArray(value) ? value : [];
}

export function toNumber(value: unknown, fallback = 0): number {
  if (value === undefined || value === null || value === '') return fallback;
  if (value instanceof Date) return value.getTime();
  if (typeof value === 'string') {
    const parsedDate = Date.parse(value);
    if (!Number.isNaN(parsedDate)) return parsedDate;
  }
  const numberValue = Number(value);
  return Number.isFinite(numberValue) ? numberValue : fallback;
}

// toStrictNumber 严格数值转换：不做日期字符串猜测，用于触发器坐标/阈值等纯数值字段
export function toStrictNumber(value: unknown, fallback = 0): number {
  if (typeof value === 'number') return Number.isFinite(value) ? value : fallback;
  if (typeof value === 'string' && value.trim() !== '') {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }
  return fallback;
}

export function parseMaybeJson<T>(value: unknown, fallback: T): T {
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

export function unwrapResponseData<T>(response: ApiResponse<T> | T): T | undefined {
  if (response && typeof response === 'object' && 'success' in (response as RawRecord)) {
    return (response as ApiResponse<T>).data;
  }
  return response as T;
}

export function withData<T, R>(
  response: ApiResponse<T>,
  normalize: (data: T | undefined) => R,
): ApiResponse<R> {
  return {
    ...response,
    data: normalize(unwrapResponseData(response)),
  };
}

export function normalizeApiResponse<T, R>(
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
  /** 注册平台：desktop（缺省）/ android，来自 agent.register（A1，见 docs/android-agent-design.md §3.3） */
  platform?: string;
  /** 设备能力词汇（ADR: Capability System），agent.register 上报；空数组 = 未上报 */
  capabilities?: string[];
  /** 词汇表外的上报项（server 标 unverified，UI 以警示色展示） */
  unknownCapabilities?: string[];
}

// 触发器条件（runtime BasicTriggerCondition）
export interface TriggerCondition {
  type: string;
  value: string;
  region: { x: number; y: number; width: number; height: number };
  tolerance: number;
  interval: number;
  enabled: boolean;
}

// 触发器动作（runtime TriggerActionData）
export interface TriggerAction {
  type: string;
  value: string;
  x: number;
  y: number;
  delay: number;
}

// Agent 触发器（runtime TriggerInstance 经 trigger.list 透传）
export interface AgentTrigger {
  id: string;
  name: string;
  enabled: boolean;
  type: string;
  condition: TriggerCondition;
  actions: TriggerAction[];
  oneShot: boolean;
  cooldown: number;
  lastTriggered: boolean;
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

export function normalizeWorkflowStatus(value: unknown): WorkflowStatus {
  const status = String(value || WorkflowStatus.Pending).toLowerCase();
  return Object.values(WorkflowStatus).includes(status as WorkflowStatus)
    ? (status as WorkflowStatus)
    : WorkflowStatus.Pending;
}

export function normalizeStepStatusValue(value: unknown): StepStatus {
  const status = String(value || StepStatus.Pending).toLowerCase();
  return Object.values(StepStatus).includes(status as StepStatus)
    ? (status as StepStatus)
    : StepStatus.Pending;
}

export function stringList(value: unknown): string[] {
  if (Array.isArray(value)) {
    return value
      .map(String)
      .map((item) => item.trim())
      .filter(Boolean);
  }
  if (typeof value === 'string') {
    return value
      .split(',')
      .map((item) => item.trim())
      .filter(Boolean);
  }
  return [];
}

export function normalizeTaskStep(value: unknown): TaskStep {
  const item = asRecord(value);
  return {
    id: String(item.id ?? item.ID ?? ''),
    name: String(item.name ?? item.Name ?? ''),
    script: String(item.script ?? item.Script ?? ''),
    workers: stringList(item.workers ?? item.Workers),
    dependsOn: stringList(item.dependsOn ?? item.DependsOn ?? item.depends_on),
    timeoutSeconds: toNumber(
      item.timeoutSeconds ?? item.TimeoutSeconds ?? item.timeout_seconds,
      300,
    ),
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

export function normalizeWorkerStatus(value: unknown): WorkerStatus {
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

export function normalizeStepStatusMap(value: unknown): Record<string, StepStatus> {
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

export function normalizeWorkerStatusMap(value: unknown): Record<string, WorkerStatus> {
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
