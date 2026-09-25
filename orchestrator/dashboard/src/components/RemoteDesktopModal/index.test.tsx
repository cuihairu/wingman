/**
 * 远程桌面弹窗测试：打开即申请票据并建立隧道、监看模式不挂键盘、
 * 关闭清理会话；阶段二：剪贴板收发（逐块 ack）、文件上传/下载、
 * record 票据与录制指示、VNC 无文件 UI。
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render, waitFor } from '@testing-library/react';
import React from 'react';
import { message } from 'antd';
import { BlobWriter, Client, Keyboard, Mouse, WebSocketTunnel } from 'guacamole-common-js';
import RemoteDesktopModal, { guacDecodeBase64, guacEncodeBase64 } from './index';
import { createRemoteTicket } from '@/services/remote';

jest.mock('@/services/remote', () => ({
  createRemoteTicket: jest.fn(),
  guacamoleWSPath: jest.fn(
    (ticket: string) => `ws://localhost/api/remote/guacamole?ticket=${ticket}`,
  ),
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
  const mocks = {
    WebSocketTunnel: jest.fn().mockImplementation((url: string) => ({ url })),
    Client: jest.fn().mockImplementation(() => ({
      getDisplay: mockDisplay,
      connect: jest.fn(),
      disconnect: jest.fn(),
      sendMouseState: jest.fn(),
      sendKeyEvent: jest.fn(),
      sendSize: jest.fn(),
      sendAck: jest.fn(),
      createClipboardStream: jest.fn().mockImplementation(() => ({
        sendBlob: jest.fn(),
        sendEnd: jest.fn(),
        onack: null,
      })),
      createFileStream: jest.fn().mockImplementation((_mimetype: string, filename: string) => ({
        sendBlob: jest.fn(),
        sendEnd: jest.fn(),
        filename,
        onack: null,
      })),
    })),
    BlobWriter: jest.fn().mockImplementation((stream: unknown) => ({
      sendBlob: jest.fn(),
      oncomplete: null,
      onerror: null,
      onprogress: null,
      stream,
    })),
    Mouse: jest.fn().mockImplementation(() => ({})),
    Keyboard: jest.fn().mockImplementation(() => ({})),
    Touchscreen: jest.fn().mockImplementation(() => ({})),
    Status: { Code: { SUCCESS: 0x0000, UNSUPPORTED: 0x0100, SERVER_ERROR: 0x0200 } },
  };
  // 真实包是 UMD（构造器挂 default）；组件走 default，测试断言用具名——
  // 同一引用两条导出路径都通。
  return { __esModule: true, default: mocks, ...mocks };
});

const mockedCreate = createRemoteTicket as jest.MockedFunction<typeof createRemoteTicket>;
const MockedClient = Client as jest.Mock;
const MockedKeyboard = Keyboard as jest.Mock;
const MockedBlobWriter = BlobWriter as jest.Mock;

function renderModal(
  open: boolean,
  props: Partial<React.ComponentProps<typeof RemoteDesktopModal>> = {},
) {
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

/** 最近一次创建的 client mock 实例 */
function lastClient() {
  return MockedClient.mock.results[MockedClient.mock.results.length - 1].value;
}

