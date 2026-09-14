/**
 * https 页面下 WebSocket URL 协议选择：
 * constructor 与 connect 各自重建 URL，均应使用 wss:。
 * 通过 @jest-environment-options 将 jsdom 页面 URL 固定为 https。
 *
 * @jest-environment jsdom
 * @jest-environment-options {"url": "https://dashboard.example.com/console"}
 */

class FakeWebSocket {
  static OPEN = 1;
  static lastInstance: FakeWebSocket | null = null;

  readyState = 0;
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  onclose: ((event: { code: number; reason: string }) => void) | null = null;
  onerror: ((error: unknown) => void) | null = null;
  sent: string[] = [];
  close = jest.fn();
  url: string;

  constructor(url: string) {
    this.url = url;
    FakeWebSocket.lastInstance = this;
  }

  send(data: string) {
    this.sent.push(data);
  }
}

describe('https 环境下 WebSocket URL 使用 wss 协议', () => {
  beforeEach(() => {
    (global as any).WebSocket = FakeWebSocket;
  });

  afterEach(() => {
    delete (global as any).WebSocket;
  });

  it('constructor 与 connect 重建 URL 均使用 wss://', async () => {
    const { WebSocketService } = await import('@/services/websocket');

    const service = new WebSocketService();
    expect(service.isConnected()).toBe(false);

    service.connect();
    expect(FakeWebSocket.lastInstance!.url).toBe('wss://dashboard.example.com/ws');

    // disconnect 后再次 connect 走 connect() 内的 URL 重建分支，仍为 wss
    service.disconnect();
    service.connect();
    expect(FakeWebSocket.lastInstance!.url).toBe('wss://dashboard.example.com/ws');
  });
});
