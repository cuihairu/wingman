import {
  AndroidOutlined,
  AppleOutlined,
  DeleteOutlined,
  DesktopOutlined,
  DownloadOutlined,
  InfoCircleOutlined,
  MobileOutlined,
  NodeIndexOutlined,
  PauseCircleOutlined,
  PlayCircleOutlined,
  ReloadOutlined,
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
  Checkbox,
  Col,
  Input,
  Popconfirm,
  Popover,
  Progress,
  Row,
  Select,
  Space,
  Statistic,
  Table,
  Tag,
  Tooltip,
  Typography,
  message,
  Modal,
  Alert,
  Badge,
} from 'antd';
import React, { useState, useEffect } from 'react';
import TriggerFormModal from '@/components/TriggerFormModal';
import RemoteDesktopModal from '@/components/RemoteDesktopModal';
import {
  deleteRecording,
  downloadRecording,
  listRecordings,
  type RecordingEntry,
  type RemoteProtocol,
} from '@/services/remote';
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
import wsService from '@/services/websocket';

const { Text } = Typography;

// PlatformTag 平台标识：desktop（含缺省，兼容未上报的旧 agent）→ 桌面、
// android → 安卓、ios → Apple、其余 → 移动设备（docs/android-agent-design.md §3.3）
const PlatformTag: React.FC<{ platform?: string }> = ({ platform }) => {
  const value = platform || 'desktop';
  const icon =
    value === 'android' ? (
      <AndroidOutlined />
    ) : value === 'ios' ? (
      <AppleOutlined />
    ) : value === 'desktop' ? (
      <DesktopOutlined />
    ) : (
      <MobileOutlined />
    );
  const color = value === 'android' ? 'green' : value === 'desktop' ? 'geekblue' : 'default';
  return (
    <Tag icon={icon} color={color}>
      {value.toUpperCase()}
    </Tag>
  );
};

// extractErrorMessage 从 umi request 抛出的错误中提取可读文本；
// 模块级纯函数拿不到 intl，兜底文案由调用方传入
function extractErrorMessage(error: unknown, fallback: string): string {
  const candidate = error as
    | { data?: { error?: string; message?: string }; message?: string }
    | undefined;
  return candidate?.data?.error || candidate?.data?.message || candidate?.message || fallback;
}

