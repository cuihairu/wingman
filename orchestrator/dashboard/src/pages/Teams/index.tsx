// 团队页（多 agent 协作，见 docs/architecture-decisions.md「Team inbox push」）：
// TeamManager（agent 通道内存态）的只读视图 + 团队范围收件箱推送入口。
// 列表/详情来自 GET /api/teams（teamId 升序）与 GET /api/teams/:teamId
// （含 memberAgents 成员→设备映射）；推送走 POST /api/teams/:teamId/messages。
// 服务重启后团队丢失，本页只反映当前在线视图。
import { TeamOutlined } from '@ant-design/icons';
import {
  PageContainer,
  type ProColumns,
  ProDescriptions,
  ProTable,
} from '@ant-design/pro-components';
import { useIntl } from '@umijs/max';
import {
  Alert,
  Button,
  Card,
  Drawer,
  Form,
  Input,
  Modal,
  Space,
  Table,
  Tag,
  Typography,
  message,
} from 'antd';
import React, { useState } from 'react';
import {
  type TeamMessageSummary,
  type TeamView,
  getTeams,
  pushTeamMessage,
} from '@/services/wingman';
import { buildMemberRows, sortTeamsByTeamId } from '@/utils/teams';

const { Text } = Typography;

// stateTagColor 团队状态着色（TeamManager 生命周期状态，未知状态走默认灰）
function stateTagColor(state: string): string {
  switch (state) {
    case 'active':
      return 'green';
    case 'dissolved':
      return 'red';
    default:
      return 'default';
  }
}

