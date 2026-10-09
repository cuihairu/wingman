import { useIntl } from '@umijs/max';
import React, { useEffect, useState } from 'react';
import {
  Alert,
  Button,
  Card,
  Empty,
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
import { PageContainer } from '@ant-design/pro-components';
import {
  createAgentToken,
  listAgentTokens,
  revokeAgentToken,
  type AgentToken,
} from '@/services/api';

/**
 * Agent 注册 Token 管理（A3-P2）：
 * - 明文只在签发弹窗出现一次（轮换 = 重新签发）；
 * - 吊销立即生效（listener 校验点实时查库），幂等；
 * - DB 源启用口径 = 存在签发记录（含已吊销，fail-closed）；
 *   与 WINGMAN_AGENT_TOKENS env 白名单双源并存，清空 env 即纯管理面模式。
 */
export default function AgentTokensPage() {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [rows, setRows] = useState<AgentToken[]>([]);
  const [loading, setLoading] = useState(false);
  const [createOpen, setCreateOpen] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [issued, setIssued] = useState<{ token: string; label: string } | null>(null);
  const [form] = Form.useForm();

  const load = async () => {
    setLoading(true);
    try {
      setRows(await listAgentTokens());
    } catch {
      // 错误横幅由全局拦截器提示；列表保持上次数据
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  const submitCreate = async () => {
    const values = await form.validateFields();
    setSubmitting(true);
    try {
      const result = await createAgentToken(values.label.trim(), (values.agentId || '').trim());
      setCreateOpen(false);
      form.resetFields();
      setIssued({ token: result.token, label: result.record.label });
      await load();
    } finally {
      setSubmitting(false);
    }
  };

  const submitRevoke = async (record: AgentToken) => {
    await revokeAgentToken(record.ID);
    message.success(formatMessage('pages.agentTokens.revoked'));
    await load();
  };

  const columns = [
    { title: 'ID', dataIndex: 'ID', width: 64 },
    { title: formatMessage('pages.agentTokens.label'), dataIndex: 'label' },
    {
      title: formatMessage('pages.agentTokens.prefix'),
      dataIndex: 'prefix',
      render: (v: string) => <Typography.Text code>{v}</Typography.Text>,
    },
    {
      title: formatMessage('pages.agentTokens.agentId'),
      dataIndex: 'agentId',
      render: (v: string) =>
        v ? <Tag color="blue">{v}</Tag> : <Tag>{formatMessage('pages.agentTokens.unbound')}</Tag>,
    },
    { title: formatMessage('pages.agentTokens.createdBy'), dataIndex: 'createdBy', width: 120 },
    {
      title: formatMessage('pages.agentTokens.createdAt'),
      dataIndex: 'createdAt',
      width: 180,
      render: (v: string) => (v ? new Date(v).toLocaleString() : '-'),
    },
    {
      title: formatMessage('pages.agentTokens.lastSeenAt'),
      dataIndex: 'lastSeenAt',
      width: 180,
      render: (v?: string) => (v ? new Date(v).toLocaleString() : '-'),
    },
    {
      title: formatMessage('pages.agentTokens.status'),
      dataIndex: 'revokedAt',
      width: 100,
      render: (v?: string) =>
        v ? (
          <Tag color="red">{formatMessage('pages.agentTokens.revokedTag')}</Tag>
        ) : (
          <Tag color="green">{formatMessage('pages.agentTokens.activeTag')}</Tag>
        ),
    },
    {
      title: formatMessage('pages.agentTokens.actions'),
      width: 100,
      render: (_: unknown, record: AgentToken) =>
        record.revokedAt ? null : (
          <Popconfirm
            title={formatMessage('pages.agentTokens.revokeConfirm')}
            onConfirm={() => submitRevoke(record)}
          >
            <Button size="small" danger>
              {formatMessage('pages.agentTokens.revoke')}
            </Button>
          </Popconfirm>
        ),
    },
  ];

  return (
    <PageContainer>
      <Card
        title={formatMessage('menu.Admin.Agent Tokens')}
        extra={
          <Button type="primary" onClick={() => setCreateOpen(true)}>
            {formatMessage('pages.agentTokens.create')}
          </Button>
        }
      >
        <Alert
          type="info"
          showIcon
          style={{ marginBottom: 16 }}
          message={formatMessage('pages.agentTokens.notice')}
        />
        <Table
          rowKey="ID"
          loading={loading}
          columns={columns as any}
          dataSource={rows}
          locale={{
            emptyText: <Empty description={formatMessage('pages.agentTokens.empty')} />,
          }}
          pagination={false}
        />
      </Card>

      <Modal
        title={formatMessage('pages.agentTokens.create')}
        open={createOpen}
        confirmLoading={submitting}
        onOk={submitCreate}
        onCancel={() => setCreateOpen(false)}
        destroyOnClose
      >
        <Form form={form} layout="vertical">
          <Form.Item
            name="label"
            label={formatMessage('pages.agentTokens.label')}
            rules={[{ required: true, message: formatMessage('pages.agentTokens.labelRequired') }]}
          >
            <Input placeholder="pixel-8 真机" maxLength={128} />
          </Form.Item>
          <Form.Item
            name="agentId"
            label={formatMessage('pages.agentTokens.agentId')}
            extra={formatMessage('pages.agentTokens.agentIdExtra')}
          >
            <Input placeholder="agent-pixel-8" maxLength={128} />
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={formatMessage('pages.agentTokens.issuedTitle')}
        open={!!issued}
        footer={[
          <Button key="ok" type="primary" onClick={() => setIssued(null)}>
            {formatMessage('pages.agentTokens.issuedAck')}
          </Button>,
        ]}
        closable={false}
        maskClosable={false}
      >
        <Alert
          type="warning"
          showIcon
          style={{ marginBottom: 12 }}
          message={formatMessage('pages.agentTokens.issuedWarning')}
        />
        <Typography.Paragraph copyable code style={{ wordBreak: 'break-all' }}>
          {issued?.token}
        </Typography.Paragraph>
        {issued?.label ? (
          <Typography.Text type="secondary">{issued.label}</Typography.Text>
        ) : null}
      </Modal>
    </PageContainer>
  );
}
