/**
 * scope.ts 状态_store：读写、持久化开关、事件广播、订阅退订、hydrate。
 */

let scopeModule: typeof import('@/stores/scope');

const localStorageMock = {
  getItem: jest.fn(),
  setItem: jest.fn(),
  removeItem: jest.fn(),
  clear: jest.fn(),
};

describe('scope store', () => {
  beforeAll(async () => {
    Object.defineProperty(window, 'localStorage', { value: localStorageMock, configurable: true });
    scopeModule = await import('@/stores/scope');
  });

  beforeEach(() => {
    jest.clearAllMocks();
    // 重置模块内缓存状态（setScope 是合并语义，需显式置空字段）
    scopeModule.setScope({ gameId: undefined, env: undefined }, { persist: false, emit: false });
  });

  it('setScope 默认持久化并广播事件', () => {
    const listener = jest.fn();
    const unsub = scopeModule.subscribeScope(listener);
    const eventListener = jest.fn();
    window.addEventListener('scope:change', eventListener);

    const next = scopeModule.setScope({ gameId: 'g1', env: 'prod' });

    expect(next).toEqual({ gameId: 'g1', env: 'prod' });
    expect(scopeModule.getScope()).toEqual({ gameId: 'g1', env: 'prod' });
    expect(localStorageMock.setItem).toHaveBeenCalledWith('game_id', 'g1');
    expect(localStorageMock.setItem).toHaveBeenCalledWith('env', 'prod');
    expect(listener).toHaveBeenCalledWith({ gameId: 'g1', env: 'prod' });
    expect(eventListener).toHaveBeenCalledTimes(1);
    expect((eventListener.mock.calls[0][0] as CustomEvent).detail).toEqual({
      gameId: 'g1',
      env: 'prod',
    });

    window.removeEventListener('scope:change', eventListener);
    unsub();
  });

  it('persist=false / emit=false 关闭持久化与广播', () => {
    const listener = jest.fn();
    scopeModule.subscribeScope(listener);
    const eventListener = jest.fn();
    window.addEventListener('scope:change', eventListener);

    scopeModule.setScope({ gameId: 'g2' }, { persist: false, emit: false });

    expect(localStorageMock.setItem).not.toHaveBeenCalled();
    expect(listener).not.toHaveBeenCalled();
    expect(eventListener).not.toHaveBeenCalled();

    window.removeEventListener('scope:change', eventListener);
  });

  it('只设置部分字段时保留其余字段（合并语义）', () => {
    scopeModule.setScope({ gameId: 'keep', env: 'e1' }, { persist: false, emit: false });
    scopeModule.setScope({ env: 'e2' }, { persist: false, emit: false });
    expect(scopeModule.getScope()).toEqual({ gameId: 'keep', env: 'e2' });
  });

  it('持久化时只写存在的字段', () => {
    jest.clearAllMocks();
    scopeModule.setScope({ env: 'only-env' });
    expect(localStorageMock.setItem).toHaveBeenCalledTimes(1);
    expect(localStorageMock.setItem).toHaveBeenCalledWith('env', 'only-env');
  });

  it('持久化时缺 env 字段则跳过写入', () => {
    jest.clearAllMocks();
    scopeModule.setScope({ gameId: 'solo-game' });
    expect(localStorageMock.setItem).toHaveBeenCalledTimes(1);
    expect(localStorageMock.setItem).toHaveBeenCalledWith('game_id', 'solo-game');
  });

  it('hydrateScope 从 localStorage 恢复', () => {
    localStorageMock.getItem.mockImplementation((key: string) =>
      key === 'game_id' ? 'hg' : key === 'env' ? 'test' : null,
    );

    const restored = scopeModule.hydrateScope();
    expect(restored).toEqual({ gameId: 'hg', env: 'test' });
    expect(scopeModule.getScope()).toEqual({ gameId: 'hg', env: 'test' });
  });

  it('hydrateScope 存储为空时不覆盖当前状态', () => {
    localStorageMock.getItem.mockReturnValue(null);
    scopeModule.setScope({ gameId: 'stay' }, { persist: false, emit: false });
    expect(scopeModule.hydrateScope()).toEqual({ gameId: 'stay' });
  });

  it('subscribeScope 返回退订函数', () => {
    const listener = jest.fn();
    const unsub = scopeModule.subscribeScope(listener);
    unsub();
    scopeModule.setScope({ gameId: 'x' }, { persist: false, emit: true });
    expect(listener).not.toHaveBeenCalled();
  });

  it('读取存储抛异常时 hydrate 安全回退空 scope', () => {
    localStorageMock.getItem.mockImplementation(() => {
      throw new Error('storage blocked');
    });
    expect(scopeModule.hydrateScope()).toEqual({});
  });

  it('持久化抛异常时 setScope 不中断', () => {
    localStorageMock.setItem.mockImplementation(() => {
      throw new Error('quota exceeded');
    });
    expect(() => scopeModule.setScope({ gameId: 'q1' })).not.toThrow();
    expect(scopeModule.getScope()).toEqual({ gameId: 'q1' });
  });
});