const TeamsPage: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });

  const [detail, setDetail] = useState<TeamView | null>(null);
  const [detailOpen, setDetailOpen] = useState(false);
  const [pushTarget, setPushTarget] = useState<TeamView | null>(null);
  const [pushOpen, setPushOpen] = useState(false);
  const [pushForm] = Form.useForm<{ messageType: string; payloadText?: string }>();
  const [pushing, setPushing] = useState(false);
  const [pushSummary, setPushSummary] = useState<TeamMessageSummary | null>(null);

  const openDetail = (record: TeamView) => {
    setDetail(record);
    setDetailOpen(true);
  };

  const openPush = (record: TeamView) => {
    setPushTarget(record);
    setPushSummary(null);
    pushForm.resetFields();
    setPushOpen(true);
  };

  const submitPush = async () => {
    if (!pushTarget) return;
    const values = await pushForm.validateFields();
    let payload: unknown;
    const payloadText = (values.payloadText ?? '').trim();
    if (payloadText) {
      try {
        payload = JSON.parse(payloadText);
      } catch {
        message.error(formatMessage('pages.teams.push.payloadInvalid'));
        return;
      }
    }
    setPushing(true);
    try {
      const resp = await pushTeamMessage(pushTarget.teamId, values.messageType.trim(), payload);
      if (resp?.success && resp.data) {
        setPushSummary(resp.data);
        message.success(formatMessage('pages.teams.push.success'));
      } else {
        message.error(resp?.error || formatMessage('pages.teams.push.failed'));
      }
    } catch {
      message.error(formatMessage('pages.teams.push.failed'));
    } finally {
      setPushing(false);
    }
  };

  const memberColumns = [
    {
      title: formatMessage('pages.teams.members.id'),
      dataIndex: 'memberId',
      render: (memberId: string, row: ReturnType<typeof buildMemberRows>[number]) => (
        <Space size={4}>
          <Text style={{ fontSize: 12 }} copyable={{ text: memberId }}>
            {memberId}
          </Text>
          {row.leader && <Tag color="gold">{formatMessage('pages.teams.members.leader')}</Tag>}
        </Space>
      ),
    },
    {
      title: formatMessage('pages.teams.members.agent'),
      dataIndex: 'agentId',
      render: (agentId: string, row: ReturnType<typeof buildMemberRows>[number]) =>
        row.bound ? (
          <Text style={{ fontSize: 12 }}>{agentId}</Text>
        ) : (
          <Tag>{formatMessage('pages.teams.members.unbound')}</Tag>
        ),
    },
  ];

  const columns: ProColumns<TeamView>[] = [
    {
      title: formatMessage('pages.teams.teamId'),
      dataIndex: 'teamId',
      width: 140,
      render: (_, record) => (
        <Text style={{ fontSize: 12 }} copyable={{ text: record.teamId }}>
          {record.teamId}
        </Text>
      ),
    },
    {
      title: formatMessage('pages.teams.name'),
      dataIndex: 'name',
      width: 160,
      ellipsis: true,
    },
    {
      title: formatMessage('pages.teams.leader'),
      dataIndex: 'leaderId',
      width: 130,
      ellipsis: true,
    },
    {
      title: formatMessage('pages.teams.members.count'),
      dataIndex: 'members',
      width: 90,
      search: false,
      render: (_, record) => record.members.length,
    },
    {
      title: formatMessage('pages.teams.state'),
      dataIndex: 'state',
      width: 110,
      render: (_, record) => <Tag color={stateTagColor(record.state)}>{record.state || '—'}</Tag>,
    },
    {
      title: formatMessage('pages.teams.createdAt'),
      dataIndex: 'createdAt',
      width: 160,
      search: false,
      render: (_, record) => (record.createdAt ? new Date(record.createdAt).toLocaleString() : '—'),
    },
    {
      title: formatMessage('pages.teams.actions'),
      dataIndex: 'actions',
      width: 150,
      search: false,
      render: (_, record) => (
        <Space size={0}>
          <Button type="link" size="small" onClick={() => openDetail(record)}>
            {formatMessage('pages.teams.detail')}
          </Button>
          <Button type="link" size="small" onClick={() => openPush(record)}>
            {formatMessage('pages.teams.push.action')}
          </Button>
        </Space>
      ),
    },
  ];

  return (
    <PageContainer>
      <Card>
        <ProTable<TeamView>
          headerTitle={
            <Space>
              <TeamOutlined />
              {formatMessage('pages.teams.title')}
            </Space>
          }
          rowKey="teamId"
          columns={columns}
          search={false}
          pagination={{ defaultPageSize: 20, showSizeChanger: true }}
          request={async () => {
            const resp = await getTeams();
            const teams = sortTeamsByTeamId(resp?.data ?? []);
            return {
              data: teams,
              total: teams.length,
              success: resp?.success !== false,
            };
          }}
        />
      </Card>

      <Drawer
        title={formatMessage('pages.teams.detail')}
        open={detailOpen}
        onClose={() => setDetailOpen(false)}
        width={560}
      >
        {detail && (
          <>
            <ProDescriptions
              column={1}
              dataSource={detail}
              columns={[
                {
                  title: formatMessage('pages.teams.teamId'),
                  dataIndex: 'teamId',
                  render: (_, record) => <Text copyable>{record.teamId}</Text>,
                },
                {
                  title: formatMessage('pages.teams.name'),
                  dataIndex: 'name',
                },
                {
                  title: formatMessage('pages.teams.description'),
                  dataIndex: 'description',
                  render: (_, record) => record.description || '—',
                },
                {
                  title: formatMessage('pages.teams.leader'),
                  dataIndex: 'leaderId',
                },
                {
                  title: formatMessage('pages.teams.state'),
                  dataIndex: 'state',
                  render: (_, record) => (
                    <Tag color={stateTagColor(record.state)}>{record.state || '—'}</Tag>
                  ),
                },
                {
                  title: formatMessage('pages.teams.createdAt'),
                  dataIndex: 'createdAt',
                  render: (_, record) =>
                    record.createdAt ? new Date(record.createdAt).toLocaleString() : '—',
                },
                {
                  title: formatMessage('pages.teams.updatedAt'),
                  dataIndex: 'updatedAt',
                  render: (_, record) =>
                    record.updatedAt ? new Date(record.updatedAt).toLocaleString() : '—',
                },
              ]}
            />
            <Typography.Title level={5} style={{ marginTop: 16 }}>
              {formatMessage('pages.teams.members.title')}
            </Typography.Title>
            <Table
              size="small"
              rowKey="memberId"
              columns={memberColumns}
              dataSource={buildMemberRows(detail)}
              pagination={false}
            />
            <Alert
              style={{ marginTop: 12 }}
              type="info"
              showIcon
              message={formatMessage('pages.teams.members.unboundHint')}
            />
          </>
        )}
      </Drawer>

      <Modal
        title={`${formatMessage('pages.teams.push.title')} · ${pushTarget?.teamId ?? ''}`}
        open={pushOpen}
        onCancel={() => setPushOpen(false)}
        onOk={submitPush}
        confirmLoading={pushing}
        okText={formatMessage('pages.teams.push.send')}
        destroyOnClose
      >
        <Form form={pushForm} layout="vertical">
          <Form.Item
            name="messageType"
            label={formatMessage('pages.teams.push.messageType')}
            rules={[
              {
                required: true,
                whitespace: true,
                message: formatMessage('pages.teams.push.messageTypeRequired'),
              },
            ]}
          >
            <Input placeholder="system.notice" />
          </Form.Item>
          <Form.Item
            name="payloadText"
            label={formatMessage('pages.teams.push.payload')}
            extra={formatMessage('pages.teams.push.payloadHint')}
          >
            <Input.TextArea rows={4} placeholder='{"text": "hello"}' />
          </Form.Item>
        </Form>
        {pushSummary && (
          <>
            <Alert
              type={pushSummary.skipped > 0 ? 'warning' : 'success'}
              showIcon
              message={`${formatMessage('pages.teams.push.resultTotal')}: ${pushSummary.total} · ${formatMessage('pages.teams.push.resultDelivered')}: ${pushSummary.delivered} · ${formatMessage('pages.teams.push.resultSkipped')}: ${pushSummary.skipped}`}
            />
            <Table
              style={{ marginTop: 8 }}
              size="small"
              rowKey="memberId"
              columns={[
                {
                  title: formatMessage('pages.teams.members.id'),
                  dataIndex: 'memberId',
                },
                {
                  title: formatMessage('pages.teams.members.agent'),
                  dataIndex: 'agentId',
                  render: (agentId: string) => agentId || '—',
                },
                {
                  title: formatMessage('pages.teams.push.resultState'),
                  dataIndex: 'delivered',
                  render: (delivered: boolean, row) =>
                    delivered ? (
                      <Tag color="green">{formatMessage('pages.teams.push.delivered')}</Tag>
                    ) : (
                      <Tag color="orange">
                        {row.skipped || formatMessage('pages.teams.push.skipped')}
                      </Tag>
                    ),
                },
              ]}
              dataSource={pushSummary.results}
              pagination={false}
            />
          </>
        )}
      </Modal>
    </PageContainer>
  );
};

export default TeamsPage;
