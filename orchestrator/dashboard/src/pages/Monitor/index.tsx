/**
 * 游戏监控页面
 * 集成实时截图、触发器管理、宏命令等功能
 */
import React, { useState, useEffect } from 'react';
import { PageContainer, ProCard, ProDescriptions } from '@ant-design/pro-components';
import {
  Row,
  Col,
  Space,
  Typography,
  Statistic,
  Card,
  Button,
  Switch,
  Tag,
  Alert,
  Tabs,
  List,
  Divider,
  Tooltip,
} from 'antd';
import {
  PlayCircleOutlined,
  PauseCircleOutlined,
  ReloadOutlined,
  ScreenshotOutlined,
  ThunderboltOutlined,
  CodeOutlined,
  SettingOutlined,
  DesktopOutlined,
  MemoryOutlined,
  ClockCircleOutlined,
  PlusOutlined,
} from '@ant-design/icons';
import ScreenshotView from '@/components/ScreenshotView';
import wsService from '@/services/websocket';

const { Title, Text } = Typography;
const { TabPane } = Tabs;

interface TriggerInfo {
  id: string;
  name: string;
  enabled: boolean;
  type: string;
  condition: string;
  actions: number;
  cooldown: number;
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
}

const Monitor: React.FC = () => {
  const [isRunning, setIsRunning] = useState(false);
  const [systemStatus, setSystemStatus] = useState<SystemStatus>({
    cpu: 0,
    memory: 0,
    uptime: 0,
    fps: 0,
  });
  const [triggers, setTriggers] = useState<TriggerInfo[]>([
    { id: '1', name: '技能冷却检测', enabled: true, type: 'color', condition: '绿色出现', actions: 1, cooldown: 500 },
    { id: '2', name: '血量监控', enabled: true, type: 'color', condition: '红色低于30%', actions: 2, cooldown: 1000 },
    { id: '3', name: '自动拾取', enabled: false, type: 'image', condition: '掉落物品', actions: 1, cooldown: 200 },
  ]);
  const [macros, setMacros] = useState<MacroInfo[]>([
    { id: '1', name: '日常采集', actions: 15, lastRun: '2小时前' },
    { id: '2', name: '自动战斗', actions: 8, lastRun: '5小时前' },
  ]);

  // 模拟系统状态更新
  useEffect(() => {
    const timer = setInterval(() => {
      setSystemStatus({
        cpu: Math.random() * 20 + 5,
        memory: Math.random() * 10 + 40,
        uptime: Date.now() - new Date().setHours(8, 0, 0, 0),
        fps: isRunning ? Math.floor(Math.random() * 5 + 55) : 0,
      });
    }, 2000);

    return () => clearInterval(timer);
  }, [isRunning]);

  // 切换运行状态
  const handleToggleRun = () => {
    setIsRunning(!isRunning);
  };

  // 触发器操作
  const handleToggleTrigger = (id: string) => {
    setTriggers(triggers.map(t =>
      t.id === id ? { ...t, enabled: !t.enabled } : t
    ));
  };

  // 宏操作
  const handleRunMacro = (id: string) => {
    console.log('Running macro:', id);
  };

  const formatUptime = (ms: number) => {
    const seconds = Math.floor(ms / 1000);
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    return `${hours}h ${minutes}m`;
  };

  return (
    <PageContainer
      header={{
        title: '游戏监控',
        breadcrumb: {},
      }}
      extra={[
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
        {/* 系统状态概览 */}
        <Row gutter={16}>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="CPU 使用率"
                value={systemStatus.cpu}
                suffix="%"
                prefix={<DesktopOutlined />}
                valueStyle={{ color: systemStatus.cpu > 80 ? '#ff4d4f' : '#3f8600' }}
              />
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="内存使用"
                value={systemStatus.memory}
                suffix="%"
                prefix={<MemoryOutlined />}
                valueStyle={{ color: systemStatus.memory > 80 ? '#ff4d4f' : '#1890ff' }}
              />
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="运行时长"
                value={formatUptime(systemStatus.uptime)}
                prefix={<ClockCircleOutlined />}
              />
            </Card>
          </Col>
          <Col xs={24} sm={12} md={6}>
            <Card>
              <Statistic
                title="画面帧率"
                value={systemStatus.fps}
                suffix="FPS"
                prefix={<ScreenshotOutlined />}
                valueStyle={{ color: systemStatus.fps >= 50 ? '#3f8600' : '#faad14' }}
              />
            </Card>
          </Col>
        </Row>

        {/* 主内容区 */}
        <Row gutter={16}>
          {/* 左侧：游戏画面 */}
          <Col xs={24} lg={16}>
            <ScreenshotView height={500} />
          </Col>

          {/* 右侧：触发器和宏 */}
          <Col xs={24} lg={8}>
            <Card
              title={
                <Space>
                  <ThunderboltOutlined />
                  <span>触发器</span>
                  <Tag color="blue">{triggers.filter(t => t.enabled).length}/{triggers.length}</Tag>
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
                      description={`${trigger.condition} · ${trigger.actions}个动作 · ${trigger.cooldown}ms`}
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

        {/* 详细信息 */}
        <Row gutter={16}>
          <Col xs={24}>
            <Card title="运行日志" extra={<Button icon={<ReloadOutlined />}>刷新</Button>}>
              <div style={{ height: 200, overflowY: 'auto', fontFamily: 'monospace', fontSize: 12 }}>
                <Text type="secondary">
                  [12:34:56] 系统启动完成
                  <br />
                  [12:35:01] 触发器 "技能冷却检测" 被触发，执行按键 "1"
                  <br />
                  [12:35:02] 触发器 "技能冷却检测" 冷却中，跳过
                  <br />
                  [12:35:05] 宏命令 "日常采集" 开始执行
                  <br />
                  [12:35:10] 截图上传成功，大小: 45KB
                  <br />
                </Text>
              </div>
            </Card>
          </Col>
        </Row>
      </Space>
    </PageContainer>
  );
};

export default Monitor;
