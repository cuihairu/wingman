/**
 * 远程桌面错误分类与降级提示测试（拆分自 index.test.tsx）：
 * classifyRemoteError 四类判定 + RemoteErrorNotice 渲染。
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render } from '@testing-library/react';
import React from 'react';
import { classifyRemoteError, RemoteErrorNotice } from './index';

jest.mock('@/services/remote', () => ({
  createRemoteTicket: jest.fn(),
  guacamoleWSPath: jest.fn(() => 'ws://host/api/remote/guacamole'),
}));

jest.mock('guacamole-common-js', () => {
  const m = require('./index.testMocks');
  return { __esModule: true, default: m.mocks, ...m.mocks };
});

beforeEach(() => {
  jest.clearAllMocks();
});


describe('classifyRemoteError', () => {
  it('权限类：不可重试', () => {
    const info = classifyRemoteError('desktop:control permission required for control mode');
    expect(info.kind).toBe('permission');
    expect(info.retryable).toBe(false);
    expect(classifyRemoteError('Forbidden').kind).toBe('permission');
  });

  it('未配置类：不可重试，保留后端 hint 原文', () => {
    const info = classifyRemoteError('session recording not configured (set WINGMAN_...)');
    expect(info.kind).toBe('unconfigured');
    expect(info.retryable).toBe(false);
    expect(info.detail).toContain('WINGMAN_');
    expect(classifyRemoteError('guacamole daemon unavailable').kind).toBe('unconfigured');
  });

  it('断链类：可重试', () => {
    expect(classifyRemoteError('桌面会话异常断开，请重新打开')).toMatchObject({
      kind: 'disconnected',
      retryable: true,
    });
    expect(classifyRemoteError('tunnel closed').kind).toBe('disconnected');
  });

  it('未知类：可重试；空串走兜底文案', () => {
    expect(classifyRemoteError('something odd')).toMatchObject({
      kind: 'unknown',
      retryable: true,
    });
    const empty = classifyRemoteError('');
    expect(empty.detail).toBe('桌面连接失败');
    expect(classifyRemoteError('   ').title).toBe('桌面连接失败');
  });

  it('原文被清空到空串时，各类的 detail 都有兜底（非原文即兜底）', () => {
    // 分类靠关键词，原文一旦为空则全部落到兜底 detail——断言不出现 undefined/空白
    for (const kind of ['permission', 'not configured', '断开', '未知']) {
      const info = classifyRemoteError(kind);
      expect(info.detail.trim().length).toBeGreaterThan(0);
    }
  });
});

describe('RemoteErrorNotice', () => {
  it('空错误不渲染', () => {
    const { container } = render(<RemoteErrorNotice error="" />);
    expect(container.firstChild).toBeNull();
  });

  it('权限错误：无重试按钮', () => {
    const onRetry = jest.fn();
    const { getByTestId, queryByText } = render(
      <RemoteErrorNotice error="permission required" onRetry={onRetry} />,
    );
    expect(getByTestId('remote-error-notice')).toBeTruthy();
    expect(queryByText('重试')).toBeNull();
  });

  it('断链错误且给了 onRetry：渲染并可点击重试', () => {
    const onRetry = jest.fn();
    const { getByText } = render(<RemoteErrorNotice error="会话已断开" onRetry={onRetry} />);
    fireEvent.click(getByText('重试'));
    expect(onRetry).toHaveBeenCalled();
  });

  it('未给 onRetry 时不渲染重试按钮', () => {
    const { queryByText, getByTestId } = render(<RemoteErrorNotice error="未知错误" />);
    expect(getByTestId('remote-error-notice')).toBeTruthy();
    expect(queryByText('重试')).toBeNull();
  });
});

// ---------- 主面板 ----------
