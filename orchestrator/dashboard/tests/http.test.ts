/**
 * services/core/http.ts：fetchJSON 头部装配（token/scope/Content-Type）、
 * 错误抛出、204/JSON 解析失败回退；createEventSource / buildDownloadUrl
 * 参数拼接。scope 头联动 stores/scope。
 */
import { buildDownloadUrl, createEventSource, fetchJSON } from '@/services/core/http';
import { setScope } from '@/stores/scope';

const localStorageMock = {
  getItem: jest.fn(),
  setItem: jest.fn(),
  removeItem: jest.fn(),
  clear: jest.fn(),
};

const fetchMock = jest.fn();

class FakeEventSource {
  static instances: FakeEventSource[] = [];
  url: string;
  constructor(url: string) {
    this.url = url;
    FakeEventSource.instances.push(this);
  }
}

function jsonResponse(overrides: Partial<Response> = {}): Response {
  return {
    ok: true,
    status: 200,
    text: async () => '',
    json: async () => ({}),
    ...overrides,
  } as unknown as Response;
}

describe('fetchJSON', () => {
  beforeAll(() => {
    Object.defineProperty(window, 'localStorage', { value: localStorageMock, configurable: true });
    (global as any).EventSource = FakeEventSource;
  });

  beforeEach(() => {
    jest.clearAllMocks();
    (global as any).fetch = fetchMock;
    localStorageMock.getItem.mockImplementation((key: string) =>
      key === 'token' ? 'tk-1' : null,
    );
    setScope({ gameId: 'g1', env: 'prod' }, { persist: false, emit: false });
  });

  it('装配 Authorization / X-Game-ID / X-Env / Accept / Content-Type 头', async () => {
    fetchMock.mockResolvedValueOnce(jsonResponse({ json: async () => ({ ok: 1 }) }));

    await fetchJSON('/api/admin/users', { body: JSON.stringify({ a: 1 }) });

    const [, init] = fetchMock.mock.calls[0];
    const headers = new Headers(init.headers);
    expect(headers.get('Authorization')).toBe('Bearer tk-1');
    expect(headers.get('X-Game-ID')).toBe('g1');
    expect(headers.get('X-Env')).toBe('prod');
    expect(headers.get('Accept')).toBe('application/json');
    expect(headers.get('Content-Type')).toBe('application/json');
  });

  it('scope 值含非 ASCII 时不写入对应头', async () => {
    setScope({ gameId: '中文游戏', env: 'prod' }, { persist: false, emit: false });
    fetchMock.mockResolvedValueOnce(jsonResponse());

    await fetchJSON('/api/x');

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.has('X-Game-ID')).toBe(false);
    expect(headers.get('X-Env')).toBe('prod');
  });

  it('skipAuth / skipScopeHeaders 关闭对应头', async () => {
    fetchMock.mockResolvedValueOnce(jsonResponse());

    await fetchJSON('/api/x', { skipAuth: true, skipScopeHeaders: true });

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.has('Authorization')).toBe(false);
    expect(headers.has('X-Game-ID')).toBe(false);
    expect(headers.has('X-Env')).toBe(false);
  });

  it('已有 Authorization 头时不覆盖', async () => {
    fetchMock.mockResolvedValueOnce(jsonResponse());

    await fetchJSON('/api/x', { headers: { Authorization: 'Bearer manual' } });

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.get('Authorization')).toBe('Bearer manual');
  });

  it('localStorage 抛异常时无 token 也正常发起请求', async () => {
    localStorageMock.getItem.mockImplementation(() => {
      throw new Error('blocked');
    });
    fetchMock.mockResolvedValueOnce(jsonResponse({ status: 204 }));

    const data = await fetchJSON('/api/x');
    expect(data).toBeUndefined();

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.has('Authorization')).toBe(false);
  });

  it('非 2xx 响应抛出带 status 与 responseText 的错误', async () => {
    fetchMock.mockResolvedValueOnce(
      jsonResponse({ ok: false, status: 500, text: async () => 'boom' }),
    );

    await expect(fetchJSON('/api/x')).rejects.toMatchObject({
      message: 'boom',
      status: 500,
      responseText: 'boom',
    });
  });

  it('text() 失败时回退到默认错误消息', async () => {
    fetchMock.mockResolvedValueOnce(
      jsonResponse({
        ok: false,
        status: 502,
        text: async () => {
          throw new Error('no text');
        },
      }),
    );

    await expect(fetchJSON('/api/x')).rejects.toMatchObject({
      message: 'Request failed: 502',
      status: 502,
    });
  });

  it('json() 解析失败时返回 undefined', async () => {
    fetchMock.mockResolvedValueOnce(
      jsonResponse({
        json: async () => {
          throw new Error('bad json');
        },
      }),
    );

    await expect(fetchJSON('/api/x')).resolves.toBeUndefined();
  });

  it('fetchJSON 已带 Content-Type 头时不重复设置', async () => {
    fetchMock.mockResolvedValueOnce(jsonResponse());

    await fetchJSON('/api/x', {
      headers: { 'Content-Type': 'text/plain' },
      body: JSON.stringify({ a: 1 }),
    });

    const headers = new Headers(fetchMock.mock.calls[0][1].headers);
    expect(headers.get('Content-Type')).toBe('text/plain');
  });
});

