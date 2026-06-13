import React, { useEffect, useMemo, useState } from 'react';
import { Card, Table, Space, Input, Button, DatePicker, Tag } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { listAudit, type AuditEvent } from '@/services/api';

const defaultKinds = [
  'script.create',
  'script.delete',
  'script.save',
  'script.run',
  'script.stop',
  'workflow.create',
  'workflow.cancel',
  'agent.shutdown',
];

export default function OperationLogsPage() {
  const [rows, setRows] = useState<AuditEvent[]>([]);
  const [total, setTotal] = useState<number>(0);
  const [loading, setLoading] = useState(false);
  const [actor, setActor] = useState<string>(
    () => new URLSearchParams(location.search).get('actor') || '',
  );
  const [kinds, setKinds] = useState<string[]>(defaultKinds);
  const [timeRange, setTimeRange] = useState<any>(null);
  const [page, setPage] = useState<number>(1);
  const [size, setSize] = useState<number>(20);

  const load = async () => {
    setLoading(true);
    try {
      const params: any = { page, size };
      if (actor) params.actor = actor;
      params.kinds = (kinds.length > 0 ? kinds : defaultKinds).join(',');
      if (timeRange?.[0]) params.start = timeRange[0].toISOString();
      if (timeRange?.[1]) params.end = timeRange[1].toISOString();
      const response = await listAudit(params);
      setRows(response.events || []);
      setTotal(response.total || (response.events || []).length);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, [page, size]);

  const exportCSV = () => {
    const data = rows.map((event: any) => [
      new Date(event.time || '').toISOString(),
      event.kind || '',
      event.actor || '',
      event.target || '',
      event.meta?.ip || '',
      event.meta?.ip_region || '',
      event.meta?.agent_id || '',
      event.meta?.workflow_id || '',
      event.meta?.script_path || '',
    ]);
    data.unshift([
      'time',
      'kind',
      'actor',
      'target',
      'ip',
      'region',
      'agent_id',
      'workflow_id',
      'script_path',
    ]);
    const csv = data
      .map((row) =>
        row
          .map((cell) =>
            /[",\n]/.test(String(cell)) ? `"${String(cell).replace(/"/g, '""')}"` : String(cell),
          )
          .join(','),
      )
      .join('\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = 'operation_logs.csv';
    anchor.click();
    URL.revokeObjectURL(url);
  };

  const kindTags = useMemo(
    () => (
      <Space size={4} wrap>
        {defaultKinds.map((kind) => (
          <Tag
            key={kind}
            color={kinds.includes(kind) ? 'blue' : 'default'}
            onClick={() =>
              setKinds((previous) =>
                previous.includes(kind)
                  ? previous.filter((value) => value !== kind)
                  : [...previous, kind],
              )
            }
            style={{ cursor: 'pointer' }}
          >
            {kind}
          </Tag>
        ))}
      </Space>
    ),
    [kinds],
  );

  return (
    <PageContainer>
      <Card title="Operation Logs">
        <Space style={{ marginBottom: 12 }} wrap>
          <Input
            placeholder="Actor"
            value={actor}
            onChange={(event) => setActor(event.target.value)}
            style={{ width: 160 }}
          />
          {kindTags}
          <DatePicker.RangePicker
            showTime
            value={timeRange as any}
            onChange={setTimeRange as any}
          />
          <Button
            type="primary"
            onClick={() => {
              setPage(1);
              load();
            }}
          >
            Search
          </Button>
          <Button onClick={exportCSV}>Export CSV</Button>
        </Space>
        <Table
          rowKey={(record) => record.hash || `${record.time}-${record.kind}-${record.actor}`}
          loading={loading}
          dataSource={rows}
          columns={[
            { title: 'Time', dataIndex: 'time', render: (value) => new Date(value).toLocaleString() },
            { title: 'Kind', dataIndex: 'kind' },
            { title: 'Actor', dataIndex: 'actor' },
            { title: 'Target', dataIndex: 'target' },
            { title: 'IP', dataIndex: ['meta', 'ip'] },
            { title: 'Region', render: (_value, record: any) => String(record?.meta?.ip_region || '-') },
            { title: 'Agent', dataIndex: ['meta', 'agent_id'] },
            { title: 'Workflow', dataIndex: ['meta', 'workflow_id'] },
            { title: 'Script Path', dataIndex: ['meta', 'script_path'] },
          ]}
          pagination={{
            current: page,
            pageSize: size,
            total,
            showSizeChanger: true,
            onChange: (nextPage, nextSize) => {
              setPage(nextPage);
              setSize(nextSize || 20);
            },
          }}
        />
      </Card>
    </PageContainer>
  );
}
