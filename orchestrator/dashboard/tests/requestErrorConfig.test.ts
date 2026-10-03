/**
 * requestErrorConfig：429 限流文案（带服务端剩余分钟数）、登录 401 全局静默
 * （由登录页自行提示）、payload 分支标记 __wmErrorShown 供调用方避免双 toast。
 */
import { waitFor } from '@testing-library/react';

const historyMock = { push: jest.fn(), replace: jest.fn(), location: { pathname: '/x' } };
jest.mock('@umijs/max', () => ({
  __esModule: true,
  history: historyMock,
}));

const mockMessage = {
  error: jest.fn(),
  warning: jest.fn(),
  info: jest.fn(),
  success: jest.fn(),
};
jest.mock('@/utils/antdApp', () => ({
  __esModule: true,
  getMessage: () => mockMessage,
  getNotification: () => ({ open: jest.fn() }),
}));

import { errorConfig } from '@/requestErrorConfig';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const errorHandler = (errorConfig as any).errorConfig.errorHandler;

// 模拟 umi request 抛出的 REST 错误形状：error.response.{status, data, config.url}
function restError(status: number, data: any, url?: string) {
  return {
    response: {
      status,
      data,
      ...(url ? { config: { url } } : {}),
    },
  };
}

describe('requestErrorConfig errorHandler', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  it('429 带 retry_after_seconds → 「请 N 分钟后再试」并标记已提示', async () => {
    const err = restError(
      429,
      {
        success: false,
        error: 'Too many failed attempts. Please try again later.',
        retry_after_seconds: 130,
      },
      '/api/v1/auth/login',
    );

    errorHandler(err, {});

    await waitFor(() => {
      expect(mockMessage.error).toHaveBeenCalledTimes(1);
    });
    expect(mockMessage.error).toHaveBeenCalledWith('尝试过于频繁，请 3 分钟后再试');
    expect((err as any).__wmErrorShown).toBe(true);
    expect(historyMock.push).not.toHaveBeenCalled();
  });

  it('429 缺 retry_after_seconds → 不带分钟数的通用提示', async () => {
    errorHandler(restError(429, { success: false, error: 'Too many failed attempts.' }), {});

    await waitFor(() => {
      expect(mockMessage.error).toHaveBeenCalledWith('尝试过于频繁，请稍后再试');
    });
  });

  it('登录请求 401 全局静默（登录页自行提示，不弹「未授权」）', async () => {
    errorHandler(restError(401, { success: false, error: 'Invalid credentials' }, '/api/v1/auth/login'), {});

    // defer 的 toast 都是 setTimeout(0)，等一拍确认没有弹任何文案
    await new Promise((resolve) => setTimeout(resolve, 10));
    expect(mockMessage.error).not.toHaveBeenCalled();
    expect(mockMessage.warning).not.toHaveBeenCalled();
  });

  it('非登录 401 保持原有清 token + 跳转登录行为', async () => {
    errorHandler(restError(401, { success: false, error: 'unauthorized' }, '/api/v1/agents'), {});

    await waitFor(() => {
      expect(mockMessage.warning).toHaveBeenCalled();
    });
    expect(historyMock.push).toHaveBeenCalledWith('/user/login');
  });
});
