import {
  DebugSession,
  InitializedEvent,
  LoggingDebugSession,
  OutputEvent,
  TerminatedEvent,
  StoppedEvent,
} from 'vscode-debugadapter';
import { DebugProtocol } from 'vscode-debugprotocol';
import * as WebSocket from 'ws';

/**
 * Wingman Debug Adapter Protocol 实现
 * 支持断点、变量查看、步进等调试功能
 * 使用 WebSocket 接收调试器事件推送
 */
export class WingmanDebugger extends LoggingDebugSession {
  private static THREAD_ID = 1;
  private serverHost = 'localhost';
  private serverPort = 8080;
  private isConnected = false;
  private breakpoints = new Map<string, DebugProtocol.SourceBreakpoint[]>();
  private ws: WebSocket | null = null;

  protected initializeRequest(
    response: DebugProtocol.InitializeResponse,
    args: DebugProtocol.InitializeRequestArguments
  ): void {
    response.body = response.body || {};

    response.body.supportsConfigurationDoneRequest = true;
    response.body.supportsEvaluateForHovers = false;
    response.body.supportsConditionalBreakpoints = false;
    response.body.supportsHitConditionalBreakpoints = false;
    response.body.supportsStepBack = false;
    response.body.supportsStepInTargetsRequest = false;
    response.body.supportsGotoTargetsRequest = false;
    response.body.supportsCancelRequest = false;
    response.body.supportsCompletionsRequest = false;
    response.body.supportsModulesRequest = false;
    response.body.supportsRestartRequest = false;
    response.body.supportsSetVariable = false;
    response.body.supportsReadMemoryRequest = false;
    response.body.supportsDisassembleRequest = false;
    response.body.supportsTerminateRequest = false;

    this.sendResponse(response);
  }

  protected async launchRequest(
    response: DebugProtocol.LaunchResponse,
    args: any
  ): Promise<void> {
    // 获取服务器配置
    this.serverHost = args.host || 'localhost';
    this.serverPort = args.port || 9527;  // Wingman 默认端口

    try {
      // 连接到 HTTP 服务器
      await this.connectToServer();

      // 建立 WebSocket 连接接收调试器事件
      await this.connectWebSocket();

      this.isConnected = true;
      this.sendResponse(response);
      this.sendEvent(new InitializedEvent());
    } catch (err) {
      this.sendErrorResponse(response, {
        id: 1001,
        format: `连接服务器失败: ${err}`
      });
    }
  }

  protected setBreakPointsRequest(
    response: DebugProtocol.SetBreakpointsResponse,
    args: DebugProtocol.SetBreakpointsArguments
  ): void {
    const path = args.source.path as string;
    const clientBreakpoints = args.breakpoints || [];

    this.breakpoints.set(path, clientBreakpoints);

    // 发送断点到服务器
    this.setBreakpointsOnServer(path, clientBreakpoints)
      .then(() => {
        response.body = {
          breakpoints: clientBreakpoints.map(bp => ({
            verified: true,
            line: bp.line
          }))
        };
        this.sendResponse(response);
      })
      .catch(err => {
        this.sendErrorResponse(response, {
          id: 3001,
          format: `设置断点失败: ${err}`
        });
      });
  }

  protected continueRequest(
    response: DebugProtocol.ContinueResponse,
    args: DebugProtocol.ContinueArguments
  ): void {
    this.sendCommand('continue')
      .then(() => {
        response.body = { allThreadsContinued: true };
        this.sendResponse(response);
      })
      .catch(err => {
        this.sendErrorResponse(response, {
          id: 2001,
          format: `继续执行失败: ${err}`
        });
      });
  }

  protected nextRequest(
    response: DebugProtocol.NextResponse,
    args: DebugProtocol.NextArguments
  ): void {
    this.sendCommand('stepOver')
      .then(() => this.sendResponse(response))
      .catch(err => this.sendErrorResponse(response, {
        id: 2002,
        format: `单步失败: ${err}`
      }));
  }

  protected stepInRequest(
    response: DebugProtocol.StepInResponse,
    args: DebugProtocol.StepInArguments
  ): void {
    this.sendCommand('stepIn')
      .then(() => this.sendResponse(response))
      .catch(err => this.sendErrorResponse(response, {
        id: 2003,
        format: `步入失败: ${err}`
      }));
  }

  protected stepOutRequest(
    response: DebugProtocol.StepOutResponse,
    args: DebugProtocol.StepOutArguments
  ): void {
    this.sendCommand('stepOut')
      .then(() => this.sendResponse(response))
      .catch(err => this.sendErrorResponse(response, {
        id: 2004,
        format: `步出失败: ${err}`
      }));
  }

  protected pauseRequest(
    response: DebugProtocol.PauseResponse,
    args: DebugProtocol.PauseArguments
  ): void {
    this.sendCommand('pause')
      .then(() => this.sendResponse(response))
      .catch(err => this.sendErrorResponse(response, {
        id: 2005,
        format: `暂停失败: ${err}`
      }));
  }

