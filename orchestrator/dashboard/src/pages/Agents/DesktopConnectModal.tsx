import React, { useEffect, useState } from 'react';
import { useIntl } from '@umijs/max';
import { Button, Checkbox, Input, message, Modal, Select, Space, Typography } from 'antd';
import type { RemoteProtocol } from '@/services/remote';
import {
  getVaultStatus,
  listVaultCredentials,
  saveVaultCredential,
  unlockVault,
  type VaultEntryMeta,
  type VaultStatus,
} from '@/services/vault';
import RemoteDesktopModal from '@/components/RemoteDesktopModal';

const { Text } = Typography;

/** 远程桌面连接表单初值：行操作按 agent 平台预填（android→vnc，其余→rdp） */
export interface DesktopConnectForm {
  agentId: string;
  protocol: RemoteProtocol;
  port: string;
  username: string;
  password: string;
  readOnly: boolean;
  record: boolean;
  /** 从保险箱取已存凭据填充缺失字段（服务端注入，显式输入优先） */
  useSaved: boolean;
  /** 连接同时把本次输入保存进保险箱 */
  saveToVault: boolean;
}

// 远程桌面（Guacamole 像素面）连接流程：连接参数表单 + 凭据保险箱探测/内联
// 解锁 + 确认后建连（RemoteDesktopModal 一次性票据 + WS 隧道，关闭即断开）。
// 表单态与保险箱探测态均为组件内部状态，父级只持有「打开动作」的初值。
const DesktopConnectModal: React.FC<{
  form: DesktopConnectForm | null;
  onCancel: () => void;
}> = ({ form, onCancel }) => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });

  // 表单草稿：父级 row 操作重设 form 初值时同步；编辑只在草稿上发生
  const [draft, setDraft] = useState<DesktopConnectForm | null>(form);
  useEffect(() => {
    setDraft(form);
  }, [form]);

  // ===== 凭据保险箱（连接表单探测态） =====
  const [vaultStatus, setVaultStatus] = useState<VaultStatus | null>(null);
  const [vaultEntries, setVaultEntries] = useState<VaultEntryMeta[]>([]);
  const [unlockInput, setUnlockInput] = useState('');
  const [unlockBusy, setUnlockBusy] = useState(false);

  // 连接表单打开/换目标时探测保险箱：已有该目标的已存凭据且解锁 → 默认
  // 启用快捷调用（用户名/密码可留空）。探测失败静默（保险箱不可用时退回
  // 手输，不阻断连接）。
  const desktopFormOpen = draft !== null;
  useEffect(() => {
    if (!desktopFormOpen || !draft) {
      return undefined;
    }
    let cancelled = false;
    Promise.all([getVaultStatus(), listVaultCredentials()])
      .then(([status, entries]) => {
        if (cancelled) {
          return;
        }
        setVaultStatus(status);
        setVaultEntries(entries);
        setDraft((prev) => {
          if (!prev) {
            return prev;
          }
          const hit = entries.some(
            (entry) => entry.agentId === prev.agentId && entry.protocol === prev.protocol,
          );
          return { ...prev, useSaved: hit && status.unlocked };
        });
      })
      .catch(() => undefined);
    return () => {
      cancelled = true;
    };
    // 探测只在弹窗开闭/目标切换时触发；输入框打字不重查
  }, [desktopFormOpen, draft?.agentId, draft?.protocol]);

  // 连接表单内联解锁：解锁成功即刷新探测并默认启用取用
  const inlineUnlock = async () => {
    if (!unlockInput) {
      return;
    }
    setUnlockBusy(true);
    try {
      await unlockVault(unlockInput);
      const [status, entries] = await Promise.all([getVaultStatus(), listVaultCredentials()]);
      setVaultStatus(status);
      setVaultEntries(entries);
      setUnlockInput('');
      setDraft((prev) => {
        if (!prev) {
          return prev;
        }
        const hit = entries.some(
          (entry) => entry.agentId === prev.agentId && entry.protocol === prev.protocol,
        );
        return { ...prev, useSaved: hit };
      });
      message.success('保险箱已解锁');
    } catch (err) {
      message.error(err instanceof Error ? err.message : '解锁失败');
    } finally {
      setUnlockBusy(false);
    }
  };

  // 确认后待连目标（触发 RemoteDesktopModal 建连）
  const [desktopTarget, setDesktopTarget] = useState<{
    agentId: string;
    protocol: RemoteProtocol;
    port: string;
    username: string;
    password: string;
    readOnly: boolean;
    record: boolean;
    useSaved: boolean;
  } | null>(null);

  // 表单确认：先按需落库保险箱，再把表单参数固化为连接目标。
  // 保存失败不阻断连接——手输凭据仍有效。
  const openDesktop = async () => {
    if (!draft) {
      return;
    }
    const formValue = { ...draft };
    if (formValue.saveToVault && vaultStatus?.unlocked) {
      try {
        await saveVaultCredential({
          agentId: formValue.agentId,
          protocol: formValue.protocol,
          port: formValue.port ? Number(formValue.port) : undefined,
          label: `${formValue.agentId}·${formValue.protocol.toUpperCase()}`,
          username: formValue.username,
          password: formValue.password || undefined,
        });
        formValue.useSaved = true; // 刚存的这条本次连接即取用
        message.success('凭据已保存到保险箱');
      } catch (err) {
        message.warning(err instanceof Error ? err.message : '保存到保险箱失败，继续连接');
      }
    }
    setDesktopTarget(formValue);
    onCancel();
  };

  return (
    <>
      <Modal
        open={!!draft}
        title={draft ? `远程桌面 - ${draft.agentId}` : ''}
        width={420}
        onOk={openDesktop}
        okText="连接"
        onCancel={onCancel}
        destroyOnClose
      >
        <Space direction="vertical" style={{ width: '100%' }} size="middle">
          <Select
            style={{ width: '100%' }}
            value={draft?.protocol}
            onChange={(value) => draft && setDraft({ ...draft, protocol: value })}
            options={[
              { value: 'rdp', label: 'RDP (Windows 桌面)' },
              { value: 'vnc', label: 'VNC (macOS / Linux / Android)' },
              { value: 'ssh', label: 'SSH (终端)' },
            ]}
          />
          <Input
            placeholder={
              draft?.protocol === 'ssh'
                ? '端口（默认 22）'
                : draft?.protocol === 'vnc'
                  ? '端口（默认 5900）'
                  : '端口（默认 3389）'
            }
            value={draft?.port}
            onChange={(e) => draft && setDraft({ ...draft, port: e.target.value })}
          />
          <Input
            placeholder="用户名（可选）"
            value={draft?.username}
            onChange={(e) => draft && setDraft({ ...draft, username: e.target.value })}
          />
          <Input
            placeholder={draft?.useSaved ? '密码（已存凭据可留空）' : '密码（可选）'}
            type="password"
            value={draft?.password}
            onChange={(e) => draft && setDraft({ ...draft, password: e.target.value })}
          />
          {/* 凭据保险箱（快捷调用）：已存且解锁 → 默认取用；未解锁 → 内联解锁；
              未设置 → 指引到工具栏管理弹窗 */}
          {vaultStatus?.configured && !vaultStatus.unlocked && (
            <Space.Compact style={{ width: '100%' }}>
              <Input.Password
                placeholder="保险箱已锁定——输入主口令解锁"
                value={unlockInput}
                onChange={(e) => setUnlockInput(e.target.value)}
                onPressEnter={inlineUnlock}
              />
              <Button loading={unlockBusy} onClick={inlineUnlock}>
                解锁
              </Button>
            </Space.Compact>
          )}
          {vaultStatus?.unlocked &&
            draft &&
            vaultEntries.some(
              (entry) => entry.agentId === draft.agentId && entry.protocol === draft.protocol,
            ) && (
              <Checkbox
                checked={draft.useSaved}
                onChange={(e) => draft && setDraft({ ...draft, useSaved: e.target.checked })}
              >
                使用保险箱已存凭据（
                {(() => {
                  const hit = vaultEntries.find(
                    (entry) => entry.agentId === draft.agentId && entry.protocol === draft.protocol,
                  );
                  return hit?.label || hit?.username || '已保存';
                })()}
                ，用户名/密码留空自动填充）
              </Checkbox>
            )}
          {vaultStatus?.unlocked && (
            <Checkbox
              checked={draft?.saveToVault}
              onChange={(e) => draft && setDraft({ ...draft, saveToVault: e.target.checked })}
            >
              保存到保险箱（下次免输入）
            </Checkbox>
          )}
          {vaultStatus && !vaultStatus.configured && (
            <Text type="secondary">
              保险箱未设置——在工具栏「凭据保险箱」设置主口令后，凭据可加密保存并自动取用
            </Text>
          )}
          <Select
            style={{ width: '100%' }}
            value={draft?.readOnly ? 'view' : 'control'}
            onChange={(value) => draft && setDraft({ ...draft, readOnly: value === 'view' })}
            options={[
              { value: 'view', label: '监看（只读）' },
              { value: 'control', label: '接管（需 desktop:control 权限）' },
            ]}
          />
          <Checkbox
            checked={draft?.record}
            onChange={(e) => draft && setDraft({ ...draft, record: e.target.checked })}
          >
            {formatMessage('pages.agents.recordings.checkboxLabel')}
          </Checkbox>
        </Space>
      </Modal>
      {desktopTarget && (
        <RemoteDesktopModal
          open
          agentId={desktopTarget.agentId}
          protocol={desktopTarget.protocol}
          port={desktopTarget.port ? Number(desktopTarget.port) : undefined}
          username={desktopTarget.username || undefined}
          password={desktopTarget.password || undefined}
          readOnly={desktopTarget.readOnly}
          record={desktopTarget.record}
          useSaved={desktopTarget.useSaved || undefined}
          onCancel={() => setDesktopTarget(null)}
        />
      )}
    </>
  );
};

export default DesktopConnectModal;
