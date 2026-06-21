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
    source: 'no-agent',
  });
  const [events, setEvents] = useState<RuntimeEvent[]>([]);
  const lastScreenshotRef = useRef<number>(0);
  const [triggers, setTriggers] = useState<TriggerInfo[]>([]);

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
        const nested = data.data && typeof data.data === 'object' ? (data.data as Record<string, unknown>) : data;
        const triggerId = String(nested.triggerId || nested.id || data.triggerId || data.id || '');
        const triggerName = String(nested.name || nested.triggerName || data.name || data.triggerName || triggerId || '未知触发器');
        setTriggers((previous) =>
          {
            const index = previous.findIndex(
              (trigger) => trigger.id === triggerId || trigger.name === triggerName,
            );
            const nextHit = {
              id: triggerId || triggerName,
              name: triggerName,
              enabled: true,
              type: String(nested.type || 'event'),
              condition: String(nested.condition || 'runtime event'),
              actions: safeNumber(nested.actions, 0),
              cooldown: safeNumber(nested.cooldown, 0),
              lastTriggered: new Date().toLocaleTimeString(),
              hitCount: 1,
            };
            if (index < 0) {
              return [nextHit, ...previous].slice(0, 20);
            }
            return previous.map((trigger, itemIndex) =>
              itemIndex === index
                ? {
                    ...trigger,
                    ...nextHit,
                    hitCount: (trigger.hitCount || 0) + 1,
                  }
                : trigger,
            );
          },
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

  const handleToggleRun = () => {
    setIsRunning(!isRunning);
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
            description="页面会继续监听 WebSocket，Agent 上线后自动显示真实指标。"
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
                  <Tag color="blue">{triggers.length}</Tag>
                </Space>
              }
              extra={<Tag>事件驱动</Tag>}
            >
              <List
                size="small"
                dataSource={triggers}
                locale={{ emptyText: '等待 runtime trigger_fired 事件。远程 trigger.list API 尚未暴露。' }}
                renderItem={(trigger) => (
                  <List.Item>
                    <List.Item.Meta
                      avatar={
                        <Tag color={trigger.type === 'color' ? 'blue' : 'green'}>
                          {trigger.type === 'event' ? '事件' : trigger.type}
                        </Tag>
                      }
                      title={trigger.name}
                      description={`${trigger.condition} · 命中 ${trigger.hitCount || 0} · ${trigger.lastTriggered || '未触发'}`}
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
            >
              <Alert
                type="info"
                showIcon
                message="宏命令 API 尚未接入"
                description="当前远程 Agent 协议还没有暴露 macro.list / macro.run。这里不再展示硬编码示例，避免误下发假命令。"
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
