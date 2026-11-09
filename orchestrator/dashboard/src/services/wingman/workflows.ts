import { request } from '@umijs/max';
import {
  ApiResponse,
  RawRecord,
  Workflow,
  normalizeWorkflow,
  normalizeWorkflowInstance,
  asArray,
  asRecord,
  normalizeApiResponse,
  toNumber,
  toStrictNumber,
  stringList,
} from './types';

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

export async function submitWorkflow(
  workflow: Omit<Workflow, 'id' | 'status' | 'createdTime' | 'startTime' | 'endTime'>,
) {
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
