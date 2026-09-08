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
  Divider,
  Form,
  Input,
  InputNumber,
  List,
  Modal,
  Popconfirm,
  Progress,
  Row,
  Select,
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
import { history } from '@umijs/max';
import ScreenshotView from '@/components/ScreenshotView';
import wsService from '@/services/websocket';
import {
  AgentInfo,
  AgentStatus,
  AgentTrigger,
  AgentTriggerConfigInput,
  createAgentTrigger,
  getAgentTriggers,
  getAgents,
  removeAgentTrigger,
  toggleAgentTrigger,
  updateAgentTrigger,
} from '@/services/wingman';

const { Text } = Typography;

// runtime TriggerType 全量 11 种（与 trigger_handler.cpp 对齐）
const TRIGGER_CONDITION_TYPES = [
  'ColorFound',
  'ColorLost',
  'ImageFound',
  'ImageLost',
  'WindowOpened',
  'WindowClosed',
  'ProcessStarted',
  'ProcessStopped',
  'TimeElapsed',
  'HotkeyPressed',
  'PixelChanged',
];

// runtime BasicTriggerAction 全量 10 种
const TRIGGER_ACTION_TYPES = [
  'RunScript',
  'Click',
  'KeyPress',
  'Type',
  'StopScript',
  'PauseScript',
  'ShowMessage',
  'PlayAudio',
  'Log',
  'Delay',
];

interface TriggerFormValues {
  name: string;
  enabled: boolean;
  oneShot: boolean;
  cooldown: number;
  condition: {
    type: string;
    value: string;
    tolerance: number;
    interval: number;
    region: { x: number; y: number; width: number; height: number };
  };
  actions: Array<{ type: string; value: string; x: number; y: number; delay: number }>;
}

// trigger → 表单值（编辑模式回填）
function triggerToFormValues(trigger: AgentTrigger): TriggerFormValues {
  const condition = trigger.condition || ({} as AgentTrigger['condition']);
  return {
    name: trigger.name || '',
    enabled: trigger.enabled !== false,
    oneShot: trigger.oneShot === true,
    cooldown: trigger.cooldown || 0,
    condition: {
      type: condition.type || trigger.type || 'ColorFound',
      value: condition.value || '',
      tolerance: condition.tolerance ?? 10,
      interval: condition.interval ?? 1000,
      region: condition.region || { x: 0, y: 0, width: 0, height: 0 },
    },
    actions: (trigger.actions || []).map((action) => ({
      type: action.type || 'Log',
      value: action.value || '',
      x: action.x || 0,
      y: action.y || 0,
      delay: action.delay || 0,
    })),
  };
}

// 表单值 → runtime TriggerConfig 载荷
function formValuesToConfig(values: TriggerFormValues): AgentTriggerConfigInput {
  return {
    name: values.name,
    enabled: values.enabled,
    oneShot: values.oneShot,
    cooldown: values.cooldown,
    condition: {
      type: values.condition.type,
      value: values.condition.value,
      tolerance: values.condition.tolerance,
      interval: values.condition.interval,
      region: values.condition.region,
    },
    actions: values.actions.map((action) => ({
      type: action.type,
      value: action.value,
      x: action.x,
      y: action.y,
      delay: action.delay,
    })),
  };
}

