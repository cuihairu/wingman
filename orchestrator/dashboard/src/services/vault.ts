/**
 * 密钥保险箱服务（远程凭据加密存储 + 连接快捷调用）。
 * @description 后端见 orchestrator/server internal/handlers/vault.go：
 *              主口令 PBKDF2 派生 KEK 包裹 per-user DEK，解锁期 DEK 驻留
 *              服务端内存（空闲 30 分钟自动锁回）；列表/导出永不出明文。
 */

import { request } from '@umijs/max';
import type { RemoteProtocol } from '@/components/RemoteDesktop/types';
import type { ApiResponse } from './wingman';

/** 保险箱状态（弹窗打开时探测「已存凭据」与解锁态） */
export interface VaultStatus {
  /** 是否已设置主口令（false = 首次使用须先 setup） */
  configured: boolean;
  /** 当前是否解锁（未解锁时保存/取用均不可用） */
  unlocked: boolean;
  /** 解锁到期时刻（RFC3339；未解锁为空） */
  unlockedUntil?: string;
  /** 空闲自动锁定秒数 */
  autoLockAfterS: number;
}

/** 一条已存凭据的元数据（无密文——明文/密文都不出服务端） */
export interface VaultEntryMeta {
  id: number;
  agentId: string;
  protocol: RemoteProtocol;
  port: number;
  label: string;
  username: string;
  domain?: string;
  hasPassword: boolean;
  hasSecret: boolean;
  updatedAt: string;
}

/** 保存入参（agentId + protocol 必填；秘密字段留空 = 保留旧值） */
export interface VaultSaveInput {
  agentId: string;
  protocol: RemoteProtocol;
  /** 0/缺省 = 协议默认端口（服务端归一） */
  port?: number;
  label?: string;
  username?: string;
  domain?: string;
  password?: string;
  privateKey?: string;
}

async function vaultRequest<T>(
  url: string,
  options: { method: string; data?: unknown; params?: Record<string, string> } = {
    method: 'GET',
  },
): Promise<T> {
  const res = await request<ApiResponse<T>>(url, options);
  if (!res?.success) {
    throw new Error(res?.error || '保险箱请求失败');
  }
  // data 缺失仅在「成功但无载荷」端点出现（lock/changePassword），调用方
  // 均以 Promise<void> 消化 undefined
  return res.data as T;
}

/** 保险箱状态 */
export function getVaultStatus(): Promise<VaultStatus> {
  return vaultRequest<VaultStatus>('/api/remote/vault/status');
}

/** 首次设置主口令（设置即解锁）；已设置返回 409 */
export function setupVault(masterPassword: string): Promise<{ unlockedUntil: string }> {
  return vaultRequest<{ unlockedUntil: string }>('/api/remote/vault/setup', {
    method: 'POST',
    data: { masterPassword },
  });
}

/** 修改主口令（DEK 重包裹，已存凭据不受影响） */
export function changeVaultPassword(
  currentPassword: string,
  newPassword: string,
): Promise<void> {
  return vaultRequest<void>('/api/remote/vault/change-password', {
    method: 'POST',
    data: { currentPassword, newPassword },
  });
}

/** 解锁（主口令校验；错误返回 401） */
export function unlockVault(masterPassword: string): Promise<{ unlockedUntil: string }> {
  return vaultRequest<{ unlockedUntil: string }>('/api/remote/vault/unlock', {
    method: 'POST',
    data: { masterPassword },
  });
}

/** 锁定（幂等）：立即丢弃服务端内存中的 DEK */
export function lockVault(): Promise<void> {
  return vaultRequest<void>('/api/remote/vault/lock', { method: 'POST' });
}

/** 凭据列表（元数据） */
export function listVaultCredentials(): Promise<VaultEntryMeta[]> {
  return vaultRequest<VaultEntryMeta[]>('/api/remote/vault/credentials').then(
    (data) => data || [],
  );
}

/** 保存/更新一条凭据（须解锁；同目标重复保存视为更新） */
export function saveVaultCredential(entry: VaultSaveInput): Promise<VaultEntryMeta> {
  return vaultRequest<VaultEntryMeta>('/api/remote/vault/credentials', {
    method: 'PUT',
    data: entry,
  });
}

/** 删除一条凭据（须解锁） */
export function deleteVaultCredential(id: number): Promise<void> {
  return vaultRequest<void>(`/api/remote/vault/credentials/${id}`, { method: 'DELETE' });
}

/**
 * 导出保险箱密文束（须解锁 + 显式确认）。内容为主口令加密的密文，
 * 不含任何明文凭据——隐私默认：凭据不出本机，导出须明示确认。
 */
export function exportVaultBundle(): Promise<Record<string, unknown>> {
  return vaultRequest<Record<string, unknown>>('/api/remote/vault/export', {
    method: 'GET',
    params: { confirm: 'true' },
  });
}
