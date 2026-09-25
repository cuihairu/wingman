/**
 * 远程桌面服务层测试：票据申请请求形状（含 record）+ WS 路径拼接 +
 * 会话录像检索（列表/下载/删除）。
 */
import { request } from '@umijs/max';
import {
  createRemoteTicket,
  deleteRecording,
  downloadRecording,
  guacamoleWSPath,
  listRecordings,
} from './remote';

jest.mock('@umijs/max', () => ({
  request: jest.fn(),
}));

const mockedRequest = request as jest.MockedFunction<typeof request>;

describe('services/remote', () => {
  beforeEach(() => {
    mockedRequest.mockReset();
  });

  describe('createRemoteTicket', () => {
    it('POST /api/remote/tickets 并返回票据', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: true,
        data: { ticket: 'tk-1', expiresAt: '2026-09-23T12:00:00Z' },
      });

      const ticket = await createRemoteTicket({
        agentId: 'agent-1',
        protocol: 'rdp',
        port: 3389,
        username: 'ubuntu',
        password: 'secret',
        readOnly: true,
        width: 1280,
        height: 800,
      });

      expect(ticket.ticket).toBe('tk-1');
      expect(mockedRequest).toHaveBeenCalledWith('/api/remote/tickets', {
        method: 'POST',
        data: {
          agentId: 'agent-1',
          protocol: 'rdp',
          port: 3389,
          username: 'ubuntu',
          password: 'secret',
          readOnly: true,
          width: 1280,
          height: 800,
        },
      });
    });

    it('record 透传到请求体（会话录制，设计 §16）', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: true,
        data: { ticket: 'tk-rec', expiresAt: '2026-09-25T12:00:00Z' },
      });

      await createRemoteTicket({ agentId: 'agent-1', protocol: 'ssh', record: true });

      expect(mockedRequest).toHaveBeenCalledWith('/api/remote/tickets', {
        method: 'POST',
        data: expect.objectContaining({ protocol: 'ssh', record: true }),
      });
    });

    it('success=false 时抛出后端 error', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: false,
        error: 'desktop:control permission required for control mode',
      });
      await expect(createRemoteTicket({ agentId: 'a', protocol: 'ssh' })).rejects.toThrow(
        'desktop:control permission required for control mode',
      );
    });

    it('响应缺 ticket 时抛出兜底错误', async () => {
      mockedRequest.mockResolvedValueOnce({ success: true, data: {} });
      await expect(createRemoteTicket({ agentId: 'a', protocol: 'vnc' })).rejects.toThrow(
        '申请桌面连接票据失败',
      );
    });
  });

  describe('guacamoleWSPath', () => {
    it('拼接 ws 隧道地址并编码票据', () => {
      expect(guacamoleWSPath('abc+def/1')).toBe(
        'ws://localhost:8000/api/remote/guacamole?ticket=abc%2Bdef%2F1',
      );
    });

    it('https 页面升级为 wss（防把 TLS 页面降级成明文隧道）', () => {
      // jsdom 的 location.protocol 不可改写，按签名注入来源
      expect(
        guacamoleWSPath('tk', { protocol: 'https:', host: 'desk.example.com' } as Location),
      ).toBe('wss://desk.example.com/api/remote/guacamole?ticket=tk');
    });

    it('http 页面保持 ws（显式注入来源，与默认 location 行为一致）', () => {
      expect(
        guacamoleWSPath('tk', { protocol: 'http:', host: 'desk.example.com' } as Location),
      ).toBe('ws://desk.example.com/api/remote/guacamole?ticket=tk');
    });
  });

  describe('listRecordings', () => {
    it('GET /api/remote/recordings 返回列表', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: true,
        data: [{ name: 'agent-1-sess-9.mjs', sizeBytes: 1024, modifiedAt: '2026-09-25T10:00:00Z' }],
      });

      const list = await listRecordings();

      expect(mockedRequest).toHaveBeenCalledWith('/api/remote/recordings');
      expect(list).toHaveLength(1);
      expect(list[0].name).toBe('agent-1-sess-9.mjs');
    });

    it('success=false（501 未配置）抛出后端 error 供面板展示指引', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: false,
        error: 'session recording not configured',
      });
      await expect(listRecordings()).rejects.toThrow('session recording not configured');
    });

    it('success=false 且无 error 时抛兜底文案', async () => {
      mockedRequest.mockResolvedValueOnce({ success: false });
      await expect(listRecordings()).rejects.toThrow('获取会话录像列表失败');
    });

    it('成功但 data 缺失时返回空数组（面板不炸）', async () => {
      mockedRequest.mockResolvedValueOnce({ success: true });
      await expect(listRecordings()).resolves.toEqual([]);
    });
  });

  describe('downloadRecording', () => {
    beforeEach(() => {
      // jsdom 未实现 createObjectURL：下载路径需要手动补
      (URL as unknown as { createObjectURL: jest.Mock }).createObjectURL = jest.fn(
        () => 'blob:mock',
      );
      (URL as unknown as { revokeObjectURL: jest.Mock }).revokeObjectURL = jest.fn();
    });

    it('blob 响应触发浏览器下载（名字编码进路径）', async () => {
      mockedRequest.mockResolvedValueOnce(new Blob(['session-bytes']));

      await downloadRecording('agent-1-sess-9.mjs');

      expect(mockedRequest).toHaveBeenCalledWith(
        '/api/remote/recordings/agent-1-sess-9.mjs/download',
        {
          responseType: 'blob',
        },
      );
      expect(URL.createObjectURL).toHaveBeenCalled();
      expect(URL.revokeObjectURL).toHaveBeenCalledWith('blob:mock');
    });

    it('非 Blob 响应（如拦截器已解析为字符串）仍能触发下载', async () => {
      mockedRequest.mockResolvedValueOnce('raw-session-bytes');

      await downloadRecording('agent-1-sess-9.mjs');

      expect(URL.createObjectURL).toHaveBeenCalled();
      const blob = (URL.createObjectURL as jest.Mock).mock.calls[0][0] as Blob;
      expect(blob).toBeInstanceOf(Blob);
    });
  });

  describe('deleteRecording', () => {
    it('DELETE /api/remote/recordings/:name', async () => {
      mockedRequest.mockResolvedValueOnce({ success: true, data: null });

      await deleteRecording('gone.mjs');

      expect(mockedRequest).toHaveBeenCalledWith('/api/remote/recordings/gone.mjs', {
        method: 'DELETE',
      });
    });

    it('success=false 抛出后端 error', async () => {
      mockedRequest.mockResolvedValueOnce({
        success: false,
        error: 'desktop:control permission required',
      });
      await expect(deleteRecording('gone.mjs')).rejects.toThrow(
        'desktop:control permission required',
      );
    });

    it('success=false 且无 error 时抛兜底文案', async () => {
      mockedRequest.mockResolvedValueOnce({ success: false });
      await expect(deleteRecording('gone.mjs')).rejects.toThrow('删除会话录像失败');
    });
  });
});
