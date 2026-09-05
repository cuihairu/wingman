/**
 * services/api 剩余函数：admin 用户/角色 CRUD、audit 全参数、
 * me profile 归一化（camel/snake 双写、envMeta 派生 envs）、
 * messages 归一化与已读接口。
 */
import { request } from '@umijs/max';
import * as admin from '@/services/api/admin';
import * as audit from '@/services/api/audit';
import * as me from '@/services/api/me';
import * as messages from '@/services/api/messages';

const mockedRequest = request as unknown as jest.Mock;

const fetchMock = jest.fn(async () => ({
  ok: true,
  status: 200,
  json: async () => ({}),
}));

beforeEach(() => {
  mockedRequest.mockReset();
  fetchMock.mockClear();
  (global as any).fetch = fetchMock;
});

function lastFetch(): { url: string; init: RequestInit } {
  const calls = fetchMock.mock.calls;
  expect(calls.length).toBeGreaterThan(0);
  return { url: String(calls[calls.length - 1][0]), init: calls[calls.length - 1][1] ?? {} };
}

describe('admin.ts 用户/角色 CRUD', () => {
  it('listUsers 支持 role/keyword 过滤', async () => {
    await admin.listUsers({ role: 'admin', keyword: 'li' });
    const { url } = lastFetch();
    expect(url).toBe('/api/admin/users?role=admin&keyword=li');
  });

  it('updateUser → PUT /api/admin/users/:id', async () => {
    await admin.updateUser(5, { active: false });
    const { url, init } = lastFetch();
    expect(url).toBe('/api/admin/users/5');
    expect(init.method).toBe('PUT');
    expect(JSON.parse(String(init.body))).toEqual({ active: false });
  });

  it('deleteUser → DELETE /api/admin/users/:id', async () => {
    await admin.deleteUser(6);
    const { url, init } = lastFetch();
    expect(url).toBe('/api/admin/users/6');
    expect(init.method).toBe('DELETE');
  });

  it('createRole / updateRole / deleteRole', async () => {
    await admin.createRole({ code: 'ops', permissions: ['a.read'] });
    expect(lastFetch().init.method).toBe('POST');

    await admin.updateRole('ops team', { name: '运维' });
    const upd = lastFetch();
    expect(upd.url).toBe('/api/admin/roles/ops%20team');
    expect(upd.init.method).toBe('PUT');

    await admin.deleteRole('ops');
    expect(lastFetch()).toMatchObject({ url: '/api/admin/roles/ops', init: { method: 'DELETE' } });
  });
});

describe('admin.ts 权限目录分类', () => {
  it('listPermissionCatalog 支持 category 过滤', async () => {
    await admin.listPermissionCatalog('net');
    expect(lastFetch().url).toBe('/api/admin/permissions?category=net');
  });
});

describe('audit.ts 全参数', () => {
  it('listAudit 透传 actor/start/end/size/page', async () => {
    await audit.listAudit({ actor: 'admin', kinds: 'login', start: 't0', end: 't1', size: 50, page: 3 });
    const { url } = lastFetch();
    expect(url).toBe('/api/audit?actor=admin&kinds=login&start=t0&end=t1&size=50&page=3');
  });
});

