import React, { useEffect, useState } from 'react';
import { Button, Card, Form, Input, Modal, Popconfirm, Select, Space, Switch, Table, Tag, App } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { useIntl } from '@umijs/max';
import {
  createUser,
  deleteUser,
  listRoles,
  listUsers,
  resetUserPassword,
  updateUser,
  type AdminRole,
  type AdminUser,
} from '@/services/api';

export default function UsersPage() {
  const { message } = App.useApp();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [rows, setRows] = useState<AdminUser[]>([]);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(false);
  const [page, setPage] = useState(1);
  const [size, setSize] = useState(20);
  const [keyword, setKeyword] = useState('');
  const [roles, setRoles] = useState<AdminRole[]>([]);

  const [createOpen, setCreateOpen] = useState(false);
  const [editTarget, setEditTarget] = useState<AdminUser | null>(null);
  const [resetTarget, setResetTarget] = useState<AdminUser | null>(null);
  const [createForm] = Form.useForm();
  const [editForm] = Form.useForm();
  const [resetForm] = Form.useForm();

  const load = async () => {
    setLoading(true);
    try {
      const resp = await listUsers({ page, size, keyword: keyword || undefined });
      setRows(resp.items || []);
      setTotal(resp.total || 0);
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.adminUsers.loadFailed'));
    } finally {
      setLoading(false);
    }
  };

  const loadRoles = async () => {
    try {
      const resp = await listRoles();
      setRoles(resp.items || []);
    } catch {
      // 忽略：角色加载失败时仍允许按角色码手动输入
    }
  };

  useEffect(() => {
    load();
  }, [page, size]);

  useEffect(() => {
    loadRoles();
  }, []);

  const roleColor = (role: string) => {
    if (role === 'admin') return 'red';
    if (role === 'operator') return 'blue';
    return 'default';
  };

  // 角色下拉选项：内置角色在名称后追加分隔标注
  const roleOptions = (list: AdminRole[]) =>
    list.map((r) => ({
      label: r.builtin
        ? intl.formatMessage({ id: 'pages.adminUsers.builtinRole' }, { name: r.name || r.code })
        : r.name || r.code,
      value: r.code,
    }));

  const submitCreate = async () => {
    try {
      const values = await createForm.validateFields();
      await createUser(values);
      message.success(formatMessage('pages.adminUsers.created'));
      setCreateOpen(false);
      createForm.resetFields();
      load();
    } catch (err: any) {
      if (err?.errorFields) return; // 校验错误
      message.error(err?.message || formatMessage('pages.adminUsers.createFailed'));
    }
  };

  const submitEdit = async () => {
    if (!editTarget) return;
    try {
      const values = await editForm.validateFields();
      await updateUser(editTarget.id, values);
      message.success(formatMessage('pages.adminUsers.updated'));
      setEditTarget(null);
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || formatMessage('pages.adminUsers.updateFailed'));
    }
  };

  const submitReset = async () => {
    if (!resetTarget) return;
    try {
      const values = await resetForm.validateFields();
      await resetUserPassword(resetTarget.id, values.newPassword);
      message.success(formatMessage('pages.adminUsers.passwordReset'));
      setResetTarget(null);
      resetForm.resetFields();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || formatMessage('pages.adminUsers.resetFailed'));
    }
  };

  const onDelete = async (id: number) => {
    try {
      await deleteUser(id);
      message.success(formatMessage('pages.adminUsers.deleted'));
      load();
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.adminUsers.deleteFailed'));
    }
  };

  const columns = [
    { title: 'ID', dataIndex: 'id', width: 70 },
    { title: formatMessage('pages.adminUsers.username'), dataIndex: 'username' },
    {
      title: formatMessage('pages.adminUsers.role'),
      dataIndex: 'role',
      render: (role: string) => <Tag color={roleColor(role)}>{role}</Tag>,
    },
    {
      title: formatMessage('pages.common.status'),
      dataIndex: 'active',
      render: (active: boolean) =>
        active ? (
          <Tag color="green">{formatMessage('pages.adminUsers.enabled')}</Tag>
        ) : (
          <Tag>{formatMessage('pages.adminUsers.disabled')}</Tag>
        ),
    },
    { title: formatMessage('pages.adminUsers.createdAt'), dataIndex: 'createdAt', width: 180 },
    {
      title: formatMessage('pages.common.action'),
      width: 280,
      render: (_: any, record: AdminUser) => (
        <Space>
          <Button
            size="small"
            onClick={() => {
              setEditTarget(record);
              editForm.setFieldsValue({ role: record.role, active: record.active });
            }}
          >
            {formatMessage('pages.adminUsers.edit')}
          </Button>
          <Button
            size="small"
            onClick={() => {
              setResetTarget(record);
              resetForm.resetFields();
            }}
          >
            {formatMessage('pages.adminUsers.resetPassword')}
          </Button>
          <Popconfirm
            title={intl.formatMessage({ id: 'pages.adminUsers.deleteConfirm' }, {
              name: record.username,
            })}
            onConfirm={() => onDelete(record.id)}
            okButtonProps={{ danger: true }}
          >
            <Button size="small" danger>
              {formatMessage('pages.adminUsers.delete')}
            </Button>
          </Popconfirm>
        </Space>
      ),
    },
  ];

  return (
    <PageContainer>
      <Card>
        <Space style={{ marginBottom: 16 }} wrap>
          <Input.Search
            placeholder={formatMessage('pages.adminUsers.searchPlaceholder')}
            allowClear
            value={keyword}
            onChange={(e) => setKeyword(e.target.value)}
            onSearch={() => {
              setPage(1);
              load();
            }}
            style={{ width: 240 }}
          />
          <Button
            type="primary"
            onClick={() => {
              createForm.resetFields();
              setCreateOpen(true);
            }}
          >
            {formatMessage('pages.adminUsers.create')}
          </Button>
        </Space>

        <Table
          rowKey="id"
          loading={loading}
          dataSource={rows}
          columns={columns as any}
          pagination={{
            current: page,
            pageSize: size,
            total,
            showSizeChanger: true,
            onChange: (p, s) => {
              setPage(p);
              setSize(s);
            },
          }}
        />
      </Card>

      <Modal
        title={formatMessage('pages.adminUsers.create')}
        open={createOpen}
        onOk={submitCreate}
        onCancel={() => setCreateOpen(false)}
        destroyOnClose
      >
        <Form form={createForm} layout="vertical" initialValues={{ role: 'viewer', active: true }}>
          <Form.Item
            name="username"
            label={formatMessage('pages.adminUsers.username')}
            rules={[{ required: true, message: formatMessage('pages.adminUsers.usernameRequired') }]}
          >
            <Input placeholder={formatMessage('pages.adminUsers.usernamePlaceholder')} />
          </Form.Item>
          <Form.Item
            name="password"
            label={formatMessage('pages.adminUsers.password')}
            rules={[{ required: true, message: formatMessage('pages.adminUsers.passwordRequired') }]}
          >
            <Input.Password placeholder={formatMessage('pages.adminUsers.passwordPlaceholder')} />
          </Form.Item>
          <Form.Item
            name="role"
            label={formatMessage('pages.adminUsers.role')}
            rules={[{ required: true }]}
          >
            <Select options={roleOptions(roles)} />
          </Form.Item>
          <Form.Item
            name="active"
            label={formatMessage('pages.adminUsers.enabled')}
            valuePropName="checked"
          >
            <Switch />
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={intl.formatMessage({ id: 'pages.adminUsers.editModalTitle' }, {
          name: editTarget?.username || '',
        })}
        open={!!editTarget}
        onOk={submitEdit}
        onCancel={() => setEditTarget(null)}
        destroyOnClose
      >
        <Form form={editForm} layout="vertical">
          <Form.Item
            name="role"
            label={formatMessage('pages.adminUsers.role')}
            rules={[{ required: true }]}
          >
            <Select options={roleOptions(roles)} />
          </Form.Item>
          <Form.Item
            name="active"
            label={formatMessage('pages.adminUsers.enabled')}
            valuePropName="checked"
          >
            <Switch />
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={intl.formatMessage({ id: 'pages.adminUsers.resetModalTitle' }, {
          name: resetTarget?.username || '',
        })}
        open={!!resetTarget}
        onOk={submitReset}
        onCancel={() => setResetTarget(null)}
        destroyOnClose
      >
        <Form form={resetForm} layout="vertical">
          <Form.Item
            name="newPassword"
            label={formatMessage('pages.adminUsers.newPassword')}
            rules={[
              { required: true, message: formatMessage('pages.adminUsers.newPasswordRequired') },
            ]}
          >
            <Input.Password placeholder={formatMessage('pages.adminUsers.passwordPlaceholder')} />
          </Form.Item>
        </Form>
      </Modal>
    </PageContainer>
  );
}
