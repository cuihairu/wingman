import { buildMemberRows, sortTeamsByTeamId } from './teams';
import type { TeamView } from '@/services/wingman';

function teamOf(overrides: Partial<TeamView>): TeamView {
  return {
    teamId: 'team_1',
    name: 'demo',
    description: '',
    leaderId: 'admin',
    members: [],
    state: 'active',
    createdAt: 0,
    updatedAt: 0,
    ...overrides,
  };
}

describe('buildMemberRows', () => {
  it('merges members with memberAgents and sorts by memberId', () => {
    const rows = buildMemberRows(
      teamOf({
        members: ['m-3', 'admin', 'm-1'],
        memberAgents: { 'm-1': 'agent-a', admin: '' },
      }),
    );
    expect(rows.map((row) => row.memberId)).toEqual(['admin', 'm-1', 'm-3']);
    expect(rows[0]).toMatchObject({ agentId: '', bound: false, leader: true });
    expect(rows[1]).toMatchObject({ agentId: 'agent-a', bound: true, leader: false });
    expect(rows[2]).toMatchObject({ agentId: '', bound: false, leader: false });
  });

  it('treats all members as unbound when memberAgents is absent (list view)', () => {
    const rows = buildMemberRows(teamOf({ members: ['m-1', 'm-2'] }));
    expect(rows.every((row) => !row.bound && row.agentId === '')).toBe(true);
  });

  it('flags the leader row and skips unknown mapping entries', () => {
    const rows = buildMemberRows(
      teamOf({
        leaderId: 'admin',
        members: ['admin'],
        // admin 未在 members 中出现（理论脏数据）：映射多出的条目不产生行
        memberAgents: { admin: '', ghost: 'agent-x' },
      }),
    );
    expect(rows).toHaveLength(1);
    expect(rows[0]).toMatchObject({ memberId: 'admin', agentId: '', bound: false, leader: true });
  });

  it('returns empty rows for a team without members', () => {
    expect(buildMemberRows(teamOf({}))).toEqual([]);
  });
});

describe('sortTeamsByTeamId', () => {
  it('sorts in byte order (matching server sort.Strings) and does not mutate the input', () => {
    const input = [
      teamOf({ teamId: 'team_2' }),
      teamOf({ teamId: 'team_10' }),
      teamOf({ teamId: 'team_1' }),
    ];
    const sorted = sortTeamsByTeamId(input);
    expect(sorted.map((team) => team.teamId)).toEqual(['team_1', 'team_10', 'team_2']);
    expect(input.map((team) => team.teamId)).toEqual(['team_2', 'team_10', 'team_1']);
  });

  it('handles empty list', () => {
    expect(sortTeamsByTeamId([])).toEqual([]);
  });
});
