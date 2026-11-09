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
  const isRunning = Boolean(
    item.isRunning ?? item.is_running ?? item.IsRunning ?? status === 'running',
  );

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
