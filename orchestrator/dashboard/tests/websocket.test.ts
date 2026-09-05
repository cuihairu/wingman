/**
 * WebSocketService 单测：
 * - trigger 事件分发（server 广播 type="agent" + event="trigger_fired"）
 * - server ping → client pong 应答
 * - 心跳死链检测：超过 2 个 ping 周期无下行消息时主动断开触发重连
 */

class FakeWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;

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

  constructor(url: string) {
    this.url = url;
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

type Fixture = {
  service: import('@/services/websocket').WebSocketService;
  socket: FakeWebSocket;
};

let WebSocketServiceClass: typeof import('@/services/websocket').WebSocketService;

describe('WebSocketService', () => {
  beforeAll(async () => {
    ({ WebSocketService } = await import('@/services/websocket'));
    WebSocketServiceClass = WebSocketService;
  });

  beforeEach(() => {
    jest.useFakeTimers();
    (global as any).WebSocket = FakeWebSocket;
  });

  afterEach(() => {
    jest.useRealTimers();
    delete (global as any).WebSocket;
  });

  function createConnected(): Fixture {
    const service = new WebSocketServiceClass();
    service.connect();
    const socket = (service as unknown as { ws: FakeWebSocket }).ws;
    expect(socket).toBeDefined();
    socket.simulateOpen();
    return { service, socket };
  }

  it('onTriggerFired 分发展平后的 runtime 载荷', () => {
    const { service, socket } = createConnected();
    const listener = jest.fn();

    service.onTriggerFired(listener);
    socket.simulateMessage({
      type: 'agent',
      event: 'trigger_fired',
      data: { agentId: 'agent-1', data: { id: 't1', name: '捡拾触发器', type: 'image' } },
    });

    expect(listener).toHaveBeenCalledTimes(1);
    expect(listener).toHaveBeenCalledWith(
      expect.objectContaining({ id: 't1', name: '捡拾触发器', type: 'image', agentId: 'agent-1' }),
    );
  });

  it('onTriggerFired 忽略非 trigger_fired 的 agent 事件', () => {
    const { service, socket } = createConnected();
    const listener = jest.fn();

    service.onTriggerFired(listener);
    socket.simulateMessage({ type: 'agent', event: 'connected', data: { agentId: 'a1' } });
    socket.simulateMessage({ type: 'agent', event: 'status_changed', data: { agentId: 'a1' } });

    expect(listener).not.toHaveBeenCalled();
  });

  it('onAgentConnected 按 type=agent + event=connected 分发（回归保护）', () => {
    const { service, socket } = createConnected();
    const listener = jest.fn();

    service.onAgentConnected(listener);
    socket.simulateMessage({ type: 'agent', event: 'connected', data: { agentId: 'a1' } });

    expect(listener).toHaveBeenCalledWith({ agentId: 'a1' });
  });

  it('收到 server ping 时以 pong 应答', () => {
    const { socket } = createConnected();

    socket.simulateMessage({ type: 'ping' });

    const pong = socket.sent.find((raw) => raw.includes('"pong"'));
    expect(pong).toBeDefined();
  });

  it('心跳死链检测：链路活跃时不断开，静默超过阈值后主动 close', () => {
    const { socket } = createConnected();

    // 第一个 30s tick：刚活动过，不断开
    jest.advanceTimersByTime(30000);
    expect(socket.close).not.toHaveBeenCalled();

    // 持续有下行消息时永不判死
    for (let i = 0; i < 4; i += 1) {
      socket.simulateMessage({ type: 'script', event: 'output', data: { message: 'x' } });
      jest.advanceTimersByTime(30000);
      expect(socket.close).not.toHaveBeenCalled();
    }

    // 静默跨过 75s 阈值（两个 30s ping 周期 + 余量）
    jest.advanceTimersByTime(30000);
    jest.advanceTimersByTime(30000);
    jest.advanceTimersByTime(30000);
    expect(socket.close).toHaveBeenCalledTimes(1);
  });
});
