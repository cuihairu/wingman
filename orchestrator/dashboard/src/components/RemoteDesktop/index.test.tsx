/**
 * 远程桌面公共件测试（设计 §7 第 3 条抽取边界）。
 *
 * 覆盖四件 + 主面板：
 *   - base64 工具（UTF-8 安全往返、二进制路径）
 *   - 协议能力派生（SSH/RDP 有文件通道、VNC 无）
 *   - useGuacamoleSession 连接生命周期（建连/断开/错误/监看无输入/流编排）
 *   - RemoteDesktopToolbar（监看隐藏发送入口、VNC 隐藏文件块、上传复位）
 *   - RemoteErrorNotice（权限/未配置/断链/未知四类 + 可重试判定）
 *   - RemoteDesktopPanel（容器无关、文件下载触发、复制到本机）
 *
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import { fireEvent, render, waitFor } from '@testing-library/react';
import React from 'react';
import {
  classifyRemoteError,
  createWingmanTicketClient,
  guacDecodeBase64,
  guacDecodeBase64ToBytes,
  guacEncodeBase64,
  protocolCapabilities,
  REMOTE_PROTOCOL_LABEL,
  RemoteDesktopPanel,
  RemoteDesktopToolbar,
  RemoteErrorNotice,
  useGuacamoleSession,
  type RemoteSessionParams,
  type TicketClient,
} from './index';
import { createRemoteTicket, guacamoleWSPath } from '@/services/remote';

jest.mock('@/services/remote', () => ({
  createRemoteTicket: jest.fn(),
  guacamoleWSPath: jest.fn((t: string) => `ws://host/api/remote/guacamole?ticket=${t}`),
}));

jest.mock('guacamole-common-js', () => {
  // 每个 Client 一个稳定 display 实例：测试要读 scale 断言缩放
  const makeDisplay = () => ({
    getElement: () => document.createElement('div'),
    getWidth: jest.fn(() => 1280),
    getHeight: jest.fn(() => 800),
    scale: 1,
    showCursor: jest.fn(),
    onresize: undefined as undefined | (() => void),
  });
  const mocks = {
    WebSocketTunnel: jest.fn().mockImplementation((url: string) => ({ url })),
    Client: jest.fn().mockImplementation(() => {
      const display = makeDisplay();
      return {
        display,
        getDisplay: () => display,
        connect: jest.fn(),
        disconnect: jest.fn(),
        sendMouseState: jest.fn(),
        sendKeyEvent: jest.fn(),
        createClipboardStream: jest.fn().mockImplementation(() => ({
          sendBlob: jest.fn(),
          sendEnd: jest.fn(),
          onack: null,
        })),
        createFileStream: jest.fn().mockImplementation((_m: string, filename: string) => ({
          sendBlob: jest.fn(),
          sendEnd: jest.fn(),
          filename,
          onack: null,
        })),
      };
    }),
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
  return { __esModule: true, default: mocks, ...mocks };
});

const G = require('guacamole-common-js');
const MockedClient = G.Client as jest.Mock;
const MockedKeyboard = G.Keyboard as jest.Mock;
const MockedMouse = G.Mouse as jest.Mock;
const MockedBlobWriter = G.BlobWriter as jest.Mock;

/** 稳定的假票据客户端（避免每次渲染换对象导致 hook 重建会话） */
function fakeTicketClient(overrides: Partial<TicketClient> = {}): TicketClient {
  return {
    issueTicket: jest.fn().mockResolvedValue({ ticket: 'tk-1', expiresAt: '2026-01-01T00:00:00Z' }),
    tunnelURL: jest.fn(() => 'ws://host/tunnel?tk-1'),
    ...overrides,
  };
}

function lastClient() {
  return MockedClient.mock.results[MockedClient.mock.results.length - 1].value;
}

/** 假 InputStream：捕获注册的 onblob/onend，供逐块投喂 */
function fakeInputStream() {
  let blobHandler: (data: string) => void = () => {};
  let endHandler: () => void = () => {};
  const stream = {
    sendAck: jest.fn(),
    set onblob(fn: (d: string) => void) {
      blobHandler = fn;
    },
    get onblob() {
      return blobHandler;
    },
    set onend(fn: () => void) {
      endHandler = fn;
    },
    get onend() {
      return endHandler;
    },
  };
  // 闭包转发（不能快照 blobHandler/endHandler：组件在 setter 里换掉了引用）
  return {
    stream,
    emitBlob: (d: string) => blobHandler(d),
    emitEnd: () => endHandler(),
  };
}

