/**
 * Audit API 服务存根
 * TODO: 根据实际后端 API 实现审计日志功能
 */

export interface AuditEvent {
  hash?: string;
  time?: string;
  kind?: string;
  target?: string;
  meta?: Record<string, any>;
}

export interface ListAuditParams {
  actor?: string;
  kinds?: string;
  size?: number;
  page?: number;
}

export interface ListAuditResponse {
  events: AuditEvent[];
}

export async function listAudit(params: ListAuditParams): Promise<ListAuditResponse> {
  // 存根实现：返回空审计日志
  return Promise.resolve({ events: [] });
}
