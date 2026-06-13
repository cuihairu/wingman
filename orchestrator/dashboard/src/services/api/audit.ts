import { fetchJSON } from '@/services/core/http';

export interface AuditEvent {
  hash?: string;
  time?: string;
  kind?: string;
  actor?: string;
  target?: string;
  meta?: Record<string, any>;
}

export interface ListAuditParams {
  actor?: string;
  kinds?: string;
  start?: string;
  end?: string;
  size?: number;
  page?: number;
}

export interface ListAuditResponse {
  events: AuditEvent[];
  total: number;
}

export async function listAudit(params: ListAuditParams): Promise<ListAuditResponse> {
  const search = new URLSearchParams();
  if (params.actor) search.set('actor', params.actor);
  if (params.kinds) search.set('kinds', params.kinds);
  if (params.start) search.set('start', params.start);
  if (params.end) search.set('end', params.end);
  if (params.size) search.set('size', String(params.size));
  if (params.page) search.set('page', String(params.page));
  const suffix = search.toString();
  return fetchJSON<ListAuditResponse>(`/api/audit${suffix ? `?${suffix}` : ''}`);
}
