import {
  DeleteOutlined,
  InfoCircleOutlined,
  NodeIndexOutlined,
  PauseCircleOutlined,
  PlayCircleOutlined,
  ReloadOutlined,
  ScheduleOutlined,
  ThunderboltOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import {
  PageContainer,
  ProCard,
  type ProColumns,
  ProDescriptions,
  ProTable,
} from '@ant-design/pro-components';
import { useAccess, useRequest } from '@umijs/max';
import {
  Button,
  Card,
  Col,
  Input,
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

// extractErrorMessage 从 umi request 抛出的错误中提取可读文本
function extractErrorMessage(error: unknown): string {
  const candidate = error as
    | { data?: { error?: string; message?: string }; message?: string }
    | undefined;
  return candidate?.data?.error || candidate?.data?.message || candidate?.message || '请求失败';
}

const Agents: React.FC = () => {
  const access = useAccess();
  const canAgentManage = Boolean(access.canAgentManage);
  const canScriptRun = Boolean(access.canScriptRun);
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
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
        message.success(`Agent ${data.hostname || data.agentId} 已上线`);
      }),
    );

    // Agent 断开
    unsubscribes.push(
      wsService.onAgentDisconnected((data) => {
        setAgents((prev) =>
          prev.map((a) => (a.agentId === data.agentId ? { ...a, status: AgentStatus.Offline } : a)),
        );
        message.warning(`Agent ${data.hostname || data.agentId} 已离线`);
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

  // 保存标签
  const handleSaveTags = async (agentId: string) => {
    if (!tagDraft) return;
    if (!canAgentManage) {
      message.warning('需要 agents:manage 权限');
      return;
    }
    try {
      await setAgentTags(agentId, tagDraft.tags);
      message.success('标签已更新');
      setTagDraft(null);
      refresh();
    } catch {
      message.error('标签更新失败（可能需要管理员权限）');
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
      message.error(extractErrorMessage(error));
    } finally {
      setScriptsLoading(false);
    }
  };

  const submitBatchRun = () => {
    if (!runPath) {
      message.warning('请先选择脚本');
      return;
    }
    Modal.confirm({
      title: '批量运行脚本',
      content: `将在选中的 ${selectedIds.length} 台 Agent 上运行 ${runPath}，确认执行？`,
      okText: '批量运行',
      onOk: async () => {
        try {
          const response = await batchRunScript({ agentIds: selectedIds }, runPath);
          if (!response.success) {
            message.error(response.error || '批量运行失败');
            return;
          }
          setBatchModal(null);
          showBatchResult(response.data);
        } catch (error) {
          message.error(extractErrorMessage(error));
        }
      },
    });
  };

  const submitBatchStop = () => {
    const executionId = stopExecutionId.trim();
    if (!executionId) {
      message.warning('请输入执行 ID（脚本名）');
      return;
    }
    Modal.confirm({
      title: '批量停止脚本',
      content: `将在选中的 ${selectedIds.length} 台 Agent 上停止 ${executionId}，确认执行？`,
      okText: '批量停止',
      onOk: async () => {
        try {
          const response = await batchStopScript({ agentIds: selectedIds }, executionId);
          if (!response.success) {
            message.error(response.error || '批量停止失败');
            return;
          }
          setBatchModal(null);
          showBatchResult(response.data);
        } catch (error) {
          message.error(extractErrorMessage(error));
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
            <ScheduleOutlined /> 系统资源
          </>
        }
      >
        <Row gutter={[16, 16]}>
          <Col xs={24} sm={12}>
            <Statistic
              title="CPU 使用率"
              value={cpu.usage}
              suffix="%"
              valueStyle={{
                color: cpu.usage > 80 ? '#ff4d4f' : cpu.usage > 50 ? '#faad14' : '#52c41a',
              }}
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
      render: (_, record) => (
        <Tag color={getAgentStatusColor(record.status)}>{record.status.toUpperCase()}</Tag>
      ),
    },
    {
      title: '标签',
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
              placeholder="输入标签后回车"
              value={tagDraft?.agentId === record.agentId ? tagDraft.tags : tags}
              onChange={(vals) => setTagDraft({ agentId: record.agentId, tags: vals as string[] })}
            />
            <div style={{ marginTop: 8, textAlign: 'right' }}>
              <Space>
                <Button size="small" onClick={() => setTagDraft(null)}>
                  取消
                </Button>
                <Button size="small" type="primary" onClick={() => handleSaveTags(record.agentId)}>
                  保存
                </Button>
              </Space>
            </div>
          </div>
        );
        const tagList = (
          <Space size={[0, 4]} wrap style={{ cursor: canAgentManage ? 'pointer' : 'default' }}>
            {tags.length === 0 ? (
              <Tag style={{ borderStyle: 'dashed' }}>{canAgentManage ? '+ 添加' : '无标签'}</Tag>
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
          return <Tooltip title="需要 agents:manage 权限才能编辑标签">{tagList}</Tooltip>;
        }
        return (
          <Popover content={content} title="编辑标签" trigger="click" placement="bottom">
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
      title: '内存',
      dataIndex: ['resources', 'memory', 'usage'],
      key: 'memory',
      width: 100,
      render: (_, record) => (
        <Progress percent={Math.round(record.resources?.memory?.usage || 0)} size="small" />
      ),
    },
    {
      title: '最后活跃',
      dataIndex: 'lastSeen',
      key: 'lastSeen',
      width: 120,
      render: (_, record) => {
        const diff = Date.now() - record.lastSeen;
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
        title: 'Agent 管理',
        subTitle: '监控和管理所有已注册的 Agent',
        extra: wsConnected ? (
          <Badge status="processing" text="实时连接" />
        ) : (
          <Space>
            <Badge status="default" text="轮询模式" />
            <Button size="small" onClick={() => wsService.reconnect()}>
              重连
            </Button>
          </Space>
        ),
      }}
    >
      <Row gutter={[16, 16]}>
        {/* 统计卡片 */}
        <Col xs={24} sm={6}>
          <Card>
            <Statistic title="总数" value={agents.length} prefix={<NodeIndexOutlined />} />
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <Statistic
              title="在线"
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
                <Button icon={<ReloadOutlined />} onClick={refresh} loading={loading}>
                  刷新
                </Button>
              </Space>
            }
          >
            {/* 批量工具栏：标签筛选只过滤视图；批量按钮选中 0 台时禁用 */}
            <Space style={{ marginBottom: 16, display: 'flex' }} wrap>
              <Select
                mode="multiple"
                allowClear
                placeholder="按标签筛选"
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
                    批量运行脚本
                  </Button>
                  <Button
                    icon={<PauseCircleOutlined />}
                    disabled={selectedIds.length === 0}
                    onClick={() => {
                      setStopExecutionId('');
                      setBatchModal('stop');
                    }}
                  >
                    批量停止脚本
                  </Button>
                </>
              )}
              {canAgentManage && (
                <Button
                  icon={<ThunderboltOutlined />}
                  disabled={selectedIds.length === 0}
                  onClick={() => setTriggerModalOpen(true)}
                >
                  批量下发触发器
                </Button>
              )}
              {selectedIds.length > 0 && <Text type="secondary">已选 {selectedIds.length} 台</Text>}
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
                title="Agent 详情"
                extra={
                  <Button type="text" onClick={() => setSelectedAgent(null)}>
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
                      render: (_, record) => <Text copyable>{record.agentId}</Text>,
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
                      render: (_, record) => (
                        <Tag color={getAgentStatusColor(record.status)}>
                          {record.status.toUpperCase()}
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

      {/* 批量运行脚本：脚本列表来自 /api/scripts，服务端会先做路径校验 */}
      <Modal
        open={batchModal === 'run'}
        title={`批量运行脚本（已选 ${selectedIds.length} 台）`}
        width={520}
        onOk={submitBatchRun}
        onCancel={() => setBatchModal(null)}
        destroyOnClose
      >
        <Select
          showSearch
          style={{ width: '100%' }}
          placeholder="选择要运行的脚本"
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
        title={`批量停止脚本（已选 ${selectedIds.length} 台）`}
        width={520}
        onOk={submitBatchStop}
        onCancel={() => setBatchModal(null)}
        destroyOnClose
      >
        <Input
          placeholder="执行 ID（脚本名）"
          value={stopExecutionId}
          onChange={(event) => setStopExecutionId(event.target.value)}
        />
      </Modal>

      {/* 批量下发触发器：复用共享表单，提交即 fan-out trigger.add */}
      <TriggerFormModal
        open={triggerModalOpen}
        title={`批量下发触发器（已选 ${selectedIds.length} 台）`}
        onSubmit={async (config) => {
          let response;
          try {
            response = await batchCreateTrigger({ agentIds: selectedIds }, config);
          } catch (error) {
            throw new Error(extractErrorMessage(error));
          }
          if (!response.success) {
            throw new Error(response.error || '批量下发失败');
          }
          setTriggerModalOpen(false);
          showBatchResult(response.data);
        }}
        onClose={() => setTriggerModalOpen(false)}
      />

      {/* 批量操作结果：汇总 + 逐台明细（含离线/失败原因） */}
      <Modal
        open={batchResult !== null}
        title="批量操作结果"
        width={620}
        footer={
          <Button type="primary" onClick={() => setBatchResult(null)}>
            关闭
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
              message={`共 ${batchResult.total} 台：成功 ${batchResult.succeeded} 台，失败 ${batchResult.failed} 台`}
            />
            <Table
              size="small"
              rowKey="agentId"
              dataSource={batchResult.results}
              pagination={batchResult.results.length > 10 ? { pageSize: 10 } : false}
              columns={[
                { title: 'Agent ID', dataIndex: 'agentId', ellipsis: true },
                {
                  title: '结果',
                  dataIndex: 'success',
                  width: 90,
                  render: (ok: boolean) =>
                    ok ? <Tag color="success">成功</Tag> : <Tag color="error">失败</Tag>,
                },
                {
                  title: '失败原因',
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
