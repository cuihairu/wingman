/**
 * 远程桌面会话审计面板（设计 §11 P1「审计报表呈现」）。
 *
 * 与录像管理面板并列挂在 Agents 页的「会话录像」入口旁——两者是同一批会话
 * 的两个侧面：录像面板管**文件**（下载/删除），本面板管**行为**（谁看了/接管
 * 了哪台机器、多久、失败几次）。
 *
 * 口径全部来自服务端一次响应（列表 + 汇总 + 维度聚合 + 时间趋势），前端不做
 * 再聚合——后端已保证四块视图同口径，前端二次计算只会引入分叉。
 */
import { useCallback, useEffect, useState } from 'react';
import {
  Alert,
  Button,
  Card,
  Col,
  Modal,
  Row,
  Select,
  Space,
  Statistic,
  Table,
  Tag,
  Typography,
} from 'antd';
import { ReloadOutlined } from '@ant-design/icons';
import {
  listRemoteSessions,
  type RemoteSessionBucket,
  type RemoteSessionEntry,
  type RemoteSessionGroup,
  type RemoteSessionQuery,
  type RemoteSessionSummary,
} from '@/services/remote';

const { Text } = Typography;

const STATUS_LABEL: Record<RemoteSessionEntry['status'], string> = {
  closed: '正常断开',
  failed: '建连失败',
};

const PROTOCOL_LABEL: Record<RemoteSessionEntry['protocol'], string> = {
  rdp: 'RDP',
  vnc: 'VNC',
  ssh: 'SSH',
};

const GROUP_BY_LABEL: Record<NonNullable<RemoteSessionQuery['groupBy']>, string> = {
  protocol: '按协议',
  operator: '按操作者',
  agentId: '按机器',
};

const BUCKET_LABEL: Record<NonNullable<RemoteSessionQuery['bucket']>, string> = {
  hour: '按小时',
  day: '按天',
  week: '按周',
  month: '按月',
};

/** 会话时长格式化（ms → 人读；0 显示为 0ms 以区分「瞬间」与「无时长」） */
function formatDuration(ms: number): string {
  if (ms <= 0) {
    return '0ms';
  }
  const s = Math.floor(ms / 1000);
  if (s < 60) {
    return `${s}秒`;
  }
  const m = Math.floor(s / 60);
  if (m < 60) {
    return `${m}分${s % 60}秒`;
  }
  const h = Math.floor(m / 60);
  return `${h}时${m % 60}分`;
}

/**
 * 维度键的展示名。
 *
 * groupBy=protocol 时键就是协议名（rdp/vnc/ssh）→ 显示友好名；
 * 其余维度（operator/agentId）的键是用户名/机器名，原样显示。
 * 未知协议（后端白名单外的灰度值）回退原值，不显示成 undefined。
 */
function formatGroupKey(key: string, groupBy: NonNullable<RemoteSessionQuery['groupBy']>): string {
  if (groupBy !== 'protocol') {
    return key;
  }
  return PROTOCOL_LABEL[key as RemoteSessionEntry['protocol']] ?? key;
}

export interface RemoteSessionReportModalProps {
  open: boolean;
  onCancel: () => void;
  /** 时间范围预设（小时数）；不传则不预设 */
  defaultWindowHours?: number;
}

const EMPTY_SUMMARY: RemoteSessionSummary = {
  total: 0,
  closed: 0,
  failed: 0,
  recorded: 0,
  control: 0,
  viewOnly: 0,
  totalMsSum: 0,
};

