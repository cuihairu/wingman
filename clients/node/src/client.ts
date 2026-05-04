/**
 * Wingman Node.js Client
 */

import * as net from 'net';
import { EventEmitter } from 'eventemitter3';
import {
  RequestMessage,
  ResponseMessage,
  EventMessage,
  ClientConfig,
  Response,
} from './types';
import {
  WingmanError,
  ConnectionError,
  RequestTimeout,
  RequestFailed,
  assertConnected,
} from './errors';

/**
 * Wingman 客户端类
 */
export class WingmanClient extends EventEmitter {
  private readonly host: string;
  private readonly port: number;
  private readonly timeout: number;
  private readonly autoReconnect: boolean;
  private readonly reconnectInterval: number;

  private socket: net.Socket | null = null;
  private connected = false;
  private requestId = 0;
  private pendingRequests = new Map<number, {
    resolve: (data: any) => void;
    reject: (error: Error) => void;
    timeout: NodeJS.Timeout;
  }>();
  private reconnectTimer: NodeJS.Timeout | null = null;
  private receiveBuffer = Buffer.alloc(0);
  private expectedLength: number | null = null;

  // 模块访问器
  readonly screen = new ScreenModule(this);
  readonly input = new InputModule(this);
  readonly window = new WindowModule(this);
  readonly process = new ProcessModule(this);
  readonly system = new SystemModule(this);
  readonly http = new HttpMethod(this);
  readonly json = new JsonModule(this);
  readonly kv = new KvModule(this);

  constructor(config: ClientConfig = {}) {
    super();
    this.host = config.host || 'localhost';
    this.port = config.port || 8080;
    this.timeout = config.timeout || 30000;
    this.autoReconnect = config.autoReconnect ?? true;
    this.reconnectInterval = config.reconnectInterval || 5000;
  }

  /**
   * 连接到服务器
   */
  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket = new net.Socket();
      this.socket.setNoDelay(true);

      this.socket.on('connect', () => {
        this.connected = true;
        this.receiveBuffer = Buffer.alloc(0);
        this.expectedLength = null;
        this.emit('connected');
        resolve();
      });

      this.socket.on('data', (data: Buffer) => {
        this.handleData(data);
      });

      this.socket.on('close', () => {
        this.handleDisconnect();
      });

      this.socket.on('error', (err) => {
        this.emit('error', err);
        if (!this.connected) {
          reject(new ConnectionError(`连接失败: ${err.message}`));
        }
      });

      this.socket.connect(this.port, this.host);
    });
  }

  /**
   * 断开连接
   */
  disconnect(): void {
    this.connected = false;
    this.autoReconnect = false;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }
    // 清理待处理请求
    for (const pending of this.pendingRequests.values()) {
      pending.reject(new ConnectionError('连接已断开'));
      clearTimeout(pending.timeout);
    }
    this.pendingRequests.clear();
  }

  /**
   * 检查是否已连接
   */
  isConnected(): boolean {
    return this.connected;
  }

  /**
   * 发送请求
   */
  async sendRequest<T = any>(method: string, params: Record<string, any> = {}): Promise<T> {
    assertConnected(this.connected);

    const requestId = ++this.requestId;
    const message: RequestMessage = { id: requestId, method, params };

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingRequests.delete(requestId);
        reject(new RequestTimeout(`请求超时: ${method}`));
      }, this.timeout);

      this.pendingRequests.set(requestId, { resolve, reject, timeout });
      this.sendMessage(message);
    });
  }

  /**
   * 发送消息
   */
  private sendMessage(message: RequestMessage): void {
    if (!this.socket) {
      throw new ConnectionError('Socket 未初始化');
    }

    const data = JSON.stringify(message);
    const buffer = Buffer.from(data, 'utf-8');
    const length = Buffer.alloc(4);
    length.writeUInt32BE(buffer.length, 0);

    this.socket.write(Buffer.concat([length, buffer]));
  }

  /**
   * 处理接收数据
   */
  private handleData(data: Buffer): void {
    this.receiveBuffer = Buffer.concat([this.receiveBuffer, data]);

    while (true) {
      // 读取长度头
      if (this.expectedLength === null) {
        if (this.receiveBuffer.length < 4) {
          break;
        }
        this.expectedLength = this.receiveBuffer.readUInt32BE(0);
        this.receiveBuffer = this.receiveBuffer.slice(4);
      }

      // 读取消息体
      if (this.receiveBuffer.length < this.expectedLength) {
        break;
      }

      const messageData = this.receiveBuffer.slice(0, this.expectedLength);
      this.receiveBuffer = this.receiveBuffer.slice(this.expectedLength);
      this.expectedLength = null;

      try {
        const message = JSON.parse(messageData.toString('utf-8'));
        this.handleMessage(message);
      } catch (err) {
        this.emit('error', new WingmanError(`解析消息失败: ${err}`));
      }
    }
  }

  /**
   * 处理消息
   */
  private handleMessage(message: ResponseMessage | EventMessage): void {
    if ('id' in message) {
      // 响应消息
      const pending = this.pendingRequests.get(message.id);
      if (pending) {
        this.pendingRequests.delete(message.id);
        clearTimeout(pending.timeout);

        if (message.success) {
          pending.resolve(message.data);
        } else {
          pending.reject(new RequestFailed(message.error || '请求失败'));
        }
      }
    } else if ('event' in message) {
      // 事件消息
      this.emit(message.event, message.data);
    }
  }

  /**
   * 处理断开连接
   */
  private handleDisconnect(): void {
    const wasConnected = this.connected;
    this.connected = false;
    this.socket = null;
    this.receiveBuffer = Buffer.alloc(0);
    this.expectedLength = null;

    // 清理待处理请求
    for (const pending of this.pendingRequests.values()) {
      pending.reject(new ConnectionError('连接已断开'));
      clearTimeout(pending.timeout);
    }
    this.pendingRequests.clear();

    if (wasConnected) {
      this.emit('disconnected');

      // 自动重连
      if (this.autoReconnect) {
        this.scheduleReconnect();
      }
    }
  }

  /**
   * 安排重连
   */
  private scheduleReconnect(): void {
    if (this.reconnectTimer) {
      return;
    }

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      if (!this.connected && this.autoReconnect) {
        this.emit('reconnecting');
        this.connect().catch(() => {
          // 重连失败，继续尝试
        });
      }
    }, this.reconnectInterval);
  }
}

