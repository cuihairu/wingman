import React, { useEffect, useState } from 'react';
import { Button, Card, Form, Input, Modal, Popconfirm, Select, Space, Switch, Table, Tag, App } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
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
      message.error(err?.message || '加载用户失败');
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

  const submitCreate = async () => {
    try {
      const values = await createForm.validateFields();
      await createUser(values);
      message.success('用户已创建');
      setCreateOpen(false);
      createForm.resetFields();
      load();
    } catch (err: any) {
      if (err?.errorFields) return; // 校验错误
      message.error(err?.message || '创建失败');
    }
  };

  const submitEdit = async () => {
    if (!editTarget) return;
    try {
      const values = await editForm.validateFields();
      await updateUser(editTarget.id, values);
      message.success('用户已更新');
      setEditTarget(null);
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || '更新失败');
    }
  };

  const submitReset = async () => {
    if (!resetTarget) return;
    try {
      const values = await resetForm.validateFields();
      await resetUserPassword(resetTarget.id, values.newPassword);
      message.success('密码已重置');
      setResetTarget(null);
      resetForm.resetFields();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || '重置失败');
    }
  };

  const onDelete = async (id: number) => {
    try {
      await deleteUser(id);
      message.success('用户已删除');
      load();
    } catch (err: any) {
      message.error(err?.message || '删除失败');
    }
  };

  const columns = [
    { title: 'ID', dataIndex: 'id', width: 70 },
    { title: '用户名', dataIndex: 'username' },
    {
      title: '角色',
      dataIndex: 'role',
      render: (role: string) => <Tag color={roleColor(role)}>{role}</Tag>,
    },
    {
      title: '状态',
      dataIndex: 'active',
      render: (active: boolean) =>
        active ? <Tag color="green">启用</Tag> : <Tag>禁用</Tag>,
    },
    { title: '创建时间', dataIndex: 'createdAt', width: 180 },
    {
      title: '操作',
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
            编辑
          </Button>
          <Button size="small" onClick={() => { setResetTarget(record); resetForm.resetFields(); }}>
            重置密码
          </Button>
          <Popconfirm
            title={`删除用户 ${record.username}?`}
            onConfirm={() => onDelete(record.id)}
            okButtonProps={{ danger: true }}
          >
            <Button size="small" danger>
              删除
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
            placeholder="搜索用户名"
            allowClear
            value={keyword}
            onChange={(e) => setKeyword(e.target.value)}
            onSearch={() => { setPage(1); load(); }}
            style={{ width: 240 }}
          />
          <Button type="primary" onClick={() => { createForm.resetFields(); setCreateOpen(true); }}>
            新建用户
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
            onChange: (p, s) => { setPage(p); setSize(s); },
          }}
        />
      </Card>

      <Modal
        title="新建用户"
        open={createOpen}
        onOk={submitCreate}
        onCancel={() => setCreateOpen(false)}
        destroyOnClose
      >
        <Form form={createForm} layout="vertical" initialValues={{ role: 'viewer', active: true }}>
          <Form.Item name="username" label="用户名" rules={[{ required: true, message: '请输入用户名' }]}>
            <Input placeholder="3-32 位字母/数字/_/-" />
          </Form.Item>
          <Form.Item name="password" label="密码" rules={[{ required: true, message: '请输入密码' }]}>
            <Input.Password placeholder="至少 8 位，含大小写/数字/特殊字符任意三种" />
          </Form.Item>
          <Form.Item name="role" label="角色" rules={[{ required: true }]}>
            <Select
              options={roles.map((r) => ({ label: `${r.name || r.code}${r.builtin ? ' (内置)' : ''}`, value: r.code }))}
            />
          </Form.Item>
          <Form.Item name="active" label="启用" valuePropName="checked">
            <Switch />
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={`编辑用户 - ${editTarget?.username || ''}`}
        open={!!editTarget}
        onOk={submitEdit}
        onCancel={() => setEditTarget(null)}
        destroyOnClose
      >
        <Form form={editForm} layout="vertical">
          <Form.Item name="role" label="角色" rules={[{ required: true }]}>
            <Select
              options={roles.map((r) => ({ label: `${r.name || r.code}${r.builtin ? ' (内置)' : ''}`, value: r.code }))}
            />
          </Form.Item>
          <Form.Item name="active" label="启用" valuePropName="checked">
            <Switch />
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={`重置密码 - ${resetTarget?.username || ''}`}
        open={!!resetTarget}
        onOk={submitReset}
        onCancel={() => setResetTarget(null)}
        destroyOnClose
      >
        <Form form={resetForm} layout="vertical">
          <Form.Item
            name="newPassword"
            label="新密码"
            rules={[{ required: true, message: '请输入新密码' }]}
          >
            <Input.Password placeholder="至少 8 位，含大小写/数字/特殊字符任意三种" />
          </Form.Item>
        </Form>
      </Modal>
    </PageContainer>
  );
}
