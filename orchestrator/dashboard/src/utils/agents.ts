import type { AgentInfo } from '@/services/wingman';

/**
 * 设备视图（Dashboard 设备视图：分组 / 批量下发 / 状态大盘）的纯函数口径。
 * 从 pages/Agents 抽出的分组与平台统计逻辑集中在此，保证视图层可单测。
 */

/** 标签分组视图的单个分组；tag 为 null 表示「未分组」（无任何标签的 agent） */
export interface AgentTagGroup {
  tag: string | null;
  agents: AgentInfo[];
}

/**
 * 按标签分组：一个 agent 可同属多个标签组（多标签不互斥）；
 * 无标签的 agent 归入 tag=null 的「未分组」组，始终排在末尾。
 * 组名升序（localeCompare），组内保持入参顺序（列表已按接口返回序）。
 */
export function groupAgentsByTag(agents: AgentInfo[]): AgentTagGroup[] {
  const groups = new Map<string, AgentInfo[]>();
  const untagged: AgentInfo[] = [];

  for (const agent of agents) {
    const tags = agent.tags ?? [];
    if (tags.length === 0) {
      untagged.push(agent);
      continue;
    }
    for (const tag of tags) {
      const members = groups.get(tag) ?? [];
      members.push(agent);
      groups.set(tag, members);
    }
  }

  const grouped: AgentTagGroup[] = Array.from(groups.entries())
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([tag, members]) => ({ tag, agents: members }));

  if (untagged.length > 0) {
    grouped.push({ tag: null, agents: untagged });
  }
  return grouped;
}

/** 平台维度统计条目（desktop / android / ios） */
export interface AgentPlatformCount {
  platform: string;
  count: number;
}

/**
 * 状态大盘的平台维度：按平台计数，platform 缺失归入 desktop
 * （与页面 PlatformTag 同口径：旧 agent 未上报 platform 时按桌面处理）。
 * 降序按数量排列（数量相同按平台名升序），便于大盘展示主要机型。
 */
export function countByPlatform(agents: AgentInfo[]): AgentPlatformCount[] {
  const counts = new Map<string, number>();
  for (const agent of agents) {
    const platform = agent.platform || 'desktop';
    counts.set(platform, (counts.get(platform) ?? 0) + 1);
  }
  return Array.from(counts.entries())
    .map(([platform, count]) => ({ platform, count }))
    .sort((a, b) => b.count - a.count || a.platform.localeCompare(b.platform));
}
