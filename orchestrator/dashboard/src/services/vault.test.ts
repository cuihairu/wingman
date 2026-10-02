/**
 * 保险箱服务层测试：端点/方法/载荷形状 + 错误透传 + 列表空兜底。
 * 加密与生命周期语义在后端（Go vault_test.go）覆盖，这里只钉「前端说话
 * 的形状」——端点拼错是这一层唯一能自己犯的错。
 */
import { request } from '@umijs/max';
import {
  changeVaultPassword,
  deleteVaultCredential,
  exportVaultBundle,
  getVaultStatus,
  listVaultCredentials,
  lockVault,
  saveVaultCredential,
  setupVault,
  unlockVault,
} from './vault';

jest.mock('@umijs/max', () => ({
  request: jest.fn(),
}));

const mockedRequest = request as jest.MockedFunction<typeof request>;

describe('services/vault', () => {
  beforeEach(() => {
    mockedRequest.mockReset();
  });

  it('getVaultStatus GET status', async () => {
    mockedRequest.mockResolvedValueOnce({
      success: true,
      data: { configured: true, unlocked: false, autoLockAfterS: 1800 },
    });
    const status = await getVaultStatus();
    expect(status.configured).toBe(true);
    expect(mockedRequest).toHaveBeenCalledWith('/api/remote/vault/status', { method: 'GET' });
  });

  it('setup/unlock/lock/changePassword 各端点与载荷', async () => {
    mockedRequest.mockResolvedValue({ success: true, data: {} });
    await setupVault('master-pass');
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/remote/vault/setup', {
      method: 'POST',
      data: { masterPassword: 'master-pass' },
    });
    await unlockVault('master-pass');
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/remote/vault/unlock', {
      method: 'POST',
      data: { masterPassword: 'master-pass' },
    });
    await lockVault();
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/remote/vault/lock', { method: 'POST' });
    await changeVaultPassword('old-pass-1', 'new-pass-1');
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/remote/vault/change-password', {
      method: 'POST',
      data: { currentPassword: 'old-pass-1', newPassword: 'new-pass-1' },
    });
  });

  it('saveVaultCredential PUT credentials（秘密字段留空 = 保留旧值的改标签形状）', async () => {
    mockedRequest.mockResolvedValue({ success: true, data: { id: 7 } });
    await saveVaultCredential({
      agentId: 'ag1',
      protocol: 'ssh',
      port: 22,
      label: '工作机',
      username: 'bob',
    });
    expect(mockedRequest).toHaveBeenCalledWith('/api/remote/vault/credentials', {
      method: 'PUT',
      data: { agentId: 'ag1', protocol: 'ssh', port: 22, label: '工作机', username: 'bob' },
    });
  });

  it('deleteVaultCredential DELETE 按 id', async () => {
    mockedRequest.mockResolvedValue({ success: true });
    await deleteVaultCredential(9);
    expect(mockedRequest).toHaveBeenCalledWith('/api/remote/vault/credentials/9', {
      method: 'DELETE',
    });
  });

  it('exportVaultBundle 必须带 confirm=true（隐私默认：导出须显式确认）', async () => {
    mockedRequest.mockResolvedValue({ success: true, data: { format: 'wingman-vault-v1' } });
    await exportVaultBundle();
    expect(mockedRequest).toHaveBeenCalledWith('/api/remote/vault/export', {
      method: 'GET',
      params: { confirm: 'true' },
    });
  });

  it('listVaultCredentials 空 data 兜底为空数组', async () => {
    mockedRequest.mockResolvedValueOnce({ success: true });
    await expect(listVaultCredentials()).resolves.toEqual([]);
  });

  it('失败响应抛服务端 error 文案', async () => {
    mockedRequest.mockResolvedValueOnce({ success: false, error: 'vault locked' });
    await expect(listVaultCredentials()).rejects.toThrow('vault locked');
  });
});
