/**
 * 凭据保险箱管理弹窗（添加由连接表单承担，这里管生命周期与条目）：
 * 主口令设置 / 解锁 / 锁定 / 改口令、条目列表（改标签 / 删除）、密文导出。
 *
 * 文案与远程桌面一族（Agents 页连接表单、会话录像、会话审计）保持一致，
 * 均为本仓中文直书；后端语义见 services/vault.ts 头注释。
 */
import { useCallback, useEffect, useState } from 'react';
import {
  Alert,
  Button,
  Form,
  Input,
  Modal,
  Popconfirm,
  Space,
  Table,
  Tag,
  Typography,
  message,
} from 'antd';
import { LockOutlined, UnlockOutlined, ExportOutlined, KeyOutlined } from '@ant-design/icons';
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
  type VaultEntryMeta,
  type VaultStatus,
} from '@/services/vault';
import { REMOTE_PROTOCOL_LABEL, type RemoteProtocol } from '@/components/RemoteDesktop';

const PROTOCOL_DEFAULT_PORT: Record<RemoteProtocol, number> = { rdp: 3389, vnc: 5900, ssh: 22 };

/** 解锁/设置/改口令共用的小表单弹窗 */
function MasterPasswordModal({
  open,
  mode,
  onCancel,
  onDone,
}: {
  open: boolean;
  /** setup = 设置（两次输入）；unlock = 解锁（一次）；change = 改口令（当前+新两次） */
  mode: 'setup' | 'unlock' | 'change';
  onCancel: () => void;
  onDone: () => void;
}) {
  const [form] = Form.useForm();
  const [busy, setBusy] = useState(false);

  const submit = async () => {
    const values = await form.validateFields();
    setBusy(true);
    try {
      if (mode === 'setup') {
        await setupVault(values.masterPassword);
        message.success('保险箱已设置并解锁');
      } else if (mode === 'unlock') {
        await unlockVault(values.masterPassword);
        message.success('保险箱已解锁');
      } else {
        await changeVaultPassword(values.currentPassword, values.newPassword);
        message.success('主口令已修改（已存凭据不受影响）');
      }
      form.resetFields();
      onDone();
    } catch (err) {
      message.error(err instanceof Error ? err.message : '操作失败');
    } finally {
      setBusy(false);
    }
  };

  return (
    <Modal
      open={open}
      title={
        mode === 'setup' ? '设置保险箱主口令' : mode === 'unlock' ? '解锁保险箱' : '修改主口令'
      }
      onOk={submit}
      confirmLoading={busy}
      onCancel={() => {
        form.resetFields();
        onCancel();
      }}
      destroyOnClose
    >
      <Form form={form} layout="vertical">
        {mode === 'change' && (
          <Form.Item name="currentPassword" label="当前主口令">
            <Input.Password autoFocus />
          </Form.Item>
        )}
        <Form.Item
          name={mode === 'change' ? 'newPassword' : 'masterPassword'}
          label="主口令"
          rules={[{ min: 8, message: '主口令至少 8 个字符' }]}
        >
          <Input.Password autoFocus={mode !== 'change'} />
        </Form.Item>
        {(mode === 'setup' || mode === 'change') && (
          <Form.Item
            name="masterPasswordAgain"
            label="再次输入主口令"
            dependencies={[mode === 'change' ? 'newPassword' : 'masterPassword']}
            rules={[
              ({ getFieldValue }) => ({
                validator(_, value) {
                  const first =
                    mode === 'change'
                      ? getFieldValue('newPassword')
                      : getFieldValue('masterPassword');
                  if (!value || value === first) {
                    return Promise.resolve();
                  }
                  return Promise.reject(new Error('两次输入不一致'));
                },
              }),
            ]}
          >
            <Input.Password />
          </Form.Item>
        )}
      </Form>
    </Modal>
  );
}

