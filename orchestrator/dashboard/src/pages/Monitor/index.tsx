/**
 * 游戏监控页面
 * 集成实时截图、Agent 资源状态、触发器事件流和宏命令入口。
 * 触发器列表经 /api/agents/:id/triggers 读取真实 runtime 数据，
 * trigger_fired 事件实时叠加命中计数；
 * 支持新增/编辑/删除（trigger.add/update/remove 透传，需 agents:manage）。
 */
import React, { useCallback, useEffect, useRef, useState } from 'react';
import { PageContainer } from '@ant-design/pro-components';
import {
  Alert,
  Button,
  Card,
  Col,
  List,
  Modal,
  Popconfirm,
  Progress,
  Row,
  Space,
  Statistic,
  Switch,
  Tag,
  Timeline,
  Typography,
} from 'antd';
import {
  CameraOutlined,
  ClockCircleOutlined,
  CodeOutlined,
  DatabaseOutlined,
  DeleteOutlined,
  DesktopOutlined,
  EditOutlined,
  PauseCircleOutlined,
  PlayCircleOutlined,
  PlusOutlined,
  ReloadOutlined,
  SettingOutlined,
  ThunderboltOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import { history, useIntl } from '@umijs/max';
import ScreenshotView from '@/components/ScreenshotView';
import TriggerFormModal from '@/components/TriggerFormModal';
import wsService from '@/services/websocket';
import {
  AgentInfo,
  AgentStatus,
  AgentTrigger,
  createAgentTrigger,
  getAgentTriggers,
  getAgents,
  removeAgentTrigger,
  toggleAgentTrigger,
  updateAgentTrigger,
} from '@/services/wingman';

const { Text } = Typography;

interface MonitorTrigger extends AgentTrigger {
  hitCount: number;
  lastFiredAt?: string;
}

interface SystemStatus {
  cpu: number;
  memory: number;
  uptime: number;
  fps: number;
  networkUp: number;
  networkDown: number;
  onlineAgents: number;
  source: string;
}

interface RuntimeEvent {
  id: string;
  time: string;
  type: 'agent' | 'trigger' | 'script' | 'screenshot';
  message: string;
  level: 'success' | 'processing' | 'warning' | 'error' | 'default';
}

function safeNumber(value: unknown, fallback = 0): number {
  const numberValue = Number(value);
  return Number.isFinite(numberValue) ? numberValue : fallback;
}

function formatBytes(value: number): string {
  if (!value) return '0 B/s';
  if (value < 1024) return `${Math.round(value)} B/s`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB/s`;
  return `${(value / 1024 / 1024).toFixed(1)} MB/s`;
}

function formatUptime(ms: number): string {
  const seconds = Math.floor(ms / 1000);
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return `${hours}h ${minutes}m`;
}

// 相对时间文案模块级生成不了（拿不到 intl），由调用方传入翻译函数
// （同 extractErrorMessage 的 fallback 模式）
function formatLastSeen(
  lastSeen: number,
  translate: (id: string, values?: Record<string, string | number>) => string,
): string {
  if (!lastSeen) return translate('pages.monitor.lastSeenUnknown');
  const delta = Date.now() - lastSeen;
  if (delta < 0) return translate('pages.monitor.lastSeenJustNow');
  if (delta < 60_000)
    return translate('pages.monitor.lastSeenSecondsAgo', { seconds: Math.floor(delta / 1000) });
  if (delta < 3_600_000)
    return translate('pages.monitor.lastSeenMinutesAgo', { minutes: Math.floor(delta / 60_000) });
  return translate('pages.monitor.lastSeenHoursAgo', { hours: Math.floor(delta / 3_600_000) });
}

// extractErrorMessage 从 umi request 抛出的错误中提取可读文本；
// 模块级纯函数拿不到 intl，兜底文案由调用方传入
function extractErrorMessage(error: unknown, fallback: string): string {
  const candidate = error as
    | { data?: { error?: string; message?: string }; message?: string }
    | undefined;
  return candidate?.data?.error || candidate?.data?.message || candidate?.message || fallback;
}

const connectedStatuses = [AgentStatus.Online, AgentStatus.Idle, AgentStatus.Busy];

function agentConnected(agent: AgentInfo | undefined): boolean {
  return !!agent && connectedStatuses.includes(agent.status);
}

function primaryAgent(agents: AgentInfo[]): AgentInfo | undefined {
  return (
    agents.find((agent) => agent.status === AgentStatus.Busy) ||
    agents.find(
      (agent) => agent.status === AgentStatus.Online || agent.status === AgentStatus.Idle,
    ) ||
    agents[0]
  );
}

function upsertAgent(agents: AgentInfo[], data: Record<string, unknown>): AgentInfo[] {
  const agentId = String(data.agentId || data.id || '');
  if (!agentId) return agents;
  const index = agents.findIndex((agent) => agent.agentId === agentId);
  if (index < 0) {
    return [
      ...agents,
      {
        agentId,
        hostname: String(data.hostname || agentId),
        ip: String(data.ip || ''),
        status: String(data.status || AgentStatus.Online) as AgentStatus,
        currentTask: String(data.currentTask || ''),
        resources: (data.resources || {}) as AgentInfo['resources'],
        lastSeen: safeNumber(data.lastSeen, Date.now()),
      },
    ];
  }
  return agents.map((agent, itemIndex) =>
    itemIndex === index
      ? {
          ...agent,
          ...data,
          agentId,
          status: String(data.status || agent.status) as AgentStatus,
          resources: (data.resources || agent.resources) as AgentInfo['resources'],
          lastSeen: safeNumber(data.lastSeen, Date.now()),
        }
      : agent,
  );
}

const Monitor: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const translate = (id: string, values?: Record<string, string | number>) =>
    intl.formatMessage({ id }, values);
  const requestFailed = formatMessage('pages.monitor.requestFailed');
  // 「运行中/已暂停」控制本页事件记录（时间线 + 触发器列表）是否写入，
  // WebSocket 连接本身保持（与 Agents 页共享单例，不受影响）。
  const [isRunning, setIsRunning] = useState(true);
  const isRunningRef = useRef(true);
  const [wsConnected, setWsConnected] = useState(false);
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [systemStatus, setSystemStatus] = useState<SystemStatus>({
    cpu: 0,
    memory: 0,
    uptime: 0,
    fps: 0,
    networkUp: 0,
    networkDown: 0,
    onlineAgents: 0,
    source: 'no-agent',
  });
  const [events, setEvents] = useState<RuntimeEvent[]>([]);
  const lastScreenshotRef = useRef<number>(0);
  const [triggers, setTriggers] = useState<MonitorTrigger[]>([]);
  const [triggersLoading, setTriggersLoading] = useState(false);
  const [triggersError, setTriggersError] = useState<string | null>(null);
  const [toggling, setToggling] = useState<Record<string, boolean>>({});
  const selectedAgentRef = useRef<AgentInfo | undefined>(undefined);

  const selectedAgent = primaryAgent(agents);

  useEffect(() => {
    selectedAgentRef.current = selectedAgent;
  }, [selectedAgent]);
  const displayStatus: SystemStatus = selectedAgent?.resources
    ? {
        cpu: safeNumber(selectedAgent.resources.cpu?.usage),
        memory: safeNumber(selectedAgent.resources.memory?.usage),
        uptime: systemStatus.uptime,
        fps: systemStatus.fps,
        networkUp: safeNumber(selectedAgent.resources.network?.up),
        networkDown: safeNumber(selectedAgent.resources.network?.down),
        onlineAgents: agents.filter(
          (agent) =>
            agent.status === AgentStatus.Online ||
            agent.status === AgentStatus.Idle ||
            agent.status === AgentStatus.Busy,
        ).length,
        source: selectedAgent.hostname || selectedAgent.agentId,
      }
    : systemStatus;

  const addEvent = (event: Omit<RuntimeEvent, 'id' | 'time'>) => {
    if (!isRunningRef.current) return;
    setEvents((previous) =>
      [
        {
          ...event,
          id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
          time: new Date().toLocaleTimeString(),
        },
        ...previous,
      ].slice(0, 12),
    );
  };

  useEffect(() => {
    let mounted = true;
    getAgents()
      .then((response) => {
        if (!mounted) return;
        setAgents(response.data || []);
      })
      .catch(() => {
        addEvent({
          type: 'agent',
          level: 'warning',
          message: formatMessage('pages.monitor.loadAgentsFailed'),
        });
      });

    const unsubscribes: (() => void)[] = [];
    unsubscribes.push(wsService.onConnectionState(setWsConnected));

    unsubscribes.push(
      wsService.onAgentConnected((data) => {
        setAgents((previous) => upsertAgent(previous, { ...data, status: AgentStatus.Online }));
        addEvent({
          type: 'agent',
          level: 'success',
          message: translate('pages.monitor.agentOnline', {
            name: String(data.hostname || data.agentId || ''),
          }),
        });
      }),
    );

    unsubscribes.push(
      wsService.onAgentDisconnected((data) => {
        setAgents((previous) => upsertAgent(previous, { ...data, status: AgentStatus.Offline }));
        addEvent({
          type: 'agent',
          level: 'warning',
          message: translate('pages.monitor.agentOffline', {
            name: String(data.hostname || data.agentId || ''),
          }),
        });
      }),
    );

    unsubscribes.push(
      wsService.onAgentStatusChanged((data) => {
        setAgents((previous) => upsertAgent(previous, data));
      }),
    );

    unsubscribes.push(
      wsService.on('screenshot', (message) => {
        const timestamp = safeNumber(message.data?.timestamp, Date.now());
        const previous = lastScreenshotRef.current;
        lastScreenshotRef.current = timestamp;
        if (previous > 0 && timestamp > previous) {
          const fps = Math.min(60, Math.max(1, Math.round(1000 / (timestamp - previous))));
          setSystemStatus((state) => ({ ...state, fps }));
        }
      }),
    );

    unsubscribes.push(
      wsService.onTriggerFired((data) => {
        if (!isRunningRef.current) return;
        // 事件载荷（runtime TriggerInstance）：{id, name, triggered, lastTriggerTime, agentId}
        const triggerId = String(data.id ?? data.triggerId ?? '');
        const triggerName =
          String(data.name || data.triggerName || triggerId || '') ||
          formatMessage('pages.monitor.unknownTrigger');
        const eventAgentId = String(data.agentId || '');
        const selected = selectedAgentRef.current;
        // 只统计当前展示 agent 的命中，避免多 agent 命中污染列表
        if (selected && eventAgentId && eventAgentId !== selected.agentId) return;
        const firedAt = new Date().toLocaleTimeString();
        setTriggers((previous) => {
          const index = previous.findIndex(
            (trigger) => trigger.id === triggerId || trigger.name === triggerName,
          );
          if (index < 0) {
            // 列表尚未同步到的新触发器：以事件信息占位，待下次刷新补全配置
            return [
              {
                id: triggerId || triggerName,
                name: triggerName,
                enabled: true,
                type: 'event',
                condition: {
                  type: '',
                  value: '',
                  region: { x: 0, y: 0, width: 0, height: 0 },
                  tolerance: 0,
                  interval: 0,
                  enabled: true,
                },
                actions: [],
                oneShot: false,
                cooldown: 0,
                lastTriggered: true,
                hitCount: 1,
                lastFiredAt: firedAt,
              },
              ...previous,
            ].slice(0, 20);
          }
          return previous.map((trigger, itemIndex) =>
            itemIndex === index
              ? {
                  ...trigger,
                  lastTriggered: true,
                  hitCount: (trigger.hitCount || 0) + 1,
                  lastFiredAt: firedAt,
                }
              : trigger,
          );
        });
        addEvent({
          type: 'trigger',
          level: 'processing',
          message: translate('pages.monitor.triggerFired', { name: triggerName }),
        });
      }),
    );

    unsubscribes.push(
      wsService.on('script', (message) => {
        const data = message.data || {};
        const runtimeState = String(
          (data.data as Record<string, unknown>)?.state || data.state || '',
        );
        addEvent({
          type: 'script',
          level: runtimeState === 'error' ? 'error' : 'default',
          message:
            String(data.message || data.scriptId || '') ||
            formatMessage('pages.monitor.scriptStatusUpdate'),
        });
      }),
    );

    wsService.connect();

    return () => {
      mounted = false;
      unsubscribes.forEach((unsubscribe) => unsubscribe());
    };
  }, []);

  useEffect(() => {
    if (agents.length > 0) return;
    // 无 agent 在线时显示明确空态，不再用 Math.random() 伪造指标
    setSystemStatus((previous) => ({
      ...previous,
      cpu: 0,
      memory: 0,
      uptime: 0,
      fps: 0,
      networkUp: 0,
      networkDown: 0,
      onlineAgents: 0,
      source: 'no-agent',
    }));
    return () => {};
  }, [agents.length, isRunning]);

  // ===== 触发器真实列表：随选中 agent 变化拉取 /api/agents/:id/triggers =====
  const loadTriggers = useCallback(
    async (agent: AgentInfo | undefined, opts?: { silent?: boolean }) => {
      if (!agent) {
        setTriggers([]);
        setTriggersError(null);
        return;
      }
      if (!agentConnected(agent)) {
        setTriggers([]);
        setTriggersError(
          translate('pages.monitor.agentOfflineNoTriggers', {
            name: agent.hostname || agent.agentId,
          }),
        );
        return;
      }
      if (!opts?.silent) setTriggersLoading(true);
      try {
        const response = await getAgentTriggers(agent.agentId);
        setTriggersError(null);
        setTriggers((previous) =>
          (response.data || []).map((trigger) => {
            const existing = previous.find(
              (item) => item.id === trigger.id || item.name === trigger.name,
            );
            return {
              ...trigger,
              hitCount: existing?.hitCount || 0,
              lastFiredAt: existing?.lastFiredAt,
            };
          }),
        );
      } catch (error) {
        setTriggersError(extractErrorMessage(error, requestFailed));
        addEvent({
          type: 'trigger',
          level: 'warning',
          message: translate('pages.monitor.triggersLoadFailed', {
            detail: extractErrorMessage(error, requestFailed),
          }),
        });
      } finally {
        if (!opts?.silent) setTriggersLoading(false);
      }
    },
    [],
  );

  useEffect(() => {
    loadTriggers(selectedAgent);
  }, [selectedAgent?.agentId, selectedAgent?.status]);

  // agent 断开时主动清理触发器状态（错误提示由 loadTriggers 写入）
  useEffect(() => {
    if (selectedAgent && !agentConnected(selectedAgent)) {
      setTriggers((previous) => (previous.length > 0 ? [] : previous));
    }
  }, [selectedAgent]);

  const handleToggleTrigger = async (trigger: MonitorTrigger) => {
    const agent = selectedAgentRef.current;
    if (!agent || !agentConnected(agent)) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: formatMessage('pages.monitor.agentNotConnectedToggle'),
      });
      return;
    }
    setToggling((previous) => ({ ...previous, [trigger.id]: true }));
    try {
      const response = await toggleAgentTrigger(agent.agentId, trigger.id);
      const enabled = response.data?.enabled ?? !trigger.enabled;
      setTriggers((previous) =>
        previous.map((item) => (item.id === trigger.id ? { ...item, enabled } : item)),
      );
      addEvent({
        type: 'trigger',
        level: 'processing',
        message: translate(enabled ? 'pages.monitor.triggerEnabled' : 'pages.monitor.triggerDisabled', {
          name: trigger.name,
        }),
      });
    } catch (error) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: translate('pages.monitor.triggerToggleFailed', {
          name: trigger.name,
          detail: extractErrorMessage(error, requestFailed),
        }),
      });
    } finally {
      setToggling((previous) => ({ ...previous, [trigger.id]: false }));
    }
  };

  const handleToggleRun = () => {
    const next = !isRunning;
    isRunningRef.current = next;
    setIsRunning(next);
  };

  // ===== 触发器 CRUD：经 Go server 透传 runtime trigger.add/update/remove =====
  const [triggerModalOpen, setTriggerModalOpen] = useState(false);
  const [editingTrigger, setEditingTrigger] = useState<MonitorTrigger | null>(null);

  const handleCreateTrigger = () => {
    const agent = selectedAgentRef.current;
    if (!agent || !agentConnected(agent)) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: formatMessage('pages.monitor.agentNotConnectedCreate'),
      });
      return;
    }
    setEditingTrigger(null);
    setTriggerModalOpen(true);
  };

  const handleEditTrigger = (trigger: MonitorTrigger) => {
    setEditingTrigger(trigger);
    setTriggerModalOpen(true);
  };

  const handleRemoveTrigger = async (trigger: MonitorTrigger) => {
    const agent = selectedAgentRef.current;
    if (!agent || !agentConnected(agent)) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: formatMessage('pages.monitor.agentNotConnectedRemove'),
      });
      return;
    }
    try {
      await removeAgentTrigger(agent.agentId, trigger.id);
      setTriggers((previous) => previous.filter((item) => item.id !== trigger.id));
      addEvent({
        type: 'trigger',
        level: 'processing',
        message: translate('pages.monitor.triggerRemoved', { name: trigger.name }),
      });
    } catch (error) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: translate('pages.monitor.triggerRemoveFailed', {
          name: trigger.name,
          detail: extractErrorMessage(error, requestFailed),
        }),
      });
    }
  };

  const handleTriggerSaved = useCallback(
    (message: string) => {
      addEvent({ type: 'trigger', level: 'processing', message });
      const agent = selectedAgentRef.current;
      if (agent) {
        loadTriggers(agent, { silent: true });
      }
    },
    [loadTriggers],
  );

  return (
    <PageContainer
      header={{
        title: formatMessage('pages.monitor.title'),
        breadcrumb: {},
      }}
      extra={[
        <Tag
          key="ws"
          color={wsConnected ? 'green' : 'orange'}
          style={wsConnected ? undefined : { cursor: 'pointer' }}
          onClick={wsConnected ? undefined : () => wsService.reconnect()}
        >
          {wsConnected
            ? formatMessage('pages.monitor.wsConnected')
            : formatMessage('pages.monitor.wsDisconnected')}
        </Tag>,
        <Button
          key="status"
          icon={isRunning ? <PlayCircleOutlined /> : <PauseCircleOutlined />}
          type={isRunning ? 'default' : 'primary'}
          onClick={handleToggleRun}
        >
          {isRunning
            ? formatMessage('pages.monitor.running')
            : formatMessage('pages.monitor.paused')}
        </Button>,
        <Button key="settings" icon={<SettingOutlined />} onClick={() => history.push('/settings')}>
          {formatMessage('pages.monitor.settings')}
        </Button>,
      ]}
    >
      <Space direction="vertical" size={16} style={{ width: '100%' }}>
        {!selectedAgent && (
          <Alert
            type="info"
            showIcon
            message={formatMessage('pages.monitor.noAgentsTitle')}
            description={formatMessage('pages.monitor.noAgentsDescription')}
          />
        )}

        <Row gutter={16}>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.cpuUsage')}
                value={displayStatus.cpu}
                precision={1}
                suffix="%"
                prefix={<DesktopOutlined />}
                valueStyle={{ color: displayStatus.cpu > 80 ? '#ff4d4f' : '#3f8600' }}
              />
              <Progress percent={Math.round(displayStatus.cpu)} size="small" showInfo={false} />
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.memoryUsage')}
                value={displayStatus.memory}
                precision={1}
                suffix="%"
                prefix={<DatabaseOutlined />}
                valueStyle={{ color: displayStatus.memory > 80 ? '#ff4d4f' : '#1890ff' }}
              />
              <Progress percent={Math.round(displayStatus.memory)} size="small" showInfo={false} />
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.uptime')}
                value={formatUptime(displayStatus.uptime)}
                prefix={<ClockCircleOutlined />}
              />
              <Text type="secondary">
                {translate('pages.monitor.dataSource', { source: displayStatus.source })}
              </Text>
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.fps')}
                value={displayStatus.fps}
                suffix="FPS"
                prefix={<CameraOutlined />}
                valueStyle={{ color: displayStatus.fps >= 50 ? '#3f8600' : '#faad14' }}
              />
              <Text type="secondary">
                {translate('pages.monitor.onlineAgents', { count: displayStatus.onlineAgents })}
              </Text>
            </Card>
          </Col>
        </Row>

        <Row gutter={16}>
          <Col xs={24} md={12}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.networkUp')}
                value={formatBytes(displayStatus.networkUp)}
                prefix={<WifiOutlined />}
              />
            </Card>
          </Col>
          <Col xs={24} md={12}>
            <Card>
              <Statistic
                title={formatMessage('pages.monitor.networkDown')}
                value={formatBytes(displayStatus.networkDown)}
                prefix={<WifiOutlined />}
              />
            </Card>
          </Col>
        </Row>

        <Row gutter={16}>
          <Col xs={24} lg={16}>
            <ScreenshotView height={500} />
          </Col>

          <Col xs={24} lg={8}>
            <Card
              title={
                <Space>
                  <ThunderboltOutlined />
                  <span>{formatMessage('pages.monitor.triggers')}</span>
                  <Tag color="blue">{triggers.length}</Tag>
                </Space>
              }
              extra={
                <Space>
                  <Button
                    size="small"
                    type="primary"
                    ghost
                    icon={<PlusOutlined />}
                    disabled={!agentConnected(selectedAgent)}
                    onClick={handleCreateTrigger}
                  >
                    {formatMessage('pages.monitor.add')}
                  </Button>
                  <Button
                    size="small"
                    type="text"
                    icon={<ReloadOutlined spin={triggersLoading} />}
                    disabled={!agentConnected(selectedAgent) || triggersLoading}
                    onClick={() => loadTriggers(selectedAgent)}
                  >
                    {formatMessage('pages.common.refresh')}
                  </Button>
                </Space>
              }
            >
              {/* Runtime 连接状态：agent↔orchestrator 链路可用性决定触发器数据源 */}
              <Space direction="vertical" size={2} style={{ width: '100%', marginBottom: 12 }}>
                <Space size={8} wrap>
                  <Text type="secondary">{formatMessage('pages.monitor.dataSourceLabel')}:</Text>
                  {selectedAgent ? (
                    <>
                      <Tag
                        color={
                          selectedAgent.status === AgentStatus.Offline
                            ? 'red'
                            : selectedAgent.status === AgentStatus.Error
                              ? 'volcano'
                              : 'green'
                        }
                      >
                        {selectedAgent.hostname || selectedAgent.agentId}
                      </Tag>
                      <Tag
                        color={
                          selectedAgent.status === AgentStatus.Busy
                            ? 'processing'
                            : agentConnected(selectedAgent)
                              ? 'success'
                              : 'default'
                        }
                      >
                        {selectedAgent.status}
                      </Tag>
                    </>
                  ) : (
                    <Tag>{formatMessage('pages.monitor.noAgent')}</Tag>
                  )}
                  <Tag color={wsConnected ? 'green' : 'orange'}>
                    {wsConnected
                      ? formatMessage('pages.monitor.wsShortConnected')
                      : formatMessage('pages.monitor.wsShortDisconnected')}
                  </Tag>
                </Space>
                {selectedAgent && (
                  <Text type="secondary">
                    {translate('pages.monitor.agentHeartbeat', {
                      id: selectedAgent.agentId,
                      lastSeen: formatLastSeen(selectedAgent.lastSeen, translate),
                    })}
                  </Text>
                )}
              </Space>

              {triggersError && (
                <Alert
                  type={agentConnected(selectedAgent) ? 'error' : 'warning'}
                  showIcon
                  style={{ marginBottom: 12 }}
                  message={formatMessage('pages.monitor.triggersUnavailable')}
                  description={triggersError}
                />
              )}

              <List
                size="small"
                loading={triggersLoading}
                dataSource={triggers}
                locale={{ emptyText: formatMessage('pages.monitor.triggersEmpty') }}
                renderItem={(trigger) => (
                  <List.Item
                    actions={[
                      <Button
                        key="edit"
                        size="small"
                        type="text"
                        icon={<EditOutlined />}
                        disabled={!agentConnected(selectedAgent)}
                        onClick={() => handleEditTrigger(trigger)}
                      />,
                      <Popconfirm
                        key="delete"
                        title={formatMessage('pages.monitor.deleteTriggerTitle')}
                        description={translate('pages.monitor.deleteTriggerConfirm', {
                          name: trigger.name,
                        })}
                        okText={formatMessage('pages.monitor.delete')}
                        okButtonProps={{ danger: true }}
                        onConfirm={() => handleRemoveTrigger(trigger)}
                      >
                        <Button
                          size="small"
                          type="text"
                          danger
                          icon={<DeleteOutlined />}
                          disabled={!agentConnected(selectedAgent)}
                        />
                      </Popconfirm>,
                      <Switch
                        key="toggle"
                        size="small"
                        checked={trigger.enabled}
                        loading={toggling[trigger.id]}
                        disabled={!agentConnected(selectedAgent)}
                        onChange={() => handleToggleTrigger(trigger)}
                      />,
                    ]}
                  >
                    <List.Item.Meta
                      avatar={
                        <Tag color={trigger.type === 'ColorFound' ? 'blue' : 'green'}>
                          {trigger.type || 'event'}
                        </Tag>
                      }
                      title={
                        <Space size={8}>
                          <span>{trigger.name}</span>
                          {trigger.oneShot && <Tag>{formatMessage('pages.monitor.oneShot')}</Tag>}
                          {trigger.cooldown > 0 && (
                            <Tag>{translate('pages.monitor.cooldown', { ms: trigger.cooldown })}</Tag>
                          )}
                        </Space>
                      }
                      description={translate('pages.monitor.triggerDescription', {
                        condition:
                          trigger.condition?.value ||
                          trigger.condition?.type ||
                          formatMessage('pages.monitor.noCondition'),
                        actions: trigger.actions.length,
                        hits: trigger.hitCount || 0,
                        lastFired:
                          trigger.lastFiredAt || formatMessage('pages.monitor.neverFired'),
                      })}
                    />
                  </List.Item>
                )}
              />
            </Card>

            <Card
              title={
                <Space>
                  <CodeOutlined />
                  <span>{formatMessage('pages.monitor.macros')}</span>
                </Space>
              }
              style={{ marginTop: 16 }}
            >
              <Alert
                type="info"
                showIcon
                message={formatMessage('pages.monitor.macroApiNotAvailable')}
                description={formatMessage('pages.monitor.macroApiDescription')}
              />
            </Card>
          </Col>
        </Row>

        <Card
          title={formatMessage('pages.monitor.eventStream')}
          extra={<Tag>{formatMessage('pages.monitor.eventDriven')}</Tag>}
        >
          {events.length === 0 ? (
            <Text type="secondary">{formatMessage('pages.monitor.waitingForEvents')}</Text>
          ) : (
            <Timeline
              items={events.map((event) => ({
                color:
                  event.level === 'error'
                    ? 'red'
                    : event.level === 'warning'
                      ? 'orange'
                      : event.level === 'success'
                        ? 'green'
                        : 'blue',
                children: (
                  <Space direction="vertical" size={0}>
                    <Text strong>{event.message}</Text>
                    <Text type="secondary">
                      {event.time} · {event.type}
                    </Text>
                  </Space>
                ),
              }))}
            />
          )}
        </Card>
      </Space>

      {selectedAgent && (
        <TriggerFormModal
          open={triggerModalOpen}
          initial={editingTrigger}
          onSubmit={async (config) => {
            const agent = selectedAgentRef.current;
            if (!agent || !agentConnected(agent)) {
              throw new Error(formatMessage('pages.monitor.agentNotConnected'));
            }
            if (editingTrigger) {
              await updateAgentTrigger(agent.agentId, editingTrigger.id, config);
            } else {
              await createAgentTrigger(agent.agentId, config);
            }
          }}
          onClose={() => setTriggerModalOpen(false)}
          onSaved={handleTriggerSaved}
        />
      )}
    </PageContainer>
  );
};

export default Monitor;
