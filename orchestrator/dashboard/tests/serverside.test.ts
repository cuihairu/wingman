/**
 * 无 window（Node/SSR）环境兜底分支：
 * - stores/scope：读写跳过 localStorage、事件派发跳过 window
 * - services/core/http：getToken 跳过、createEventSource/buildDownloadUrl 回退 http://localhost
 *
 * @jest-environment node
 */
let scope: typeof import('@/stores/scope');
let http: typeof import('@/services/core/http');

beforeAll(async () => {
  scope = await import('@/stores/scope');
  http = await import('@/services/core/http');
});

describe('无 window 环境（typeof window === "undefined"）', () => {
  it('scope 模块加载/读写均不触碰 storage，也不派发 DOM 事件', () => {
    // 模块加载时的 hydrateScope 在无 window 下安全回退空 scope
    expect(scope.getScope()).toEqual({});

    const listener = jest.fn();
    const unsub = scope.subscribeScope(listener);
    expect(() => scope.setScope({ gameId: 'srv', env: 'prod' })).not.toThrow();
    expect(scope.getScope()).toEqual({ gameId: 'srv', env: 'prod' });
    expect(listener).toHaveBeenCalledWith({ gameId: 'srv', env: 'prod' });
    unsub();
  });

  it('fetchJSON 无 window 时不附带 Authorization / scope 头', async () => {
    scope.setScope({ gameId: undefined, env: undefined }, { persist: false, emit: false });

    const fetchMock = jest.fn(async () => ({
      ok: true,
      status: 200,
      json: async () => ({ ok: 1 }),
    }));
    (global as any).fetch = fetchMock;

    await http.fetchJSON('/api/x');

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.has('Authorization')).toBe(false);
    expect(headers.has('X-Game-ID')).toBe(false);
    expect(headers.has('X-Env')).toBe(false);
    expect(headers.get('Accept')).toBe('application/json');

    delete (global as any).fetch;
  });

  it('createEventSource / buildDownloadUrl 回退 http://localhost origin', () => {
    class FakeEventSource {
      url: string;
      constructor(url: string) {
        this.url = url;
      }
    }
    (global as any).EventSource = FakeEventSource;

    const es = http.createEventSource('/api/stream', { params: { gameId: 'g9' } });
    expect(es.url).toBe('http://localhost/api/stream?gameId=g9');

    const dl = http.buildDownloadUrl('/api/export', { kind: 'csv' });
    expect(dl).toBe('http://localhost/api/export?kind=csv');

    delete (global as any).EventSource;
  });
});
