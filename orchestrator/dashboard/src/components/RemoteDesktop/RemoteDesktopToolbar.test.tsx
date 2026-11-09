/**
 * RemoteDesktopToolbar 测试（拆分自 index.test.tsx）：监看模式隐藏发送入口、
 * VNC 无文件块、上传复位、SSH 文件浏览开合、剪贴板发送与复制。
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render } from '@testing-library/react';
import React from 'react';
import { RemoteDesktopToolbar } from './index';

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


describe('RemoteDesktopToolbar', () => {
  const noop = () => {};

  it('接管模式：模式条 + 剪贴板发送 + 文件入口（rdp）', () => {
    const { getByText, getByPlaceholderText, getByTestId } = render(
      <RemoteDesktopToolbar
        protocol="rdp"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    expect(getByText(/接管模式/)).toBeTruthy();
    expect(getByPlaceholderText('输入文本发送到远端剪贴板')).toBeTruthy();
    expect(getByTestId('remote-file-input')).toBeTruthy();
    expect(getByText(/驱动器重定向/)).toBeTruthy();
  });

  it('监看模式：隐藏发送入口、禁用上传，并明示不注入输入', () => {
    const { queryByPlaceholderText, getByText } = render(
      <RemoteDesktopToolbar
        protocol="ssh"
        readOnly
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    expect(queryByPlaceholderText('输入文本发送到远端剪贴板')).toBeNull();
    expect(getByText(/监看模式：键鼠输入不注入远端/)).toBeTruthy();
    const btn = getByText(/上传文件/).closest('button');
    expect(btn!.disabled).toBe(true);
  });

  it('vnc：整块文件 UI 不渲染', () => {
    const { queryByTestId, queryByText } = render(
      <RemoteDesktopToolbar
        protocol="vnc"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    expect(queryByTestId('remote-file-input')).toBeNull();
    expect(queryByText(/上传文件/)).toBeNull();
  });

  it('录制中显示红色指示与按键不入录像说明', () => {
    const { getByText } = render(
      <RemoteDesktopToolbar
        protocol="vnc"
        readOnly
        record
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    expect(getByText(/会话录制中/)).toBeTruthy();
    expect(getByText(/按键内容不会被录入录像/)).toBeTruthy();
  });

  it('无剪贴板内容时显示占位文案', () => {
    const { getByText } = render(
      <RemoteDesktopToolbar
        protocol="vnc"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    expect(getByText(/远端复制内容将显示在这里/)).toBeTruthy();
  });

  it('有剪贴板内容时展示内容并可复制到本机', () => {
    const onCopyToLocal = jest.fn();
    const { getByText } = render(
      <RemoteDesktopToolbar
        protocol="vnc"
        readOnly={false}
        record={false}
        clipboard="远端文本"
        onSendClipboard={noop}
        onCopyToLocal={onCopyToLocal}
        onUploadFiles={noop}
      />,
    );
    expect(getByText('远端文本')).toBeTruthy();
    fireEvent.click(getByText(/复制到本地/));
    expect(onCopyToLocal).toHaveBeenCalledWith('远端文本');
  });

  it('发送剪贴板：点击与回车均可，空文本不发送', () => {
    const onSendClipboard = jest.fn();
    const { getByPlaceholderText, getByText } = render(
      <RemoteDesktopToolbar
        protocol="vnc"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={onSendClipboard}
        onCopyToLocal={noop}
        onUploadFiles={noop}
      />,
    );
    const input = getByPlaceholderText('输入文本发送到远端剪贴板');
    // rc-input 的 onPressEnter 认 e.key，且有 keyLock：keydown 置锁、
    // keyup 才解——只发 keydown 第二次回车会被吞，故成对模拟
    const pressEnter = () => {
      fireEvent.keyDown(input, { key: 'Enter', code: 'Enter', keyCode: 13 });
      fireEvent.keyUp(input, { key: 'Enter', code: 'Enter', keyCode: 13 });
    };
    fireEvent.change(input, { target: { value: '   ' } });
    // 空文本：按钮禁用
    fireEvent.click(getByText(/^发\s?送$/));
    // 回车在空草稿上同样不发送（按钮禁用拦不住回车，函数自身再兜一层）
    pressEnter();
    expect(onSendClipboard).not.toHaveBeenCalled();

    fireEvent.change(input, { target: { value: '  hello  ' } });
    fireEvent.click(getByText(/^发\s?送$/));
    expect(onSendClipboard).toHaveBeenCalledWith('hello');
    // 发送后清空草稿
    expect((input as HTMLInputElement).value).toBe('');

    fireEvent.change(input, { target: { value: 'again' } });
    pressEnter();
    expect(onSendClipboard).toHaveBeenCalledTimes(2);
    expect(onSendClipboard).toHaveBeenLastCalledWith('again');
  });

  it('点击上传按钮触发隐藏 input，选中文件后复位', () => {
    const onUploadFiles = jest.fn();
    const { getByText, getByTestId } = render(
      <RemoteDesktopToolbar
        protocol="ssh"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={onUploadFiles}
      />,
    );
    const input = getByTestId('remote-file-input') as HTMLInputElement;
    const clickSpy = jest.spyOn(input, 'click');
    fireEvent.click(getByText(/^上传文件$/));
    expect(clickSpy).toHaveBeenCalled();

    const file = new File(['x'], 'a.txt', { type: 'text/plain' });
    fireEvent.change(input, { target: { files: [file] } });
    expect(onUploadFiles).toHaveBeenCalled();
    expect(input.value).toBe('');
  });

  it('用户取消选择（files 为空）也复位，不残留上次的文件名', () => {
    const onUploadFiles = jest.fn();
    const { getByTestId } = render(
      <RemoteDesktopToolbar
        protocol="ssh"
        readOnly={false}
        record={false}
        clipboard=""
        onSendClipboard={noop}
        onCopyToLocal={noop}
        onUploadFiles={onUploadFiles}
      />,
    );
    const input = getByTestId('remote-file-input') as HTMLInputElement;
    Object.defineProperty(input, 'files', { value: [], configurable: true });
    fireEvent.change(input, { target: { files: [] } });
    expect(onUploadFiles).toHaveBeenCalledWith([]);
    expect(input.value).toBe('');
  });

  it('ssh：文件浏览按钮就绪前禁用，就绪后可开合；展开态高亮', () => {
    const onToggleFileBrowser = jest.fn();
    const props = {
      protocol: 'ssh' as const,
      readOnly: false,
      record: false,
      clipboard: '',
      onSendClipboard: noop,
      onCopyToLocal: noop,
      onUploadFiles: noop,
      onToggleFileBrowser,
    };
    const { getByTestId, rerender } = render(
      <RemoteDesktopToolbar {...props} fileBrowserReady={false} fileBrowserOpen={false} />,
    );
    // SFTP 对象未上报：禁用且不触发回调
    const disabledBtn = getByTestId('remote-fs-toggle') as HTMLButtonElement;
    expect(disabledBtn.disabled).toBe(true);
    fireEvent.click(disabledBtn);
    expect(onToggleFileBrowser).not.toHaveBeenCalled();

    // 就绪：可点击；展开态 primary 高亮
    rerender(<RemoteDesktopToolbar {...props} fileBrowserReady fileBrowserOpen />);
    const readyBtn = getByTestId('remote-fs-toggle') as HTMLButtonElement;
    expect(readyBtn.disabled).toBe(false);
    expect(readyBtn.className).toContain('ant-btn-primary');
    fireEvent.click(readyBtn);
    expect(onToggleFileBrowser).toHaveBeenCalledTimes(1);
  });

  it('文件浏览按钮仅 SSH 渲染（vnc/rdp 无 SFTP 通道）', () => {
    for (const protocol of ['vnc', 'rdp'] as const) {
      const { queryByTestId } = render(
        <RemoteDesktopToolbar
          protocol={protocol}
          readOnly={false}
          record={false}
          clipboard=""
          onSendClipboard={noop}
          onCopyToLocal={noop}
          onUploadFiles={noop}
          fileBrowserReady
        />,
      );
      expect(queryByTestId('remote-fs-toggle')).toBeNull();
    }
  });
});

// ---------- 错误与降级 ----------
