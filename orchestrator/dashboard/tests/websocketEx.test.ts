/**
 * WebSocketService 分支补全：token URL、重复 connect 幂等、构造异常重连、
 * disconnect/reconnect 语义、未连接 send 告警、通配符监听、
 * workflow 事件两种消息形态、监听器异常隔离、重连预算耗尽。
 */

class FakeWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;
  static lastInstance: FakeWebSocket | null = null;

  readyState = FakeWebSocket.CONNECTING;
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  onclose: ((event: { code: number; reason: string }) => void) | null = null;
  onerror: ((error: unknown) => void) | null = null;
  sent: string[] = [];
  close = jest.fn(() => {
    if (this.readyState === FakeWebSocket.CLOSED) return;
    this.readyState = FakeWebSocket.CLOSED;
    this.onclose?.({ code: 1000, reason: '' });
  });
  url: string;
  throwOnConstruct = false;

  constructor(url: string) {
    this.url = url;
    if (this.throwOnConstruct) {
      throw new Error('ws construct failed');
    }
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

  simulateRaw(data: string) {
    this.onmessage?.({ data });
  }
}

let WebSocketService: typeof import('@/services/websocket').WebSocketService;

describe('WebSocketService 分支补全', () => {
  beforeAll(async () => {
    ({ WebSocketService } = await import('@/services/websocket'));
  });

  beforeEach(() => {
    jest.useFakeTimers();
    (global as any).WebSocket = FakeWebSocket;
    jest.spyOn(console, 'warn').mockImplementation(() => {});
    jest.spyOn(console, 'error').mockImplementation(() => {});
    jest.spyOn(console, 'log').mockImplementation(() => {});
  });

  afterEach(() => {
    jest.useRealTimers();
    jest.restoreAllMocks();
    delete (global as any).WebSocket;
  });

  it('constructor/connect 使用 localStorage token 组装 URL（编码后拼接）', () => {
    (window.localStorage.getItem as jest.Mock).mockImplementation((key: string) =>
      key === 'token' ? 'abc 123' : null,
    );
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    expect(socket.url).toContain('ws://localhost:8000/ws');
    expect(socket.url).toContain('token=abc%20123');
  });

  it('连接已 OPEN 时重复 connect 直接返回', () => {
    const service = new WebSocketService();
    service.connect();
    const first = FakeWebSocket.lastInstance!;
    first.simulateOpen();
    service.connect();
    expect(FakeWebSocket.lastInstance).toBe(first);
  });

  it('WebSocket 构造异常时进入重连排队', () => {
    (global as any).WebSocket = function () {
      throw new Error('construct failed');
    };

    const service = new WebSocketService();
    service.connect();
    expect(console.error).toHaveBeenCalledWith('[WS] Connect error:', expect.any(Error));

    (global as any).WebSocket = FakeWebSocket;
    // 到期后重试成功
    jest.advanceTimersByTime(3000);
    expect(FakeWebSocket.lastInstance).toBeInstanceOf(FakeWebSocket);
  });

  it('未连接 send 打印警告不抛错', () => {
    const service = new WebSocketService();
    expect(() => service.send('ping', { a: 1 })).not.toThrow();
    expect(console.warn).toHaveBeenCalledWith('[WS] Cannot send message, not connected');
  });

  it('connected send 携带 type/data/timestamp', () => {
    jest.spyOn(Date, 'now').mockReturnValue(12345);
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();
    service.send('hello', { x: 1 });

    expect(JSON.parse(socket.sent[0])).toEqual({
      type: 'hello',
      data: { x: 1 },
      timestamp: 12345,
    });
    jest.restoreAllMocks();
  });

  it('通配符监听抛错同样被隔离', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const bad = jest.fn(() => {
      throw new Error('wildcard boom');
    });
    const after = jest.fn();
    service.on('*', bad);
    service.on('*', after);

    socket.simulateMessage({ type: 'anything', data: {} });

    expect(after).toHaveBeenCalledTimes(1);
    expect(console.error).toHaveBeenCalledWith('[WS] Listener error:', expect.any(Error));
  });

  it('无通配符监听时消息正常分发不报错', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const agentListener = jest.fn();
    service.on('agent', agentListener);
    // 故意不注册 '*' 监听
    socket.simulateMessage({ type: 'agent', event: 'connected', data: { agentId: 'a' } });

    expect(agentListener).toHaveBeenCalledTimes(1);
  });

  it('on() 返回的退订函数移除监听', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const listener = jest.fn();
    const unsub = service.on('agent', listener);
    unsub();
    socket.simulateMessage({ type: 'agent', event: 'connected', data: { agentId: 'a' } });

    expect(listener).not.toHaveBeenCalled();
  });

  it('通配符 * 监听接收所有消息，监听器抛错被隔离', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const bad = jest.fn(() => {
      throw new Error('listener boom');
    });
    const good = jest.fn();
    service.on('agent', bad);
    service.on('*', good);

    socket.simulateMessage({ type: 'agent', event: 'connected', data: {} });

    expect(good).toHaveBeenCalledTimes(1);
    expect(console.error).toHaveBeenCalledWith('[WS] Listener error:', expect.any(Error));
  });

  it('onConnectionState 通知与退订，监听器抛错被捕获', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;

    const states: boolean[] = [];
    const bad = jest.fn(() => {
      throw new Error('state boom');
    });
    const unsubGood = service.onConnectionState((c) => states.push(c));
    service.onConnectionState(bad);

    socket.simulateOpen();
    expect(states).toEqual([true]);
    expect(console.error).toHaveBeenCalledWith(
      '[WS] Connection state listener error:',
      expect.any(Error),
    );

    unsubGood();
    socket.close();
    expect(states).toEqual([true]);
  });

  it('workflow 事件：message.event 顶层形态与 data.event 嵌套形态', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const submitted = jest.fn();
    const statusChanged = jest.fn();
    const progress = jest.fn();
    service.onWorkflowSubmitted(submitted);
    service.onWorkflowStatusChanged(statusChanged);
    service.onWorkflowProgress(progress);

    // 顶层 event
    socket.simulateMessage({ type: 'workflow', event: 'submitted', data: { id: 'w1' } });
    // 嵌套 event（data.data 为载荷）
    socket.simulateMessage({
      type: 'workflow',
      data: { event: 'status_changed', data: { id: 'w1', status: 'running' } },
    });
    // 嵌套 event（data.data 非对象时回退为整个 data）
    socket.simulateMessage({ type: 'workflow', data: { event: 'progress' } });
    // 非目标事件不分发
    socket.simulateMessage({ type: 'workflow', event: 'finished', data: {} });

    expect(submitted).toHaveBeenCalledWith({ id: 'w1' });
    expect(statusChanged).toHaveBeenCalledWith({ id: 'w1', status: 'running' });
    expect(progress).toHaveBeenCalledWith({ event: 'progress' });
    expect(submitted).toHaveBeenCalledTimes(1);
  });

  it('onAgentDisconnected / onAgentStatusChanged / isConnected', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    expect(service.isConnected()).toBe(false);

    socket.simulateOpen();
    expect(service.isConnected()).toBe(true);

    const disconnected = jest.fn();
    const statusChanged = jest.fn();
    service.onAgentDisconnected(disconnected);
    service.onAgentStatusChanged(statusChanged);

    socket.simulateMessage({ type: 'agent', event: 'disconnected', data: { agentId: 'a1' } });
    socket.simulateMessage({ type: 'agent', event: 'status_changed', data: { agentId: 'a1' } });
    socket.simulateMessage({ type: 'agent', data: 'weird' });

    expect(disconnected).toHaveBeenCalledWith({ agentId: 'a1' });
    expect(statusChanged).toHaveBeenCalledWith({ agentId: 'a1' });
  });

  it('agent 事件缺 data 时回退空对象', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const disconnected = jest.fn();
    const statusChanged = jest.fn();
    const trigger = jest.fn();
    service.onAgentDisconnected(disconnected);
    service.onAgentStatusChanged(statusChanged);
    service.onTriggerFired(trigger);

    socket.simulateMessage({ type: 'agent', event: 'disconnected' });
    socket.simulateMessage({ type: 'agent', event: 'status_changed' });
    socket.simulateMessage({ type: 'agent', event: 'trigger_fired', data: { agentId: 'a9' } });

    expect(disconnected).toHaveBeenCalledWith({});
    expect(statusChanged).toHaveBeenCalledWith({});
    expect(trigger).toHaveBeenCalledWith({ agentId: 'a9' });
  });

  it('workflow 事件缺 data 时回退空对象', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    const progress = jest.fn();
    service.onWorkflowProgress(progress);
    socket.simulateMessage({ type: 'workflow', event: 'progress' });

    expect(progress).toHaveBeenCalledWith({});
  });

  it('非法 JSON 消息触发 parse error 日志', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();
    socket.simulateRaw('{not json');

    expect(console.error).toHaveBeenCalledWith('[WS] Message parse error:', expect.any(Error));
  });

  it('onerror 打印错误日志', () => {
    const service = new WebSocketService();
    service.connect();
    FakeWebSocket.lastInstance!.simulateOpen();
    FakeWebSocket.lastInstance!.onerror?.('net-fail');

    expect(console.error).toHaveBeenCalledWith('[WS] Error:', 'net-fail');
  });

  it('disconnect 手动关闭不触发自动重连', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    service.disconnect();
    expect(socket.close).toHaveBeenCalled();
    expect(service.isConnected()).toBe(false);

    jest.advanceTimersByTime(60000);
    expect(FakeWebSocket.lastInstance).toBe(socket);
  });

  it('disconnect 在重连排队期间调用可取消重连', () => {
    const service = new WebSocketService();
    service.connect();
    const socket = FakeWebSocket.lastInstance!;
    socket.simulateOpen();

    // 触发重连排队
    socket.close();
    jest.advanceTimersByTime(1000); // 未到 3s，仍在排队
    service.disconnect();
    jest.advanceTimersByTime(60000);
    expect(FakeWebSocket.lastInstance).toBe(socket);
  });

  it('reconnect() 在无连接时也可直接建连', () => {
    const service = new WebSocketService();
    service.reconnect();
    expect(FakeWebSocket.lastInstance).toBeInstanceOf(FakeWebSocket);
  });

  it('reconnect() 中旧连接 close 抛错被忽略', () => {
    const service = new WebSocketService();
    service.connect();
    const first = FakeWebSocket.lastInstance!;
    first.simulateOpen();
    first.close.mockImplementationOnce(() => {
      throw new Error('close failed');
    });

    expect(() => service.reconnect()).not.toThrow();
    expect(FakeWebSocket.lastInstance).not.toBe(first);
  });

  it('reconnect() 重置退避并重建连接', () => {
    const service = new WebSocketService();
    service.connect();
    const first = FakeWebSocket.lastInstance!;
    first.simulateOpen();

    // 触发一次自动重连排队
    first.close();
    jest.advanceTimersByTime(3000);
    const second = FakeWebSocket.lastInstance!;

    service.reconnect();
    expect(second.close).toHaveBeenCalled();
    // reconnect 后重新建连
    const third = FakeWebSocket.lastInstance!;
    expect(third).not.toBe(second);
  });

  it('异常关闭后自动重连：成功 onopen 会重置重连预算', () => {
    const service = new WebSocketService();
    service.connect();
    const first = FakeWebSocket.lastInstance!;
    first.simulateOpen();

    // 反复异常关闭 → 每次都会重连（onopen 已重置 attempts，预算不清零）
    for (let i = 0; i < 3; i += 1) {
      const before = FakeWebSocket.lastInstance!;
      before.close();
      jest.advanceTimersByTime(3000);
      expect(FakeWebSocket.lastInstance).not.toBe(before);
      FakeWebSocket.lastInstance!.simulateOpen();
    }
  });

  it('连续连接失败会耗尽重连预算并停止重试', () => {
    let constructed = 0;
    (global as any).WebSocket = class {
      constructor() {
        constructed += 1;
        throw new Error('always fails');
      }
    };

    const service = new WebSocketService();
    service.connect();
    expect(console.error).toHaveBeenCalledWith('[WS] Connect error:', expect.any(Error));

    // 指数退避：3s → 6s → 12s → 24s → 48s，第 6 次连接后到达预算上限
    jest.advanceTimersByTime(100000);
    expect(constructed).toBe(6);
    expect(console.error).toHaveBeenCalledWith('[WS] Max reconnect attempts reached');

    // 预算耗尽后不再重试
    jest.advanceTimersByTime(600000);
    expect(constructed).toBe(6);
  });
});