/**
 * API 模块基类
 */
abstract class Module {
  constructor(protected readonly client: WingmanClient) {}

  protected call<T = any>(method: string, params: Record<string, any> = {}): Promise<T> {
    return this.client.sendRequest<T>(method, params);
  }
}

/**
 * 屏幕操作模块
 */
export class ScreenModule extends Module {
  getSize(): Promise<Size> {
    return this.call('screen.get_size');
  }

  capture(x = 0, y = 0, width = 0, height = 0): Promise<Buffer> {
    return this.call('screen.capture', { x, y, width, height }).then(data => {
      return Buffer.from(data.data || '', 'base64');
    });
  }

  getPixel(x: number, y: number): Promise<Color> {
    return this.call('screen.get_pixel', { x, y });
  }

  findColor(color: number, x1: number, y1: number, x2: number, y2: number, tolerance = 0): Promise<Point | null> {
    return this.call('screen.find_color', { color, x1, y1, x2, y2, tolerance });
  }

  findColors(color: number, x1: number, y1: number, x2: number, y2: number, tolerance = 0, count = 100): Promise<Point[]> {
    return this.call('screen.find_colors', { color, x1, y1, x2, y2, tolerance, count }) || [];
  }

  findImage(imagePath: string, x1: number, y1: number, x2: number, y2: number, threshold = 0.9): Promise<Point | null> {
    const fs = require('fs');
    const imageData = fs.readFileSync(imagePath).toString('base64');
    return this.call('screen.find_image', { image: imageData, x1, y1, x2, y2, threshold });
  }
}

/**
 * 输入模拟模块
 */
export class InputModule extends Module {
  getMousePosition(): Promise<Point> {
    return this.call('input.get_mouse_position');
  }

  click(x: number, y: number, button = 0): Promise<void> {
    return this.call('input.click', { x, y, button });
  }

  move(x: number, y: number, duration = 0): Promise<void> {
    return this.call('input.move', { x, y, duration });
  }

  scroll(x: number, y: number, delta: number): Promise<void> {
    return this.call('input.scroll', { x, y, delta });
  }

  keyDown(key: number): Promise<void> {
    return this.call('input.key_down', { key });
  }

  keyUp(key: number): Promise<void> {
    return this.call('input.key_up', { key });
  }

  keyPress(key: number): Promise<void> {
    return this.call('input.key_press', { key });
  }

  typeText(text: string, delay = 0): Promise<void> {
    return this.call('input.type', { text, delay });
  }

  isKeyDown(key: number): Promise<boolean> {
    return this.call('input.is_key_down', { key });
  }

  randomDelay(minMs: number, maxMs: number): Promise<void> {
    return this.call('input.random_delay', { min: minMs, max: maxMs });
  }
}

/**
 * 窗口管理模块
 */
export class WindowModule extends Module {
  getForeground(): Promise<number | null> {
    return this.call('window.get_foreground');
  }

  find(title: string): Promise<number | null> {
    return this.call('window.find', { title });
  }

  activate(hwnd: number): Promise<boolean> {
    return this.call('window.activate', { hwnd });
  }

  getTitle(hwnd: number): Promise<string | null> {
    return this.call('window.get_title', { hwnd });
  }

  getBounds(hwnd: number): Promise<Bounds | null> {
    return this.call('window.get_bounds', { hwnd });
  }

