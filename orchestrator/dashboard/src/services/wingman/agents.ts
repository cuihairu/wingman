import { request } from '@umijs/max';
import {
  ApiResponse,
  AgentInfo,
  AgentTrigger,
  asArray,
  asRecord,
  normalizeApiResponse,
  toNumber,
  toStrictNumber,
  stringList,
} from './types';

// ========== API 接口 ==========

// Agent 管理
export async function getAgents() {
  return request<ApiResponse<AgentInfo[]>>('/api/agents', {
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

// ========== 执行记录（ADR: Execution as the Platform Core Object） ==========

// ExecutionStatus 状态机：pending→queued→running→终态五分支
export type ExecutionStatus =
  | 'pending'
  | 'queued'
  | 'running'
  | 'succeeded'
  | 'failed'
  | 'cancelled'
  | 'timeout'
  | 'lost';

// ExecutionView 服务端 Execution 视图（/api/executions 输出，Result/Artifacts 已展开）
export interface ExecutionView {
  id: number;
  executionId: string;
  /** 0 = 直接下发（run_script/batch），非 workflow 步骤 */
  workflowId: number;
  stepId: string;
  agentId: string;
  scriptPath: string;
  status: ExecutionStatus;
  startedAt: string;
  finishedAt: string;
  timeoutSec: number;
  result?: unknown;
  artifacts: string[];
  createdAt: string;
}

// ExecutionListQuery 列表查询参数（分页 + 过滤）
export interface ExecutionListQuery {
  page?: number;
  pageSize?: number;
  status?: string;
  agentId?: string;
}

// ExecutionListResult 分页结果
export interface ExecutionListResult {
  data: ExecutionView[];
  total: number;
  page: number;
  pageSize: number;
}

// listExecutions 执行记录列表（登录即可）
export async function listExecutions(params: ExecutionListQuery) {
  const search = new URLSearchParams();
  if (params.page) search.set('page', String(params.page));
  if (params.pageSize) search.set('pageSize', String(params.pageSize));
  if (params.status) search.set('status', params.status);
  if (params.agentId) search.set('agentId', params.agentId);
  const qs = search.toString();
  return request<{ success: boolean } & ExecutionListResult>(
    `/api/executions${qs ? `?${qs}` : ''}`,
    {
      method: 'GET',
    },
  );
}

// getExecution 执行记录详情
export async function getExecution(id: number) {
  return request<ApiResponse<ExecutionView>>(`/api/executions/${id}`, {
    method: 'GET',
  });
}

// getExecutionStatusColor 状态语义色（与 Agent/Workflow 状态色约定一致）
export function getExecutionStatusColor(status: ExecutionStatus): string {
  switch (status) {
    case 'succeeded':
      return 'green';
    case 'failed':
      return 'red';
    case 'running':
      return 'processing';
    case 'timeout':
      return 'orange';
    case 'cancelled':
      return 'default';
    case 'lost':
      return 'volcano';
    default:
      return 'blue'; // pending / queued
  }
}

// normalizeAgentTrigger 归一化 runtime trigger.list 条目：
// 字段缺失/类型不符时回退安全默认值，避免渲染层抛错。
export function normalizeAgentTrigger(value: unknown): AgentTrigger {
  const item = asRecord(value);
  const condition = asRecord(item.condition);
  const region = asRecord(condition.region);
  return {
    id: String(item.id ?? ''),
    name: String(item.name ?? item.id ?? '未命名触发器'),
    enabled: item.enabled === true,
    type: String(item.type ?? condition.type ?? ''),
    condition: {
      type: String(condition.type ?? item.type ?? ''),
      value: String(condition.value ?? ''),
      region: {
        x: toStrictNumber(region.x),
        y: toStrictNumber(region.y),
        width: toStrictNumber(region.width),
        height: toStrictNumber(region.height),
      },
      tolerance: toStrictNumber(condition.tolerance),
      interval: toStrictNumber(condition.interval),
      enabled: condition.enabled !== false,
    },
    actions: asArray<Record<string, unknown>>(item.actions).map((action) => {
      const record = asRecord(action);
      return {
        type: String(record.type ?? ''),
        value: String(record.value ?? ''),
        x: toStrictNumber(record.x),
        y: toStrictNumber(record.y),
        delay: toStrictNumber(record.delay),
      };
    }),
    oneShot: item.oneShot === true,
    cooldown: toStrictNumber(item.cooldown),
    lastTriggered: item.lastTriggered === true,
  };
}

// getAgentTriggers 获取指定 agent 的触发器列表（经 Go server 透传 runtime trigger.list）
export async function getAgentTriggers(agentId: string) {
  const response = await request<ApiResponse<unknown[]>>(`/api/agents/${agentId}/triggers`, {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeAgentTrigger));
}

// toggleAgentTrigger 切换触发器启用状态（需 agents:manage）
export async function toggleAgentTrigger(agentId: string, triggerId: string) {
  return request<ApiResponse<{ id: string; enabled: boolean }>>(
    `/api/agents/${agentId}/triggers/toggle`,
    {
      method: 'POST',
      data: { id: triggerId },
    },
  );
}

// AgentTriggerConfigInput 触发器配置输入（runtime TriggerConfig 的可编辑子集）
export interface AgentTriggerConfigInput {
  name: string;
  enabled?: boolean;
  oneShot?: boolean;
  cooldown?: number;
  condition?: {
    type?: string;
    value?: string;
    tolerance?: number;
    interval?: number;
    enabled?: boolean;
    region?: { x: number; y: number; width: number; height: number };
  };
  actions?: Array<{
    type?: string;
    value?: string;
    x?: number;
    y?: number;
    delay?: number;
  }>;
}

// createAgentTrigger 在指定 agent 上新增触发器（需 agents:manage）
export async function createAgentTrigger(agentId: string, config: AgentTriggerConfigInput) {
  return request<ApiResponse<{ id: string; name: string }>>(`/api/agents/${agentId}/triggers`, {
    method: 'POST',
    data: config,
  });
}

// updateAgentTrigger 更新触发器配置（部分字段，需 agents:manage）
export async function updateAgentTrigger(
  agentId: string,
  triggerId: string,
  config: AgentTriggerConfigInput,
) {
  return request<ApiResponse<{ id: string }>>(`/api/agents/${agentId}/triggers/${triggerId}`, {
    method: 'PUT',
    data: config,
  });
}

// removeAgentTrigger 删除触发器（需 agents:manage）
export async function removeAgentTrigger(agentId: string, triggerId: string) {
  return request<ApiResponse<{ id: string }>>(`/api/agents/${agentId}/triggers/${triggerId}`, {
    method: 'DELETE',
  });
}
