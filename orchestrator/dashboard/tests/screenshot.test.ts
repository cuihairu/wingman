/**
 * ScreenshotService：订阅广播、退订、当前帧缓存、监听器异常隔离。
 * 通过 WebSocketService 单例注入 type="screenshot" 消息驱动。
 */

class FakeWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSED = 3;
  static lastInstance: FakeWebSocket | null = null;

  readyState = FakeWebSocket.CONNECTING;
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  onclose: ((event: unknown) => void) | null = null;
  onerror: ((error: unknown) => void) | null = null;
  sent: string[] = [];
  close = jest.fn();

  constructor() {
    FakeWebSocket.lastInstance = this;
  }

  send(data: string) {
    this.sent.push(data);
  }

  simulateOpen() {
    this.readyState = FakeWebSocket.OPEN;
    this.onopen?.();
  }

  simulateMessage(payload: unknown) {
    this.onmessage?.({ data: JSON.stringify(payload) });
  }
}

const frame = { image: 'data:image/png;base64,QQ==', width: 640, height: 480, timestamp: 123456 };

describe('ScreenshotService', () => {
  let screenshotService: typeof import('@/services/screenshot').default;

  beforeAll(async () => {
    const wsModule = await import('@/services/websocket');
    ({ default: screenshotService } = await import('@/services/screenshot'));
    (global as any).WebSocket = FakeWebSocket;
    wsModule.default.connect();
    FakeWebSocket.lastInstance?.simulateOpen();
  });

  afterAll(() => {
    delete (global as any).WebSocket;
  });

  function pushFrame(payload: unknown = frame) {
    FakeWebSocket.lastInstance!.simulateMessage({ type: 'screenshot', data: payload });
  }

  it('收到 screenshot 消息后缓存并广播，subscribe 立即回放当前帧', () => {
    const listener = jest.fn();
    const unsub = screenshotService.subscribe(listener);

    pushFrame();

    expect(listener).toHaveBeenCalledWith(frame);
    expect(screenshotService.getCurrent()).toEqual(frame);

    // 新订阅者立即拿到当前帧
    const lateListener = jest.fn();
    screenshotService.subscribe(lateListener);
    expect(lateListener).toHaveBeenCalledWith(frame);

    unsub();
  });

  it('退订后不再收到广播', () => {
    const listener = jest.fn();
    const unsub = screenshotService.subscribe(listener);
    unsub();

    pushFrame({ ...frame, timestamp: 999 });

    expect(listener).toHaveBeenCalledTimes(1); // 只有 subscribe 当时的回放
  });

  it('data 为空的消息不更新帧', () => {
    const before = screenshotService.getCurrent();
    pushFrame(null);
    expect(screenshotService.getCurrent()).toBe(before);
  });

  it('监听器抛错不影响其他监听器', () => {
    // 订阅时会立即回放当前帧（未包 try/catch），让监听器从第二次调用才开始抛错
    let badCalls = 0;
    const bad = jest.fn(() => {
      badCalls += 1;
      if (badCalls > 1) throw new Error('listener boom');
    });
    const good = jest.fn();
    const unsubBad = screenshotService.subscribe(bad);
    const unsubGood = screenshotService.subscribe(good);
    const errorSpy = jest.spyOn(console, 'error').mockImplementation(() => {});

    pushFrame({ ...frame, timestamp: 555 });

    expect(bad).toHaveBeenCalled();
    expect(good).toHaveBeenCalledWith({ ...frame, timestamp: 555 });
    expect(errorSpy).toHaveBeenCalledWith('[Screenshot] Listener error:', expect.any(Error));

    errorSpy.mockRestore();
    unsubBad();
    unsubGood();
  });
});