// TriggerFormModal 触发器新增/编辑表单（经 Go server 透传 runtime trigger.add/update）
function TriggerFormModal({
  open,
  agentId,
  trigger,
  onClose,
  onSaved,
}: {
  open: boolean;
  agentId: string;
  trigger: AgentTrigger | null; // null = 新增
  onClose: () => void;
  onSaved: (message: string) => void;
}) {
  const [form] = Form.useForm<TriggerFormValues>();
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    if (open) {
      form.resetFields();
      form.setFieldsValue(
        trigger
          ? triggerToFormValues(trigger)
          : {
              name: '',
              enabled: true,
              oneShot: false,
              cooldown: 0,
              condition: {
                type: 'ColorFound',
                value: '',
                tolerance: 10,
                interval: 1000,
                region: { x: 0, y: 0, width: 0, height: 0 },
              },
              actions: [],
            },
      );
    }
  }, [open, trigger, form]);

  const handleOk = async () => {
    let values: TriggerFormValues;
    try {
      values = await form.validateFields();
    } catch {
      return;
    }
    setSubmitting(true);
    try {
      const config = formValuesToConfig(values);
      if (trigger) {
        await updateAgentTrigger(agentId, trigger.id, config);
        onSaved(`触发器 ${values.name} 已更新`);
      } else {
        await createAgentTrigger(agentId, config);
        onSaved(`触发器 ${values.name} 已创建`);
      }
      onClose();
    } catch (error) {
      // 交给外层事件流提示（onError 简化为 Modal 内提示）
      const message = (error as { message?: string })?.message || '保存失败';
      Modal.error({ title: '触发器保存失败', content: message });
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <Modal
      open={open}
      title={trigger ? `编辑触发器：${trigger.name}` : '新增触发器'}
      width={680}
      confirmLoading={submitting}
      onOk={handleOk}
      onCancel={onClose}
      destroyOnClose
    >
      <Form form={form} layout="vertical" initialValues={{ enabled: true }}>
        <Space size={16} style={{ display: 'flex' }} align="start">
          <Form.Item
            name="name"
            label="名称"
            rules={[{ required: true, message: '请输入触发器名称' }]}
            style={{ flex: 1, minWidth: 240 }}
          >
            <Input placeholder="如 hp-watch" />
          </Form.Item>
          <Form.Item name="cooldown" label="冷却 (ms)">
            <InputNumber min={0} step={500} />
          </Form.Item>
          <Form.Item name="enabled" label="启用" valuePropName="checked">
            <Switch size="small" />
          </Form.Item>
          <Form.Item name="oneShot" label="一次性" valuePropName="checked">
            <Switch size="small" />
          </Form.Item>
        </Space>

        <Divider orientation="left" plain>
          触发条件
        </Divider>
        <Space size={16} style={{ display: 'flex' }} align="start" wrap>
          <Form.Item name={['condition', 'type']} label="类型" style={{ minWidth: 160 }}>
            <Select options={TRIGGER_CONDITION_TYPES.map((type) => ({ value: type, label: type }))} />
          </Form.Item>
          <Form.Item
            name={['condition', 'value']}
            label="条件值（颜色 #rrggbb / 图片路径 / 窗口标题 / 进程名 / 毫秒 / 键名）"
            style={{ flex: 1, minWidth: 260 }}
          >
            <Input placeholder="#ff0000" />
          </Form.Item>
          <Form.Item name={['condition', 'tolerance']} label="容差">
            <InputNumber min={0} max={255} />
          </Form.Item>
          <Form.Item name={['condition', 'interval']} label="检测间隔 (ms)">
            <InputNumber min={0} step={100} />
          </Form.Item>
        </Space>
        <Space size={16} style={{ display: 'flex' }} wrap>
          {(['x', 'y', 'width', 'height'] as const).map((key) => (
            <Form.Item key={key} name={['condition', 'region', key]} label={`区域 ${key}`} initialValue={0}>
              <InputNumber min={0} />
            </Form.Item>
          ))}
        </Space>

        <Divider orientation="left" plain>
          触发动作
        </Divider>
        <Form.List name="actions">
          {(fields, { add, remove }) => (
            <>
              {fields.map((field) => (
                <Space key={field.key} size={8} style={{ display: 'flex' }} align="baseline" wrap>
                  <Form.Item name={[field.name, 'type']} initialValue="Log" noStyle>
                    <Select
                      style={{ width: 140 }}
                      options={TRIGGER_ACTION_TYPES.map((type) => ({ value: type, label: type }))}
                    />
                  </Form.Item>
                  <Form.Item name={[field.name, 'value']} noStyle>
                    <Input placeholder="脚本路径 / 按键 / 文本 / 消息" style={{ width: 220 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'x']} noStyle>
                    <InputNumber placeholder="x" style={{ width: 80 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'y']} noStyle>
                    <InputNumber placeholder="y" style={{ width: 80 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'delay']} noStyle>
                    <InputNumber placeholder="延迟" style={{ width: 90 }} />
                  </Form.Item>
                  <Button type="text" danger icon={<DeleteOutlined />} onClick={() => remove(field.name)} />
                </Space>
              ))}
              <Form.Item>
                <Button type="dashed" block icon={<PlusOutlined />} onClick={() => add({ type: 'Log' })}>
                  添加动作
                </Button>
              </Form.Item>
            </>
          )}
        </Form.List>
      </Form>
    </Modal>
  );
}

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

function formatLastSeen(lastSeen: number): string {
  if (!lastSeen) return '未知';
  const delta = Date.now() - lastSeen;
  if (delta < 0) return '刚刚';
  if (delta < 60_000) return `${Math.floor(delta / 1000)} 秒前`;
  if (delta < 3_600_000) return `${Math.floor(delta / 60_000)} 分钟前`;
  return `${Math.floor(delta / 3_600_000)} 小时前`;
}

