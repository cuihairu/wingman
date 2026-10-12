/**
 * Teams 页面纯函数工具（见 src/utils/agents.test.ts 同款测试口径）
 */

import type { TeamView } from '@/services/wingman';

// TeamMemberRow 团队成员行：memberAgents 映射并入 members 列表后的展示行
export interface TeamMemberRow {
  memberId: string;
  /** 绑定设备 ID；空串 = 未绑定设备（收件箱推送会跳过） */
  agentId: string;
  bound: boolean;
  /** 是否为建团 leader（admin 建团时 leader 以空 agentID 成员身份在列） */
  leader: boolean;
}

// buildMemberRows 合并成员列表与成员→设备映射：按 memberId 升序，
// agentId 非空视为已绑定。详情接口未返回 memberAgents（缺省）时全部按未绑定展示。
export function buildMemberRows(team: TeamView): TeamMemberRow[] {
  const agents = team.memberAgents ?? {};
  return [...team.members]
    .sort((a, b) => a.localeCompare(b))
    .map((memberId) => {
      const agentId = agents[memberId] ?? '';
      return { memberId, agentId, bound: agentId !== '', leader: memberId === team.leaderId };
    });
}

// sortTeamsByTeamId 按 teamId 升序（server 已排序，前端对本地状态变化兜底）
export function sortTeamsByTeamId(teams: TeamView[]): TeamView[] {
  return [...teams].sort((a, b) => a.teamId.localeCompare(b.teamId));
}