  setBounds(hwnd: number, x: number, y: number, width: number, height: number): Promise<boolean> {
    return this.call('window.set_bounds', { hwnd, x, y, width, height });
  }

  waitFor(title: string, timeout = 10000): Promise<number | null> {
    return this.call('window.wait_for', { title, timeout });
  }
}

/**
 * 进程管理模块
 */
export class ProcessModule extends Module {
  find(name: string): Promise<number | null> {
    return this.call('process.find', { name });
  }

  start(path: string, args = '', workingDir = ''): Promise<number | null> {
    return this.call('process.start', { path, args, workingDir });
  }

  wait(pid: number, timeout = 30000): Promise<boolean> {
    return this.call('process.wait', { pid, timeout });
  }

  terminate(pid: number, force = false): Promise<boolean> {
    return this.call('process.terminate', { pid, force });
  }

  exists(pid: number): Promise<boolean> {
    return this.call('process.exists', { pid });
  }

  getPath(pid: number): Promise<string | null> {
    return this.call('process.get_path', { pid });
  }
}

/**
 * 系统信息模块
 */
export class SystemModule extends Module {
  getCpuInfo(): Promise<CpuInfo> {
    return this.call('system.get_cpu_info');
  }

  getMemoryInfo(): Promise<MemoryInfo> {
    return this.call('system.get_memory_info');
  }

  getDiskInfo(path = ''): Promise<DiskInfo> {
    return this.call('system.get_disk_info', { path });
  }

  getGpuInfo(): Promise<GpuInfo[]> {
    return this.call('system.get_gpu_info') || [];
  }

  getOsInfo(): Promise<OsInfo> {
    return this.call('system.get_os_info');
  }

  getNetworkAdapters(): Promise<NetworkAdapter[]> {
    return this.call('system.get_network_adapters') || [];
  }

  getDisplayInfo(): Promise<DisplayInfo[]> {
    return this.call('system.get_display_info') || [];
  }

  getUptime(): Promise<number> {
    return this.call('system.get_uptime');
  }

  getDateTime(): Promise<{ year: number; month: number; day: number; hour: number; minute: number; second: number }> {
    return this.call('system.get_date_time');
  }

  getProcessCount(): Promise<number> {
    return this.call('system.get_process_count');
  }

  getThreadCount(): Promise<number> {
    return this.call('system.get_thread_count');
  }
}

/**
 * HTTP 模块
 */
export class HttpMethod extends Module {
  get(url: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
    return this.call('http.get', { url, headers });
  }

  post(url: string, body: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
    return this.call('http.post', { url, body, headers });
  }

  put(url: string, body: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
    return this.call('http.put', { url, body, headers });
  }

  delete(url: string, headers: Record<string, string> = {}): Promise<HttpResponse> {
    return this.call('http.delete', { url, headers });
  }
}

/**
 * JSON 模块
 */
export class JsonModule extends Module {
  encode(obj: any): Promise<string> {
    return this.call('json.encode', { data: JSON.stringify(obj) });
  }

  decode<T = any>(data: string): Promise<T> {
    return this.call('json.decode', { data }).then(result => JSON.parse(result));
  }
}

/**
 * KV 存储模块
 */
export class KvModule extends Module {
  set(key: string, value: string): Promise<void> {
    return this.call('kv.set', { key, value });
  }

  get(key: string): Promise<string | null> {
    return this.call('kv.get', { key });
  }

  delete(...keys: string[]): Promise<void> {
    return this.call('kv.delete', { keys });
  }

  exists(key: string): Promise<boolean> {
    return this.call('kv.exists', { key });
  }

  incr(key: string, by = 1): Promise<number> {
    return this.call('kv.incr', { key, by });
  }

  decr(key: string, by = 1): Promise<number> {
    return this.call('kv.decr', { key, by });
  }

  hset(hash: string, field: string, value: string): Promise<void> {
    return this.call('kv.hset', { hash, field, value });
  }

  hget(hash: string, field: string): Promise<string | null> {
    return this.call('kv.hget', { hash, field });
  }

  hgetall(hash: string): Promise<Record<string, string>> {
    return this.call('kv.hgetall', { hash }) || {};
  }

  hdel(hash: string, ...fields: string[]): Promise<void> {
    return this.call('kv.hdel', { hash, fields });
  }

  lpush(list: string, ...values: string[]): Promise<number> {
    return this.call('kv.lpush', { list, values });
  }

  lpop(list: string): Promise<string | null> {
    return this.call('kv.lpop', { list });
  }

  rpush(list: string, ...values: string[]): Promise<number> {
    return this.call('kv.rpush', { list, values });
  }

  rpop(list: string): Promise<string | null> {
    return this.call('kv.rpop', { list });
  }

  llen(list: string): Promise<number> {
    return this.call('kv.llen', { list });
  }
}

// 重新导出类型
export * from './types';
export * from './errors';