/** 假 InputStream：捕获组件注册的 onblob/onend，供测试逐块投喂 */
function fakeInputStream() {
  let blobHandler: (data: string) => void = () => {};
  let endHandler: () => void = () => {};
  const stream = {
    sendAck: jest.fn(),
    set onblob(fn: (data: string) => void) {
      blobHandler = fn;
    },
    get onblob(): (data: string) => void {
      return blobHandler;
    },
    set onend(fn: () => void) {
      endHandler = fn;
    },
    get onend(): () => void {
      return endHandler;
    },
  };
  return { stream, emitBlob: (d: string) => blobHandler(d), emitEnd: () => endHandler() };
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
    const instance = lastClient();
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
    const instance = lastClient();
    rerender(
      <RemoteDesktopModal open={false} onCancel={jest.fn()} agentId="agent-1" protocol="rdp" />,
    );
    await waitFor(() => expect(instance.disconnect).toHaveBeenCalled());
  });

  it('票据申请失败展示错误', async () => {
    mockedCreate.mockRejectedValueOnce(
      new Error('desktop:control permission required for control mode'),
    );
    const { findByText } = renderModal(true, { protocol: 'vnc' });
    expect(await findByText(/desktop:control permission required/)).toBeTruthy();
  });

  // ---------- 阶段二：录制（§16） ----------

  it('record 透传票据并在界面显示录制中指示', async () => {
    const { findByText, unmount } = renderModal(true, { record: true });
    expect(await findByText(/会话录制中/)).toBeTruthy();
    expect(mockedCreate).toHaveBeenCalledWith(expect.objectContaining({ record: true }));
    unmount();
  });

  it('默认不录制：票据无 record 且无指示', async () => {
    const { queryByText, unmount } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    expect(mockedCreate).toHaveBeenCalledWith(expect.objectContaining({ record: false }));
    expect(queryByText(/会话录制中/)).toBeNull();
    unmount();
  });

  // ---------- 阶段二：剪贴板（§14） ----------

  it('onclipboard 收到文本：逐块 ack + 展示 + 复制按钮', async () => {
    const { findByText, unmount } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();

    const { stream, emitBlob, emitEnd } = fakeInputStream();
    client.onclipboard(stream, 'text/plain');
    emitBlob(guacEncodeBase64('远端复'));
    emitBlob(guacEncodeBase64('制内容'));
    emitEnd();

    expect(await findByText(/远端复制内容/)).toBeTruthy();
    expect(stream.sendAck).toHaveBeenCalledTimes(2);
    expect(stream.sendAck).toHaveBeenCalledWith('ok', 0x0000);
    expect(await findByText(/复制到本地/)).toBeTruthy();
    unmount();
  });

  it('非 text/* 剪贴板流以 UNSUPPORTED ack 拒绝', async () => {
    const { unmount } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();

    const sendAck = jest.fn();
    client.onclipboard({ sendAck }, 'image/png');
    expect(sendAck).toHaveBeenCalledWith('unsupported', 0x0100);
    unmount();
  });

  it('接管模式发送剪贴板：createClipboardStream + base64 + sendEnd', async () => {
    const { findByPlaceholderText, findByText, unmount } = renderModal(true);
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();

    const input = await findByPlaceholderText('输入文本发送到远端剪贴板');
    fireEvent.change(input, { target: { value: '本地复制内容' } });
    // antd 对双字中文按钮文案自动插空格（发 送）
    fireEvent.click(await findByText(/^发\s?送$/));

    expect(client.createClipboardStream).toHaveBeenCalledWith('text/plain');
    const stream = client.createClipboardStream.mock.results[0].value;
    expect(stream.sendBlob).toHaveBeenCalledWith(guacEncodeBase64('本地复制内容'));
    expect(stream.sendEnd).toHaveBeenCalled();
    unmount();
  });

  it('监看模式隐藏剪贴板发送入口', async () => {
    const { queryByPlaceholderText, unmount } = renderModal(true, { readOnly: true });
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    expect(queryByPlaceholderText('输入文本发送到远端剪贴板')).toBeNull();
    unmount();
  });

  // ---------- 阶段二：文件传输（§15） ----------

  it('ssh 显示上传入口；上传走 createFileStream + BlobWriter + 完成后 sendEnd', async () => {
    const messageSpy = jest.spyOn(message, 'success').mockImplementation(() => ({}) as never);
    const { container, unmount } = renderModal(true, { protocol: 'ssh' });
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();

    // Modal 内容渲染进 body portal，用 document.body 查询
    const fileInput = document.body.querySelector('input[type="file"]') as HTMLInputElement;
    expect(fileInput).toBeTruthy();

    const file = new File(['file-bytes'], 'report.txt', { type: 'text/plain' });
    fireEvent.change(fileInput, { target: { files: [file] } });

    expect(client.createFileStream).toHaveBeenCalledWith('text/plain', 'report.txt');
    const stream = client.createFileStream.mock.results[0].value;
    expect(MockedBlobWriter).toHaveBeenCalledWith(stream);
    const writer = MockedBlobWriter.mock.results[0].value;
    expect(writer.sendBlob).toHaveBeenCalledWith(file);

    // 传输完成回调：先 sendEnd 再提示（message 为全局挂载，spy 断言）
    writer.oncomplete();
    expect(stream.sendEnd).toHaveBeenCalled();
    await waitFor(() => expect(messageSpy).toHaveBeenCalledWith('report.txt 上传完成'));
    messageSpy.mockRestore();
    unmount();
  });

  it('vnc 无文件通道：整个文件传输 UI 不渲染', async () => {
    const { queryByText, unmount } = renderModal(true, { protocol: 'vnc' });
    await waitFor(() => expect(MockedClient).toHaveBeenCalled());
    expect(document.body.querySelector('input[type="file"]')).toBeNull();
    expect(queryByText(/上传文件/)).toBeNull();
    unmount();
  });

  it('监看模式上传按钮禁用（SSH）', async () => {
    const { findByText, unmount } = renderModal(true, { protocol: 'ssh', readOnly: true });
    const btn = (await findByText(/上传文件/)).closest('button');
    expect(btn).toBeTruthy();
    expect(btn!.disabled).toBe(true);
    unmount();
  });

  it('onfile 聚合分块触发浏览器下载', async () => {
    // jsdom 未实现 createObjectURL：手动补桩记录下载名
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

    try {
      const { unmount } = renderModal(true);
      await waitFor(() => expect(MockedClient).toHaveBeenCalled());
      const client = lastClient();

      const { stream, emitBlob, emitEnd } = fakeInputStream();
      client.onfile(stream, 'remote-dump.txt', 'text/plain');
      emitBlob(btoa('part-1'));
      emitBlob(btoa('part-2'));
      emitEnd();

      expect(stream.sendAck).toHaveBeenCalledTimes(2);
      expect(urlMock.createObjectURL).toHaveBeenCalled();
      expect(clicks).toContain('remote-dump.txt');
      expect(urlMock.revokeObjectURL).toHaveBeenCalledWith('blob:mock');
      unmount();
    } finally {
      createSpy.mockRestore();
    }
  });

  // ---------- base64 工具（流载荷编解码） ----------

  it('guacEncodeBase64/guacDecodeBase64 对中文往返无损', () => {
    const text = '剪贴板内容 clipboard ✓';
    expect(guacDecodeBase64(guacEncodeBase64(text))).toBe(text);
    // 与标准 base64 一致（UTF-8 字节）
    expect(guacEncodeBase64('abc')).toBe(btoa('abc'));
  });
});