/** 凭据保险箱管理弹窗 */
const VaultManagerModal: React.FC<{ open: boolean; onClose: () => void }> = ({ open, onClose }) => {
  const [status, setStatus] = useState<VaultStatus | null>(null);
  const [entries, setEntries] = useState<VaultEntryMeta[]>([]);
  const [pwModal, setPwModal] = useState<'setup' | 'unlock' | 'change' | null>(null);
  const [editing, setEditing] = useState<VaultEntryMeta | null>(null);
  const [editLabel, setEditLabel] = useState('');
  const [busy, setBusy] = useState(false);

  const refresh = useCallback(async () => {
    try {
      const [s, list] = await Promise.all([getVaultStatus(), listVaultCredentials()]);
      setStatus(s);
      setEntries(list);
    } catch (err) {
      message.error(err instanceof Error ? err.message : '获取保险箱状态失败');
    }
  }, []);

  useEffect(() => {
    if (open) {
      refresh();
    }
  }, [open, refresh]);

  const handleLock = async () => {
    setBusy(true);
    try {
      await lockVault();
      message.success('保险箱已锁定');
      await refresh();
    } finally {
      setBusy(false);
    }
  };

  const handleDelete = async (entry: VaultEntryMeta) => {
    await deleteVaultCredential(entry.id);
    message.success('已删除');
    await refresh();
  };

  const handleSaveLabel = async () => {
    if (!editing) return;
    setBusy(true);
    try {
      // 秘密字段留空 = 保留旧值（后端语义），这里只改标签
      await saveVaultCredential({
        agentId: editing.agentId,
        protocol: editing.protocol,
        port: editing.port || PROTOCOL_DEFAULT_PORT[editing.protocol],
        label: editLabel,
        username: editing.username,
        domain: editing.domain,
      });
      setEditing(null);
      await refresh();
    } finally {
      setBusy(false);
    }
  };

  const handleExport = async () => {
    setBusy(true);
    try {
      const bundle = await exportVaultBundle();
      // 密文束落盘为 JSON（主口令加密、不含明文，可用主口令在未来恢复）
      const blob = new Blob([JSON.stringify(bundle, null, 2)], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'wingman-vault-export.json';
      a.click();
      URL.revokeObjectURL(url);
      message.success('已导出密文束（不含明文）');
    } catch (err) {
      message.error(err instanceof Error ? err.message : '导出失败');
    } finally {
      setBusy(false);
    }
  };

  const unlocked = status?.unlocked === true;

  const columns = [
    {
      title: '标签',
      dataIndex: 'label',
      render: (_: unknown, entry: VaultEntryMeta) =>
        editing?.id === entry.id ? (
          <Space>
            <Input
              size="small"
              style={{ width: 160 }}
              value={editLabel}
              onChange={(e) => setEditLabel(e.target.value)}
              onPressEnter={handleSaveLabel}
            />
            <Button size="small" type="link" disabled={busy} onClick={handleSaveLabel}>
              保存
            </Button>
            <Button size="small" type="link" onClick={() => setEditing(null)}>
              取消
            </Button>
          </Space>
        ) : (
          <Typography.Text strong>
            {entry.label || `${entry.agentId}-${entry.protocol}`}
          </Typography.Text>
        ),
    },
    { title: 'Agent', dataIndex: 'agentId' },
    {
      title: '协议',
      dataIndex: 'protocol',
      render: (protocol: RemoteProtocol) => (
        <Tag>{REMOTE_PROTOCOL_LABEL[protocol] || protocol}</Tag>
      ),
    },
    { title: '端口', dataIndex: 'port' },
    { title: '用户名', dataIndex: 'username' },
    {
      title: '更新时间',
      dataIndex: 'updatedAt',
      render: (v: string) => new Date(v).toLocaleString(),
    },
    {
      title: '操作',
      render: (_: unknown, entry: VaultEntryMeta) => (
        <Space>
          <Button
            size="small"
            type="link"
            disabled={!unlocked}
            onClick={() => {
              setEditing(entry);
              setEditLabel(entry.label);
            }}
          >
            改标签
          </Button>
          <Popconfirm
            title={`删除 ${entry.label || entry.agentId} 的已存凭据？`}
            disabled={!unlocked}
            onConfirm={() => handleDelete(entry)}
          >
            <Button size="small" type="link" danger disabled={!unlocked}>
              删除
            </Button>
          </Popconfirm>
        </Space>
      ),
    },
  ];

  return (
    <Modal
      open={open}
      title="凭据保险箱"
      width={860}
      onCancel={onClose}
      footer={null}
      destroyOnClose
    >
      <Space direction="vertical" style={{ width: '100%' }} size="middle">
        {status && !status.configured && (
          <Alert
            type="info"
            showIcon
            message="保险箱未设置"
            description="设置主口令后，连接远程桌面时可保存凭据并自动取用（主口令丢失不可恢复）。"
            action={
              <Button type="primary" onClick={() => setPwModal('setup')}>
                设置主口令
              </Button>
            }
          />
        )}
        {status && status.configured && !unlocked && (
          <Alert
            type="warning"
            showIcon
            message="保险箱已锁定"
            description="解锁后方可取用、修改或删除已存凭据。"
            action={
              <Button icon={<UnlockOutlined />} onClick={() => setPwModal('unlock')}>
                解锁
              </Button>
            }
          />
        )}
        {status && unlocked && (
          <Alert
            type="success"
            showIcon
            message={`已解锁 · 空闲 ${Math.round(status.autoLockAfterS / 60)} 分钟自动锁定`}
            action={
              <Space>
                <Button icon={<LockOutlined />} loading={busy} onClick={handleLock}>
                  锁定
                </Button>
                <Button icon={<KeyOutlined />} onClick={() => setPwModal('change')}>
                  修改主口令
                </Button>
                <Popconfirm
                  title="导出保险箱（密文束）？"
                  description="导出内容为主口令加密的密文，不含任何明文凭据。"
                  onConfirm={handleExport}
                >
                  <Button icon={<ExportOutlined />} loading={busy}>
                    导出
                  </Button>
                </Popconfirm>
              </Space>
            }
          />
        )}
        <Table<VaultEntryMeta>
          size="small"
          rowKey="id"
          columns={columns}
          dataSource={entries}
          pagination={false}
          locale={{ emptyText: '暂无已存凭据（连接时勾选「保存到保险箱」生成）' }}
        />
        <Typography.Text type="secondary">
          凭据只保存在本机服务端（主口令加密），连接时自动填充、不经浏览器回显；导出为密文且需显式确认。
        </Typography.Text>
      </Space>

      <MasterPasswordModal
        open={pwModal !== null}
        mode={pwModal ?? 'unlock'}
        onCancel={() => setPwModal(null)}
        onDone={async () => {
          setPwModal(null);
          await refresh();
        }}
      />
    </Modal>
  );
};

export default VaultManagerModal;