/** RemoteSessionReportModal 会话审计报表弹窗（受控开关，内部自持查询状态）。 */
export default function RemoteSessionReportModal({
  open,
  onCancel,
  defaultWindowHours = 168,
}: RemoteSessionReportModalProps) {
  const [entries, setEntries] = useState<RemoteSessionEntry[]>([]);
  const [summary, setSummary] = useState<RemoteSessionSummary>(EMPTY_SUMMARY);
  const [groups, setGroups] = useState<RemoteSessionGroup[]>([]);
  const [buckets, setBuckets] = useState<RemoteSessionBucket[]>([]);
  const [total, setTotal] = useState(0);
  const [page, setPage] = useState(1);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  // 筛选态（groupBy/bucket 影响报表结构，单独存以便和过滤条件区分）。
  // 用 NonNullable 收窄：初值已定且 Select 只会给出固定选项，声明成可选
  // 只会催生 `|| 'protocol'` 这类不可达兜底分支。
  const [groupBy, setGroupBy] = useState<NonNullable<RemoteSessionQuery['groupBy']>>('protocol');
  const [bucket, setBucket] = useState<NonNullable<RemoteSessionQuery['bucket']>>('day');
  const [mode, setMode] = useState<RemoteSessionQuery['mode'] | undefined>(undefined);

  const fetchReport = useCallback(
    async (targetPage: number, silent = false) => {
      if (!silent) {
        setLoading(true);
      }
      setError('');
      try {
        // 默认看最近一周：远控是低频操作，默认全历史几乎总是空表
        const since = new Date(Date.now() - defaultWindowHours * 3600 * 1000).toISOString();
        const report = await listRemoteSessions({
          page: targetPage,
          size: 10,
          groupBy,
          bucket,
          mode,
          start: since,
        });
        setEntries(report.data);
        setSummary(report.summary);
        setGroups(report.groups);
        setBuckets(report.buckets);
        setTotal(report.total);
        setPage(report.page);
      } catch (e) {
        setError(e instanceof Error ? e.message : '获取会话审计报表失败');
      } finally {
        setLoading(false);
      }
    },
    [defaultWindowHours, groupBy, bucket, mode],
  );

  useEffect(() => {
    if (!open) {
      return;
    }
    // 维度/粒度/模式任一变化都回到第 1 页重查（否则可能停在越界空页）
    fetchReport(1);
  }, [open, fetchReport]);

  const onPageChange = (next: number) => {
    fetchReport(next);
  };

  const onGroupByChange = (next: NonNullable<RemoteSessionQuery['groupBy']>) => setGroupBy(next);
  const onBucketChange = (next: NonNullable<RemoteSessionQuery['bucket']>) => setBucket(next);
  const onModeChange = (next: RemoteSessionQuery['mode'] | 'all') =>
    setMode(next === 'all' ? undefined : next);

  return (
    <Modal
      open={open}
      title="远程桌面会话审计"
      width={960}
      onCancel={onCancel}
      footer={
        <Space>
          <Select
            size="small"
            value={groupBy}
            style={{ width: 120 }}
            onChange={onGroupByChange}
            options={Object.entries(GROUP_BY_LABEL).map(([value, label]) => ({ value, label }))}
          />
          <Select
            size="small"
            value={bucket}
            style={{ width: 110 }}
            onChange={onBucketChange}
            options={Object.entries(BUCKET_LABEL).map(([value, label]) => ({ value, label }))}
          />
          <Select
            size="small"
            value={mode ?? 'all'}
            style={{ width: 110 }}
            onChange={onModeChange}
            options={[
              { value: 'all', label: '监看+接管' },
              { value: 'view', label: '仅监看' },
              { value: 'control', label: '仅接管' },
            ]}
          />
          {/* 刷新走 silent：不闪 loading（用户主动点刷新，再糊一层遮罩只是刺眼） */}
          <Button size="small" icon={<ReloadOutlined />} onClick={() => fetchReport(page, true)}>
            刷新
          </Button>
          <Button type="primary" onClick={onCancel}>
            关闭
          </Button>
        </Space>
      }
    >
      {error ? (
        <Alert type="warning" showIcon message="会话审计报表不可用" description={error} />
      ) : (
        <Space direction="vertical" style={{ width: '100%' }} size="middle">
          {/* 汇总：一眼看清区间内的会话构成 */}
          <Row gutter={8}>
            <Col span={4}>
              <Card size="small">
                <Statistic title="会话总数" value={summary.total} />
              </Card>
            </Col>
            <Col span={4}>
              <Card size="small">
                <Statistic title="正常断开" value={summary.closed} />
              </Card>
            </Col>
            <Col span={4}>
              <Card size="small">
                <Statistic
                  title="建连失败"
                  value={summary.failed}
                  valueStyle={summary.failed > 0 ? { color: '#cf1322' } : undefined}
                />
              </Card>
            </Col>
            <Col span={4}>
              <Card size="small">
                <Statistic title="接管次数" value={summary.control} />
              </Card>
            </Col>
            <Col span={4}>
              <Card size="small">
                <Statistic title="监看次数" value={summary.viewOnly} />
              </Card>
            </Col>
            <Col span={4}>
              <Card size="small">
                <Statistic title="累计时长" value={formatDuration(summary.totalMsSum)} />
              </Card>
            </Col>
          </Row>

          {/* 维度分布 + 时间趋势（同一响应的两段，口径天然一致） */}
          <Row gutter={8}>
            <Col span={10}>
              <Card size="small" title={`维度分布（${GROUP_BY_LABEL[groupBy]}）`}>
                {groups.length === 0 ? (
                  <Text type="secondary">该区间无会话</Text>
                ) : (
                  <Space direction="vertical" style={{ width: '100%' }} size={4}>
                    {groups.map((g) => (
                      <div key={g.key} style={{ display: 'flex', justifyContent: 'space-between' }}>
                        <Text>{formatGroupKey(g.key, groupBy)}</Text>
                        <Text type="secondary">
                          {g.count} 次 · {formatDuration(g.msSum)}
                          {g.failed > 0 ? ` · 失败 ${g.failed}` : ''}
                        </Text>
                      </div>
                    ))}
                  </Space>
                )}
              </Card>
            </Col>
            <Col span={14}>
              <Card size="small" title={`时间趋势（${BUCKET_LABEL[bucket]}）`}>
                {buckets.length === 0 ? (
                  <Text type="secondary">该区间无会话</Text>
                ) : (
                  <Space direction="vertical" style={{ width: '100%' }} size={4}>
                    {buckets.map((bk) => (
                      <div
                        key={bk.bucket}
                        style={{ display: 'flex', justifyContent: 'space-between' }}
                      >
                        <Text>{bk.bucket}</Text>
                        <Text type="secondary">
                          {bk.count} 次 · {formatDuration(bk.msSum)}
                          {bk.failed > 0 ? ` · 失败 ${bk.failed}` : ''}
                        </Text>
                      </div>
                    ))}
                  </Space>
                )}
              </Card>
            </Col>
          </Row>

          <Table<RemoteSessionEntry>
            rowKey="id"
            size="small"
            loading={loading}
            dataSource={entries}
            pagination={{
              current: page,
              pageSize: 10,
              total,
              onChange: onPageChange,
              hideOnSinglePage: true,
            }}
            locale={{ emptyText: '该区间无会话记录' }}
            columns={[
              {
                title: '开始时间',
                dataIndex: 'startedAt',
                width: 165,
                render: (v: string) => new Date(v).toLocaleString(),
              },
              { title: '操作者', dataIndex: 'operator', width: 110 },
              { title: '机器', dataIndex: 'agentId', ellipsis: true, width: 130 },
              {
                title: '协议',
                dataIndex: 'protocol',
                width: 70,
                render: (v: RemoteSessionEntry['protocol']) => PROTOCOL_LABEL[v] || v,
              },
              {
                title: '模式',
                dataIndex: 'readOnly',
                width: 70,
                render: (v: boolean) => (v ? <Tag>监看</Tag> : <Tag color="orange">接管</Tag>),
              },
              {
                title: '状态',
                dataIndex: 'status',
                width: 100,
                render: (v: RemoteSessionEntry['status']) =>
                  v === 'failed' ? (
                    <Tag color="red">{STATUS_LABEL[v]}</Tag>
                  ) : (
                    <Tag color="green">{STATUS_LABEL[v]}</Tag>
                  ),
              },
              {
                title: '时长',
                dataIndex: 'durationMs',
                width: 90,
                render: (v: number) => formatDuration(v),
              },
              {
                title: '录像',
                dataIndex: 'recordingName',
                width: 70,
                render: (v?: string) =>
                  v ? <Tag color="blue">有</Tag> : <Text type="secondary">-</Text>,
              },
            ]}
          />
          <Text type="secondary" style={{ fontSize: 12 }}>
            审计仅记录操作者/目标/协议/时长，不含任何凭证与画面内容；进行中的会话不落行，关闭后出现。
          </Text>
        </Space>
      )}
    </Modal>
  );
}
