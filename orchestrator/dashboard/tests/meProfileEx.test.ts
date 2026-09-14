/**
 * me.ts 归一化容错补全：permissions 条目为 null / 缺 resource / actions 非数组。
 */
import { request } from '@umijs/max';
import * as me from '@/services/api/me';

const mockedRequest = request as unknown as jest.Mock;

beforeEach(() => {
  mockedRequest.mockReset();
});

describe('getMyPermissions 容错', () => {
  it('null 条目与缺 resource/actions 字段时安全回退', async () => {
    mockedRequest.mockResolvedValueOnce({
      admin: true,
      roles: ['admin'],
      permissions: [
        null,
        { actions: ['read'] },
        { resource: 'agent', actions: null },
      ],
    });

    const resp = await me.getMyPermissions({ gameId: 'g1', env: 'prod' });

    expect(resp.admin).toBe(true);
    expect(resp.permissions).toHaveLength(3);
    // null 条目：resource 回退空串，actions 回退空数组
    expect(resp.permissions[0]).toEqual({
      resource: '',
      actions: [],
      gameId: undefined,
      env: undefined,
    });
    // 缺 resource：回退空串
    expect(resp.permissions[1]).toMatchObject({ resource: '', actions: ['read'] });
    // actions 非数组：回退空数组
    expect(resp.permissions[2]).toMatchObject({ resource: 'agent', actions: [] });
  });
});
