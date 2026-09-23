/**
 * 远程桌面弹窗测试：打开即申请票据并建立隧道、监看模式不挂键盘、
 * 关闭清理会话。guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { render, waitFor } from '@testing-library/react';
import React from 'react';
import { Client, Keyboard, Mouse, WebSocketTunnel } from 'guacamole-common-js';
import RemoteDesktopModal from './index';
import { createRemoteTicket } from '@/services/remote';

jest.mock('@/services/remote', () => ({
  createRemoteTicket: jest.fn(),
  guacamoleWSPath: jest.fn((ticket: string) => `ws://localhost/api/remote/guacamole?ticket=${ticket}`),
}));

jest.mock('guacamole-common-js', () => {
  const displayElement = () => {
    const el = document.createElement('div');
    return el;
  };
  const mockDisplay = () => ({
    getElement: displayElement,
    getWidth: jest.fn(() => 1280),
    getHeight: jest.fn(() => 800),
    scale: 1,
    showCursor: jest.fn(),
  });
  return {
    WebSocketTunnel: jest.fn().mockImplementation((url: string) => ({ url })),
    Client: jest.fn().mockImplementation(() => ({
      getDisplay: mockDisplay,
      connect: jest.fn(),
      disconnect: jest.fn(),
      sendMouseState: jest.fn(),
      sendKeyEvent: jest.fn(),
      sendSize: jest.fn(),
    })),
    Mouse: jest.fn().mockImplementation(() => ({})),
    Keyboard: jest.fn().mockImplementation(() => ({})),
    Touchscreen: jest.fn().mockImplementation(() => ({})),
  };
});

const mockedCreate = createRemoteTicket as jest.MockedFunction<typeof createRemoteTicket>;
const MockedClient = Client as jest.Mock;
const MockedKeyboard = Keyboard as jest.Mock;

function renderModal(open: boolean, props: Partial<React.ComponentProps<typeof RemoteDesktopModal>> = {}) {
  const onCancel = jest.fn();
  const utils = render(
    <RemoteDesktopModal
      open={open}
      onCancel={onCancel}
      agentId="agent-1"
      protocol="rdp"
      username="ubuntu"
      password="pw"
      {...props}
    />,
  );
  return { ...utils, onCancel };
}

describe('RemoteDesktopModal', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    mockedCreate.mockResolvedValue({ ticket: 'tk-9', expiresAt: '2026-09-23T12:00:00Z' });
  });

  it('open 时申请票据并建立 WS 隧道 + connect', async () => {
    const { unmount } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    expect(mockedCreate).toHaveBeenCalledWith(
      expect.objectContaining({ agentId: 'agent-1', protocol: 'rdp', readOnly: false }),
    );
    expect(WebSocketTunnel).toHaveBeenCalledWith('ws://localhost/api/remote/guacamole?ticket=tk-9');
    const instance = MockedClient.mock.results[0].value;
    expect(instance.connect).toHaveBeenCalled();
    unmount();
  });

  it('接管模式挂载键盘注入，监看模式不挂', async () => {
    const { unmount } = renderModal(true);
    await waitFor(() => expect(Keyboard).toHaveBeenCalled());
    unmount();

    mockedCreate.mockResolvedValue({ ticket: 'tk-ro', expiresAt: 'x' });
    const { unmount: unmount2 } = renderModal(true, { readOnly: true });
    await waitFor(() => expect(MockedClient).toHaveBeenCalledTimes(2));
    expect(Keyboard).toHaveBeenCalledTimes(1); // 监看未新增键盘
    unmount2();
  });

  it('关闭时 disconnect', async () => {
    const { rerender } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    const instance = MockedClient.mock.results[0].value;
    rerender(
      <RemoteDesktopModal
        open={false}
        onCancel={jest.fn()}
        agentId="agent-1"
        protocol="rdp"
      />,
    );
    await waitFor(() => expect(instance.disconnect).toHaveBeenCalled());
  });

  it('票据申请失败展示错误', async () => {
    mockedCreate.mockRejectedValueOnce(new Error('desktop:control permission required for control mode'));
    const { findByText } = renderModal(true, { protocol: 'vnc' });
    expect(await findByText(/desktop:control permission required/)).toBeTruthy();
  });
});
