/**
 * 游戏监控页面
 * 集成实时截图、Agent 资源状态、触发器事件流和宏命令入口。
 */
import React, { useEffect, useRef, useState } from 'react';
import { PageContainer } from '@ant-design/pro-components';
import {
  Alert,
  Button,
  Card,
  Col,
  List,
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
  DesktopOutlined,
  PauseCircleOutlined,
  PlayCircleOutlined,
  PlusOutlined,
  ReloadOutlined,
  SettingOutlined,
  ThunderboltOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import ScreenshotView from '@/components/ScreenshotView';
import wsService from '@/services/websocket';
import { AgentInfo, AgentStatus, getAgents } from '@/services/wingman';

const { Text } = Typography;

interface TriggerInfo {
  id: string;
  name: string;
  enabled: boolean;
  type: string;
  condition: string;
  actions: number;
  cooldown: number;
  lastTriggered?: string;
  hitCount?: number;
}

interface MacroInfo {
  id: string;
  name: string;
  actions: number;
  lastRun: string;
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

function primaryAgent(agents: AgentInfo[]): AgentInfo | undefined {
  return (
    agents.find((agent) => agent.status === AgentStatus.Busy) ||
    agents.find((agent) => agent.status === AgentStatus.Online || agent.status === AgentStatus.Idle) ||
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
  const [isRunning, setIsRunning] = useState(false);
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
    source: 'mock',
  });
  const [events, setEvents] = useState<RuntimeEvent[]>([]);
  const lastScreenshotRef = useRef<number>(0);

  const [triggers, setTriggers] = useState<TriggerInfo[]>([
    { id: '1', name: '技能冷却检测', enabled: true, type: 'color', condition: '绿色出现', actions: 1, cooldown: 500 },
    { id: '2', name: '血量监控', enabled: true, type: 'color', condition: '红色低于30%', actions: 2, cooldown: 1000 },
    { id: '3', name: '自动拾取', enabled: false, type: 'image', condition: '掉落物品', actions: 1, cooldown: 200 },
  ]);
  const [macros] = useState<MacroInfo[]>([
    { id: '1', name: '日常采集', actions: 15, lastRun: '2小时前' },
    { id: '2', name: '自动战斗', actions: 8, lastRun: '5小时前' },
  ]);

  const selectedAgent = primaryAgent(agents);
  const displayStatus: SystemStatus = selectedAgent?.resources
    ? {
        cpu: safeNumber(selectedAgent.resources.cpu?.usage),
        memory: safeNumber(selectedAgent.resources.memory?.usage),
        uptime: systemStatus.uptime,
        fps: systemStatus.fps,
        networkUp: safeNumber(selectedAgent.resources.network?.up),
        networkDown: safeNumber(selectedAgent.resources.network?.down),
        onlineAgents: agents.filter(
          (agent) => agent.status === AgentStatus.Online || agent.status === AgentStatus.Idle || agent.status === AgentStatus.Busy,
        ).length,
        source: selectedAgent.hostname || selectedAgent.agentId,
      }
    : systemStatus;

  const addEvent = (event: Omit<RuntimeEvent, 'id' | 'time'>) => {
    setEvents((previous) => [
      {
        ...event,
        id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
        time: new Date().toLocaleTimeString(),
      },
      ...previous,
    ].slice(0, 12));
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
          message: '无法加载 Agent 列表，等待 WebSocket 事件',
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
          message: `Agent ${String(data.hostname || data.agentId || '')} 已上线`,
        });
      }),
    );

    unsubscribes.push(
      wsService.onAgentDisconnected((data) => {
        setAgents((previous) => upsertAgent(previous, { ...data, status: AgentStatus.Offline }));
        addEvent({
          type: 'agent',
          level: 'warning',
          message: `Agent ${String(data.hostname || data.agentId || '')} 已离线`,
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
      wsService.on('trigger', (message) => {
        const data = message.data || {};
        const triggerId = String(data.triggerId || data.id || '');
        const triggerName = String(data.name || data.triggerName || triggerId || '未知触发器');
        setTriggers((previous) =>
          previous.map((trigger) =>
            trigger.id === triggerId || trigger.name === triggerName
              ? {
                  ...trigger,
                  lastTriggered: new Date().toLocaleTimeString(),
                  hitCount: (trigger.hitCount || 0) + 1,
                }
              : trigger,
          ),
        );
        addEvent({
          type: 'trigger',
          level: 'processing',
          message: `触发器 ${triggerName} 被触发`,
        });
      }),
    );

    unsubscribes.push(
      wsService.on('script', (message) => {
        const data = message.data || {};
        addEvent({
          type: 'script',
          level: message.event === 'error' ? 'error' : 'default',
          message: String(data.message || data.scriptId || '脚本状态更新'),
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
    const timer = setInterval(() => {
      setSystemStatus((previous) => ({
        ...previous,
        cpu: Math.random() * 20 + 5,
        memory: Math.random() * 10 + 40,
        uptime: Date.now() - new Date().setHours(8, 0, 0, 0),
        fps: isRunning ? Math.floor(Math.random() * 5 + 55) : previous.fps,
        networkUp: Math.random() * 1024 * 48,
        networkDown: Math.random() * 1024 * 96,
        onlineAgents: 0,
        source: 'mock',
      }));
    }, 2000);

    return () => clearInterval(timer);
  }, [agents.length, isRunning]);

  const handleToggleRun = () => {
    setIsRunning(!isRunning);
  };

  const handleToggleTrigger = (id: string) => {
    setTriggers(triggers.map((trigger) =>
      trigger.id === id ? { ...trigger, enabled: !trigger.enabled } : trigger,
    ));
  };

  const handleRunMacro = (id: string) => {
    const macro = macros.find((item) => item.id === id);
    addEvent({
      type: 'script',
      level: 'processing',
      message: `宏命令 ${macro?.name || id} 已下发`,
    });
  };

  return (
    <PageContainer
      header={{
        title: '游戏监控',
        breadcrumb: {},
      }}
      extra={[
        <Tag
          key="ws"
          color={wsConnected ? 'green' : 'orange'}
          style={wsConnected ? undefined : { cursor: 'pointer' }}
          onClick={wsConnected ? undefined : () => wsService.reconnect()}
        >
          {wsConnected ? 'WebSocket 已连接' : '实时连接已断开，点击重连'}
        </Tag>,
        <Button
          key="status"
          icon={isRunning ? <PlayCircleOutlined /> : <PauseCircleOutlined />}
          type={isRunning ? 'default' : 'primary'}
          onClick={handleToggleRun}
        >
          {isRunning ? '运行中' : '已暂停'}
        </Button>,
        <Button key="settings" icon={<SettingOutlined />}>
          设置
        </Button>,
      ]}
    >
      <Space direction="vertical" size={16} style={{ width: '100%' }}>
        {!selectedAgent && (
          <Alert
            type="info"
            showIcon
            message="暂无在线 Agent 数据"
            description="页面会继续监听 WebSocket；当前 CPU、内存和网络指标使用开发兜底数据。"
          />
        )}

        <Row gutter={16}>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="CPU 使用率"
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
                title="内存使用"
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
                title="运行时长"
                value={formatUptime(displayStatus.uptime)}
                prefix={<ClockCircleOutlined />}
              />
              <Text type="secondary">数据源: {displayStatus.source}</Text>
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="画面帧率"
                value={displayStatus.fps}
                suffix="FPS"
                prefix={<CameraOutlined />}
                valueStyle={{ color: displayStatus.fps >= 50 ? '#3f8600' : '#faad14' }}
              />
              <Text type="secondary">在线 Agent: {displayStatus.onlineAgents}</Text>
            </Card>
          </Col>
        </Row>

        <Row gutter={16}>
          <Col xs={24} md={12}>
            <Card>
              <Statistic
                title="网络上行"
                value={formatBytes(displayStatus.networkUp)}
                prefix={<WifiOutlined />}
              />
            </Card>
          </Col>
          <Col xs={24} md={12}>
            <Card>
              <Statistic
                title="网络下行"
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
                  <span>触发器</span>
                  <Tag color="blue">{triggers.filter((trigger) => trigger.enabled).length}/{triggers.length}</Tag>
                </Space>
              }
              extra={<Button size="small" icon={<PlusOutlined />}>添加</Button>}
            >
              <List
                size="small"
                dataSource={triggers}
                renderItem={(trigger) => (
                  <List.Item
                    actions={[
                      <Switch
                        key="toggle"
                        size="small"
                        checked={trigger.enabled}
                        onChange={() => handleToggleTrigger(trigger.id)}
                      />,
                    ]}
                  >
                    <List.Item.Meta
                      avatar={
                        <Tag color={trigger.type === 'color' ? 'blue' : 'green'}>
                          {trigger.type === 'color' ? '颜色' : '图像'}
                        </Tag>
                      }
                      title={trigger.name}
                      description={`${trigger.condition} · ${trigger.actions}个动作 · 命中 ${trigger.hitCount || 0} · ${trigger.lastTriggered || '未触发'}`}
                    />
                  </List.Item>
                )}
              />
            </Card>

            <Card
              title={
                <Space>
                  <CodeOutlined />
                  <span>宏命令</span>
                </Space>
              }
              style={{ marginTop: 16 }}
              extra={<Button size="small" icon={<PlusOutlined />}>录制</Button>}
            >
              <List
                size="small"
                dataSource={macros}
                renderItem={(macro) => (
                  <List.Item
                    actions={[
                      <Button
                        key="run"
                        size="small"
                        type="primary"
                        icon={<PlayCircleOutlined />}
                        onClick={() => handleRunMacro(macro.id)}
                      >
                        运行
                      </Button>,
                    ]}
                  >
                    <List.Item.Meta
                      title={macro.name}
                      description={`${macro.actions}个动作 · 上次: ${macro.lastRun}`}
                    />
                  </List.Item>
                )}
              />
            </Card>
          </Col>
        </Row>

        <Card title="实时事件流" extra={<Button icon={<ReloadOutlined />}>刷新</Button>}>
          {events.length === 0 ? (
            <Text type="secondary">等待 Agent、触发器或脚本事件...</Text>
          ) : (
            <Timeline
              items={events.map((event) => ({
                color: event.level === 'error' ? 'red' : event.level === 'warning' ? 'orange' : event.level === 'success' ? 'green' : 'blue',
                children: (
                  <Space direction="vertical" size={0}>
                    <Text strong>{event.message}</Text>
                    <Text type="secondary">{event.time} · {event.type}</Text>
                  </Space>
                ),
              }))}
            />
          )}
        </Card>
      </Space>
    </PageContainer>
  );
};

export default Monitor;
