import { request } from '@umijs/max';
import {
  ApiResponse,
  RawRecord,
  asArray,
  asRecord,
  normalizeApiResponse,
  toNumber,
  toStrictNumber,
  stringList,
} from './types';
import { AgentTriggerConfigInput } from './agents';

// ========== 批量操作（POST /api/agents/batch/*） ==========

// BatchAgentSelector 批量目标选择器：agentIds 与 tags 任一非空即可，同时提供取并集
export interface BatchAgentSelector {
  agentIds?: string[];
  tags?: string[];
}

// BatchAgentResult 单台 agent 的批量操作结果
export interface BatchAgentResult {
  agentId: string;
  success: boolean;
  error?: string;
}

// BatchSummary 批量操作汇总（部分失败不算整体失败，逐台标注）
export interface BatchSummary {
  total: number;
  succeeded: number;
  failed: number;
  results: BatchAgentResult[];
}

// normalizeBatchSummary 宽容归一化批量汇总（字段缺失/类型不符回退安全值）
export function normalizeBatchSummary(value: unknown): BatchSummary {
  const item = asRecord(value);
  return {
    total: toNumber(item.total),
    succeeded: toNumber(item.succeeded),
    failed: toNumber(item.failed),
    results: asArray<Record<string, unknown>>(item.results).map((raw) => {
      const result = asRecord(raw);
      return {
        agentId: String(result.agentId ?? ''),
        success: result.success === true,
        error: result.error ? String(result.error) : undefined,
      };
    }),
  };
}

async function requestBatch(selector: BatchAgentSelector, url: string, data: RawRecord) {
  const response = await request<ApiResponse<unknown>>(url, {
    method: 'POST',
    data: { ...selector, ...data },
  });
  return normalizeApiResponse(response, (raw) => normalizeBatchSummary(raw));
}

// batchRunScript 批量运行脚本（需 scripts:run；非法路径服务端 400 且零下发）
export async function batchRunScript(selector: BatchAgentSelector, path: string) {
  return requestBatch(selector, '/api/agents/batch/run-script', { path });
}

// batchStopScript 批量停止脚本（需 scripts:run；executionId 即脚本名）
export async function batchStopScript(selector: BatchAgentSelector, executionId: string) {
  return requestBatch(selector, '/api/agents/batch/stop-script', { executionId });
}

// batchCreateTrigger 批量下发触发器（需 agents:manage；配置与单 agent 一致）
export async function batchCreateTrigger(
  selector: BatchAgentSelector,
  config: AgentTriggerConfigInput,
) {
  return requestBatch(selector, '/api/agents/batch/trigger', { ...config });
}
