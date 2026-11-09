/**
 * 远程桌面测试共享助手（拆分自 index.test.tsx 的模块级 helper 区）：
 * 假票据客户端 / 最后一个 Client 句柄 / 假 InputStream / 共享参数与
 * 放宽版 waitFor。require('guacamole-common-js') 依赖各测试文件顶部
 * hoist 的 jest.mock（ES import 会被 babel-jest 提升到 jest.mock 之后，
 * 语义与拆分前一致）。
 */
import { waitFor } from '@testing-library/react';
import { RTL_ASYNC_UTIL_TIMEOUT } from '@/testSupport/rtlWindow';
import type { RemoteSessionParams, TicketClient } from './index';

const G = require('guacamole-common-js');
const MockedClient = G.Client as jest.Mock;

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
 * 时「申请票据 → 建 client → setState」这条异步链偶尔超时会假红。窗口取
 * 共享自适应值（随 CPU 超售放大、空闲=5s，见 @/testSupport/rtlWindow）。
 * 只放宽**等待窗口**，不放宽任何断言。
 */
const WAIT_TIMEOUT = RTL_ASYNC_UTIL_TIMEOUT;
const waitForUI: typeof waitFor = (callback, options) =>
  waitFor(callback, { timeout: WAIT_TIMEOUT, ...options });

/**
 * 探针组件统一使用的稳定票据客户端。
 *
 * hook 把 ticketClient 放进 effect 依赖：若在渲染体内新建对象，每次重渲染
 * 都会重建会话（票据一次性 → 反复消耗申请 + 打断像素面），测试会陷入死循环。
 * 故每用例前在 beforeEach 造一个、组件内只引用。
 */

export { fakeTicketClient, lastClient, fakeInputStream, BASE_PARAMS, WAIT_TIMEOUT, waitForUI };
