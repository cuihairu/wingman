/**
 * 远程桌面公共件测试（设计 §7 第 3 条抽取边界）——出口契约与纯工具：
 *   - base64 工具（UTF-8 安全往返、二进制路径）
 *   - 协议能力派生（SSH/RDP 有文件通道、VNC 无）
 *   - 公共出口完整性（index.ts 契约面，cockpit 按此接入）
 *   - createWingmanTicketClient（票据/隧道路径转发）
 * 连接生命周期/工具栏/错误降级/主面板拆分见同目录各 *.test.tsx。
 *
 * guacamole-common-js 全程 mock（jsdom 无真实 WS）。
 */
import {
  createWingmanTicketClient,
  guacDecodeBase64,
  guacDecodeBase64ToBytes,
  guacEncodeBase64,
  protocolCapabilities,
  REMOTE_PROTOCOL_LABEL,
  type RemoteSessionParams,
} from './index';
import { createRemoteTicket, guacamoleWSPath } from '@/services/remote';

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

// ---------- 公共出口完整性（设计 §7 第 3 条的契约面） ----------

describe('公共出口（index.ts）', () => {
  it('四件边界 + 报表 + 工具全部可从出口取到（cockpit 按此接入）', async () => {
    const barrel = await import('./index');
    // 抽取边界四件
    expect(typeof barrel.useGuacamoleSession).toBe('function');
    expect(typeof barrel.RemoteErrorNotice).toBe('function');
    expect(typeof barrel.classifyRemoteError).toBe('function');
    expect(typeof barrel.RemoteDesktopToolbar).toBe('function');
    // 票据客户端
    expect(typeof barrel.createWingmanTicketClient).toBe('function');
    // 合成件与报表
    expect(typeof barrel.RemoteDesktopPanel).toBe('function');
    expect(typeof barrel.RemoteSessionReportModal).toBe('function');
    // base64 工具
    expect(typeof barrel.guacEncodeBase64).toBe('function');
    expect(typeof barrel.guacDecodeBase64).toBe('function');
    expect(typeof barrel.guacDecodeBase64ToBytes).toBe('function');
    // 类型与常量
    expect(typeof barrel.protocolCapabilities).toBe('function');
    expect(barrel.REMOTE_PROTOCOL_LABEL).toBeDefined();
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
    expect(client.tunnelURL('tk-9')).toBe('ws://host/api/remote/guacamole');
    expect(guacamoleWSPath).toHaveBeenCalledWith();
  });
});
