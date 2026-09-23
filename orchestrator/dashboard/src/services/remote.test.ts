/**
 * 远程桌面服务层测试：票据申请请求形状 + WS 路径拼接。
 */
import { request } from '@umijs/max';
import { createRemoteTicket, guacamoleWSPath } from './remote';

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

    it('success=false 时抛出后端 error', async () => {
      mockedRequest.mockResolvedValueOnce({ success: false, error: 'desktop:control permission required for control mode' });
      await expect(createRemoteTicket({ agentId: 'a', protocol: 'ssh' })).rejects.toThrow(
        'desktop:control permission required for control mode',
      );
    });

    it('响应缺 ticket 时抛出兜底错误', async () => {
      mockedRequest.mockResolvedValueOnce({ success: true, data: {} });
      await expect(createRemoteTicket({ agentId: 'a', protocol: 'vnc' })).rejects.toThrow('申请桌面连接票据失败');
    });
  });

  describe('guacamoleWSPath', () => {
    it('拼接 ws 隧道地址并编码票据', () => {
      expect(guacamoleWSPath('abc+def/1')).toBe('ws://localhost:8000/api/remote/guacamole?ticket=abc%2Bdef%2F1');
    });
  });
});