  protected threadsRequest(
    response: DebugProtocol.ThreadsResponse
  ): void {
    response.body = {
      threads: [
        {
          id: WingmanDebugger.THREAD_ID,
          name: 'Main Thread'
        }
      ]
    };
    this.sendResponse(response);
  }

  protected stackTraceRequest(
    response: DebugProtocol.StackTraceResponse,
    args: DebugProtocol.StackTraceArguments
  ): void {
    this.getStackTrace()
      .then(frames => {
        response.body = {
          stackFrames: frames.map((f: any, i: number) => ({
            id: i,
            name: f.name,
            source: { name: f.source, path: f.source },
            line: f.line,
            column: 0
          })),
          totalFrames: frames.length
        };
        this.sendResponse(response);
      })
      .catch(err => this.sendErrorResponse(response, {
        id: 3001,
        format: `获取堆栈失败: ${err}`
      }));
  }

  protected scopesRequest(
    response: DebugProtocol.ScopesResponse,
    args: DebugProtocol.ScopesArguments
  ): void {
    response.body = {
      scopes: [
        {
          name: 'Locals',
          variablesReference: 1,
          expensive: false
        },
        {
          name: 'Globals',
          variablesReference: 2,
          expensive: false
        }
      ]
    };
    this.sendResponse(response);
  }

  protected variablesRequest(
    response: DebugProtocol.VariablesResponse,
    args: DebugProtocol.VariablesArguments
  ): void {
    this.getVariables(args.variablesReference)
      .then((variables: any[]) => {
        response.body = {
          variables: variables.map((v: any) => ({
            name: v.name,
            value: v.value,
            type: v.type,
            variablesReference: v.variablesReference || 0
          }))
        };
        this.sendResponse(response);
      })
      .catch(err => this.sendErrorResponse(response, {
        id: 4001,
        format: `获取变量失败: ${err}`
      }));
  }

  protected disconnectRequest(
    response: DebugProtocol.DisconnectResponse,
    args: DebugProtocol.DisconnectArguments
  ): void {
    // 关闭 WebSocket 连接
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.isConnected = false;
    this.sendResponse(response);
  }

  // ========== 私有方法 ==========

  private async connectToServer(): Promise<void> {
    const url = `http://${this.serverHost}:${this.serverPort}/api/debugger/connect`;
    const response = await fetch(url, { method: 'POST' });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
  }

  private async connectWebSocket(): Promise<void> {
    return new Promise((resolve, reject) => {
      const wsUrl = `ws://${this.serverHost}:${this.serverPort}/ws`;
      this.ws = new WebSocket(wsUrl);

      this.ws.on('open', () => {
        console.log('[Wingman] WebSocket connected');
        resolve();
      });

      this.ws.on('error', (err) => {
        console.error('[Wingman] WebSocket error:', err);
        reject(err);
      });

      this.ws.on('message', (data: Buffer) => {
        try {
          const message = JSON.parse(data.toString());
          this.handleWebSocketMessage(message);
        } catch (err) {
          console.error('[Wingman] Failed to parse WebSocket message:', err);
        }
      });

      this.ws.on('close', () => {
        console.log('[Wingman] WebSocket disconnected');
      });
    });
  }

  private handleWebSocketMessage(message: any): void {
    if (message.type === 'debugger') {
      switch (message.event) {
        case 'stopped':
          // 调试器停止（断点或步进）
          this.sendEvent(new StoppedEvent(message.data.reason, WingmanDebugger.THREAD_ID));
          break;
        case 'paused':
          this.sendEvent(new StoppedEvent('pause', WingmanDebugger.THREAD_ID));
          break;
        case 'error':
          this.sendEvent(new OutputEvent(`调试器错误: ${message.data}\n`));
          break;
      }
    }
  }

  private async sendCommand(command: string): Promise<any> {
    if (!this.isConnected) {
      throw new Error('未连接到服务器');
    }
    const url = `http://${this.serverHost}:${this.serverPort}/api/debugger/command`;
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command })
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    return response.json();
  }

  private async setBreakpointsOnServer(path: string, breakpoints: DebugProtocol.SourceBreakpoint[]): Promise<void> {
    const url = `http://${this.serverHost}:${this.serverPort}/api/debugger/breakpoints`;
    await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ file: path, breakpoints })
    });
  }

  private async getStackTrace(): Promise<any[]> {
    const url = `http://${this.serverHost}:${this.serverPort}/api/debugger/stacktrace`;
    const response = await fetch(url);
    const json = await response.json();
    return json.stackFrames || [];
  }

  private async getVariables(ref: number): Promise<any[]> {
    const url = `http://${this.serverHost}:${this.serverPort}/api/debugger/variables`;
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ reference: ref })
    });
    const json = await response.json();
    return json.variables || [];
  }
}
