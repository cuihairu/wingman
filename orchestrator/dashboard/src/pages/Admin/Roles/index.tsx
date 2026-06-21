import React, { useEffect, useMemo, useState } from 'react';
import { Button, Card, Drawer, Form, Input, Modal, Popconfirm, Select, Space, Table, Tag, App, Empty } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
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
      message.error(err?.message || '加载角色失败');
    } finally {
      setLoading(false);
    }
  };

  const loadPerms = async () => {
    try {
      const resp = await listPermissionCatalog();
      setPermissions(resp.items || []);
    } catch (err: any) {
      message.error(err?.message || '加载权限目录失败');
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
      message.success('角色已创建');
      setCreateOpen(false);
      createForm.resetFields();
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || '创建失败');
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
      message.success('角色已更新');
      setEditTarget(null);
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || '更新失败');
    }
  };

  const onDelete = async (code: string) => {
    try {
      await deleteRole(code);
      message.success('角色已删除');
      load();
    } catch (err: any) {
      message.error(err?.message || '删除失败');
    }
  };

  const columns = [
    { title: '编码', dataIndex: 'code', render: (code: string, r: AdminRole) => (
      <Space><span>{code}</span>{r.builtin && <Tag color="gold">内置</Tag>}</Space>
    ) },
    { title: '名称', dataIndex: 'name' },
    { title: '描述', dataIndex: 'description', ellipsis: true },
    {
      title: '权限数',
      dataIndex: 'permissions',
      width: 90,
      render: (perms: AdminPermission[]) => (perms?.length || 0),
    },
    {
      title: '操作',
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
            编辑权限
          </Button>
          {!record.builtin && (
            <Popconfirm
              title={`删除角色 ${record.code}?`}
              onConfirm={() => onDelete(record.code)}
              okButtonProps={{ danger: true }}
            >
              <Button size="small" danger>删除</Button>
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
            新建角色
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
        title="新建角色"
        open={createOpen}
        onOk={submitCreate}
        onCancel={() => setCreateOpen(false)}
        destroyOnClose
      >
        <Form form={createForm} layout="vertical">
          <Form.Item name="code" label="角色编码" rules={[{ required: true, message: '请输入角色编码' }]}>
            <Input placeholder="2-32 位字母/数字/_/-（不可与 admin 冲突）" />
          </Form.Item>
          <Form.Item name="name" label="名称">
            <Input />
          </Form.Item>
          <Form.Item name="description" label="描述">
            <Input.TextArea rows={2} />
          </Form.Item>
          <Form.Item name="permissions" label="权限">
            <PermissionSelect grouped={groupedPermissions} />
          </Form.Item>
        </Form>
      </Modal>

      <Drawer
        title={`编辑角色 - ${editTarget?.name || editTarget?.code || ''}`}
        width={520}
        open={!!editTarget}
        onClose={() => setEditTarget(null)}
        extra={
          <Space>
            <Button onClick={() => setEditTarget(null)}>取消</Button>
            <Button type="primary" onClick={submitEdit}>保存</Button>
          </Space>
        }
        destroyOnClose
      >
        {editTarget && (
          <Form form={editForm} layout="vertical">
            <Form.Item name="name" label="名称">
              <Input disabled={editTarget.builtin} />
            </Form.Item>
            <Form.Item name="description" label="描述">
              <Input.TextArea rows={2} disabled={editTarget.builtin} />
            </Form.Item>
            <Form.Item label="权限">
              <span style={{ color: '#888', fontSize: 12 }}>
                {editTarget.code === 'admin'
                  ? 'admin 角色拥有通配权限 *，不可在此修改'
                  : `已分配 ${(editTarget.permissions || []).length} 项权限`}
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

  if (options.length === 0) return <Empty description="无权限目录" />;
  return (
    <Select
      mode="multiple"
      options={options}
      optionFilterProp="label"
      showSearch
      style={{ width: '100%' }}
      placeholder="选择权限"
    />
  );
}
