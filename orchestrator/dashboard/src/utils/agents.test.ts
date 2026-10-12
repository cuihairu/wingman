/**
 * 设备视图纯函数测试：按标签分组 + 平台统计。
 * 视图层 JSX 不在覆盖率口径内（pages/ 未被任何测试导入），分组与统计
 * 的边界口径只能在这层钉住。
 */
import type { AgentInfo } from '@/services/wingman';
import { countByPlatform, groupAgentsByTag } from './agents';

// 最小可用 AgentInfo：分组/统计只读 tags 与 platform，其余字段占位
const agent = (agentId: string, tags?: string[], platform?: string): AgentInfo =>
  ({
    agentId,
    hostname: agentId,
    ip: '0.0.0.0',
    status: 'online',
    currentTask: '',
    resources: {},
    lastSeen: 0,
    tags,
    platform,
  }) as unknown as AgentInfo;

describe('groupAgentsByTag', () => {
  it('多标签 agent 同属多个组，组名升序，组内保持入参顺序', () => {
    const agents = [agent('a', ['prod', 'us']), agent('b', ['us']), agent('c', ['prod'])];
    const groups = groupAgentsByTag(agents);
    expect(groups.map((g) => g.tag)).toEqual(['prod', 'us']);
    expect(groups[0].agents.map((a) => a.agentId)).toEqual(['a', 'c']);
    expect(groups[1].agents.map((a) => a.agentId)).toEqual(['a', 'b']);
  });

  it('无标签 agent 归入未分组组并排在末尾；tags 缺失视为未分组', () => {
    const agents = [agent('a', ['staging']), agent('b'), agent('c', [])];
    const groups = groupAgentsByTag(agents);
    expect(groups.map((g) => g.tag)).toEqual(['staging', null]);
    expect(groups[1].agents.map((a) => a.agentId)).toEqual(['b', 'c']);
  });

  it('空列表返回空分组，未分组组不出现在结果里', () => {
    expect(groupAgentsByTag([])).toEqual([]);
    expect(groupAgentsByTag([agent('a', ['only'])]).map((g) => g.tag)).toEqual(['only']);
  });
});

describe('countByPlatform', () => {
  it('按平台计数，platform 缺失按 desktop 口径归入', () => {
    const agents = [
      agent('a', undefined, 'android'),
      agent('b', undefined, 'android'),
      agent('c', undefined, 'desktop'),
      agent('d'),
    ];
    expect(countByPlatform(agents)).toEqual([
      { platform: 'android', count: 2 },
      { platform: 'desktop', count: 2 },
    ]);
  });

  it('数量相同按平台名升序；空列表返回空数组', () => {
    expect(countByPlatform([])).toEqual([]);
    expect(
      countByPlatform([agent('a', undefined, 'ios'), agent('b', undefined, 'desktop')]),
    ).toEqual([
      { platform: 'desktop', count: 1 },
      { platform: 'ios', count: 1 },
    ]);
  });
});
