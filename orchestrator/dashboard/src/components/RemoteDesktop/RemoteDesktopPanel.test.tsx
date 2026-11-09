/**
 * RemoteDesktopPanel 主面板测试（拆分自 index.test.tsx）：容器无关、
 * 建连状态呈现、文件下载触发、复制到本机、SSH 文件浏览器开合。
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render, waitFor } from '@testing-library/react';
import React from 'react';
import { guacEncodeBase64, RemoteDesktopPanel } from './index';
import {
  fakeInputStream,
  fakeTicketClient,
  lastClient,
  BASE_PARAMS,
  waitForUI,
} from './index.testHelpers';

jest.mock('@/services/remote', () => ({
  createRemoteTicket: jest.fn(),
  guacamoleWSPath: jest.fn(() => 'ws://host/api/remote/guacamole'),
}));

jest.mock('guacamole-common-js', () => {
  const m = require('./index.testMocks');
  return { __esModule: true, default: m.mocks, ...m.mocks };
});

const G = require('guacamole-common-js');
const MockedClient = G.Client as jest.Mock;

beforeEach(() => {
  jest.clearAllMocks();
});


describe('RemoteDesktopPanel', () => {
  it('建连中显示 Spin，连接后进入 connected', async () => {
    const { queryByText, getByTestId } = render(
      <RemoteDesktopPanel active params={BASE_PARAMS} ticketClient={fakeTicketClient()} />,
    );
    expect(getByTestId('remote-desktop-panel')).toBeTruthy();
    expect(getByTestId('remote-stage')).toBeTruthy();
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    await waitForUI(() => expect(queryByText(/正在建立桌面会话/)).toBeNull());
  });

  it('建连失败：展示错误条并提供重试（onRetry 透传）', async () => {
    const onRetry = jest.fn();
    const ticketClient = fakeTicketClient({
      issueTicket: jest.fn().mockRejectedValue(new Error('隧道断开')),
    });
    const { findByTestId, getByText } = render(
      <RemoteDesktopPanel
        active
        params={BASE_PARAMS}
        ticketClient={ticketClient}
        onRetry={onRetry}
      />,
    );
    await waitForUI(() => expect(findByTestId('remote-error-notice')).toBeTruthy());
    fireEvent.click(getByText('重试'));
    expect(onRetry).toHaveBeenCalled();
  });

  it('文件下发自动触发浏览器下载并提示', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const clicks: string[] = [];
    const origCreate = document.createElement.bind(document);
    const createSpy = jest.spyOn(document, 'createElement').mockImplementation((tag: string) => {
      if (tag === 'a') {
        const a = origCreate('a');
        Object.defineProperty(a, 'click', { value: () => clicks.push(a.download) });
        return a;
      }
      return origCreate(tag);
    });
    const notify = jest.fn();
    try {
      const { unmount } = render(
        <RemoteDesktopPanel
          active
          params={BASE_PARAMS}
          ticketClient={fakeTicketClient()}
          notify={notify}
        />,
      );
      await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
      const stream = fakeInputStream();
      lastClient().onfile(stream.stream, 'remote-dump.txt', 'text/plain');
      stream.emitBlob(btoa('part-1'));
      stream.emitEnd();

      await waitForUI(() => expect(clicks).toContain('remote-dump.txt'));
      expect(urlMock.createObjectURL).toHaveBeenCalled();
      expect(urlMock.revokeObjectURL).toHaveBeenCalledWith('blob:mock');
      expect(notify).toHaveBeenCalledWith('success', '已下载 remote-dump.txt');
      unmount();
    } finally {
      createSpy.mockRestore();
    }
  });

  it('复制到本机：navigator.clipboard 可写时提示成功', async () => {
    const writeText = jest.fn().mockResolvedValue(undefined);
    Object.assign(navigator, { clipboard: { writeText } });
    const notify = jest.fn();
    const { getByText } = render(
      <RemoteDesktopPanel
        active
        params={BASE_PARAMS}
        ticketClient={fakeTicketClient()}
        notify={notify}
      />,
    );
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const stream = fakeInputStream();
    lastClient().onclipboard(stream.stream, 'text/plain');
    stream.emitBlob(guacEncodeBase64('远端文本'));
    stream.emitEnd();

    const btn = await waitForUI(() => getByText(/复制到本地/));
    fireEvent.click(btn);
    await waitForUI(() => expect(writeText).toHaveBeenCalledWith('远端文本'));
    expect(notify).toHaveBeenCalledWith('success', '已复制到本地剪贴板');
  });

  it('复制到本机：clipboard 抛错时提示失败', async () => {
    const writeText = jest.fn().mockRejectedValue(new Error('denied'));
    Object.assign(navigator, { clipboard: { writeText } });
    const notify = jest.fn();
    const { getByText } = render(
      <RemoteDesktopPanel
        active
        params={BASE_PARAMS}
        ticketClient={fakeTicketClient()}
        notify={notify}
      />,
    );
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const stream = fakeInputStream();
    lastClient().onclipboard(stream.stream, 'text/plain');
    stream.emitBlob(guacEncodeBase64('x'));
    stream.emitEnd();
    fireEvent.click(await waitForUI(() => getByText(/复制到本地/)));
    await waitForUI(() =>
      expect(notify).toHaveBeenCalledWith('error', '复制失败（浏览器剪贴板权限）'),
    );
  });

  it('未注入 notify 时回退 antd message（成功与失败两路）', async () => {
    const antd = require('antd');
    const successSpy = jest.spyOn(antd.message, 'success').mockImplementation(() => ({}) as never);
    const errorSpy = jest.spyOn(antd.message, 'error').mockImplementation(() => ({}) as never);
    const writeText = jest.fn().mockResolvedValue(undefined);
    Object.assign(navigator, { clipboard: { writeText } });
    try {
      const { getByText } = render(
        <RemoteDesktopPanel active params={BASE_PARAMS} ticketClient={fakeTicketClient()} />,
      );
      await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
      const stream = fakeInputStream();
      lastClient().onclipboard(stream.stream, 'text/plain');
      stream.emitBlob(guacEncodeBase64('y'));
      stream.emitEnd();
      fireEvent.click(await waitForUI(() => getByText(/复制到本地/)));
      await waitForUI(() => expect(successSpy).toHaveBeenCalledWith('已复制到本地剪贴板'));

      // 失败路径：clipboard 拒绝 → message.error
      writeText.mockRejectedValueOnce(new Error('denied'));
      fireEvent.click(getByText(/复制到本地/));
      await waitForUI(() => expect(errorSpy).toHaveBeenCalledWith('复制失败（浏览器剪贴板权限）'));
    } finally {
      successSpy.mockRestore();
      errorSpy.mockRestore();
    }
  });

  it('stageHeight 透传到画布样式', () => {
    const { getByTestId } = render(
      <RemoteDesktopPanel
        active={false}
        params={BASE_PARAMS}
        ticketClient={fakeTicketClient()}
        height={320}
      />,
    );
    expect((getByTestId('remote-stage') as HTMLElement).style.height).toBe('320px');
  });

  it('SSH 文件浏览：SFTP 对象上报后才可展开；开合不卸载 stage', async () => {
    const { getByTestId, queryByTestId } = render(
      <RemoteDesktopPanel
        active
        params={{ agentId: 'agent-1', protocol: 'ssh' }}
        ticketClient={fakeTicketClient()}
        notify={jest.fn()}
      />,
    );
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const toggle = () => getByTestId('remote-fs-toggle') as HTMLButtonElement;
    // SFTP 对象未上报：按钮禁用，浏览器不可展开
    expect(toggle().disabled).toBe(true);

    // guacd 下发 filesystem 指令 → hook 上报对象 → 按钮就绪
    const fsObj = {
      index: 0,
      requestInputStream: jest.fn(),
      createOutputStream: jest.fn(),
    };
    lastClient().onfilesystem?.(fsObj, '/');
    await waitForUI(() => expect(toggle().disabled).toBe(false));

    // 展开：浏览器渲染并向对象请求根目录；stage 必须仍在（像素面不可重建）
    fireEvent.click(toggle());
    expect(getByTestId('remote-file-browser')).toBeTruthy();
    expect(getByTestId('remote-stage')).toBeTruthy();
    expect(fsObj.requestInputStream).toHaveBeenCalledWith('/', expect.any(Function));

    // 收起：浏览器卸载，stage 依旧挂载
    fireEvent.click(toggle());
    expect(queryByTestId('remote-file-browser')).toBeNull();
    expect(getByTestId('remote-stage')).toBeTruthy();
  });

  it('SSH 文件操作审计：面板把票据注入审计回调（§15.1 端到端）', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const audit = jest.fn();
    // 路径感知假对象：列根目录回目录体，下载请求回文件流（同步应答）
    const fsObj = {
      index: 0,
      requestInputStream: jest.fn((name: string, cb: (s: unknown, m: string) => void) => {
        const stream = {
          sendAck: jest.fn(),
          onblob: undefined as undefined | ((d: string) => void),
          onend: undefined as undefined | (() => void),
        };
        cb(stream, name === '/' ? 'text/json' : 'text/plain');
        // cb 返回时 onblob/onend 已被协议层同步注册，直接投喂
        if (name === '/') {
          stream.onblob?.(
            btoa(
              JSON.stringify([
                { name: 'a.txt', directory: false, mimetype: 'text/plain', size: 4 },
              ]),
            ),
          );
        } else {
          stream.onblob?.(btoa('data'));
        }
        stream.onend?.();
      }),
      createOutputStream: jest.fn(),
    };
    const { getByTestId, findByTestId } = render(
      <RemoteDesktopPanel
        active
        params={{ agentId: 'agent-1', protocol: 'ssh' }}
        ticketClient={fakeTicketClient()}
        notify={jest.fn()}
        audit={audit}
      />,
    );
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    lastClient().onfilesystem?.(fsObj, '/');
    // SFTP 对象就绪（state 冲刷）前按钮禁用，点了也开不了
    await waitForUI(() =>
      expect(getByTestId('remote-fs-toggle') as HTMLButtonElement).toHaveProperty(
        'disabled',
        false,
      ),
    );
    fireEvent.click(getByTestId('remote-fs-toggle') as HTMLButtonElement);
    fireEvent.click(await findByTestId('remote-fs-download-a.txt'));
    await waitForUI(() =>
      expect(audit).toHaveBeenCalledWith({
        action: 'download',
        path: '/a.txt',
        result: 'ok',
        sizeBytes: 4,
        attempts: 1,
        ticket: 'tk-1',
      }),
    );
  });

  it('不传 audit 时浏览器照常工作（回调可缺省）', async () => {
    const { getByTestId } = render(
      <RemoteDesktopPanel
        active
        params={{ agentId: 'agent-1', protocol: 'ssh' }}
        ticketClient={fakeTicketClient()}
        notify={jest.fn()}
      />,
    );
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    lastClient().onfilesystem?.(
      { index: 0, requestInputStream: jest.fn(), createOutputStream: jest.fn() },
      '/',
    );
    await waitForUI(() =>
      expect(getByTestId('remote-fs-toggle') as HTMLButtonElement).toHaveProperty(
        'disabled',
        false,
      ),
    );
    fireEvent.click(getByTestId('remote-fs-toggle') as HTMLButtonElement);
    expect(getByTestId('remote-file-browser')).toBeTruthy();
  });
});
