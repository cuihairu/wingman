/**
 * @name WebSocket Service
 * @description Real-time push notification service
 */

// Message types
export enum WSMessageType {
  // System messages
  Connected = 'connected',
  Ping = 'ping',
  Pong = 'pong',
  // Agent events
  AgentConnected = 'agent_connected',
  AgentDisconnected = 'agent_disconnected',
  AgentStatusChanged = 'agent_status_changed',
  // Workflow events
  WorkflowSubmitted = 'workflow_submitted',
  WorkflowStatusChanged = 'workflow_status_changed',
  WorkflowProgress = 'workflow_progress',
}

// WebSocket message
export interface WSMessage {
  type: string;
  event?: string;
  data?: Record<string, unknown>;
  timestamp?: number;
  connectionId?: string;
}

// Message listener types
type MessageListener = (message: WSMessage) => void;
type ConnectionStateListener = (connected: boolean) => void;

class WebSocketService {
  private ws: WebSocket | null = null;
  private url: string;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 5;
  private reconnectDelay = 3000;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null;
  private listeners: Map<string, Set<MessageListener>> = new Map();
  private connectionStateListeners: Set<ConnectionStateListener> = new Set();
  private isManualClose = false;

  constructor() {
    // Build WebSocket URL with JWT token for authentication.
    // Browser WebSocket API cannot set Authorization header, so we pass
    // the token as a query parameter. The server reads c.Query("token").
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    let url = `${protocol}//${host}/ws`;
    const token = localStorage.getItem('token');
    if (token) {
      url += `?token=${encodeURIComponent(token)}`;
    }
    this.url = url;
  }

  connect(): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return;
    }

    // Re-build URL with fresh token on each reconnect
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    let url = `${protocol}//${host}/ws`;
    const token = localStorage.getItem('token');
    if (token) {
      url += `?token=${encodeURIComponent(token)}`;
    }
    this.url = url;

    try {
      this.ws = new WebSocket(this.url);
      this.setupEventHandlers();
    } catch (error) {
      console.error('[WS] Connect error:', error);
      this.scheduleReconnect();
    }
  }

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

  send(type: string, data?: Record<string, unknown>): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      const message: WSMessage = { type, data, timestamp: Date.now() };
      this.ws.send(JSON.stringify(message));
    } else {
      console.warn('[WS] Cannot send message, not connected');
    }
  }

  on(type: string, listener: MessageListener): () => void {
    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    this.listeners.get(type)!.add(listener);

    return () => {
      const listeners = this.listeners.get(type);
      if (listeners) {
        listeners.delete(listener);
      }
    };
  }

  onConnectionState(listener: ConnectionStateListener): () => void {
    this.connectionStateListeners.add(listener);
    return () => {
      this.connectionStateListeners.delete(listener);
    };
  }

  // Agent event subscriptions
  onAgentConnected(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'connected') listener(msg.data ?? {});
    });
  }

  onAgentDisconnected(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'disconnected') listener(msg.data ?? {});
    });
  }

  onAgentStatusChanged(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('agent', (msg) => {
      if (msg.event === 'status_changed') listener(msg.data ?? {});
    });
  }

  // Workflow event subscriptions
  onWorkflowSubmitted(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'submitted') listener(msg.data ?? {});
    });
  }

  onWorkflowStatusChanged(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'status_changed') listener(msg.data ?? {});
    });
  }

  onWorkflowProgress(listener: (data: Record<string, unknown>) => void): () => void {
    return this.on('workflow', (msg) => {
      if (msg.event === 'progress') listener(msg.data ?? {});
    });
  }

  isConnected(): boolean {
    return this.ws !== null && this.ws.readyState === WebSocket.OPEN;
  }

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

  private handleMessage(message: WSMessage): void {
    if (message.type === 'ping') {
      this.send('pong');
      return;
    }

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

  private notifyConnectionState(connected: boolean): void {
    this.connectionStateListeners.forEach((listener) => {
      try {
        listener(connected);
      } catch (error) {
        console.error('[WS] Connection state listener error:', error);
      }
    });
  }

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

  private startHeartbeat(): void {
    this.stopHeartbeat();
    this.heartbeatTimer = setInterval(() => {
      // Server-initiated ping, client responds with pong in handleMessage
    }, 30000);
  }

  private stopHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }
}

const wsService = new WebSocketService();

export default wsService;
export { WebSocketService };
