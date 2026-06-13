import React, { useEffect, useMemo, useState } from 'react';
import { Card, Table, Space, Input, Button, DatePicker, Tag } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { listAudit, type AuditEvent } from '@/services/api';

export default function LoginLogsPage() {
  const [rows, setRows] = useState<AuditEvent[]>([]);
  const [total, setTotal] = useState<number>(0);
  const [loading, setLoading] = useState(false);
  const [actor, setActor] = useState<string>(
    () => new URLSearchParams(location.search).get('actor') || '',
  );
  const [kinds, setKinds] = useState<string[]>(['login', 'login_fail', 'login_rate_limited']);
  const [timeRange, setTimeRange] = useState<any>(null);
  const [page, setPage] = useState<number>(1);
  const [size, setSize] = useState<number>(20);
  const [osSel, setOsSel] = useState<string[]>([]);
  const [brSel, setBrSel] = useState<string[]>([]);

  const load = async () => {
    setLoading(true);
    try {
      const params: any = { page, size };
      if (actor) params.actor = actor;
      params.kinds = (kinds.length > 0 ? kinds : ['login']).join(',');
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
  }, []);

  const filtered = useMemo(() => {
    return rows.filter((event) => {
      const ua = String(event.meta?.ua || event.meta?.user_agent || '');
      const os = detectOS(ua);
      const browser = detectBrowser(ua);
      const osMatched = osSel.length === 0 || osSel.includes(os);
      const browserMatched = brSel.length === 0 || brSel.includes(browser);
      return osMatched && browserMatched;
    });
  }, [brSel, osSel, rows]);

  const paged = useMemo(() => {
    const startIndex = (page - 1) * size;
    return filtered.slice(startIndex, startIndex + size);
  }, [filtered, page, size]);

  const exportCSV = () => {
    const data = filtered.map((event) => {
      const ua = String(event.meta?.ua || event.meta?.user_agent || '');
      return [
        new Date(event.time || '').toISOString(),
        event.kind || '',
        event.actor || '',
        event.meta?.ip || '',
        event.meta?.ip_region || '',
        ua,
        detectOS(ua),
        detectBrowser(ua),
      ];
    });
    data.unshift(['time', 'kind', 'actor', 'ip', 'region', 'ua', 'os', 'browser']);
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
    anchor.download = 'login_logs.csv';
    anchor.click();
    URL.revokeObjectURL(url);
  };

  function detectOS(ua: string): string {
    const text = String(ua || '');
    if (/android/i.test(text)) return 'Android';
    if (/iphone|ipad|ipod/i.test(text)) return 'iOS';
    if (/windows nt/i.test(text)) return 'Windows';
    if (/mac os x/i.test(text)) return 'macOS';
    if (/linux/i.test(text)) return 'Linux';
    return text ? 'Other' : '';
  }

  function detectBrowser(ua: string): string {
    const text = String(ua || '');
    if (/edg\//i.test(text)) return 'Edge';
    if (/chrome\//i.test(text)) return 'Chrome';
    if (/safari\//i.test(text) && !/chrome\//i.test(text)) return 'Safari';
    if (/firefox\//i.test(text)) return 'Firefox';
    return text ? 'Other' : '';
  }

  return (
    <PageContainer>
      <Card title="Login Logs">
        <Space style={{ marginBottom: 12 }} wrap>
          <Input
            placeholder="Actor"
            value={actor}
            onChange={(event) => setActor(event.target.value)}
            style={{ width: 160 }}
          />
          <Space size={4}>
            {['login', 'login_fail', 'login_rate_limited'].map((kind) => (
              <Tag
                key={kind}
                color={kinds.includes(kind) ? 'blue' : 'default'}
                onClick={() => {
                  setKinds((previous) =>
                    previous.includes(kind)
                      ? previous.filter((value) => value !== kind)
                      : [...previous, kind],
                  );
                }}
                style={{ cursor: 'pointer' }}
              >
                {kind}
              </Tag>
            ))}
          </Space>
          <DatePicker.RangePicker
            showTime
            value={timeRange as any}
            onChange={setTimeRange as any}
          />
          <Button type="primary" onClick={load}>
            Search
          </Button>
          <Button onClick={exportCSV}>Export CSV</Button>
        </Space>
        <Space style={{ marginBottom: 12 }} wrap>
          <span>Device:</span>
          <Space size={4}>
            {['Windows', 'macOS', 'Linux', 'Android', 'iOS', 'Other'].map((os) => (
              <Tag
                key={os}
                color={osSel.includes(os) ? 'blue' : 'default'}
                onClick={() =>
                  setOsSel((previous) =>
                    previous.includes(os)
                      ? previous.filter((value) => value !== os)
                      : [...previous, os],
                  )
                }
                style={{ cursor: 'pointer' }}
              >
                {os}
              </Tag>
            ))}
          </Space>
          <span>Browser:</span>
          <Space size={4}>
            {['Edge', 'Chrome', 'Safari', 'Firefox', 'Other'].map((browser) => (
              <Tag
                key={browser}
                color={brSel.includes(browser) ? 'blue' : 'default'}
                onClick={() =>
                  setBrSel((previous) =>
                    previous.includes(browser)
                      ? previous.filter((value) => value !== browser)
                      : [...previous, browser],
                  )
                }
                style={{ cursor: 'pointer' }}
              >
                {browser}
              </Tag>
            ))}
          </Space>
          <Button
            onClick={() => {
              setOsSel([]);
              setBrSel([]);
            }}
          >
            Clear Device Filters
          </Button>
        </Space>
        <Table
          rowKey={(record) => record.hash || `${record.time}-${record.kind}-${record.actor}`}
          loading={loading}
          dataSource={paged}
          columns={[
            { title: 'Time', dataIndex: 'time', render: (value) => new Date(value).toLocaleString() },
            {
              title: 'Kind',
              dataIndex: 'kind',
              render: (value) => (
                <Tag color={value === 'login' ? 'green' : value === 'login_fail' ? 'red' : 'gold'}>
                  {value}
                </Tag>
              ),
            },
            { title: 'Actor', dataIndex: 'actor' },
            { title: 'IP', dataIndex: ['meta', 'ip'] },
            { title: 'Region', render: (_value, record: any) => String(record?.meta?.ip_region || '-') },
            {
              title: 'Device',
              render: (_value, record: any) =>
                detectOS(String(record.meta?.ua || record.meta?.user_agent || '')),
            },
            {
              title: 'Browser',
              render: (_value, record: any) =>
                detectBrowser(String(record.meta?.ua || record.meta?.user_agent || '')),
            },
          ]}
          expandable={{
            expandedRowRender: (record: any) => {
              const ua = String(record?.meta?.ua || record?.meta?.user_agent || '');
              return (
                <div style={{ fontFamily: 'monospace', whiteSpace: 'pre-wrap' }}>
                  Browser: {detectBrowser(ua) || '-'}
                  {'\n'}
                  UA: {ua || '-'}
                </div>
              );
            },
          }}
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
