import {
  DeleteOutlined,
  InfoCircleOutlined,
  NodeIndexOutlined,
  ReloadOutlined,
  ScheduleOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import {
  PageContainer,
  ProCard,
  ProDescriptions,
  ProTable,
} from '@ant-design/pro-components';
import { useRequest } from '@umijs/max';
import {
  Button,
  Card,
  Col,
  Progress,
  Row,
  Space,
  Statistic,
  Tag,
  Tooltip,
  Typography,
  message,
  Modal,
  Badge,
} from 'antd';
import React, { useRef, useState, useEffect, useCallback } from 'react';
import {
  AgentStatus,
  AgentInfo,
  getAgents,
  shutdownAgent,
  formatBytes,
  getAgentStatusColor,
} from '@/services/wingman';
import wsService from '@/services/websocket';

const { Text } = Typography;

const Agents: React.FC = () => {
  const actionRef = useRef();
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  const [wsConnected, setWsConnected] = useState(false);
  const [agents, setAgents] = useState<AgentInfo[]>([]); // 本地状态用于实时更新

  // 获取 Agent 列表
  const { data: agentsData, loading, refresh } = useRequest(
    async () => {
      const response = await getAgents();
      return response.data || [];
    },
    {
      onSuccess: (data) => {
        setAgents(data || []);
      },
      pollingInterval: 10000, // 降低轮询频率，主要依靠 WebSocket
    }
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
    unsubscribes.push(wsService.onAgentConnected((data) => {
      setAgents((prev) => {
        const exists = prev.some((a) => a.agentId === data.agentId);
        if (exists) {
          // 更新现有 Agent
          return prev.map((a) =>
            a.agentId === data.agentId ? { ...a, ...data, status: AgentStatus.Online } : a
          );
        }
        // 添加新 Agent
        return [...prev, { ...data, status: AgentStatus.Online } as AgentInfo];
      });
      message.success(`Agent ${data.hostname || data.agentId} 已上线`);
    }));

    // Agent 断开
    unsubscribes.push(wsService.onAgentDisconnected((data) => {
      setAgents((prev) =>
        prev.map((a) =>
          a.agentId === data.agentId ? { ...a, status: AgentStatus.Offline } : a
        )
      );
      message.warning(`Agent ${data.hostname || data.agentId} 已离线`);
    }));

    // Agent 状态变化
    unsubscribes.push(wsService.onAgentStatusChanged((data) => {
      setAgents((prev) =>
        prev.map((a) =>
          a.agentId === data.agentId ? { ...a, ...data } : a
        )
      );
    }));

    return () => {
      unsubscribes.forEach((unsub) => unsub());
    };
  }, []);

  // 关闭 Agent
  const handleShutdown = async (agentId: string) => {
    Modal.confirm({
      title: '确认关闭',
      content: `确定要关闭 Agent ${agentId} 吗？`,
      onOk: async () => {
        try {
          await shutdownAgent(agentId);
          message.success('关闭命令已发送');
          refresh();
        } catch (error) {
          message.error('关闭失败');
        }
      },
    });
  };

  // 资源卡片组件
  const ResourceCard: React.FC<{ agent: AgentInfo }> = ({ agent }) => {
    const { cpu, memory, disk, network } = agent.resources;

    return (
      <Card size="small" title={<><ScheduleOutlined /> 系统资源</>}>
        <Row gutter={[16, 16]}>
          <Col xs={24} sm={12}>
            <Statistic
              title="CPU 使用率"
              value={cpu.usage}
              suffix="%"
              valueStyle={{ color: cpu.usage > 80 ? '#ff4d4f' : cpu.usage > 50 ? '#faad14' : '#52c41a' }}
            />
            <Progress percent={cpu.usage} size="small" showInfo={false} />
            <Text type="secondary" style={{ fontSize: 12 }}>
              {cpu.cores} 核心 / {cpu.model || 'Unknown'}
            </Text>
          </Col>
          <Col xs={24} sm={12}>
            <Statistic
              title="内存使用"
              value={memory.usage}
              suffix="%"
              valueStyle={{ color: memory.usage > 80 ? '#ff4d4f' : memory.usage > 50 ? '#faad14' : '#52c41a' }}
            />
            <Progress percent={memory.usage} size="small" showInfo={false} />
            <Text type="secondary" style={{ fontSize: 12 }}>
              {formatBytes(memory.available)} / {formatBytes(memory.total)}
            </Text>
          </Col>
          <Col xs={24} sm={12}>
            <Statistic
              title="磁盘使用"
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
                  title="上行"
                  value={formatBytes(network.up)}
                  suffix="/s"
                  style={{ fontSize: 14 }}
                />
              </Col>
              <Col span={12}>
                <Statistic
                  title="下行"
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

  const columns = [
    {
      title: 'Agent ID',
      dataIndex: 'agentId',
      key: 'agentId',
      width: 200,
      render: (text: string) => (
        <Space>
          <NodeIndexOutlined />
          <Text copyable={{ text }}>{text.slice(0, 12)}...</Text>
        </Space>
      ),
    },
    {
      title: '主机名',
      dataIndex: 'hostname',
      key: 'hostname',
      width: 150,
    },
    {
      title: 'IP 地址',
      dataIndex: 'ip',
      key: 'ip',
      width: 120,
    },
    {
      title: '状态',
      dataIndex: 'status',
      key: 'status',
      width: 100,
      render: (status: AgentStatus) => (
        <Tag color={getAgentStatusColor(status)}>
          {status.toUpperCase()}
        </Tag>
      ),
    },
    {
      title: 'CPU',
      dataIndex: ['resources', 'cpu', 'usage'],
      key: 'cpu',
      width: 100,
      render: (usage: number) => (
        <Progress percent={Math.round(usage)} size="small" />
      ),
    },
    {
      title: '内存',
      dataIndex: ['resources', 'memory', 'usage'],
      key: 'memory',
      width: 100,
      render: (usage: number) => (
        <Progress percent={Math.round(usage)} size="small" />
      ),
    },
    {
      title: '最后活跃',
      dataIndex: 'lastSeen',
      key: 'lastSeen',
      width: 120,
      render: (time: number) => {
        const diff = Date.now() - time;
        if (diff < 60000) return '< 1 分钟';
        if (diff < 3600000) return `${Math.floor(diff / 60000)} 分钟`;
        return `${Math.floor(diff / 3600000)} 小时`;
      },
    },
    {
      title: '操作',
      key: 'action',
      width: 120,
      fixed: 'right' as const,
      render: (_: any, record: AgentInfo) => (
        <Space>
          <Tooltip title="查看详情">
            <Button
              type="link"
              size="small"
              icon={<InfoCircleOutlined />}
              onClick={() => setSelectedAgent(record)}
            />
          </Tooltip>
          <Tooltip title="关闭 Agent">
            <Button
              type="link"
              size="small"
              danger
              icon={<DeleteOutlined />}
              onClick={() => handleShutdown(record.agentId)}
              disabled={record.status === AgentStatus.Offline}
            />
          </Tooltip>
        </Space>
      ),
    },
  ];

  return (
    <PageContainer
      header={{
        title: 'Agent 管理',
        subTitle: '监控和管理所有已注册的 Agent',
        extra: wsConnected ? (
          <Badge status="processing" text="实时连接" />
        ) : (
          <Badge status="default" text="轮询模式" />
        ),
      }}
    >
      <Row gutter={[16, 16]}>
        {/* 统计卡片 */}
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title="总数"
              value={agents.length}
              prefix={<NodeIndexOutlined />}
            />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title="在线"
              value={agents.filter((a: AgentInfo) => a.status === AgentStatus.Online || a.status === AgentStatus.Idle).length}
              valueStyle={{ color: '#52c41a' }}
            />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title="忙碌"
              value={agents.filter((a: AgentInfo) => a.status === AgentStatus.Busy).length}
              valueStyle={{ color: '#faad14' }}
            />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title="离线"
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
            title="Agent 列表"
            extra={
              <Space>
                {wsConnected && (
                  <Tag icon={<WifiOutlined />} color="success">
                    实时更新
                  </Tag>
                )}
                <Button
                  icon={<ReloadOutlined />}
                  onClick={refresh}
                  loading={loading}
                >
                  刷新
                </Button>
              </Space>
            }
          >
            <ProTable<AgentInfo>
              columns={columns}
              dataSource={agents}
              loading={loading}
              rowKey="agentId"
              search={false}
              options={false}
              pagination={{
                pageSize: 10,
                showSizeChanger: true,
              }}
              onRow={(record) => ({
                onClick: () => setSelectedAgent(record),
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
                title="Agent 详情"
                extra={
                  <Button
                    type="text"
                    onClick={() => setSelectedAgent(null)}
                  >
                    关闭
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
                      render: (text) => <Text copyable>{text}</Text>,
                    },
                    {
                      title: '主机名',
                      dataIndex: 'hostname',
                    },
                    {
                      title: 'IP 地址',
                      dataIndex: 'ip',
                    },
                    {
                      title: '状态',
                      dataIndex: 'status',
                      render: (status: AgentStatus) => (
                        <Tag color={getAgentStatusColor(status)}>
                          {status.toUpperCase()}
                        </Tag>
                      ),
                    },
                    {
                      title: '当前任务',
                      dataIndex: 'currentTask',
                      render: (task) => task || <Text type="secondary">无</Text>,
                    },
                    {
                      title: '操作系统',
                      dataIndex: ['resources', 'system', 'os'],
                    },
                    {
                      title: '架构',
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
    </PageContainer>
  );
};

export default Agents;
