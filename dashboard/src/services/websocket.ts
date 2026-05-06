/**
 * @name WebSocket 服务
 * @description 实时推送服务
 */

// 消息类型
export enum WSMessageType {
  // 系统消息
  Connected = 'connected',
  Ping = 'ping',
  Pong = 'pong',
  // Agent 事件
  AgentConnected = 'agent_connected',
  AgentDisconnected = 'agent_disconnected',
  AgentStatusChanged = 'agent_status_changed',
  // 工作流事件
  WorkflowSubmitted = 'workflow_submitted',
  WorkflowStatusChanged = 'workflow_status_changed',
  WorkflowProgress = 'workflow_progress',
}

// WebSocket 消息
export interface WSMessage {
  type: string;
  event?: string;
  data?: any;
  timestamp?: number;
  connectionId?: string;
}

// 消息监听器类型
type MessageListener = (message: WSMessage) => void;
type ConnectionStateListener = (connected: boolean) => void;

class WebSocketService {
  private ws: WebSocket | null = null;
  private url: string;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 5;
  private reconnectDelay = 3000;
  private reconnectTimer: NodeJS.Timeout | null = null;
  private heartbeatTimer: NodeJS.Timeout | null = null;
  private listeners: Map<string, Set<MessageListener>> = new Map();
  private connectionStateListeners: Set<ConnectionStateListener> = new Set();
  private isManualClose = false;

  constructor() {
    // 构建 WebSocket URL
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    this.url = `${protocol}//${host}/ws`;
  }

  // 连接
  connect(): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return;
    }

    try {
      this.ws = new WebSocket(this.url);
      this.setupEventHandlers();
    } catch (error) {
      console.error('[WS] Connect error:', error);
      this.scheduleReconnect();
    }
  }

  // 断开连接
  disconnect(): void {
    this.isManualClose = true;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  // 发送消息
  send(type: string, data?: any): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      const message: WSMessage = { type, data, timestamp: Date.now() };
      this.ws.send(JSON.stringify(message));
    } else {
      console.warn('[WS] Cannot send message, not connected');
    }
  }

  // 订阅消息
  on(type: string, listener: MessageListener): () => void {
    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    this.listeners.get(type)!.add(listener);

    // 返回取消订阅函数
    return () => {
      const listeners = this.listeners.get(type);
      if (listeners) {
        listeners.delete(listener);
      }
    };
  }

  // 订阅连接状态变化
  onConnectionState(listener: ConnectionStateListener): () => void {
    this.connectionStateListeners.add(listener);
    return () => {
      this.connectionStateListeners.delete(listener);
    };
  }

  // Agent 事件订阅
  onAgentConnected(listener: (data: any) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'connected') listener(msg.data);
    });
  }

  onAgentDisconnected(listener: (data: any) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'disconnected') listener(msg.data);
    });
  }

  onAgentStatusChanged(listener: (data: any) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'status_changed') listener(msg.data);
    });
  }

  // 工作流事件订阅
  onWorkflowSubmitted(listener: (data: any) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'submitted') listener(msg.data);
    });
  }

  onWorkflowStatusChanged(listener: (data: any) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'status_changed') listener(msg.data);
    });
  }

  onWorkflowProgress(listener: (data: any) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'progress') listener(msg.data);
    });
  }

  // 获取连接状态
  isConnected(): boolean {
    return this.ws !== null && this.ws.readyState === WebSocket.OPEN;
  }

  // 设置事件处理器
  private setupEventHandlers(): void {
    if (!this.ws) return;

    this.ws.onopen = () => {
      console.log('[WS] Connected');
      this.reconnectAttempts = 0;
      this.notifyConnectionState(true);
      this.startHeartbeat();
    };

    this.ws.onmessage = (event) => {
      try {
        const message: WSMessage = JSON.parse(event.data);
        this.handleMessage(message);
      } catch (error) {
        console.error('[WS] Message parse error:', error);
      }
    };

    this.ws.onclose = (event) => {
      console.log('[WS] Disconnected:', event.code, event.reason);
      this.notifyConnectionState(false);
      this.stopHeartbeat();

      if (!this.isManualClose) {
        this.scheduleReconnect();
      }
    };

    this.ws.onerror = (error) => {
      console.error('[WS] Error:', error);
    };
  }

  // 处理消息
  private handleMessage(message: WSMessage): void {
    // 处理 ping
    if (message.type === 'ping') {
      this.send('pong');
      return;
    }

    // 通知类型订阅者
    const typeListeners = this.listeners.get(message.type);
    if (typeListeners) {
      typeListeners.forEach((listener) => {
        try {
          listener(message);
        } catch (error) {
          console.error('[WS] Listener error:', error);
        }
      });
    }

    // 通知所有消息订阅者
    const allListeners = this.listeners.get('*');
    if (allListeners) {
      allListeners.forEach((listener) => {
        try {
          listener(message);
        } catch (error) {
          console.error('[WS] Listener error:', error);
        }
      });
    }
  }

  // 通知连接状态变化
  private notifyConnectionState(connected: boolean): void {
    this.connectionStateListeners.forEach((listener) => {
      try {
        listener(connected);
      } catch (error) {
        console.error('[WS] Connection state listener error:', error);
      }
    });
  }

  // 安排重连
  private scheduleReconnect(): void {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[WS] Max reconnect attempts reached');
      return;
    }

    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
    }

    const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts);
    console.log(`[WS] Reconnecting in ${delay}ms... (attempt ${this.reconnectAttempts + 1}/${this.maxReconnectAttempts})`);

    this.reconnectTimer = setTimeout(() => {
      this.reconnectAttempts++;
      this.connect();
    }, delay);
  }

  // 开始心跳
  private startHeartbeat(): void {
    this.stopHeartbeat();
    this.heartbeatTimer = setInterval(() => {
      if (this.isConnected()) {
        // 心跳由服务器发起 ping，客户端回复 pong
        // 这里不需要主动发送
      }
    }, 30000);
  }

  // 停止心跳
  private stopHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }
}

// 导出单例
const wsService = new WebSocketService();

export default wsService;
export { WSMessage, WSMessageType, WebSocketService };