// extractErrorMessage 从 umi request 抛出的错误中提取可读文本
function extractErrorMessage(error: unknown): string {
  const candidate = error as
    | { data?: { error?: string; message?: string }; message?: string }
    | undefined;
  return (
    candidate?.data?.error ||
    candidate?.data?.message ||
    candidate?.message ||
    '请求失败'
  );
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
      wsService.onTriggerFired((data) => {
        if (!isRunningRef.current) return;
        // 事件载荷（runtime TriggerInstance）：{id, name, triggered, lastTriggerTime, agentId}
        const triggerId = String(data.id ?? data.triggerId ?? '');
        const triggerName = String(data.name || data.triggerName || triggerId || '未知触发器');
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
          message: `触发器 ${triggerName} 被触发`,
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
        setTriggersError(`Agent ${agent.hostname || agent.agentId} 已离线，无法读取触发器`);
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
        setTriggersError(extractErrorMessage(error));
        addEvent({
          type: 'trigger',
          level: 'warning',
          message: `触发器列表加载失败: ${extractErrorMessage(error)}`,
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
        message: 'Agent 未连接，无法切换触发器状态',
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
        message: `触发器 ${trigger.name} 已${enabled ? '启用' : '停用'}`,
      });
    } catch (error) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: `触发器 ${trigger.name} 切换失败: ${extractErrorMessage(error)}`,
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
        message: 'Agent 未连接，无法新增触发器',
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
        message: 'Agent 未连接，无法删除触发器',
      });
      return;
    }
    try {
      await removeAgentTrigger(agent.agentId, trigger.id);
      setTriggers((previous) => previous.filter((item) => item.id !== trigger.id));
      addEvent({
        type: 'trigger',
        level: 'processing',
        message: `触发器 ${trigger.name} 已删除`,
      });
    } catch (error) {
      addEvent({
        type: 'trigger',
        level: 'error',
        message: `触发器 ${trigger.name} 删除失败: ${extractErrorMessage(error)}`,
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
        <Button key="settings" icon={<SettingOutlined />} onClick={() => history.push('/settings')}>
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
                    新增
                  </Button>
                  <Button
                    size="small"
                    type="text"
                    icon={<ReloadOutlined spin={triggersLoading} />}
                    disabled={!agentConnected(selectedAgent) || triggersLoading}
                    onClick={() => loadTriggers(selectedAgent)}
                  >
                    刷新
                  </Button>
                </Space>
              }
            >
              {/* Runtime 连接状态：agent↔orchestrator 链路可用性决定触发器数据源 */}
              <Space direction="vertical" size={2} style={{ width: '100%', marginBottom: 12 }}>
                <Space size={8} wrap>
                  <Text type="secondary">数据源:</Text>
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
                    <Tag>无 Agent</Tag>
                  )}
                  <Tag color={wsConnected ? 'green' : 'orange'}>{wsConnected ? 'WS 已连' : 'WS 断开'}</Tag>
                </Space>
                {selectedAgent && (
                  <Text type="secondary">
                    Agent: {selectedAgent.agentId} · 最近心跳: {formatLastSeen(selectedAgent.lastSeen)}
                  </Text>
                )}
              </Space>

              {triggersError && (
                <Alert
                  type={agentConnected(selectedAgent) ? 'error' : 'warning'}
                  showIcon
                  style={{ marginBottom: 12 }}
                  message="触发器数据不可用"
                  description={triggersError}
                />
              )}

              <List
                size="small"
                loading={triggersLoading}
                dataSource={triggers}
                locale={{ emptyText: '该 Agent 暂无触发器（runtime trigger.list 为空）' }}
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
                        title="删除触发器"
                        description={`确定删除「${trigger.name}」？该操作会即时下发到 runtime。`}
                        okText="删除"
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
                          {trigger.oneShot && <Tag>一次性</Tag>}
                          {trigger.cooldown > 0 && <Tag>冷却 {trigger.cooldown}ms</Tag>}
                        </Space>
                      }
                      description={`${trigger.condition?.value || trigger.condition?.type || '无条件配置'} · 动作 ${trigger.actions.length} · 命中 ${trigger.hitCount || 0} · ${trigger.lastFiredAt || '未触发'}`}
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

        <Card title="实时事件流" extra={<Tag>事件驱动</Tag>}>
          {events.length === 0 ? (
            <Text type="secondary">等待 Agent、触发器或脚本事件...</Text>
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
          agentId={selectedAgent.agentId}
          trigger={editingTrigger}
          onClose={() => setTriggerModalOpen(false)}
          onSaved={handleTriggerSaved}
        />
      )}
    </PageContainer>
  );
};

export default Monitor;
