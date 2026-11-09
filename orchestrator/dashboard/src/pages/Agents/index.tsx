import {
  AuditOutlined,
  DeleteOutlined,
  DesktopOutlined,
  InfoCircleOutlined,
  NodeIndexOutlined,
  PauseCircleOutlined,
  PlayCircleOutlined,
  ReloadOutlined,
  SafetyCertificateOutlined,
  ScheduleOutlined,
  ThunderboltOutlined,
  VideoCameraOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import {
  PageContainer,
  ProCard,
  type ProColumns,
  ProDescriptions,
  ProTable,
} from '@ant-design/pro-components';
import { useAccess, useIntl, useRequest } from '@umijs/max';
import {
  Button,
  Card,
  Col,
  Input,
  Popover,
  Progress,
  Row,
  Segmented,
  Select,
  Space,
  Statistic,
  Tag,
  Tooltip,
  Typography,
  message,
  Modal,
  Badge,
} from 'antd';
import React, { useState, useEffect } from 'react';
import TriggerFormModal from '@/components/TriggerFormModal';
import VaultManagerModal from '@/components/VaultManagerModal';
import RemoteSessionReportModal from '@/components/RemoteDesktop/RemoteSessionReportModal';
import {
  AgentStatus,
  AgentInfo,
  BatchSummary,
  ScriptInfo,
  batchCreateTrigger,
  batchRunScript,
  batchStopScript,
  getAgents,
  getScripts,
  shutdownAgent,
  setAgentTags,
  formatBytes,
  getAgentStatusColor,
} from '@/services/wingman';
import { countByPlatform, groupAgentsByTag } from '@/utils/agents';
import wsService from '@/services/websocket';
import DesktopConnectModal, { type DesktopConnectForm } from './DesktopConnectModal';
import RecordingsModal from './RecordingsModal';
import BatchResultModal from './BatchResultModal';
import { CapabilityTags, PlatformTag, extractErrorMessage } from './shared';

const { Text } = Typography;

/** 设备列表视图模式：平铺 ↔ 按标签分组 */
type ViewMode = 'flat' | 'grouped';

const Agents: React.FC = () => {
  const access = useAccess();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const canAgentManage = Boolean(access.canAgentManage);
  const canScriptRun = Boolean(access.canScriptRun);
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  // 远程桌面（Guacamole 像素面）：row 操作写入连接表单初值，表单态/保险箱
  // 探测态/建连目标均由 DesktopConnectModal 内部持有（DesktopConnectForm）
  const [desktopForm, setDesktopForm] = useState<DesktopConnectForm | null>(null);
  // ===== 凭据保险箱管理弹窗开关（连接表单内的探测/解锁在 DesktopConnectModal） =====
  const [vaultOpen, setVaultOpen] = useState(false);
  // ===== 会话录像（设计 §16）：desktop:view 列/下载/回放，desktop:control 删 =====
  const canDesktopView = Boolean(access.canDesktopView);
  const [recordingsOpen, setRecordingsOpen] = useState(false);
  const openRecordings = () => {
    setRecordingsOpen(true);
  };
  // 会话审计报表（设计 §11 P1）：只读视图，desktop:view 即可
  const [sessionReportOpen, setSessionReportOpen] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [agents, setAgents] = useState<AgentInfo[]>([]); // 本地状态用于实时更新
  const [tagDraft, setTagDraft] = useState<{ agentId: string; tags: string[] } | null>(null);
  // ===== 批量操作：rowSelection + 批量运行/停止脚本 + 批量下发触发器 =====
  const [selectedRowKeys, setSelectedRowKeys] = useState<React.Key[]>([]);
  const [tagFilter, setTagFilter] = useState<string[]>([]); // 只过滤视图，不影响选中集
  // 按标签批量下发：选择器 = 勾选 agentIds ∪ 标签命中 tags（服务端取并集），
  // 与「按标签筛选」tagFilter 语义正交——后者只过滤可见行，不参与下发选择器。
  const [batchTags, setBatchTags] = useState<string[]>([]);
  // 列表视图模式：平铺（单一表格）↔ 按标签分组（每组一表，勾选跨组共享）
  const [viewMode, setViewMode] = useState<'flat' | 'grouped'>('flat');
  const [batchModal, setBatchModal] = useState<'run' | 'stop' | null>(null);
  const [scripts, setScripts] = useState<ScriptInfo[]>([]);
  const [scriptsLoading, setScriptsLoading] = useState(false);
  const [runPath, setRunPath] = useState<string | undefined>(undefined);
  const [stopExecutionId, setStopExecutionId] = useState('');
  const [triggerModalOpen, setTriggerModalOpen] = useState(false);
  const [batchResult, setBatchResult] = useState<BatchSummary | null>(null);

  const selectedIds = selectedRowKeys.map(String);
  const requestFailed = formatMessage('pages.agents.requestFailed');
  // 标签选项从全量 agent 去重派生
  const tagOptions = Array.from(new Set(agents.flatMap((a) => a.tags ?? []))).sort();
  const visibleAgents =
    tagFilter.length === 0
      ? agents
      : agents.filter((a) => (a.tags ?? []).some((t) => tagFilter.includes(t)));
  // 下发选择器（agentIds ∪ tags）：两端皆空时按钮禁用
  const batchSelector = { agentIds: selectedIds, tags: batchTags };
  const batchSelectorEmpty = selectedIds.length === 0 && batchTags.length === 0;
  const batchTagText = batchTags.join(',');
  // 分组视图：在「按标签筛选」后的可见行上再分组（筛选项优先收敛范围）
  const tagGroups = groupAgentsByTag(visibleAgents);
  const platformCounts = countByPlatform(agents);

  // 获取 Agent 列表。umi 的 useRequest 包装器默认注入 formatResult:
  // r => r?.data（见 .umi/plugin-request/request.ts），因此 service 须返回
  // 完整 ApiResponse 信封由 formatResult 解包——返回裸数组会被二次取
  // .data 变 undefined，列表恒空。
  const { loading, refresh } = useRequest(async () => getAgents(), {
    onSuccess: (data) => {
      setAgents(Array.isArray(data) ? (data as AgentInfo[]) : []);
    },
    pollingInterval: 10000, // 降低轮询频率，主要依靠 WebSocket
  });

  // WebSocket 连接状态
  useEffect(() => {
    const unsubscribe = wsService.onConnectionState((connected) => {
      setWsConnected(connected);
    });

    // 连接 WebSocket
    wsService.connect();

    return () => {
      unsubscribe();
      // 不自动断开，保持全局连接
    };
  }, []);

  // 监听 Agent 事件
  useEffect(() => {
    const unsubscribes: (() => void)[] = [];

    // Agent 连接
    unsubscribes.push(
      wsService.onAgentConnected((data) => {
        setAgents((prev) => {
          const exists = prev.some((a) => a.agentId === data.agentId);
          if (exists) {
            // 更新现有 Agent
            return prev.map((a) =>
              a.agentId === data.agentId ? { ...a, ...data, status: AgentStatus.Online } : a,
            );
          }
          // 添加新 Agent
          return [...prev, { ...data, status: AgentStatus.Online } as AgentInfo];
        });
        message.success(
          intl.formatMessage(
            { id: 'pages.agents.agentOnline' },
            { name: String(data.hostname || data.agentId) },
          ),
        );
      }),
    );

    // Agent 断开
    unsubscribes.push(
      wsService.onAgentDisconnected((data) => {
        setAgents((prev) =>
          prev.map((a) => (a.agentId === data.agentId ? { ...a, status: AgentStatus.Offline } : a)),
        );
        message.warning(
          intl.formatMessage(
            { id: 'pages.agents.agentOffline' },
            { name: String(data.hostname || data.agentId) },
          ),
        );
      }),
    );

    // Agent 状态变化
    unsubscribes.push(
      wsService.onAgentStatusChanged((data) => {
        setAgents((prev) => prev.map((a) => (a.agentId === data.agentId ? { ...a, ...data } : a)));
      }),
    );

    return () => {
      unsubscribes.forEach((unsub) => unsub());
    };
  }, []);

  // 关闭 Agent
  const handleShutdown = async (agentId: string) => {
    Modal.confirm({
      title: formatMessage('pages.agents.shutdownConfirmTitle'),
      content: intl.formatMessage({ id: 'pages.agents.shutdownConfirmContent' }, { id: agentId }),
      onOk: async () => {
        try {
          await shutdownAgent(agentId);
          message.success(formatMessage('pages.agents.shutdownSent'));
          refresh();
        } catch (error) {
          message.error(formatMessage('pages.agents.shutdownFailed'));
        }
      },
    });
  };

  // 保存标签
  const handleSaveTags = async (agentId: string) => {
    if (!tagDraft) return;
    if (!canAgentManage) {
      message.warning(formatMessage('pages.agents.managePermissionRequired'));
      return;
    }
    try {
      await setAgentTags(agentId, tagDraft.tags);
      message.success(formatMessage('pages.agents.tagsUpdated'));
      setTagDraft(null);
      refresh();
    } catch {
      message.error(formatMessage('pages.agents.tagsUpdateFailed'));
    }
  };

  // ===== 批量操作：一律先 Modal.confirm 二次确认，完成后弹结果表并清空选择 =====

  // showBatchResult 展示逐台结果，清空勾选与标签选择器并刷新列表
  const showBatchResult = (summary: BatchSummary | undefined) => {
    setBatchResult(summary ?? { total: 0, succeeded: 0, failed: 0, results: [] });
    setSelectedRowKeys([]);
    setBatchTags([]);
    refresh();
  };

  // openBatchRun 打开运行脚本弹窗并加载脚本列表
  const openBatchRun = async () => {
    setRunPath(undefined);
    setBatchModal('run');
    setScriptsLoading(true);
    try {
      const response = await getScripts();
      setScripts(response.data || []);
    } catch (error) {
      message.error(extractErrorMessage(error, requestFailed));
    } finally {
      setScriptsLoading(false);
    }
  };

  const submitBatchRun = () => {
    if (!runPath) {
      message.warning(formatMessage('pages.agents.selectScriptFirst'));
      return;
    }
    Modal.confirm({
      title: formatMessage('pages.agents.batchRunConfirmTitle'),
      content: intl.formatMessage(
        batchTags.length > 0
          ? { id: 'pages.agents.batchRunConfirmContentTags' }
          : { id: 'pages.agents.batchRunConfirmContent' },
        { count: selectedIds.length, target: runPath, tags: batchTagText },
      ),
      okText: formatMessage('pages.agents.batchRun'),
      onOk: async () => {
        try {
          const response = await batchRunScript(batchSelector, runPath);
          if (!response.success) {
            message.error(response.error || formatMessage('pages.agents.batchRunFailed'));
            return;
          }
          setBatchModal(null);
          showBatchResult(response.data);
        } catch (error) {
          message.error(extractErrorMessage(error, requestFailed));
        }
      },
    });
  };

  const submitBatchStop = () => {
    const executionId = stopExecutionId.trim();
    if (!executionId) {
      message.warning(formatMessage('pages.agents.inputExecutionId'));
      return;
    }
    Modal.confirm({
      title: formatMessage('pages.agents.batchStopConfirmTitle'),
      content: intl.formatMessage(
        batchTags.length > 0
          ? { id: 'pages.agents.batchStopConfirmContentTags' }
          : { id: 'pages.agents.batchStopConfirmContent' },
        { count: selectedIds.length, target: executionId, tags: batchTagText },
      ),
      okText: formatMessage('pages.agents.batchStop'),
      onOk: async () => {
        try {
          const response = await batchStopScript(batchSelector, executionId);
          if (!response.success) {
            message.error(response.error || formatMessage('pages.agents.batchStopFailed'));
            return;
          }
          setBatchModal(null);
          showBatchResult(response.data);
        } catch (error) {
          message.error(extractErrorMessage(error, requestFailed));
        }
      },
    });
  };

  // 资源卡片组件
  const ResourceCard: React.FC<{ agent: AgentInfo }> = ({ agent }) => {
    const { cpu, memory, disk, network } = agent.resources;

    return (
      <Card
        size="small"
        title={
          <>
            <ScheduleOutlined /> {formatMessage('pages.agents.systemResources')}
          </>
        }
      >
        <Row gutter={[16, 16]}>
          <Col xs={24} sm={12}>
            <Statistic
              title={formatMessage('pages.agents.cpuUsage')}
              value={cpu.usage}
              suffix="%"
              valueStyle={{
                color: cpu.usage > 80 ? '#ff4d4f' : cpu.usage > 50 ? '#faad14' : '#52c41a',
              }}
            />
            <Progress percent={cpu.usage} size="small" showInfo={false} />
            <Text type="secondary" style={{ fontSize: 12 }}>
              {intl.formatMessage(
                { id: 'pages.agents.coresModel' },
                { cores: cpu.cores, model: cpu.model || 'Unknown' },
              )}
            </Text>
          </Col>
          <Col xs={24} sm={12}>
            <Statistic
              title={formatMessage('pages.agents.memoryUsage')}
              value={memory.usage}
              suffix="%"
              valueStyle={{
                color: memory.usage > 80 ? '#ff4d4f' : memory.usage > 50 ? '#faad14' : '#52c41a',
              }}
            />
            <Progress percent={memory.usage} size="small" showInfo={false} />
            <Text type="secondary" style={{ fontSize: 12 }}>
              {formatBytes(memory.available)} / {formatBytes(memory.total)}
            </Text>
          </Col>
          <Col xs={24} sm={12}>
            <Statistic
              title={formatMessage('pages.agents.diskUsage')}
              value={disk.usage}
              suffix="%"
              valueStyle={{ color: disk.usage > 90 ? '#ff4d4f' : '#52c41a' }}
            />
            <Progress percent={disk.usage} size="small" showInfo={false} />
            <Text type="secondary" style={{ fontSize: 12 }}>
              {formatBytes(disk.available)} / {formatBytes(disk.total)}
            </Text>
          </Col>
          <Col xs={24} sm={12}>
            <Row gutter={8}>
              <Col span={12}>
                <Statistic
                  title={formatMessage('pages.agents.upload')}
                  value={formatBytes(network.up)}
                  suffix="/s"
                  style={{ fontSize: 14 }}
                />
              </Col>
              <Col span={12}>
                <Statistic
                  title={formatMessage('pages.agents.download')}
                  value={formatBytes(network.down)}
                  suffix="/s"
                  style={{ fontSize: 14 }}
                />
              </Col>
            </Row>
            <Text type="secondary" style={{ fontSize: 12 }}>
              {network.localIp}
            </Text>
          </Col>
        </Row>
      </Card>
    );
  };

  const columns: ProColumns<AgentInfo>[] = [
    {
      title: 'Agent ID',
      dataIndex: 'agentId',
      key: 'agentId',
      width: 200,
      render: (_, record) => (
        <Space>
          <NodeIndexOutlined />
          <Text copyable={{ text: record.agentId }}>{record.agentId.slice(0, 12)}...</Text>
        </Space>
      ),
    },
    {
      title: formatMessage('pages.agents.hostname'),
      dataIndex: 'hostname',
      key: 'hostname',
      width: 150,
    },
    {
      title: formatMessage('pages.agents.platform'),
      dataIndex: 'platform',
      key: 'platform',
      width: 120,
      render: (_, record) => <PlatformTag platform={record.platform} />,
    },
    {
      title: formatMessage('pages.agents.capabilities'),
      dataIndex: 'capabilities',
      key: 'capabilities',
      width: 220,
      render: (_, record) => <CapabilityTags record={record} />,
    },
    {
      title: formatMessage('pages.agents.ipAddress'),
      dataIndex: 'ip',
      key: 'ip',
      width: 120,
    },
    {
      title: formatMessage('pages.common.status'),
      dataIndex: 'status',
      key: 'status',
      width: 100,
      render: (_, record) => (
        <Tag color={getAgentStatusColor(record.status)}>{record.status.toUpperCase()}</Tag>
      ),
    },
    {
      title: formatMessage('pages.agents.tags'),
      dataIndex: 'tags',
      key: 'tags',
      width: 180,
      render: (_, record) => {
        const tags = record.tags || [];
        const content = (
          <div style={{ width: 220 }}>
            <Select
              mode="tags"
              style={{ width: '100%' }}
              placeholder={formatMessage('pages.agents.tagInputPlaceholder')}
              value={tagDraft?.agentId === record.agentId ? tagDraft.tags : tags}
              onChange={(vals) => setTagDraft({ agentId: record.agentId, tags: vals as string[] })}
            />
            <div style={{ marginTop: 8, textAlign: 'right' }}>
              <Space>
                <Button size="small" onClick={() => setTagDraft(null)}>
                  {formatMessage('pages.common.cancel')}
                </Button>
                <Button size="small" type="primary" onClick={() => handleSaveTags(record.agentId)}>
                  {formatMessage('pages.common.save')}
                </Button>
              </Space>
            </div>
          </div>
        );
        const tagList = (
          <Space size={[0, 4]} wrap style={{ cursor: canAgentManage ? 'pointer' : 'default' }}>
            {tags.length === 0 ? (
              <Tag style={{ borderStyle: 'dashed' }}>
                {canAgentManage
                  ? formatMessage('pages.agents.addTag')
                  : formatMessage('pages.agents.noTags')}
              </Tag>
            ) : (
              tags.map((t) => (
                <Tag key={t} color="blue">
                  {t}
                </Tag>
              ))
            )}
          </Space>
        );
        if (!canAgentManage) {
          return (
            <Tooltip title={formatMessage('pages.agents.tagPermissionTooltip')}>{tagList}</Tooltip>
          );
        }
        return (
          <Popover
            content={content}
            title={formatMessage('pages.agents.editTags')}
            trigger="click"
            placement="bottom"
          >
            {tagList}
          </Popover>
        );
      },
    },
    {
      title: 'CPU',
      dataIndex: ['resources', 'cpu', 'usage'],
      key: 'cpu',
      width: 100,
      render: (_, record) => (
        <Progress percent={Math.round(record.resources?.cpu?.usage || 0)} size="small" />
      ),
    },
    {
      title: formatMessage('pages.agents.memory'),
      dataIndex: ['resources', 'memory', 'usage'],
      key: 'memory',
      width: 100,
      render: (_, record) => (
        <Progress percent={Math.round(record.resources?.memory?.usage || 0)} size="small" />
      ),
    },
    {
      title: formatMessage('pages.agents.lastActive'),
      dataIndex: 'lastSeen',
      key: 'lastSeen',
      width: 120,
      render: (_, record) => {
        const diff = Date.now() - record.lastSeen;
        if (diff < 60000) return formatMessage('pages.agents.lessThanMinute');
        if (diff < 3600000)
          return intl.formatMessage(
            { id: 'pages.agents.minutesAgo' },
            {
              count: Math.floor(diff / 60000),
            },
          );
        return intl.formatMessage(
          { id: 'pages.agents.hoursAgo' },
          {
            count: Math.floor(diff / 3600000),
          },
        );
      },
    },
    {
      title: formatMessage('pages.common.action'),
      key: 'action',
      width: 120,
      fixed: 'right' as const,
      render: (_: any, record: AgentInfo) => (
        <Space>
          <Tooltip title={formatMessage('pages.agents.viewDetails')}>
            <Button
              type="link"
              size="small"
              icon={<InfoCircleOutlined />}
              onClick={() => setSelectedAgent(record)}
            />
          </Tooltip>
          <Tooltip title="远程桌面">
            <Button
              type="link"
              size="small"
              icon={<DesktopOutlined />}
              onClick={() =>
                setDesktopForm({
                  agentId: record.agentId,
                  protocol: record.platform === 'android' ? 'vnc' : 'rdp',
                  port: '',
                  username: '',
                  password: '',
                  readOnly: true,
                  record: false,
                  useSaved: false,
                  saveToVault: false,
                })
              }
              disabled={record.status === AgentStatus.Offline}
            />
          </Tooltip>
          <Tooltip title={formatMessage('pages.agents.shutdownTooltip')}>
            <Button
              type="link"
              size="small"
              danger
              icon={<DeleteOutlined />}
              onClick={() => handleShutdown(record.agentId)}
              disabled={!canAgentManage || record.status === AgentStatus.Offline}
            />
          </Tooltip>
        </Space>
      ),
    },
  ];

  // 设备表（平铺与分组视图共用同一列定义、勾选集与行点击行为）
  const renderAgentTable = (dataSource: AgentInfo[]) => (
    <ProTable<AgentInfo>
      columns={columns}
      dataSource={dataSource}
      loading={loading}
      rowKey="agentId"
      search={false}
      options={false}
      rowSelection={
        canScriptRun || canAgentManage
          ? {
              selectedRowKeys,
              onChange: (keys: React.Key[]) => setSelectedRowKeys(keys),
            }
          : undefined
      }
      pagination={
        viewMode === 'grouped'
          ? { pageSize: 5, showSizeChanger: false }
          : { pageSize: 10, showSizeChanger: true }
      }
      onRow={(record) => ({
        onClick: (event) => {
          // 勾选批量选择框时不打开详情面板
          if ((event.target as HTMLElement).closest('.ant-table-selection-column')) {
            return;
          }
          setSelectedAgent(record);
        },
        style: { cursor: 'pointer' },
      })}
    />
  );

  return (
    <PageContainer
      header={{
        title: formatMessage('pages.agents.title'),
        subTitle: formatMessage('pages.agents.subtitle'),
        extra: wsConnected ? (
          <Badge status="processing" text={formatMessage('pages.agents.realtimeConnected')} />
        ) : (
          <Space>
            <Badge status="default" text={formatMessage('pages.agents.pollingMode')} />
            <Button size="small" onClick={() => wsService.reconnect()}>
              {formatMessage('pages.agents.reconnect')}
            </Button>
          </Space>
        ),
      }}
    >
      <Row gutter={[16, 16]}>
        {/* 统计卡片 */}
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title={formatMessage('pages.common.total')}
              value={agents.length}
              prefix={<NodeIndexOutlined />}
            />
            {/* 平台维度大盘：desktop/android/ios 计数（缺失 platform 按桌面口径） */}
            <div style={{ marginTop: 8 }}>
              {platformCounts.map((entry) => (
                <PlatformTag key={entry.platform} platform={entry.platform} count={entry.count} />
              ))}
            </div>
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title={formatMessage('pages.agents.online')}
              value={
                agents.filter(
                  (a: AgentInfo) =>
                    a.status === AgentStatus.Online || a.status === AgentStatus.Idle,
                ).length
              }
              valueStyle={{ color: '#52c41a' }}
            />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title={formatMessage('pages.agents.busy')}
              value={agents.filter((a: AgentInfo) => a.status === AgentStatus.Busy).length}
              valueStyle={{ color: '#faad14' }}
            />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title={formatMessage('pages.agents.offline')}
              value={agents.filter((a: AgentInfo) => a.status === AgentStatus.Offline).length}
              valueStyle={{ color: '#999' }}
            />
          </Card>
        </Col>
      </Row>

      <Row gutter={[16, 16]} style={{ marginTop: 16 }}>
        {/* Agent 列表 */}
        <Col xs={24} lg={selectedAgent ? 16 : 24}>
          <ProCard
            title={formatMessage('pages.agents.list')}
            extra={
              <Space>
                {wsConnected && (
                  <Tag icon={<WifiOutlined />} color="success">
                    {formatMessage('pages.agents.realtimeUpdate')}
                  </Tag>
                )}
                {canDesktopView && (
                  <Button icon={<VideoCameraOutlined />} onClick={openRecordings}>
                    会话录像
                  </Button>
                )}
                {/* 会话审计报表（设计 §11 P1）：录像看文件，报表看行为 */}
                {canDesktopView && (
                  <Button icon={<AuditOutlined />} onClick={() => setSessionReportOpen(true)}>
                    会话审计
                  </Button>
                )}
                {/* 凭据保险箱：主口令加密存储 + 连接快捷调用（vault.go） */}
                {canDesktopView && (
                  <Button icon={<SafetyCertificateOutlined />} onClick={() => setVaultOpen(true)}>
                    凭据保险箱
                  </Button>
                )}
                <Button icon={<ReloadOutlined />} onClick={refresh} loading={loading}>
                  {formatMessage('pages.common.refresh')}
                </Button>
              </Space>
            }
          >
            {/* 批量工具栏：标签筛选只过滤视图；批量按钮在选择器为空时禁用 */}
            <Space style={{ marginBottom: 16, display: 'flex' }} wrap>
              <Select
                mode="multiple"
                allowClear
                placeholder={formatMessage('pages.agents.filterByTag')}
                style={{ minWidth: 200 }}
                value={tagFilter}
                onChange={(vals) => setTagFilter(vals)}
                options={tagOptions.map((tag) => ({ value: tag, label: tag }))}
              />
              {/* 视图切换：平铺 ↔ 按标签分组（有标签时才有意义） */}
              {tagOptions.length > 0 && (
                <Segmented<ViewMode>
                  value={viewMode}
                  onChange={(val) => setViewMode(val)}
                  options={[
                    { value: 'flat', label: formatMessage('pages.agents.viewFlat') },
                    { value: 'grouped', label: formatMessage('pages.agents.viewGrouped') },
                  ]}
                />
              )}
              {/* 按标签下发：与勾选取并集（docs/architecture-decisions.md 批量下发表） */}
              {canScriptRun && tagOptions.length > 0 && (
                <Select
                  mode="multiple"
                  allowClear
                  placeholder={formatMessage('pages.agents.batchByTags')}
                  style={{ minWidth: 160 }}
                  value={batchTags}
                  onChange={(vals) => setBatchTags(vals)}
                  options={tagOptions.map((tag) => ({ value: tag, label: tag }))}
                />
              )}
              {canScriptRun && (
                <>
                  <Button
                    icon={<PlayCircleOutlined />}
                    disabled={batchSelectorEmpty}
                    onClick={openBatchRun}
                  >
                    {formatMessage('pages.agents.batchRunScript')}
                  </Button>
                  <Button
                    icon={<PauseCircleOutlined />}
                    disabled={batchSelectorEmpty}
                    onClick={() => {
                      setStopExecutionId('');
                      setBatchModal('stop');
                    }}
                  >
                    {formatMessage('pages.agents.batchStopScript')}
                  </Button>
                </>
              )}
              {canAgentManage && (
                <Button
                  icon={<ThunderboltOutlined />}
                  disabled={batchSelectorEmpty}
                  onClick={() => setTriggerModalOpen(true)}
                >
                  {formatMessage('pages.agents.batchDeployTrigger')}
                </Button>
              )}
              {selectedIds.length > 0 && (
                <Text type="secondary">
                  {intl.formatMessage(
                    { id: 'pages.agents.selectedCount' },
                    {
                      count: selectedIds.length,
                    },
                  )}
                </Text>
              )}
              {batchTags.length > 0 && (
                <Text type="secondary">
                  {intl.formatMessage(
                    { id: 'pages.agents.byTagsSelected' },
                    { tags: batchTagText },
                  )}
                </Text>
              )}
            </Space>
            {/* 列表：平铺单表 或 按标签分组多表（勾选跨组共享同一选择集） */}
            {(viewMode === 'flat' || tagGroups.length === 0) && renderAgentTable(visibleAgents)}
            {viewMode === 'grouped' &&
              tagGroups.map((group) => (
                <ProCard
                  key={group.tag ?? '__ungrouped__'}
                  title={group.tag ?? formatMessage('pages.agents.untaggedGroup')}
                  style={{ marginBottom: 16 }}
                >
                  {renderAgentTable(group.agents)}
                </ProCard>
              ))}
          </ProCard>
        </Col>

        {/* Agent 详情 */}
        {selectedAgent && (
          <Col xs={24} lg={8}>
            <Space direction="vertical" style={{ width: '100%' }} size="large">
              <ProCard
                title={formatMessage('pages.agents.detailTitle')}
                extra={
                  <Button type="text" onClick={() => setSelectedAgent(null)}>
                    {formatMessage('pages.common.close')}
                  </Button>
                }
              >
                <ProDescriptions
                  column={1}
                  dataSource={selectedAgent}
                  columns={[
                    {
                      title: 'Agent ID',
                      dataIndex: 'agentId',
                      render: (_, record) => <Text copyable>{record.agentId}</Text>,
                    },
                    {
                      title: formatMessage('pages.agents.hostname'),
                      dataIndex: 'hostname',
                    },
                    {
                      title: formatMessage('pages.agents.platform'),
                      dataIndex: 'platform',
                      render: (_, record) => <PlatformTag platform={record.platform} />,
                    },
                    {
                      title: formatMessage('pages.agents.capabilities'),
                      dataIndex: 'capabilities',
                      render: (_, record) => <CapabilityTags record={record} />,
                    },
                    {
                      title: formatMessage('pages.agents.ipAddress'),
                      dataIndex: 'ip',
                    },
                    {
                      title: formatMessage('pages.common.status'),
                      dataIndex: 'status',
                      render: (_, record) => (
                        <Tag color={getAgentStatusColor(record.status)}>
                          {record.status.toUpperCase()}
                        </Tag>
                      ),
                    },
                    {
                      title: formatMessage('pages.agents.currentTask'),
                      dataIndex: 'currentTask',
                      render: (task) =>
                        task || <Text type="secondary">{formatMessage('pages.common.none')}</Text>,
                    },
                    {
                      title: formatMessage('pages.agents.os'),
                      dataIndex: ['resources', 'system', 'os'],
                    },
                    {
                      title: formatMessage('pages.agents.arch'),
                      dataIndex: ['resources', 'system', 'arch'],
                    },
                  ]}
                />
              </ProCard>

              <ResourceCard agent={selectedAgent} />
            </Space>
          </Col>
        )}
      </Row>

      {/* 远程桌面（Guacamole 像素面）：一次性票据 + WS 隧道，关闭即断开。
          连接参数（协议/凭据/监看）经小表单确认后下发，建连在组件内部完成。 */}
      <DesktopConnectModal form={desktopForm} onCancel={() => setDesktopForm(null)} />

      {/* 凭据保险箱管理：主口令生命周期 + 条目列表（改标签/删除）+ 密文导出 */}
      <VaultManagerModal open={vaultOpen} onClose={() => setVaultOpen(false)} />

      {/* 会话录像（设计 §16）：guacd 录制目录检索；.mjs 下载后可用
          guacenc 离线转 mp4；删除需 desktop:control；列表/回放在组件内部 */}
      <RecordingsModal open={recordingsOpen} onClose={() => setRecordingsOpen(false)} />

      {/* 远程桌面会话审计报表（设计 §11 P1）：谁在何时接管了哪台机器、多久、
          失败几次；与录像面板同属一批会话的两个侧面（文件 vs 行为） */}
      {canDesktopView && (
        <RemoteSessionReportModal
          open={sessionReportOpen}
          onCancel={() => setSessionReportOpen(false)}
        />
      )}

      {/* 批量运行脚本：脚本列表来自 /api/scripts，服务端会先做路径校验 */}
      <Modal
        open={batchModal === 'run'}
        title={intl.formatMessage(
          { id: 'pages.agents.batchRunModalTitle' },
          {
            count: selectedIds.length,
          },
        )}
        width={520}
        onOk={submitBatchRun}
        onCancel={() => setBatchModal(null)}
        destroyOnClose
      >
        <Select
          showSearch
          style={{ width: '100%' }}
          placeholder={formatMessage('pages.agents.selectScriptPlaceholder')}
          loading={scriptsLoading}
          value={runPath}
          onChange={(value) => setRunPath(value)}
          optionFilterProp="label"
          options={scripts.map((script) => ({
            value: script.path,
            label: script.path || script.name,
          }))}
        />
      </Modal>

      {/* 批量停止脚本：executionId 即脚本名，与单 agent stop 语义一致 */}
      <Modal
        open={batchModal === 'stop'}
        title={intl.formatMessage(
          { id: 'pages.agents.batchStopModalTitle' },
          {
            count: selectedIds.length,
          },
        )}
        width={520}
        onOk={submitBatchStop}
        onCancel={() => setBatchModal(null)}
        destroyOnClose
      >
        <Input
          placeholder={formatMessage('pages.agents.executionIdPlaceholder')}
          value={stopExecutionId}
          onChange={(event) => setStopExecutionId(event.target.value)}
        />
      </Modal>

      {/* 批量下发触发器：复用共享表单，提交即 fan-out trigger.add */}
      <TriggerFormModal
        open={triggerModalOpen}
        title={intl.formatMessage(
          batchTags.length > 0
            ? { id: 'pages.agents.batchTriggerModalTitleTags' }
            : { id: 'pages.agents.batchTriggerModalTitle' },
          {
            count: selectedIds.length,
            tags: batchTagText,
          },
        )}
        onSubmit={async (config) => {
          let response;
          try {
            response = await batchCreateTrigger(batchSelector, config);
          } catch (error) {
            throw new Error(extractErrorMessage(error, requestFailed));
          }
          if (!response.success) {
            throw new Error(response.error || formatMessage('pages.agents.batchDeployFailed'));
          }
          setTriggerModalOpen(false);
          showBatchResult(response.data);
        }}
        onClose={() => setTriggerModalOpen(false)}
      />

      {/* 批量操作结果：汇总 + 逐台明细（含离线/失败原因） */}
      <BatchResultModal result={batchResult} onClose={() => setBatchResult(null)} />
    </PageContainer>
  );
};

export default Agents;