describe('me.ts 归一化', () => {
  it('getMyProfile 兼容 profileInfo 嵌套与 snake_case', async () => {
    mockedRequest.mockResolvedValueOnce({
      profileInfo: {
        id: 3,
        username: 'op',
        display_name: '操作员',
        active: false,
        roles: ['viewer'],
        created_at: 'c',
        last_login_at: 'l',
      },
    });

    const profile = await me.getMyProfile();
    expect(profile).toMatchObject({
      id: 3,
      username: 'op',
      displayName: '操作员',
      active: false,
      roles: ['viewer'],
      createdAt: 'c',
      lastLoginAt: 'l',
    });
  });

  it('getMyProfile 空响应安全', async () => {
    mockedRequest.mockResolvedValueOnce(null);
    const profile = await me.getMyProfile();
    expect(profile.username).toBe('');
    expect(profile.roles).toEqual([]);
  });

  it('getMyGames：envs 数组优先，envMeta 派生 envs，snake_case 字段回退', async () => {
    mockedRequest.mockResolvedValueOnce({
      games: [
        {
          game_id: 'g1',
          name: 'game-one',
          display_name: '游戏一',
          envMeta: [{ env: 'dev' }, { env: 'prod' }, { env: null }],
          permissions: ['a.read'],
        },
        {
          gameId: 'g2',
          gameName: '游戏二',
          envs: ['staging'],
        },
        {},
      ],
    });

    const { games } = await me.getMyGames();
    expect(games[0]).toEqual({
      gameId: 'g1',
      name: 'game-one',
      gameName: '游戏一',
      envMeta: [{ env: 'dev' }, { env: 'prod' }, { env: null }],
      envs: ['dev', 'prod'],
      permissions: ['a.read'],
    });
    expect(games[1]).toMatchObject({ gameId: 'g2', gameName: '游戏二', envs: ['staging'] });
    expect(games[2]).toMatchObject({ gameId: undefined, envs: [], permissions: [] });
  });

  it('getMyGames 响应缺 games 时返回空数组', async () => {
    mockedRequest.mockResolvedValueOnce({});
    expect((await me.getMyGames()).games).toEqual([]);
  });

  it('getMyPermissions 归一化 + 携带查询参数', async () => {
    mockedRequest.mockResolvedValueOnce({
      admin: true,
      permissions: [
        { resource: 'agents', actions: ['read'], game_id: 'g1', env: 'prod' },
        { resource: 'users' },
      ],
    });

    const resp = await me.getMyPermissions({ gameId: 'g1', env: 'prod' });
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/v1/profile/permissions', {
      params: { gameId: 'g1', env: 'prod' },
    });
    expect(resp.admin).toBe(true);
    expect(resp.permissions[0]).toEqual({
      resource: 'agents',
      actions: ['read'],
      gameId: 'g1',
      env: 'prod',
    });
    expect(resp.permissions[1]).toEqual({
      resource: 'users',
      actions: [],
      gameId: undefined,
      env: undefined,
    });
  });

  it('getMyPermissions 无参数时不带 params', async () => {
    mockedRequest.mockResolvedValueOnce({});
    await me.getMyPermissions();
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/v1/profile/permissions', {
      params: undefined,
    });
  });

  it('updateMyProfile：displayName 兜底 nickname', async () => {
    mockedRequest.mockResolvedValueOnce(undefined);
    await me.updateMyProfile({ displayName: '昵称', email: 'a@b.c' });
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/v1/profile', {
      method: 'PUT',
      data: { nickname: '昵称', email: 'a@b.c', phone: undefined, avatar: undefined },
    });
  });

  it('changeMyPassword 字段映射', async () => {
    mockedRequest.mockResolvedValueOnce(undefined);
    await me.changeMyPassword({ current: 'old', password: 'new' });
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/v1/profile/password', {
      method: 'PUT',
      data: { oldPassword: 'old', newPassword: 'new' },
    });
  });
});

describe('messages.ts 归一化与已读', () => {
  it('listMessages 归一化条目（id 数字转字符串、created_at 回退、状态默认 unread）', async () => {
    fetchMock.mockImplementationOnce(
      async () =>
        ({
          ok: true,
          status: 200,
          json: async () => ({
            total: 3,
            items: [
              { id: 7, title: 't', content: 'c', status: 'read', createdAt: '2024-01-01' },
              { id: 8, status: 'other', created_at: '2024-01-02' },
              { title: '无 id 条目' },
            ],
          }),
        }) as unknown as Response,
    );

    const resp = await messages.listMessages({ status: 'unread', pageSize: 10, page: 2 });
    expect(lastFetch().url).toBe('/api/messages?status=unread&pageSize=10&page=2');
    expect(resp.total).toBe(3);
    expect(resp.items[0]).toMatchObject({ id: '7', status: 'read', createdAt: '2024-01-01' });
    expect(resp.items[1]).toMatchObject({ id: '8', status: 'unread', createdAt: '2024-01-02' });
    expect(resp.items[2]).toMatchObject({ id: undefined, status: 'unread' });
  });

  it('listMessages items 缺失时返回空数组', async () => {
    const resp = await messages.listMessages();
    expect(resp.items).toEqual([]);
    expect(resp.total).toBeUndefined();
  });

  it('markMessageRead → POST /api/messages/:id/read（URL 编码）', async () => {
    await messages.markMessageRead('a b');
    const { url, init } = lastFetch();
    expect(url).toBe('/api/messages/a%20b/read');
    expect(init.method).toBe('POST');
  });
});
