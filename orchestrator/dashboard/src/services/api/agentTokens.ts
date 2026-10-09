import { fetchJSON } from '@/services/core/http';

// ========== Agent 注册 Token 管理（A3-P2，默认仅 admin）==========

export interface AgentToken {
  ID: number;
  label: string;
  prefix: string;
  agentId: string;
  createdBy: string;
  createdAt: string;
  revokedAt?: string;
  lastSeenAt?: string;
}

export interface CreateAgentTokenResult {
  /** 明文 token，只在签发响应出现这一次 */
  token: string;
  record: AgentToken;
}

export async function listAgentTokens(): Promise<AgentToken[]> {
  const resp = await fetchJSON<{ success: boolean; data: AgentToken[] }>(
    '/api/agent-tokens',
    { method: 'GET' },
  );
  return resp.data || [];
}

export async function createAgentToken(
  label: string,
  agentId = '',
): Promise<CreateAgentTokenResult> {
  const resp = await fetchJSON<{ success: boolean; data: CreateAgentTokenResult }>(
    '/api/agent-tokens',
    { method: 'POST', body: JSON.stringify({ label, agentId }) },
  );
  return resp.data;
}

export async function revokeAgentToken(
  id: number,
): Promise<{ revoked: boolean }> {
  const resp = await fetchJSON<{ success: boolean; data: { revoked: boolean } }>(
    `/api/agent-tokens/${id}`,
    { method: 'DELETE' },
  );
  return resp.data;
}
