import { request } from '@umijs/max';
import {
  ApiResponse,
  asArray,
  asRecord,
  normalizeApiResponse,
  toNumber,
  toStrictNumber,
  stringList,
} from './types';

// ========== 团队（多 agent 协作，见 docs/architecture-decisions.md「Team inbox push」） ==========

// TeamView 服务端团队视图（/api/teams 输出）。团队运行时状态由 agent 通道的
// TeamManager 持有（内存态），服务重启后丢失，这里只反映当前在线视图。
export interface TeamView {
  teamId: string;
  name: string;
  description: string;
  leaderId: string;
  /** 成员 ID 列表（map 展开，顺序不保证） */
  members: string[];
  /** 团队状态（TeamManager 生命周期状态） */
  state: string;
  createdAt: number;
  updatedAt: number;
  /** 成员 → 绑定设备映射（仅 /api/teams/:teamId 详情返回；缺省 = 未绑定设备） */
  memberAgents?: Record<string, string>;
}

// normalizeTeam 防御性归一：字段缺失/类型漂移时回退默认值
export function normalizeTeam(value: unknown): TeamView {
  const record = asRecord(value);
  const memberAgentsRaw = asRecord(record.memberAgents);
  const memberAgents: Record<string, string> = {};
  for (const [memberId, agentId] of Object.entries(memberAgentsRaw)) {
    if (typeof agentId === 'string') memberAgents[memberId] = agentId;
  }
  return {
    teamId: String(record.teamId ?? ''),
    name: String(record.name ?? ''),
    description: String(record.description ?? ''),
    leaderId: String(record.leaderId ?? ''),
    members: asArray(record.members).map(String),
    state: String(record.state ?? ''),
    createdAt: toNumber(record.createdAt),
    updatedAt: toNumber(record.updatedAt),
    // 详情接口才有 memberAgents；列表视图不带该字段（避免空对象与缺省混淆）
    ...(Object.keys(memberAgentsRaw).length > 0 ? { memberAgents } : {}),
  };
}

// TeamMessageSummary 团队推送逐成员汇总（POST /api/teams/:teamId/messages 输出）
export interface TeamMessageSummary {
  teamId: string;
  total: number;
  delivered: number;
  skipped: number;
  results: { memberId: string; agentId: string; delivered: boolean; skipped?: string }[];
}

export function normalizeTeamMessageSummary(value: unknown): TeamMessageSummary {
  const record = asRecord(value);
  return {
    teamId: String(record.teamId ?? ''),
    total: toNumber(record.total),
    delivered: toNumber(record.delivered),
    skipped: toNumber(record.skipped),
    results: asArray(record.results).map((item) => {
      const result = asRecord(item);
      return {
        memberId: String(result.memberId ?? ''),
        agentId: String(result.agentId ?? ''),
        delivered: Boolean(result.delivered),
        skipped: typeof result.skipped === 'string' ? result.skipped : undefined,
      };
    }),
  };
}

// getTeams 团队列表（按 teamId 升序；需 agents:manage）
export async function getTeams() {
  const response = await request<ApiResponse<unknown[]>>('/api/teams', {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => asArray(data).map(normalizeTeam));
}

// getTeam 团队详情（基础字段 + memberAgents 成员→设备映射）
export async function getTeam(teamId: string) {
  const response = await request<ApiResponse<unknown>>(`/api/teams/${teamId}`, {
    method: 'GET',
  });
  return normalizeApiResponse(response, (data) => normalizeTeam(data));
}

// pushTeamMessage 团队范围收件箱推送（需 agents:manage；payload 为 JSON 对象）
export async function pushTeamMessage(teamId: string, messageType: string, payload?: unknown) {
  return request<ApiResponse<TeamMessageSummary>>(`/api/teams/${teamId}/messages`, {
    method: 'POST',
    data: { messageType, ...(payload !== undefined ? { payload } : {}) },
  });
}

// 工作流管理
