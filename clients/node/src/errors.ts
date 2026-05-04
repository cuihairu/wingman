/**
 * Wingman 错误类
 */

/**
 * Wingman 基础错误
 */
export class WingmanError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'WingmanError';
  }
}

/**
 * 连接错误
 */
export class ConnectionError extends WingmanError {
  constructor(message: string) {
    super(message);
    this.name = 'ConnectionError';
  }
}

/**
 * 请求超时错误
 */
export class RequestTimeout extends WingmanError {
  constructor(message: string) {
    super(message);
    this.name = 'RequestTimeout';
  }
}

/**
 * 请求失败错误
 */
export class RequestFailed extends WingmanError {
  constructor(message: string, public readonly code?: string) {
    super(message);
    this.name = 'RequestFailed';
  }
}

/**
 * 断言连接状态
 */
export function assertConnected(connected: boolean): void {
  if (!connected) {
    throw new ConnectionError('未连接到服务器');
  }
}
