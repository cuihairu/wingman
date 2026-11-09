/**
 * useGuacamoleSession 连接生命周期测试（拆分自 index.test.tsx）：
 * 建连/断开/错误/监看无输入/剪贴板与文件流编排/卸载竞态。
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render, waitFor } from '@testing-library/react';
import React from 'react';
import {
  guacEncodeBase64,
  useGuacamoleSession,
  type RemoteSessionParams,
  type TicketClient,
} from './index';
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
const MockedKeyboard = G.Keyboard as jest.Mock;
const MockedMouse = G.Mouse as jest.Mock;
const MockedBlobWriter = G.BlobWriter as jest.Mock;

/**
 * 探针组件统一使用的稳定票据客户端。
 *
 * hook 把 ticketClient 放进 effect 依赖：若在渲染体内新建对象，每次重渲染
 * 都会重建会话（票据一次性 → 反复消耗申请 + 打断像素面），测试会陷入死循环。
 * 故每用例前在 beforeEach 造一个、组件内只引用。
 */
let probeClient: TicketClient;

beforeEach(() => {
  jest.clearAllMocks();
  probeClient = fakeTicketClient();
});

describe('useGuacamoleSession', () => {
  /**
   * renderHook 挂载探针组件。ticketClient / stageRef 必须在组件外创建：
   * 放渲染体内每次渲染换引用 → hook 依赖变化 → 重建会话的死循环。
   */
  function renderHook(
    active: boolean,
    params: RemoteSessionParams = BASE_PARAMS,
    extra: Record<string, unknown> = {},
  ) {
    const stageRef = { current: document.createElement('div') };
    const ticketClient = fakeTicketClient();
    const result: { current?: ReturnType<typeof useGuacamoleSession> } = {};
    function Probe() {
      result.current = useGuacamoleSession({
        active,
        params,
        ticketClient,
        stageRef: stageRef as never,
        ...extra,
      });
      return null;
    }
    const utils = render(<Probe />);
    return { ...utils, result, stageRef, ticketClient };
  }

  /** 等待探针进入 connected（票据申请 + client.connect 完成后） */
  async function connected(result: { current?: ReturnType<typeof useGuacamoleSession> }) {
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
  }

  it('active=true 时申请票据 → 建隧道 → connect，phase 转 connected', async () => {
    const onClipboard = jest.fn();
    const ticketClient = fakeTicketClient();
    const stageRef = { current: document.createElement('div') };
    let value: ReturnType<typeof useGuacamoleSession> | undefined;
    function Probe() {
      value = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
        onClipboard,
      });
      return null;
    }
    const { unmount } = render(<Probe />);

    expect(value!.phase).toBe('connecting');
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    await waitForUI(() => expect(value!.phase).toBe('connected'));
    expect(ticketClient.issueTicket).toHaveBeenCalledWith(
      expect.objectContaining({ agentId: 'agent-1', protocol: 'rdp', readOnly: false }),
    );
    expect(ticketClient.tunnelURL).toHaveBeenCalledWith('tk-1');
    expect(lastClient().connect).toHaveBeenCalled();
    // 画布已挂 display
    expect(stageRef.current.children.length).toBe(1);
    unmount();
    expect(lastClient().disconnect).toHaveBeenCalled();
  });

  it('active=false 不建连', () => {
    const { result } = renderHook(false);
    expect(result.current!.phase).toBe('idle');
    expect(MockedClient).not.toHaveBeenCalled();
  });

  it('会话票据随建连暴露在状态中（§15.1 文件操作审计反查键）', async () => {
    const { result } = renderHook(true);
    expect(result.current!.ticket).toBeUndefined();
    await connected(result);
    expect(result.current!.ticket).toBe('tk-1');
  });

  it('接管模式挂键鼠注入，监看模式不挂', async () => {
    const { unmount } = renderHook(true, { ...BASE_PARAMS, readOnly: false });
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    expect(MockedKeyboard).toHaveBeenCalledTimes(1);
    expect(MockedMouse).toHaveBeenCalledTimes(1);
    unmount();

    const { unmount: unmount2 } = renderHook(true, { ...BASE_PARAMS, readOnly: true });
    await waitForUI(() => expect(MockedClient).toHaveBeenCalledTimes(2));
    expect(MockedKeyboard).toHaveBeenCalledTimes(1);
    expect(MockedMouse).toHaveBeenCalledTimes(1);
    unmount2();
  });

  it('鼠标状态与按键事件转发给 client（接管注入闭环）', async () => {
    const { unmount } = renderHook(true, { ...BASE_PARAMS, readOnly: false });
    await waitForUI(() => expect(MockedKeyboard).toHaveBeenCalled());
    const mouse = MockedMouse.mock.results[0].value;
    const keyboard = MockedKeyboard.mock.results[0].value;
    const state = { x: 1, y: 2, left: true, middle: false, right: false, up: false, down: false };
    mouse.onmousedown(state);
    mouse.onmouseup(state);
    mouse.onmousemove(state);
    keyboard.onkeydown(97);
    keyboard.onkeyup(97);
    const client = lastClient();
    expect(client.sendMouseState).toHaveBeenCalledTimes(3);
    expect(client.sendKeyEvent).toHaveBeenNthCalledWith(1, 1, 97);
    expect(client.sendKeyEvent).toHaveBeenNthCalledWith(2, 0, 97);
    unmount();
  });

  it('等比缩放：window resize 与 display.onresize 都重算 scale', async () => {
    const stage = document.createElement('div');
    // jsdom 无布局引擎，clientWidth/Height 恒 0：显式打桩才能走到缩放分支
    Object.defineProperty(stage, 'clientWidth', { value: 640, configurable: true });
    Object.defineProperty(stage, 'clientHeight', { value: 480, configurable: true });
    const stageRef = { current: stage };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const display = lastClient().display;
    // display 1280x800 装进 stage 640x480 → min(0.5, 0.6) = 0.5
    expect(display.scale).not.toHaveBeenCalled();
    fireEvent(window, new Event('resize'));
    expect(display.scale).toHaveBeenCalledWith(expect.closeTo(0.5));
    // 挂载后的 display.onresize 走同一条 fitScale 闭包
    Object.defineProperty(stage, 'clientWidth', { value: 320, configurable: true });
    Object.defineProperty(stage, 'clientHeight', { value: 800, configurable: true });
    display.onresize?.();
    expect(display.scale).toHaveBeenCalledWith(expect.closeTo(0.25));
    unmount();
  });

  it('stage 无布局尺寸时不缩放（scale 保持初值）', async () => {
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    fireEvent(window, new Event('resize'));
    expect(lastClient().display.scale).not.toHaveBeenCalled();
    unmount();
  });

  it('票据申请失败 → phase=error 且携带后端原文', async () => {
    const ticketClient = fakeTicketClient({
      issueTicket: jest
        .fn()
        .mockRejectedValue(new Error('desktop:control permission required for control mode')),
    });
    const stageRef = { current: document.createElement('div') };
    let value: ReturnType<typeof useGuacamoleSession> | undefined;
    function Probe() {
      value = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    render(<Probe />);
    await waitForUI(() => expect(value!.phase).toBe('error'));
    expect(value!.error).toMatch(/permission required/);
  });

  it('非 Error 异常（字符串）走兜底文案', async () => {
    const ticketClient = fakeTicketClient({
      issueTicket: jest.fn().mockRejectedValue('boom'),
    });
    const stageRef = { current: document.createElement('div') };
    let value: ReturnType<typeof useGuacamoleSession> | undefined;
    function Probe() {
      value = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    render(<Probe />);
    await waitForUI(() => expect(value!.phase).toBe('error'));
    expect(value!.error).toBe('桌面连接失败');
  });

  it('票据申请在卸载后才失败：catch 静默，不回写已卸载组件', async () => {
    let rejectTicket: (e: Error) => void = () => {};
    const ticketClient = fakeTicketClient({
      issueTicket: jest.fn().mockImplementation(
        () =>
          new Promise((_resolve, reject) => {
            rejectTicket = reject;
          }),
      ),
    });
    const stageRef = { current: document.createElement('div') };
    const errors: string[] = [];
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    unmount();
    rejectTicket(new Error('late failure'));
    await Promise.resolve();
    await Promise.resolve();
    // 已卸载：既不建 client 也不抛错（错误被吞在 cancelled 分支）
    expect(MockedClient).not.toHaveBeenCalled();
    expect(errors).toEqual([]);
  });

  it('卸载发生在 display 挂载与 connect 之间：不误报 connected', async () => {
    const stage = document.createElement('div');
    let unmountFn: () => void = () => {};
    // 在 appendChild（即挂载 display 的那一刻）同步卸载，覆盖「挂载中途
    // 组件消失」这条竞态：正常代码必须识别 cancelled 而不是宣告已连接
    const appendChild = stage.appendChild.bind(stage);
    jest.spyOn(stage, 'appendChild').mockImplementation(((node: Node) => {
      const result = appendChild(node as never);
      unmountFn();
      return result;
    }) as never);
    const stageRef = { current: stage };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const utils = render(<Probe />);
    unmountFn = utils.unmount;
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    expect(() => utils.unmount()).not.toThrow();
  });

  it('stage ref 为空时跳过挂载且不建输入面', async () => {
    const ticketClient = fakeTicketClient();
    const stageRef = { current: null };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    render(<Probe />);
    await waitForUI(() => expect(ticketClient.issueTicket).toHaveBeenCalled());
    expect(MockedKeyboard).not.toHaveBeenCalled();
  });

  it('onerror 回调把已连接会话打回 error', async () => {
    let value: ReturnType<typeof useGuacamoleSession> | undefined;
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      value = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    render(<Probe />);
    await waitForUI(() => expect(value!.phase).toBe('connected'));
    lastClient().onerror?.({ code: 0x0200, message: 'err' });
    await waitForUI(() => expect(value!.phase).toBe('error'));
    expect(value!.error).toMatch(/异常断开/);
  });

  it('参数变化即重建会话（票据一次性不可复用）', async () => {
    const stageRef = { current: document.createElement('div') };
    function Probe({ agentId }: { agentId: string }) {
      useGuacamoleSession({
        active: true,
        params: { ...BASE_PARAMS, agentId },
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { rerender, unmount } = render(<Probe agentId="a1" />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalledTimes(1));
    const first = lastClient();
    rerender(<Probe agentId="a2" />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalledTimes(2));
    expect(first.disconnect).toHaveBeenCalled();
    unmount();
  });

  it('卸载发生在建连途中：不再 setState 也不建 client', async () => {
    let resolveTicket: (v: { ticket: string; expiresAt: string }) => void = () => {};
    const ticketClient = fakeTicketClient({
      issueTicket: jest.fn().mockImplementation(
        () =>
          new Promise<{ ticket: string; expiresAt: string }>((resolve) => {
            resolveTicket = resolve;
          }),
      ),
    });
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    unmount();
    resolveTicket({ ticket: 'late', expiresAt: 'x' });
    await Promise.resolve();
    await Promise.resolve();
    expect(MockedClient).not.toHaveBeenCalled();
  });

  it('disconnect 抛异常不影响清理（票据超时兜底）', async () => {
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    lastClient().disconnect.mockImplementation(() => {
      throw new Error('already closed');
    });
    expect(() => unmount()).not.toThrow();
  });

  it('stage ref 在 cleanup 时为 null 也安全', async () => {
    const stageRef = { current: document.createElement('div') as HTMLDivElement | null };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    stageRef.current = null;
    expect(() => unmount()).not.toThrow();
  });

  // ---------- 剪贴板（设计 §14） ----------

  it('onclipboard 文本流：逐块 ack + 汇总 + 回调上层', async () => {
    const onClipboard = jest.fn();
    let value: ReturnType<typeof useGuacamoleSession> | undefined;
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      value = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
        onClipboard,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(value!.phase).toBe('connected'));

    const { stream, emitBlob, emitEnd } = fakeInputStream();
    lastClient().onclipboard(stream, 'text/plain');
    emitBlob(guacEncodeBase64('远端复'));
    emitBlob(guacEncodeBase64('制内容'));
    emitEnd();

    await waitForUI(() => expect(value!.clipboard).toBe('远端复制内容'));
    expect(stream.sendAck).toHaveBeenCalledTimes(2);
    expect(stream.sendAck).toHaveBeenCalledWith('ok', 0x0000);
    expect(onClipboard).toHaveBeenCalledWith('远端复制内容');
    unmount();
  });

  it('非 text/* 剪贴板流以 UNSUPPORTED ack 拒绝', async () => {
    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const sendAck = jest.fn();
    lastClient().onclipboard({ sendAck }, 'image/png');
    expect(sendAck).toHaveBeenCalledWith('unsupported', 0x0100);
    unmount();
  });

  it('空剪贴板流不覆盖已有内容（onend 时 text 为空）', async () => {
    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const { emitEnd } = fakeInputStream();
    lastClient().onclipboard(fakeInputStream().stream, 'text/plain');
    emitEnd();
    expect(result.current!.clipboard).toBe('');
    unmount();
  });

  it('sendClipboard 走 text/plain 单流 + base64 + sendEnd', async () => {
    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();
    result.current!.sendClipboard('  本地内容  ');
    const stream = client.createClipboardStream.mock.results[0].value;
    expect(client.createClipboardStream).toHaveBeenCalledWith('text/plain');
    expect(stream.sendBlob).toHaveBeenCalledWith(guacEncodeBase64('本地内容'));
    expect(stream.sendEnd).toHaveBeenCalled();
    unmount();
  });

  it('sendClipboard 在未建连 / 空文本时为 no-op', async () => {
    const { result } = renderHook(false);
    result.current!.sendClipboard('x');
    expect(MockedClient).not.toHaveBeenCalled();
  });

  it('sendClipboard 成功建连后仍遇空文本为 no-op', async () => {
    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();
    result.current!.sendClipboard('   ');
    expect(client.createClipboardStream).not.toHaveBeenCalled();
    unmount();
  });

  it('剪贴板 ack 失败经 onNotify 上报错误码', async () => {
    const onNotify = jest.fn();
    const { result, unmount } = renderHook(true, BASE_PARAMS, { onNotify });
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();
    result.current!.sendClipboard('x');
    const stream = client.createClipboardStream.mock.results[0].value;
    stream.onack({ code: 0x0200, message: 'nope' });
    expect(onNotify).toHaveBeenCalledWith('error', '剪贴板发送失败（0x200）');
    // 成功 ack 不报错
    stream.onack({ code: 0x0000, message: 'ok' });
    expect(onNotify).toHaveBeenCalledTimes(1);
    unmount();
  });

  // ---------- 文件传输（设计 §15） ----------

  it('uploadFiles 逐文件 createFileStream + BlobWriter + 完成后 sendEnd', async () => {
    const onNotify = jest.fn();
    const { result, unmount } = renderHook(true, BASE_PARAMS, { onNotify });
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();

    const file = new File(['bytes'], 'report.txt', { type: 'text/plain' });
    const list = { length: 1, 0: file } as unknown as FileList;
    result.current!.uploadFiles(list);

    expect(client.createFileStream).toHaveBeenCalledWith('text/plain', 'report.txt');
    const stream = client.createFileStream.mock.results[0].value;
    expect(MockedBlobWriter).toHaveBeenCalledWith(stream);
    const writer = MockedBlobWriter.mock.results[0].value;
    expect(writer.sendBlob).toHaveBeenCalledWith(file);

    writer.oncomplete();
    expect(stream.sendEnd).toHaveBeenCalled();
    expect(onNotify).toHaveBeenCalledWith('success', 'report.txt 上传完成');

    writer.onerror();
    expect(onNotify).toHaveBeenCalledWith('error', 'report.txt 上传失败');
    unmount();
  });

  it('无 mimetype 的文件回退 application/octet-stream', async () => {
    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const file = new File(['x'], 'bin');
    result.current!.uploadFiles({ length: 1, 0: file } as unknown as FileList);
    expect(lastClient().createFileStream).toHaveBeenCalledWith('application/octet-stream', 'bin');
    unmount();
  });

  it('uploadFiles 在未建连 / 空列表 / null 时为 no-op', async () => {
    const idle = renderHook(false);
    idle.result.current!.uploadFiles({ length: 1 } as unknown as FileList);
    expect(MockedClient).not.toHaveBeenCalled();
    idle.unmount();

    const { result, unmount } = renderHook(true);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();
    result.current!.uploadFiles(null);
    result.current!.uploadFiles({ length: 0 } as unknown as FileList);
    expect(client.createFileStream).not.toHaveBeenCalled();
    unmount();
  });

  it('onfile 聚合并上交 Blob（文件名兜底 remote-file）', async () => {
    const onFile = jest.fn();
    const { result, unmount } = renderHook(true, BASE_PARAMS, { onFile });
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    const client = lastClient();

    const first = fakeInputStream();
    client.onfile(first.stream, 'dump.txt', 'text/plain');
    first.emitBlob(btoa('part-1'));
    first.emitBlob(btoa('part-2'));
    first.emitEnd();
    expect(first.stream.sendAck).toHaveBeenCalledTimes(2);
    await waitForUI(() => expect(onFile).toHaveBeenCalled());
    expect(onFile.mock.calls[0][0].filename).toBe('dump.txt');
    expect(onFile.mock.calls[0][0].blob.size).toBe(12);

    // 空文件名兜底 + 无 mimetype 兜底
    const second = fakeInputStream();
    client.onfile(second.stream, '', '');
    second.emitEnd();
    await waitForUI(() => expect(onFile).toHaveBeenCalledTimes(2));
    expect(onFile.mock.calls[1][0].filename).toBe('remote-file');
    expect(onFile.mock.calls[1][0].blob.type).toBe('application/octet-stream');
    unmount();
  });

  it('会话已取消时 onfile 结束不再上交（避免卸载后触发下载）', async () => {
    const onFile = jest.fn();
    const stageRef = { current: document.createElement('div') };
    const ticketClient = fakeTicketClient();
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient,
        stageRef: stageRef as never,
        onFile,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const stream = fakeInputStream();
    lastClient().onfile(stream.stream, 'late.txt', 'text/plain');
    unmount();
    stream.emitEnd();
    expect(onFile).not.toHaveBeenCalled();
  });

  it('会话已取消时剪贴板 onend 不再 setState', async () => {
    const onClipboard = jest.fn();
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
        onClipboard,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const stream = fakeInputStream();
    lastClient().onclipboard(stream.stream, 'text/plain');
    unmount();
    stream.emitBlob(guacEncodeBase64('late'));
    stream.emitEnd();
    expect(onClipboard).not.toHaveBeenCalled();
  });

  it('会话已取消时 onerror 不再 setState', async () => {
    const stageRef = { current: document.createElement('div') };
    function Probe() {
      useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();
    unmount();
    expect(() => client.onerror?.({ code: 1, message: 'x' })).not.toThrow();
  });

  it('onfilesystem 上报 → state.filesystem 就绪；参数变化重建会话即复位', async () => {
    const stageRef = { current: document.createElement('div') };
    const result: { current?: ReturnType<typeof useGuacamoleSession> } = {};
    function Probe({ agentId }: { agentId: string }) {
      result.current = useGuacamoleSession({
        active: true,
        params: { agentId, protocol: 'ssh' },
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { rerender, unmount } = render(<Probe agentId="agent-1" />);
    await waitForUI(() => expect(result.current!.phase).toBe('connected'));
    // SFTP 对象未上报前 filesystem 为空（工具栏据此禁用文件浏览）
    expect(result.current!.filesystem).toBeUndefined();
    lastClient().onfilesystem?.({ index: 0 }, '/');
    await waitForUI(() => expect(result.current!.filesystem).toBeDefined());
    // 任一参数变化 → 票据一次性必须重建会话 → 旧 SFTP 对象作废，复位为空
    rerender(<Probe agentId="agent-2" />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalledTimes(2));
    expect(result.current!.filesystem).toBeUndefined();
    unmount();
  });

  it('会话已取消时 onfilesystem 不再 setState', async () => {
    const stageRef = { current: document.createElement('div') };
    const result: { current?: ReturnType<typeof useGuacamoleSession> } = {};
    function Probe() {
      result.current = useGuacamoleSession({
        active: true,
        params: BASE_PARAMS,
        ticketClient: probeClient,
        stageRef: stageRef as never,
      });
      return null;
    }
    const { unmount } = render(<Probe />);
    await waitForUI(() => expect(MockedClient).toHaveBeenCalled());
    const client = lastClient();
    unmount();
    expect(() => client.onfilesystem?.({ index: 0 }, '/')).not.toThrow();
    expect(result.current!.filesystem).toBeUndefined();
  });
});

// ---------- 工具栏 ----------
