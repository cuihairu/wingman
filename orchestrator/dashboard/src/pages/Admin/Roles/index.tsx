import React, { useEffect, useMemo, useState } from 'react';
import { Button, Card, Drawer, Form, Input, Modal, Popconfirm, Select, Space, Table, Tag, App, Empty } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { useIntl } from '@umijs/max';
import {
  createRole,
  deleteRole,
  listPermissionCatalog,
  listRoles,
  updateRole,
  type AdminPermission,
  type AdminRole,
} from '@/services/api';

export default function RolesPage() {
  const { message } = App.useApp();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [roles, setRoles] = useState<AdminRole[]>([]);
  const [permissions, setPermissions] = useState<AdminPermission[]>([]);
  const [loading, setLoading] = useState(false);

  const [createOpen, setCreateOpen] = useState(false);
  const [editTarget, setEditTarget] = useState<AdminRole | null>(null);
  const [createForm] = Form.useForm();
  const [editForm] = Form.useForm();

  const load = async () => {
    setLoading(true);
    try {
      const resp = await listRoles();
      setRoles(resp.items || []);
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.adminRoles.loadFailed'));
    } finally {
      setLoading(false);
    }
  };

  const loadPerms = async () => {
    try {
      const resp = await listPermissionCatalog();
      setPermissions(resp.items || []);
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.adminRoles.loadPermsFailed'));
    }
  };

  useEffect(() => {
    load();
    loadPerms();
  }, []);

  // 权限按类别分组，便于编辑时展示
  const groupedPermissions = useMemo(() => {
    const groups: Record<string, AdminPermission[]> = {};
    for (const p of permissions) {
      const key = p.category || 'other';
      (groups[key] = groups[key] || []).push(p);
    }
    return groups;
  }, [permissions]);

  const submitCreate = async () => {
    try {
      const values = await createForm.validateFields();
      await createRole(values);
      message.success(formatMessage('pages.adminRoles.created'));
      setCreateOpen(false);
      createForm.resetFields();
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || formatMessage('pages.adminRoles.createFailed'));
    }
  };

  const submitEdit = async () => {
    if (!editTarget) return;
    try {
      const values = await editForm.validateFields();
      await updateRole(editTarget.code, {
        name: values.name,
        description: values.description,
        permissions: values.permissions,
      });
      message.success(formatMessage('pages.adminRoles.updated'));
      setEditTarget(null);
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || formatMessage('pages.adminRoles.updateFailed'));
    }
  };

  const onDelete = async (code: string) => {
    try {
      await deleteRole(code);
      message.success(formatMessage('pages.adminRoles.deleted'));
      load();
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.adminRoles.deleteFailed'));
    }
  };

  const columns = [
    { title: formatMessage('pages.adminRoles.code'), dataIndex: 'code', render: (code: string, r: AdminRole) => (
      <Space><span>{code}</span>{r.builtin && <Tag color="gold">{formatMessage('pages.adminRoles.builtinTag')}</Tag>}</Space>
    ) },
    { title: formatMessage('pages.adminRoles.name'), dataIndex: 'name' },
    { title: formatMessage('pages.adminRoles.description'), dataIndex: 'description', ellipsis: true },
    {
      title: formatMessage('pages.adminRoles.permCount'),
      dataIndex: 'permissions',
      width: 90,
      render: (perms: AdminPermission[]) => (perms?.length || 0),
    },
    {
      title: formatMessage('pages.common.action'),
      width: 200,
      render: (_: any, record: AdminRole) => (
        <Space>
          <Button
            size="small"
            onClick={() => {
              setEditTarget(record);
              editForm.setFieldsValue({
                name: record.name,
                description: record.description,
                permissions: (record.permissions || []).map((p) => p.code),
              });
            }}
          >
            {formatMessage('pages.adminRoles.editPerms')}
          </Button>
          {!record.builtin && (
            <Popconfirm
              title={intl.formatMessage({ id: 'pages.adminRoles.deleteConfirm' }, {
                code: record.code,
              })}
              onConfirm={() => onDelete(record.code)}
              okButtonProps={{ danger: true }}
            >
              <Button size="small" danger>{formatMessage('pages.adminRoles.delete')}</Button>
            </Popconfirm>
          )}
        </Space>
      ),
    },
  ];

  return (
    <PageContainer>
      <Card>
        <Space style={{ marginBottom: 16 }}>
          <Button type="primary" onClick={() => { createForm.resetFields(); setCreateOpen(true); }}>
            {formatMessage('pages.adminRoles.create')}
          </Button>
        </Space>
        <Table
          rowKey="id"
          loading={loading}
          dataSource={roles}
          columns={columns as any}
          pagination={false}
        />
      </Card>

      <Modal
        title={formatMessage('pages.adminRoles.create')}
        open={createOpen}
        onOk={submitCreate}
        onCancel={() => setCreateOpen(false)}
        destroyOnClose
      >
        <Form form={createForm} layout="vertical">
          <Form.Item
            name="code"
            label={formatMessage('pages.adminRoles.codeLabel')}
            rules={[{ required: true, message: formatMessage('pages.adminRoles.codeRequired') }]}
          >
            <Input placeholder={formatMessage('pages.adminRoles.codePlaceholder')} />
          </Form.Item>
          <Form.Item name="name" label={formatMessage('pages.adminRoles.name')}>
            <Input />
          </Form.Item>
          <Form.Item name="description" label={formatMessage('pages.adminRoles.description')}>
            <Input.TextArea rows={2} />
          </Form.Item>
          <Form.Item name="permissions" label={formatMessage('pages.adminRoles.permsLabel')}>
            <PermissionSelect grouped={groupedPermissions} />
          </Form.Item>
        </Form>
      </Modal>

      <Drawer
        title={intl.formatMessage({ id: 'pages.adminRoles.drawerTitle' }, {
          name: editTarget?.name || editTarget?.code || '',
        })}
        width={520}
        open={!!editTarget}
        onClose={() => setEditTarget(null)}
        extra={
          <Space>
            <Button onClick={() => setEditTarget(null)}>{formatMessage('pages.common.cancel')}</Button>
            <Button type="primary" onClick={submitEdit}>{formatMessage('pages.common.save')}</Button>
          </Space>
        }
        destroyOnClose
      >
        {editTarget && (
          <Form form={editForm} layout="vertical">
            <Form.Item name="name" label={formatMessage('pages.adminRoles.name')}>
              <Input disabled={editTarget.builtin} />
            </Form.Item>
            <Form.Item name="description" label={formatMessage('pages.adminRoles.description')}>
              <Input.TextArea rows={2} disabled={editTarget.builtin} />
            </Form.Item>
            <Form.Item label={formatMessage('pages.adminRoles.permsLabel')}>
              <span style={{ color: '#888', fontSize: 12 }}>
                {editTarget.code === 'admin'
                  ? formatMessage('pages.adminRoles.adminWildcardHint')
                  : intl.formatMessage({ id: 'pages.adminRoles.assignedCount' }, {
                    count: (editTarget.permissions || []).length,
                  })}
              </span>
            </Form.Item>
            {editTarget.code !== 'admin' && (
              <Form.Item name="permissions">
                <PermissionSelect grouped={groupedPermissions} />
              </Form.Item>
            )}
          </Form>
        )}
      </Drawer>
    </PageContainer>
  );
}

// 按类别分组的权限多选组件
function PermissionSelect({
  grouped,
  disabled,
}: {
  grouped: Record<string, AdminPermission[]>;
  disabled?: boolean;
}) {
  const intl = useIntl();
  const options = useMemo(() => {
    return Object.entries(grouped).map(([category, perms]) => ({
      label: <span style={{ fontWeight: 600 }}>{category}</span>,
      title: category,
      options: perms.map((p) => ({
        label: `${p.name || p.code} (${p.code})`,
        value: p.code,
        disabled,
      })),
    }));
  }, [grouped, disabled]);

  if (options.length === 0) return <Empty description={intl.formatMessage({ id: 'pages.adminRoles.noCatalog' })} />;
  return (
    <Select
      mode="multiple"
      options={options}
      optionFilterProp="label"
      showSearch
      style={{ width: '100%' }}
      placeholder={intl.formatMessage({ id: 'pages.adminRoles.selectPlaceholder' })}
    />
  );
}
