// 执行记录页（ADR: Execution as the Platform Core Object）：
// 服务端统一执行对象的只读视图——列表（分页 + status/agentId 过滤）与详情。
// 数据来自 GET /api/executions（run_script / batch / workflow 步骤统一落库）。
import {
  HistoryOutlined,
} from '@ant-design/icons';
import {
  PageContainer,
  type ProColumns,
  ProDescriptions,
  ProTable,
} from '@ant-design/pro-components';
import { useIntl } from '@umijs/max';
import { Button, Card, Drawer, Space, Tag, Typography } from 'antd';
import React, { useState } from 'react';
import {
  type ExecutionListQuery,
  type ExecutionStatus,
  type ExecutionView,
  getExecution,
  getExecutionStatusColor,
  listExecutions,
} from '@/services/wingman';

const { Text } = Typography;

// 过滤用的全量状态集合（ADR 状态机八态）
const ALL_STATUSES: ExecutionStatus[] = [
  'pending',
  'queued',
  'running',
  'succeeded',
  'failed',
  'cancelled',
  'timeout',
  'lost',
];

// scriptBasename 脚本路径仅展示文件名（完整路径入详情）
function scriptBasename(p: string): string {
  if (!p) return '';
  const idx = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'));
  return idx >= 0 ? p.slice(idx + 1) : p;
}

const ExecutionsPage: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [detail, setDetail] = useState<ExecutionView | null>(null);
  const [detailOpen, setDetailOpen] = useState(false);

  const openDetail = async (record: ExecutionView) => {
    // 列表行已是完整视图；仍请求一次详情保证新鲜，失败回退行数据
    setDetail(record);
    setDetailOpen(true);
    try {
      const resp = await getExecution(record.id);
      if (resp?.data) setDetail(resp.data);
    } catch {
      // 保留行数据
    }
  };

  const columns: ProColumns<ExecutionView>[] = [
    {
      title: 'Execution ID',
      dataIndex: 'executionId',
      width: 170,
      ellipsis: true,
      render: (_, record) => (
        <Text copyable={{ text: record.executionId }} style={{ fontSize: 12 }}>
          {record.executionId.slice(0, 16)}
        </Text>
      ),
    },
    {
      title: formatMessage('pages.executions.status'),
      dataIndex: 'status',
      width: 120,
      valueType: 'select',
      fieldProps: {
        options: ALL_STATUSES.map((s) => ({ label: s, value: s })),
      },
      render: (_, record) => (
        <Tag color={getExecutionStatusColor(record.status)}>
          {record.status.toUpperCase()}
        </Tag>
      ),
    },
    {
      title: formatMessage('pages.executions.agent'),
      dataIndex: 'agentId',
      width: 150,
      ellipsis: true,
    },
    {
      title: formatMessage('pages.executions.script'),
      dataIndex: 'scriptPath',
      width: 180,
      ellipsis: true,
      render: (_, record) => (
        <Text style={{ fontSize: 12 }}>{scriptBasename(record.scriptPath) || '—'}</Text>
      ),
    },
    {
      title: formatMessage('pages.executions.startedAt'),
      dataIndex: 'startedAt',
      width: 160,
      render: (_, record) =>
        record.startedAt || <Text type="secondary">—</Text>,
    },
    {
      title: formatMessage('pages.executions.finishedAt'),
      dataIndex: 'finishedAt',
      width: 160,
      render: (_, record) =>
        record.finishedAt || <Text type="secondary">—</Text>,
    },
    {
      title: formatMessage('pages.executions.actions'),
      dataIndex: 'actions',
      width: 90,
      search: false,
      render: (_, record) => (
        <Button type="link" size="small" onClick={() => openDetail(record)}>
          {formatMessage('pages.executions.detail')}
        </Button>
      ),
    },
  ];

  return (
    <PageContainer>
      <Card>
        <ProTable<ExecutionView>
          headerTitle={
            <Space>
              <HistoryOutlined />
              {formatMessage('pages.executions.title')}
            </Space>
          }
          rowKey="id"
          columns={columns}
          search={{ labelWidth: 'auto' }}
          pagination={{ defaultPageSize: 20, showSizeChanger: true }}
          request={async (params) => {
            const query: ExecutionListQuery = {
              page: params.current ?? 1,
              pageSize: params.pageSize ?? 20,
              status: params.status || undefined,
              agentId: (params.agentId as string) || undefined,
            };
            const resp = await listExecutions(query);
            return {
              data: resp?.data ?? [],
              total: resp?.total ?? 0,
              success: resp?.success !== false,
            };
          }}
        />
      </Card>

      <Drawer
        title={formatMessage('pages.executions.detail')}
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
                  title: 'Execution ID',
                  dataIndex: 'executionId',
                  render: (_, record) => <Text copyable>{record.executionId}</Text>,
                },
                {
                  title: formatMessage('pages.executions.status'),
                  dataIndex: 'status',
                  render: (_, record) => (
                    <Tag color={getExecutionStatusColor(record.status)}>
                      {record.status.toUpperCase()}
                    </Tag>
                  ),
                },
                {
                  title: formatMessage('pages.executions.agent'),
                  dataIndex: 'agentId',
                },
                {
                  title: formatMessage('pages.executions.script'),
                  dataIndex: 'scriptPath',
                  render: (_, record) => (
                    <Text style={{ fontSize: 12 }} copyable>
                      {record.scriptPath || '—'}
                    </Text>
                  ),
                },
                {
                  title: formatMessage('pages.executions.startedAt'),
                  dataIndex: 'startedAt',
                  render: (_, record) => record.startedAt || '—',
                },
                {
                  title: formatMessage('pages.executions.finishedAt'),
                  dataIndex: 'finishedAt',
                  render: (_, record) => record.finishedAt || '—',
                },
                {
                  title: formatMessage('pages.executions.timeout'),
                  dataIndex: 'timeoutSec',
                },
              ]}
            />
            <Typography.Title level={5} style={{ marginTop: 16 }}>
              {formatMessage('pages.executions.result')}
            </Typography.Title>
            <pre style={{ maxHeight: 240, overflow: 'auto', fontSize: 12 }}>
              {detail.result !== undefined && detail.result !== null
                ? JSON.stringify(detail.result, null, 2)
                : '—'}
            </pre>
            <Typography.Title level={5}>
              {formatMessage('pages.executions.artifacts')}
            </Typography.Title>
            {detail.artifacts && detail.artifacts.length > 0 ? (
              <Space direction="vertical">
                {detail.artifacts.map((a) => (
                  <Text key={a} style={{ fontSize: 12 }}>
                    {a}
                  </Text>
                ))}
              </Space>
            ) : (
              <Text type="secondary">—</Text>
            )}
          </>
        )}
      </Drawer>
    </PageContainer>
  );
};

export default ExecutionsPage;