describe('createEventSource', () => {
  beforeAll(() => {
    (global as any).EventSource = FakeEventSource;
  });

  beforeEach(() => {
    FakeEventSource.instances = [];
    localStorageMock.getItem.mockImplementation((key: string) =>
      key === 'token' ? 'tk-2' : null,
    );
  });

  it('合并 params 并附加 token', () => {
    createEventSource('/api/stream', { params: { gameId: 'g1', size: 20 } });

    const url = FakeEventSource.instances[0].url;
    const params = new URL(url, 'http://localhost:8000').searchParams;
    expect(params.get('gameId')).toBe('g1');
    expect(params.get('size')).toBe('20');
    expect(params.get('token')).toBe('tk-2');
  });

  it('undefined/null 参数被跳过，attachToken=false 不附带 token', () => {
    createEventSource('/api/stream', {
      params: { gameId: undefined, env: null as unknown as string },
      attachToken: false,
    });

    const params = new URL(FakeEventSource.instances[0].url, 'http://localhost:8000').searchParams;
    expect(params.has('gameId')).toBe(false);
    expect(params.has('env')).toBe(false);
    expect(params.has('token')).toBe(false);
  });

  it('无 opts 时直接使用路径查询参数', () => {
    createEventSource('/api/stream?kind=all');

    const params = new URL(FakeEventSource.instances[0].url, 'http://localhost:8000').searchParams;
    expect(params.get('kind')).toBe('all');
    expect(params.get('token')).toBe('tk-2');
  });

  it('本地无 token 时不附带 token 参数', () => {
    localStorageMock.getItem.mockReturnValue(null);
    createEventSource('/api/stream');

    const params = new URL(FakeEventSource.instances[0].url, 'http://localhost:8000').searchParams;
    expect(params.has('token')).toBe(false);
  });
});

describe('buildDownloadUrl', () => {
  it('拼接查询参数并过滤 undefined/null', () => {
    const url = buildDownloadUrl('/api/export', { kind: 'csv', page: 2, empty: undefined });
    const params = new URL(url, 'http://localhost:8000').searchParams;
    expect(params.get('kind')).toBe('csv');
    expect(params.get('page')).toBe('2');
    expect(params.has('empty')).toBe(false);
  });

  it('无参数时原样返回', () => {
    const url = buildDownloadUrl('/api/export');
    expect(new URL(url, 'http://localhost:8000').pathname).toBe('/api/export');
  });
});
