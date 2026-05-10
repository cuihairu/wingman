import {
  DeleteOutlined,
  EyeOutlined,
  PlusOutlined,
  PlayCircleOutlined,
  ReloadOutlined,
  StopOutlined,
  UnorderedListOutlined,
  WifiOutlined,
} from '@ant-design/icons';
import {
  ModalForm,
  PageContainer,
  ProCard,
  ProDescriptions,
  ProFormText,
  ProFormTextArea,
  ProFormList,
  ProTable,
} from '@ant-design/pro-components';
import { useRequest } from '@umijs/max';
import {
  Button,
  Card,
  Col,
  Descriptions,
  Row,
  Space,
  Steps,
  Tag,
  Typography,
  message,
  Drawer,
  Alert,
  Badge,
} from 'antd';
import React, { useRef, useState, useEffect } from 'react';
import {
  Workflow,
  WorkflowInstance,
  WorkflowStatus,
  TaskStep,
  StepStatus,
  getWorkflows,
  getWorkflow,
  submitWorkflow,
  cancelWorkflow,
  formatDuration,
  getWorkflowStatusColor,
  getStepStatusColor,
} from '@/services/wingman';
import wsService from '@/services/websocket';

const { Text, Title } = Typography;

const Workflows: React.FC = () => {
  const actionRef = useRef();
  const [selectedWorkflow, setSelectedWorkflow] = useState<WorkflowInstance | null>(null);
  const [drawerVisible, setDrawerVisible] = useState(false);
  const [createModalVisible, setCreateModalVisible] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [workflows, setWorkflows] = useState<Workflow[]>([]); // 本地状态用于实时更新

  // 获取工作流列表
  const { data: workflowsData, loading, refresh } = useRequest(
    async () => {
      const response = await getWorkflows();
      return response.data || [];
    },
    {
      onSuccess: (data) => {
        setWorkflows(data || []);
      },
      pollingInterval: 10000, // 降低轮询频率
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
    };
  }, []);

  // 监听工作流事件
  useEffect(() => {
    const unsubscribes: (() => void)[] = [];

    // 工作流提交
    unsubscribes.push(wsService.onWorkflowSubmitted((data) => {
      setWorkflows((prev) => [{ ...data, status: WorkflowStatus.Pending }, ...prev]);
      message.success(`工作流 ${data.name} 已提交`);
    }));

    // 工作流状态变化
    unsubscribes.push(wsService.onWorkflowStatusChanged((data) => {
      setWorkflows((prev) =>
        prev.map((w) =>
          w.id === data.id ? { ...w, ...data } : w
        )
      );

      // 更新详情面板中当前选中的工作流
      if (selectedWorkflow && selectedWorkflow.id === data.id) {
        setSelectedWorkflow((prev) => prev ? { ...prev, ...data } : null);
      }
    }));

    // 工作流进度更新
    unsubscribes.push(wsService.onWorkflowProgress((data) => {
      // 更新详情面板
      if (selectedWorkflow && selectedWorkflow.id === data.workflowId) {
        setSelectedWorkflow((prev) => {
          if (!prev) return null;
          return {
            ...prev,
            stepStatus: { ...prev.stepStatus, [data.stepId]: data.status },
            currentStepId: data.stepId,
          };
        });
      }
    }));

    return () => {
      unsubscribes.forEach((unsub) => unsub());
    };
  }, [selectedWorkflow]);

  // 获取工作流详情
  const { run: fetchDetail } = useRequest(
    async (workflowId: string) => {
      const response = await getWorkflow(workflowId);
      return response.data;
    },
    {
      manual: true,
      onSuccess: (data) => {
        setSelectedWorkflow(data);
        setDrawerVisible(true);
      },
    }
  );

  // 提交工作流
  const handleSubmit = async (values: any) => {
    try {
      const workflow = {
        name: values.name,
        description: values.description,
        steps: values.steps.map((step: any) => ({
          id: step.id,
          name: step.name,
          script: step.script,
          workers: step.workers || [],
          dependsOn: step.dependsOn || [],
          timeoutSeconds: step.timeoutSeconds || 300,
          parameters: step.parameters || {},
        })),
        sharedContext: values.sharedContext || {},
      };
      await submitWorkflow(workflow);
      message.success('工作流已提交');
      setCreateModalVisible(false);
      refresh();
    } catch (error) {
      message.error('提交失败');
    }
  };

  // 取消工作流
  const handleCancel = async (workflowId: string) => {
    try {
      await cancelWorkflow(workflowId);
      message.success('工作流已取消');
      refresh();
      if (selectedWorkflow?.id === workflowId) {
        fetchDetail(workflowId);
      }
    } catch (error) {
      message.error('取消失败');
    }
  };

  const columns = [
    {
      title: '工作流 ID',
      dataIndex: 'id',
      key: 'id',
      width: 150,
      render: (text: string) => (
        <Space>
          <UnorderedListOutlined />
          <Text copyable={{ text }}>{text.slice(0, 8)}...</Text>
        </Space>
      ),
    },
    {
      title: '名称',
      dataIndex: 'name',
      key: 'name',
      width: 200,
    },
    {
      title: '描述',
      dataIndex: 'description',
      key: 'description',
      ellipsis: true,
    },
    {
      title: '状态',
      dataIndex: 'status',
      key: 'status',
      width: 100,
      render: (status: WorkflowStatus) => (
        <Tag color={getWorkflowStatusColor(status)}>
          {status.toUpperCase()}
        </Tag>
      ),
    },
    {
      title: '步骤数',
      dataIndex: 'steps',
      key: 'steps',
      width: 80,
      render: (steps: TaskStep[]) => steps?.length || 0,
    },
    {
      title: '创建时间',
      dataIndex: 'createdTime',
      key: 'createdTime',
      width: 150,
      render: (time: number) => new Date(time).toLocaleString(),
    },
    {
      title: '运行时长',
      key: 'duration',
      width: 100,
      render: (_: any, record: Workflow) => {
        if (!record.startTime) return '-';
        const end = record.endTime || Date.now();
        return formatDuration(end - record.startTime);
      },
    },
    {
      title: '操作',
      key: 'action',
      width: 150,
      fixed: 'right' as const,
      render: (_: any, record: Workflow) => (
        <Space>
          <Button
            type="link"
            size="small"
            icon={<EyeOutlined />}
            onClick={() => fetchDetail(record.id)}
          />
          {record.status === WorkflowStatus.Running && (
            <Button
              type="link"
              size="small"
              danger
              icon={<StopOutlined />}
              onClick={() => handleCancel(record.id)}
            />
          )}
        </Space>
      ),
    },
  ];

  // 渲染步骤状态
  const renderStepStatus = (step: TaskStep, workflow: WorkflowInstance) => {
    const status = workflow.stepStatus?.[step.id];
    const isCompleted = status === StepStatus.Completed;
    const isRunning = status === StepStatus.Running;
    const isFailed = status === StepStatus.Failed;

    let icon = null;
    if (isCompleted) icon = '✓';
    else if (isFailed) icon = '✗';
    else if (isRunning) icon = '⟳';

    return (
      <div
        key={step.id}
        style={{
          padding: '12px',
          border: `1px solid ${isRunning ? '#1890ff' : isFailed ? '#ff4d4f' : '#d9d9d9'}`,
          borderRadius: '4px',
          background: isRunning ? '#e6f7ff' : isFailed ? '#fff2f0' : undefined,
        }}
      >
        <Space direction="vertical" style={{ width: '100%' }}>
          <Space>
            {icon && <span style={{ fontWeight: 'bold' }}>{icon}</span>}
            <Text strong>{step.name}</Text>
            {status && (
              <Tag color={getStepStatusColor(status)}>
                {status.toUpperCase()}
              </Tag>
            )}
          </Space>
          <Text type="secondary" style={{ fontSize: 12 }}>
            Script: {step.script}
          </Text>
          {step.workers.length > 0 && (
            <Space wrap>
              {step.workers.map((w) => (
                <Tag key={w}>{w}</Tag>
              ))}
            </Space>
          )}
          {step.dependsOn.length > 0 && (
            <Text type="secondary" style={{ fontSize: 12 }}>
              依赖: {step.dependsOn.join(', ')}
            </Text>
          )}
        </Space>
      </div>
    );
  };

  return (
    <PageContainer
      header={{
        title: '工作流管理',
        subTitle: '创建和管理自动化工作流任务',
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
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic title="总数" value={workflows.length} />
              <UnorderedListOutlined style={{ fontSize: 24, color: '#999' }} />
            </div>
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic
                title="运行中"
                value={workflows.filter((w: Workflow) => w.status === WorkflowStatus.Running).length}
                valueStyle={{ color: '#1890ff' }}
              />
              <PlayCircleOutlined style={{ fontSize: 24, color: '#1890ff' }} />
            </div>
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic
                title="已完成"
                value={workflows.filter((w: Workflow) => w.status === WorkflowStatus.Completed).length}
                valueStyle={{ color: '#52c41a' }}
              />
              <span style={{ fontSize: 24 }}>✓</span>
            </div>
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic
                title="失败"
                value={workflows.filter((w: Workflow) => w.status === WorkflowStatus.Failed).length}
                valueStyle={{ color: '#ff4d4f' }}
              />
              <span style={{ fontSize: 24 }}>✗</span>
            </div>
          </Card>
        </Col>
      </Row>

      <ProCard
        style={{ marginTop: 16 }}
        title="工作流列表"
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
            <Button
              type="primary"
              icon={<PlusOutlined />}
              onClick={() => setCreateModalVisible(true)}
            >
              创建工作流
            </Button>
          </Space>
        }
      >
        <ProTable<Workflow>
          columns={columns}
          dataSource={workflows}
          loading={loading}
          rowKey="id"
          search={false}
          options={false}
          pagination={{
            pageSize: 10,
            showSizeChanger: true,
          }}
        />
      </ProCard>

      {/* 创建工作流弹窗 */}
      <ModalForm
        title="创建工作流"
        open={createModalVisible}
        onOpenChange={setCreateModalVisible}
        onFinish={handleSubmit}
        width={800}
        modalProps={{
          destroyOnClose: true,
        }}
      >
        <ProFormText
          name="name"
          label="工作流名称"
          rules={[{ required: true }]}
        />
        <ProFormTextArea
          name="description"
          label="描述"
          fieldProps={{ rows: 2 }}
        />
        <ProFormList
          name="steps"
          label="步骤"
          creatorButtonText="添加步骤"
          min={1}
          itemRender={({ listDom, action }, { index, record, ...rest }) => (
            <ProCard
              bordered
              header={`${record.name || '步骤'} (${index + 1})`}
              extra={action}
              style={{ marginBottom: 16 }}
            >
              {listDom}
            </ProCard>
          )}
        >
          <ProFormText name="id" label="步骤 ID" rules={[{ required: true }]} placeholder="step_1" />
          <ProFormText name="name" label="步骤名称" rules={[{ required: true }]} placeholder="数据采集" />
          <ProFormText name="script" label="脚本路径" rules={[{ required: true }]} placeholder="scripts/collect.lua" />
          <ProFormText name="workers" label="分配 Agent" fieldProps={{ placeholder: 'agent1,agent2' }} />
          <ProFormText name="dependsOn" label="依赖步骤" fieldProps={{ placeholder: 'step_1,step_2' }} />
          <ProFormText
            name="timeoutSeconds"
            label="超时时间(秒)"
            initialValue={300}
            fieldProps={{ type: 'number' }}
          />
        </ProFormList>
      </ModalForm>

      {/* 工作流详情抽屉 */}
      <Drawer
        title="工作流详情"
        width={720}
        open={drawerVisible}
        onClose={() => setDrawerVisible(false)}
      >
        {selectedWorkflow && (
          <Space direction="vertical" style={{ width: '100%' }} size="large">
            {/* 基本信息 */}
            <ProCard title="基本信息" headerBordered>
              <Descriptions column={2} size="small">
                <Descriptions.Item label="工作流 ID">
                  <Text copyable>{selectedWorkflow.id}</Text>
                </Descriptions.Item>
                <Descriptions.Item label="名称">
                  {selectedWorkflow.name}
                </Descriptions.Item>
                <Descriptions.Item label="描述" span={2}>
                  {selectedWorkflow.description || '-'}
                </Descriptions.Item>
                <Descriptions.Item label="状态">
                  <Tag color={getWorkflowStatusColor(selectedWorkflow.status)}>
                    {selectedWorkflow.status.toUpperCase()}
                  </Tag>
                </Descriptions.Item>
                <Descriptions.Item label="创建时间">
                  {new Date(selectedWorkflow.createdTime).toLocaleString()}
                </Descriptions.Item>
                {selectedWorkflow.startTime && (
                  <Descriptions.Item label="开始时间">
                    {new Date(selectedWorkflow.startTime).toLocaleString()}
                  </Descriptions.Item>
                )}
                {selectedWorkflow.endTime && (
                  <Descriptions.Item label="结束时间">
                    {new Date(selectedWorkflow.endTime).toLocaleString()}
                  </Descriptions.Item>
                )}
              </Descriptions>
            </ProCard>

            {/* 执行进度 */}
            <ProCard title="执行进度" headerBordered>
              <Steps
                current={selectedWorkflow.steps.findIndex(
                  (s) => selectedWorkflow.stepStatus?.[s.id] !== StepStatus.Completed
                )}
                direction="vertical"
              >
                {selectedWorkflow.steps.map((step) => {
                  const status = selectedWorkflow.stepStatus?.[step.id];
                  let stepState: 'wait' | 'process' | 'finish' | 'error' = 'wait';
                  if (status === StepStatus.Completed) stepState = 'finish';
                  else if (status === StepStatus.Running) stepState = 'process';
                  else if (status === StepStatus.Failed) stepState = 'error';

                  return (
                    <Steps.Step
                      key={step.id}
                      title={step.name}
                      description={
                        <Space>
                          <Text type="secondary">{step.id}</Text>
                          {status && (
                            <Tag color={getStepStatusColor(status)}>
                              {status.toUpperCase()}
                            </Tag>
                          )}
                        </Space>
                      }
                      status={stepState}
                    />
                  );
                })}
              </Steps>
            </ProCard>

            {/* 步骤详情 */}
            <ProCard title="步骤详情" headerBordered>
              <Space direction="vertical" style={{ width: '100%' }} size="middle">
                {selectedWorkflow.steps.map((step) => renderStepStatus(step, selectedWorkflow))}
              </Space>
            </ProCard>

            {/* 当前状态 */}
            {selectedWorkflow.status === WorkflowStatus.Running && (
              <Alert
                message="工作流正在运行中"
                description={`当前步骤: ${selectedWorkflow.currentStepId || '无'}`}
                type="info"
                showIcon
              />
            )}
          </Space>
        )}
      </Drawer>
    </PageContainer>
  );
};

export default Workflows;