const Agents: React.FC = () => {
  const access = useAccess();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const canAgentManage = Boolean(access.canAgentManage);
  const canScriptRun = Boolean(access.canScriptRun);
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  // 远程桌面（Guacamole 像素面）：desktopForm = 连接参数表单；desktopTarget = 确认后待连目标
  const [desktopForm, setDesktopForm] = useState<{
    agentId: string;
    protocol: RemoteProtocol;
    username: string;
    password: string;
    readOnly: boolean;
    record: boolean;
  } | null>(null);
  const [desktopTarget, setDesktopTarget] = useState<{
    agentId: string;
    protocol: RemoteProtocol;
    username: string;
    password: string;
    readOnly: boolean;
    record: boolean;
  } | null>(null);
  // openDesktop 表单确认：把表单参数固化为连接目标（触发 RemoteDesktopModal 建连）
  const openDesktop = () => {
    if (desktopForm) {
      setDesktopTarget(desktopForm);
    }
    setDesktopForm(null);
  };
  // ===== 会话录像（设计 §16）：desktop:view 列/下载，desktop:control 删 =====
  const canDesktopView = Boolean(access.canDesktopView);
  const canDesktopControl = Boolean(access.canDesktopControl);
  const [recordingsOpen, setRecordingsOpen] = useState(false);
  const [recordings, setRecordings] = useState<RecordingEntry[]>([]);
  const [recordingsError, setRecordingsError] = useState('');
  const [recordingsLoading, setRecordingsLoading] = useState(false);
  const [deletingRecording, setDeletingRecording] = useState<string | null>(null);
  const fetchRecordings = async (silent = false) => {
    if (!silent) {
      setRecordingsLoading(true);
    }
    setRecordingsError('');
    try {
      setRecordings(await listRecordings());
    } catch (e) {
      // 501（未配置）与其他错误都转为面板文案：错误串本身带配置指引
      setRecordingsError(e instanceof Error ? e.message : '获取会话录像列表失败');
    } finally {
      setRecordingsLoading(false);
    }
  };
  const openRecordings = () => {
    setRecordingsOpen(true);
    fetchRecordings();
  };
  const handleDownloadRecording = async (name: string) => {
    try {
      await downloadRecording(name);
    } catch (e) {
      message.error(extractErrorMessage(e, '下载会话录像失败'));
    }
  };
  const handleDeleteRecording = async (name: string) => {
    setDeletingRecording(name);
    try {
      await deleteRecording(name);
      message.success(`已删除 ${name}`);
      await fetchRecordings(true);
    } catch (e) {
      message.error(extractErrorMessage(e, '删除会话录像失败'));
    } finally {
      setDeletingRecording(null);
    }
  };
  const [wsConnected, setWsConnected] = useState(false);
  const [agents, setAgents] = useState<AgentInfo[]>([]); // 本地状态用于实时更新
  const [tagDraft, setTagDraft] = useState<{ agentId: string; tags: string[] } | null>(null);
  // ===== 批量操作：rowSelection + 批量运行/停止脚本 + 批量下发触发器 =====
  const [selectedRowKeys, setSelectedRowKeys] = useState<React.Key[]>([]);
  const [tagFilter, setTagFilter] = useState<string[]>([]); // 只过滤视图，不影响选中集
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

  // 获取 Agent 列表
  const { loading, refresh } = useRequest(
    async (): Promise<AgentInfo[]> => {
      const response = await getAgents();
      return response.data || [];
    },
    {
      onSuccess: (data) => {
        setAgents(Array.isArray(data) ? (data as AgentInfo[]) : []);
      },
      pollingInterval: 10000, // 降低轮询频率，主要依靠 WebSocket
    },
  );

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

  // showBatchResult 展示逐台结果，清空选择并刷新列表
  const showBatchResult = (summary: BatchSummary | undefined) => {
    setBatchResult(summary ?? { total: 0, succeeded: 0, failed: 0, results: [] });
    setSelectedRowKeys([]);
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
        { id: 'pages.agents.batchRunConfirmContent' },
        { count: selectedIds.length, target: runPath },
      ),
      okText: formatMessage('pages.agents.batchRun'),
      onOk: async () => {
        try {
          const response = await batchRunScript({ agentIds: selectedIds }, runPath);
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
        { id: 'pages.agents.batchStopConfirmContent' },
        { count: selectedIds.length, target: executionId },
      ),
      okText: formatMessage('pages.agents.batchStop'),
      onOk: async () => {
        try {
          const response = await batchStopScript({ agentIds: selectedIds }, executionId);
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
                  username: '',
                  password: '',
                  readOnly: true,
                  record: false,
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
                <Button icon={<ReloadOutlined />} onClick={refresh} loading={loading}>
                  {formatMessage('pages.common.refresh')}
                </Button>
              </Space>
            }
          >
            {/* 批量工具栏：标签筛选只过滤视图；批量按钮选中 0 台时禁用 */}
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
              {canScriptRun && (
                <>
                  <Button
                    icon={<PlayCircleOutlined />}
                    disabled={selectedIds.length === 0}
                    onClick={openBatchRun}
                  >
                    {formatMessage('pages.agents.batchRunScript')}
                  </Button>
                  <Button
                    icon={<PauseCircleOutlined />}
                    disabled={selectedIds.length === 0}
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
                  disabled={selectedIds.length === 0}
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
            </Space>
            <ProTable<AgentInfo>
              columns={columns}
              dataSource={visibleAgents}
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
              pagination={{
                pageSize: 10,
                showSizeChanger: true,
              }}
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
          连接参数（协议/凭据/监看）经小表单确认后下发。 */}
      <Modal
        open={!!desktopForm}
        title={desktopForm ? `远程桌面 - ${desktopForm.agentId}` : ''}
        width={420}
        onOk={openDesktop}
        okText="连接"
        onCancel={() => setDesktopForm(null)}
        destroyOnClose
      >
        <Space direction="vertical" style={{ width: '100%' }} size="middle">
          <Select
            style={{ width: '100%' }}
            value={desktopForm?.protocol}
            onChange={(value) => desktopForm && setDesktopForm({ ...desktopForm, protocol: value })}
            options={[
              { value: 'rdp', label: 'RDP (Windows 桌面)' },
              { value: 'vnc', label: 'VNC (macOS / Linux / Android)' },
              { value: 'ssh', label: 'SSH (终端)' },
            ]}
          />
          <Input
            placeholder="用户名（可选）"
            value={desktopForm?.username}
            onChange={(e) =>
              desktopForm && setDesktopForm({ ...desktopForm, username: e.target.value })
            }
          />
          <Input
            placeholder="密码（可选）"
            type="password"
            value={desktopForm?.password}
            onChange={(e) =>
              desktopForm && setDesktopForm({ ...desktopForm, password: e.target.value })
            }
          />
          <Select
            style={{ width: '100%' }}
            value={desktopForm?.readOnly ? 'view' : 'control'}
            onChange={(value) =>
              desktopForm && setDesktopForm({ ...desktopForm, readOnly: value === 'view' })
            }
            options={[
              { value: 'view', label: '监看（只读）' },
              { value: 'control', label: '接管（需 desktop:control 权限）' },
            ]}
          />
          <Checkbox
            checked={desktopForm?.record}
            onChange={(e) =>
              desktopForm && setDesktopForm({ ...desktopForm, record: e.target.checked })
            }
          >
            会话录制（需服务端配置录制路径，录像不含按键内容）
          </Checkbox>
        </Space>
      </Modal>
      {desktopTarget && (
        <RemoteDesktopModal
          open
          agentId={desktopTarget.agentId}
          protocol={desktopTarget.protocol}
          username={desktopTarget.username || undefined}
          password={desktopTarget.password || undefined}
          readOnly={desktopTarget.readOnly}
          record={desktopTarget.record}
          onCancel={() => setDesktopTarget(null)}
        />
      )}

      {/* 会话录像（设计 §16）：guacd 录制目录检索；.mjs 下载后可用
          guacenc 离线转 mp4；删除需 desktop:control */}
      <Modal
        open={recordingsOpen}
        title="会话录像"
        width={680}
        footer={
          <Button type="primary" onClick={() => setRecordingsOpen(false)}>
            关闭
          </Button>
        }
        onCancel={() => setRecordingsOpen(false)}
      >
        {recordingsError ? (
          <Alert
            type="warning"
            showIcon
            message="会话录像不可用"
            description={recordingsError}
            action={
              <Button size="small" onClick={() => fetchRecordings()}>
                重试
              </Button>
            }
          />
        ) : (
          <Table
            rowKey="name"
            size="small"
            loading={recordingsLoading}
            dataSource={recordings}
            pagination={{ pageSize: 8, hideOnSinglePage: true }}
            locale={{ emptyText: '暂无录像（连接时勾选“会话录制”生成）' }}
            columns={[
              { title: '文件名', dataIndex: 'name', ellipsis: true },
              {
                title: '大小',
                dataIndex: 'sizeBytes',
                width: 90,
                render: (v: number) => formatBytes(v),
              },
              {
                title: '录制时间',
                dataIndex: 'modifiedAt',
                width: 170,
                render: (v: string) => (v ? new Date(v).toLocaleString() : '-'),
              },
              {
                title: '操作',
                key: 'actions',
                width: 150,
                render: (_: unknown, r: RecordingEntry) => (
                  <Space>
                    <Button
                      type="link"
                      size="small"
                      icon={<DownloadOutlined />}
                      onClick={() => handleDownloadRecording(r.name)}
                    >
                      下载
                    </Button>
                    {canDesktopControl && (
                      <Popconfirm
                        title={`删除 ${r.name}？`}
                        description="删除后不可恢复"
                        okText="删除"
                        okButtonProps={{ danger: true }}
                        onConfirm={() => handleDeleteRecording(r.name)}
                      >
                        <Button
                          type="link"
                          size="small"
                          danger
                          icon={<DeleteOutlined />}
                          loading={deletingRecording === r.name}
                        >
                          删除
                        </Button>
                      </Popconfirm>
                    )}
                  </Space>
                ),
              },
            ]}
          />
        )}
      </Modal>

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
          { id: 'pages.agents.batchTriggerModalTitle' },
          {
            count: selectedIds.length,
          },
        )}
        onSubmit={async (config) => {
          let response;
          try {
            response = await batchCreateTrigger({ agentIds: selectedIds }, config);
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
      <Modal
        open={batchResult !== null}
        title={formatMessage('pages.agents.batchResultTitle')}
        width={620}
        footer={
          <Button type="primary" onClick={() => setBatchResult(null)}>
            {formatMessage('pages.common.close')}
          </Button>
        }
        onCancel={() => setBatchResult(null)}
      >
        {batchResult && (
          <Space direction="vertical" style={{ width: '100%' }} size="middle">
            <Alert
              type={
                batchResult.failed === 0
                  ? 'success'
                  : batchResult.succeeded === 0
                    ? 'error'
                    : 'warning'
              }
              showIcon
              message={intl.formatMessage(
                { id: 'pages.agents.batchResultSummary' },
                {
                  total: batchResult.total,
                  succeeded: batchResult.succeeded,
                  failed: batchResult.failed,
                },
              )}
            />
            <Table
              size="small"
              rowKey="agentId"
              dataSource={batchResult.results}
              pagination={batchResult.results.length > 10 ? { pageSize: 10 } : false}
              columns={[
                { title: 'Agent ID', dataIndex: 'agentId', ellipsis: true },
                {
                  title: formatMessage('pages.agents.resultColumn'),
                  dataIndex: 'success',
                  width: 90,
                  render: (ok: boolean) =>
                    ok ? (
                      <Tag color="success">{formatMessage('pages.common.success')}</Tag>
                    ) : (
                      <Tag color="error">{formatMessage('pages.common.failure')}</Tag>
                    ),
                },
                {
                  title: formatMessage('pages.agents.failureReason'),
                  dataIndex: 'error',
                  ellipsis: true,
                  render: (text?: string) => text || '-',
                },
              ]}
            />
          </Space>
        )}
      </Modal>
    </PageContainer>
  );
};

export default Agents;