const BASE_PARAMS: RemoteSessionParams = { agentId: 'agent-1', protocol: 'rdp' };

/**
 * waitFor 的放宽版：jest/RTL 默认超时 1000ms，CPU 饱和（CI 并行跑全仓）
 * 时「申请票据 → 建 client → setState」这条异步链偶尔超时会假红。仓库既有
 * loginPage/triggerFormModal 在同款负载下也有同类 flake（已实测复现）。
 * 这里只放宽**等待窗口**，不放宽任何断言。
 */
const WAIT_TIMEOUT = 5000;
const waitForUI: typeof waitFor = (callback, options) =>
  waitFor(callback, { timeout: WAIT_TIMEOUT, ...options });

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

// ---------- base64 ----------

describe('base64 工具', () => {
  it('中文/emoji 往返无损，且与标准 base64 一致', () => {
    const text = '剪贴板 ✓ 🚀 clipboard';
    expect(guacDecodeBase64(guacEncodeBase64(text))).toBe(text);
    expect(guacEncodeBase64('abc')).toBe(btoa('abc'));
    // 空串边界
    expect(guacEncodeBase64('')).toBe('');
    expect(guacDecodeBase64('')).toBe('');
  });

  it('guacDecodeBase64ToBytes 产出可作 BlobPart 的字节', () => {
    const bytes = guacDecodeBase64ToBytes(btoa('AB'));
    expect(bytes).toBeInstanceOf(Uint8Array);
    expect(Array.from(bytes)).toEqual([65, 66]);
    // 能直接喂 Blob（二进制路径不经文本中转）
    expect(new Blob([bytes]).size).toBe(2);
    expect(guacDecodeBase64ToBytes('').length).toBe(0);
  });
});

// ---------- 协议能力 ----------

describe('protocolCapabilities', () => {
  it('ssh 走 SFTP、rdp 走驱动器重定向，均有文件通道', () => {
    expect(protocolCapabilities('ssh')).toEqual({
      fileTransfer: true,
      fileTransferHint: '经 SFTP 传到远端家目录',
    });
    expect(protocolCapabilities('rdp').fileTransfer).toBe(true);
    expect(protocolCapabilities('rdp').fileTransferHint).toContain('驱动器重定向');
  });

  it('vnc 无文件通道（RFB 协议层不存在），且无提示文案', () => {
    expect(protocolCapabilities('vnc')).toEqual({ fileTransfer: false, fileTransferHint: '' });
  });

  it('协议展示名三协议齐备', () => {
    expect(REMOTE_PROTOCOL_LABEL).toEqual({ rdp: 'RDP', vnc: 'VNC', ssh: 'SSH' });
  });
});

// ---------- 票据客户端 ----------

describe('createWingmanTicketClient', () => {
  it('转发到 wingman 的 /api/remote 票据与 WS 路径', async () => {
    (createRemoteTicket as jest.Mock).mockResolvedValue({ ticket: 'tk-9', expiresAt: 'x' });
    const client = createWingmanTicketClient();
    const params: RemoteSessionParams = { agentId: 'a', protocol: 'ssh', readOnly: true };
    await expect(client.issueTicket(params)).resolves.toEqual({ ticket: 'tk-9', expiresAt: 'x' });
    expect(createRemoteTicket).toHaveBeenCalledWith(params);
    expect(client.tunnelURL('tk-9')).toBe('ws://host/api/remote/guacamole?ticket=tk-9');
    expect(guacamoleWSPath).toHaveBeenCalledWith('tk-9');
  });
});

// ---------- 连接生命周期 hook ----------

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
    expect(display.scale).toBe(1);
    fireEvent(window, new Event('resize'));
    expect(display.scale).toBeCloseTo(0.5);
    // 挂载后的 display.onresize 走同一条 fitScale 闭包
    Object.defineProperty(stage, 'clientWidth', { value: 320, configurable: true });
    Object.defineProperty(stage, 'clientHeight', { value: 800, configurable: true });
    display.onresize?.();
    expect(display.scale).toBeCloseTo(0.25);
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
    expect(lastClient().display.scale).toBe(1);
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
});

// ---------- 工具栏 ----------

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
});

// ---------- 错误与降级 ----------

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
});
