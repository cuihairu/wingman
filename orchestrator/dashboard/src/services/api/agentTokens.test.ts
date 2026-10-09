/**
 * 注册 Token 服务层测试：端点/方法/载荷形状（与 vault.test.ts 同口径——
 * 端点拼错是这一层唯一能自己犯的错；创建/吊销语义在 Go 侧覆盖）。
 */
import { fetchJSON } from '@/services/core/http';
import { createAgentToken, listAgentTokens, revokeAgentToken } from './agentTokens';

jest.mock('@/services/core/http', () => ({
  fetchJSON: jest.fn(),
}));

const mockedFetch = fetchJSON as jest.MockedFunction<typeof fetchJSON>;

describe('services/api/agentTokens', () => {
  beforeEach(() => {
    mockedFetch.mockReset();
  });

  it('listAgentTokens GET /api/agent-tokens，空数据兜底', async () => {
    mockedFetch.mockResolvedValueOnce({ success: true, data: [] });
    const list = await listAgentTokens();
    expect(list).toEqual([]);
    expect(mockedFetch).toHaveBeenCalledWith('/api/agent-tokens', { method: 'GET' });

    mockedFetch.mockResolvedValueOnce({ success: true, data: undefined as any });
    expect(await listAgentTokens()).toEqual([]);
  });

  it('createAgentToken POST 载荷形状，明文与记录透传', async () => {
    const payload = { token: 'wt_abc', record: { ID: 1, label: 'p8', prefix: 'wt_abc' } };
    mockedFetch.mockResolvedValueOnce({ success: true, data: payload });
    const result = await createAgentToken('p8', 'agent-pixel-8');
    expect(result.token).toBe('wt_abc');
    expect(mockedFetch).toHaveBeenCalledWith('/api/agent-tokens', {
      method: 'POST',
      body: JSON.stringify({ label: 'p8', agentId: 'agent-pixel-8' }),
    });
  });

  it('revokeAgentToken DELETE 路径携带 id', async () => {
    mockedFetch.mockResolvedValueOnce({ success: true, data: { revoked: true } });
    expect(await revokeAgentToken(7)).toEqual({ revoked: true });
    expect(mockedFetch).toHaveBeenCalledWith('/api/agent-tokens/7', { method: 'DELETE' });
  });
});
