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
  type ProColumns,
  type ProFormInstance,
  ProDescriptions,
  ProFormText,
  ProFormTextArea,
  ProFormList,
  ProTable,
} from '@ant-design/pro-components';
import { useIntl, useRequest } from '@umijs/max';
import {
  Button,
  Card,
  Col,
  Descriptions,
  Row,
  Select,
  Space,
  Steps,
  Tag,
  Typography,
  message,
  Drawer,
  Alert,
  Badge,
  Statistic,
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
  getWorkflowTemplates,
  type WorkflowTemplate,
  submitWorkflow,
  cancelWorkflow,
  formatDuration,
  getWorkflowStatusColor,
  getStepStatusColor,
  normalizeWorkflow,
} from '@/services/wingman';
import wsService from '@/services/websocket';

const { Text, Title } = Typography;

function splitList(value: unknown): string[] {
  if (Array.isArray(value)) return value.map(String).map((item) => item.trim()).filter(Boolean);
  if (typeof value === 'string') return value.split(',').map((item) => item.trim()).filter(Boolean);
  return [];
}

const Workflows: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const actionRef = useRef();
  const createFormRef = useRef<ProFormInstance>();
  const [selectedWorkflow, setSelectedWorkflow] = useState<WorkflowInstance | null>(null);
  const [drawerVisible, setDrawerVisible] = useState(false);
  const [createModalVisible, setCreateModalVisible] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [workflows, setWorkflows] = useState<Workflow[]>([]); // 本地状态用于实时更新
  const [templates, setTemplates] = useState<WorkflowTemplate[]>([]);

  // 获取工作流列表
  const { data: workflowsData, loading, refresh } = useRequest(
    async (): Promise<Workflow[]> => {
      const response = await getWorkflows();
      return response.data || [];
    },
    {
      onSuccess: (data) => {
        setWorkflows(Array.isArray(data) ? (data as Workflow[]) : []);
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
      const nextWorkflow = normalizeWorkflow(data);
      setWorkflows((prev) => {
        const exists = prev.some((workflow) => workflow.id === nextWorkflow.id);
        return exists ? prev.map((workflow) => workflow.id === nextWorkflow.id ? nextWorkflow : workflow) : [nextWorkflow, ...prev];
      });
      message.success(intl.formatMessage({ id: 'pages.workflows.submitted' }, { name: String(data.name ?? '') }));
    }));

    // 工作流状态变化
    unsubscribes.push(wsService.onWorkflowStatusChanged((data) => {
      const workflowId = String(data.id ?? data.workflowId ?? '');
      setWorkflows((prev) =>
        prev.map((w) =>
          w.id === workflowId ? { ...w, status: String(data.status || w.status) as WorkflowStatus, endTime: Date.now() } : w
        )
      );

      // 更新详情面板中当前选中的工作流
      if (selectedWorkflow && selectedWorkflow.id === workflowId) {
        setSelectedWorkflow((prev) => prev ? { ...prev, status: String(data.status || prev.status) as WorkflowStatus, endTime: Date.now() } : null);
      }
    }));

    // 工作流进度更新
    unsubscribes.push(wsService.onWorkflowProgress((data) => {
      const workflowId = String(data.workflowId ?? data.id ?? '');
      // 更新详情面板
      if (selectedWorkflow && selectedWorkflow.id === workflowId) {
        setSelectedWorkflow((prev) => {
          if (!prev) return null;
          return {
            ...prev,
            stepStatus: { ...prev.stepStatus, [String(data.stepId)]: data.status as StepStatus },
            currentStepId: String(data.stepId || ''),
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

  // 加载内置模板目录
  useEffect(() => {
    getWorkflowTemplates()
      .then((items) => setTemplates(Array.isArray(items) ? items : []))
      .catch(() => {});
  }, []);

  // 从模板填充创建表单（workers/dependsOn 数组转逗号串以匹配表单字段）
  const applyTemplate = (templateId: string) => {
    const tpl = templates.find((t) => t.id === templateId);
    if (!tpl) return;
    createFormRef.current?.setFieldsValue({
      name: tpl.name,
      description: tpl.description || '',
      steps: (tpl.steps || []).map((s) => ({
        id: s.id,
        name: s.name || s.id,
        script: s.script || '',
        workers: (s.workers || []).join(','),
        dependsOn: (s.dependsOn || []).join(','),
        timeoutSeconds: s.timeoutSeconds ?? 300,
      })),
    });
    message.success(intl.formatMessage({ id: 'pages.workflows.templateLoaded' }, { name: tpl.name }));
  };

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
          workers: splitList(step.workers),
          dependsOn: splitList(step.dependsOn),
          timeoutSeconds: Number(step.timeoutSeconds) || 300,
          parameters: step.parameters || {},
        })),
        sharedContext: values.sharedContext || {},
      };
      await submitWorkflow(workflow);
      message.success(formatMessage('pages.workflows.submitSuccess'));
      setCreateModalVisible(false);
      refresh();
    } catch (error) {
      message.error(formatMessage('pages.workflows.submitFailed'));
    }
  };

  // 取消工作流
  const handleCancel = async (workflowId: string) => {
    try {
      await cancelWorkflow(workflowId);
      message.success(formatMessage('pages.workflows.cancelSuccess'));
      refresh();
      if (selectedWorkflow?.id === workflowId) {
        fetchDetail(workflowId);
      }
    } catch (error) {
      message.error(formatMessage('pages.workflows.cancelFailed'));
    }
  };

  const columns: ProColumns<Workflow>[] = [
    {
      title: formatMessage('pages.workflows.id'),
      dataIndex: 'id',
      key: 'id',
      width: 150,
      render: (_, record) => (
        <Space>
          <UnorderedListOutlined />
          <Text copyable={{ text: record.id }}>{record.id.slice(0, 8)}...</Text>
        </Space>
      ),
    },
    {
      title: formatMessage('pages.workflows.name'),
      dataIndex: 'name',
      key: 'name',
      width: 200,
    },
    {
      title: formatMessage('pages.workflows.description'),
      dataIndex: 'description',
      key: 'description',
      ellipsis: true,
    },
    {
      title: formatMessage('pages.common.status'),
      dataIndex: 'status',
      key: 'status',
      width: 100,
      render: (_, record) => (
        <Tag color={getWorkflowStatusColor(record.status)}>
          {record.status.toUpperCase()}
        </Tag>
      ),
    },
    {
      title: formatMessage('pages.workflows.stepCount'),
      dataIndex: 'steps',
      key: 'steps',
      width: 80,
      render: (_, record) => record.steps?.length || 0,
    },
    {
      title: formatMessage('pages.workflows.createdTime'),
      dataIndex: 'createdTime',
      key: 'createdTime',
      width: 150,
      render: (_, record) => new Date(record.createdTime).toLocaleString(),
    },
    {
      title: formatMessage('pages.workflows.duration'),
      key: 'duration',
      width: 100,
      render: (_: any, record: Workflow) => {
        if (!record.startTime) return '-';
        const end = record.endTime || Date.now();
        return formatDuration(end - record.startTime);
      },
    },
    {
      title: formatMessage('pages.common.action'),
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
              {intl.formatMessage({ id: 'pages.workflows.dependsOn' }, { names: step.dependsOn.join(', ') })}
            </Text>
          )}
        </Space>
      </div>
    );
  };

  return (
    <PageContainer
      header={{
        title: formatMessage('pages.workflows.title'),
        subTitle: formatMessage('pages.workflows.subtitle'),
        extra: wsConnected ? (
          <Badge status="processing" text={formatMessage('pages.workflows.realtimeConnected')} />
        ) : (
          <Space>
            <Badge status="default" text={formatMessage('pages.workflows.pollingMode')} />
            <Button size="small" onClick={() => wsService.reconnect()}>
              {formatMessage('pages.workflows.reconnect')}
            </Button>
          </Space>
        ),
      }}
    >
      <Row gutter={[16, 16]}>
        {/* 统计卡片 */}
        <Col xs={24} sm={6}>
          <Card>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic title={formatMessage('pages.common.total')} value={workflows.length} />
              <UnorderedListOutlined style={{ fontSize: 24, color: '#999' }} />
            </div>
          </Card>
        </Col>
        <Col xs={24} sm={6}>
          <Card>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <Statistic
                title={formatMessage('pages.workflows.running')}
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
                title={formatMessage('pages.workflows.completed')}
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
                title={formatMessage('pages.common.failure')}
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
        title={formatMessage('pages.workflows.list')}
        extra={
          <Space>
            {wsConnected && (
              <Tag icon={<WifiOutlined />} color="success">
                {formatMessage('pages.workflows.realtimeUpdate')}
              </Tag>
            )}
            <Button
              icon={<ReloadOutlined />}
              onClick={refresh}
              loading={loading}
            >
              {formatMessage('pages.common.refresh')}
            </Button>
            <Button
              type="primary"
              icon={<PlusOutlined />}
              onClick={() => setCreateModalVisible(true)}
            >
              {formatMessage('pages.workflows.create')}
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
        title={formatMessage('pages.workflows.create')}
        formRef={createFormRef as any}
        open={createModalVisible}
        onOpenChange={setCreateModalVisible}
        onFinish={handleSubmit}
        width={800}
        modalProps={{
          destroyOnClose: true,
        }}
      >
        {templates.length > 0 && (
          <div style={{ marginBottom: 16 }}>
            <Select
              style={{ width: '100%' }}
              placeholder={formatMessage('pages.workflows.loadFromTemplate')}
              allowClear
              onChange={(val) => val && applyTemplate(val)}
              options={templates.map((t) => ({
                label: `${t.name}${t.category ? ` [${t.category}]` : ''}`,
                value: t.id,
              }))}
            />
          </div>
        )}
        <ProFormText
          name="name"
          label={formatMessage('pages.workflows.workflowName')}
          rules={[{ required: true }]}
        />
        <ProFormTextArea
          name="description"
          label={formatMessage('pages.workflows.description')}
          fieldProps={{ rows: 2 }}
        />
        <ProFormList
          name="steps"
          label={formatMessage('pages.workflows.steps')}
          creatorButtonProps={{ creatorButtonText: formatMessage('pages.workflows.addStep') }}
          min={1}
          itemRender={({ listDom, action }, { index, record, ...rest }) => (
            <ProCard
              bordered
              title={`${record.name || formatMessage('pages.workflows.stepFallback')} (${index + 1})`}
              extra={action}
              style={{ marginBottom: 16 }}
            >
              {listDom}
            </ProCard>
          )}
        >
          <ProFormText name="id" label={formatMessage('pages.workflows.stepId')} rules={[{ required: true }]} placeholder="step_1" />
          <ProFormText name="name" label={formatMessage('pages.workflows.stepName')} rules={[{ required: true }]} placeholder="数据采集" />
          <ProFormText name="script" label={formatMessage('pages.workflows.scriptPath')} rules={[{ required: true }]} placeholder="scripts/collect.lua" />
          <ProFormText name="workers" label={formatMessage('pages.workflows.assignAgents')} fieldProps={{ placeholder: 'agent1,agent2' }} />
          <ProFormText name="dependsOn" label={formatMessage('pages.workflows.dependsOnSteps')} fieldProps={{ placeholder: 'step_1,step_2' }} />
          <ProFormText
            name="timeoutSeconds"
            label={formatMessage('pages.workflows.timeoutSeconds')}
            initialValue={300}
            fieldProps={{ type: 'number' }}
          />
        </ProFormList>
      </ModalForm>

      {/* 工作流详情抽屉 */}
      <Drawer
        title={formatMessage('pages.workflows.detailTitle')}
        width={720}
        open={drawerVisible}
        onClose={() => setDrawerVisible(false)}
      >
        {selectedWorkflow && (
          <Space direction="vertical" style={{ width: '100%' }} size="large">
            {/* 基本信息 */}
            <ProCard title={formatMessage('pages.workflows.basicInfo')} headerBordered>
              <Descriptions column={2} size="small">
                <Descriptions.Item label={formatMessage('pages.workflows.id')}>
                  <Text copyable>{selectedWorkflow.id}</Text>
                </Descriptions.Item>
                <Descriptions.Item label={formatMessage('pages.workflows.name')}>
                  {selectedWorkflow.name}
                </Descriptions.Item>
                <Descriptions.Item label={formatMessage('pages.workflows.description')} span={2}>
                  {selectedWorkflow.description || '-'}
                </Descriptions.Item>
                <Descriptions.Item label={formatMessage('pages.common.status')}>
                  <Tag color={getWorkflowStatusColor(selectedWorkflow.status)}>
                    {selectedWorkflow.status.toUpperCase()}
                  </Tag>
                </Descriptions.Item>
                <Descriptions.Item label={formatMessage('pages.workflows.createdTime')}>
                  {new Date(selectedWorkflow.createdTime).toLocaleString()}
                </Descriptions.Item>
                {selectedWorkflow.startTime && (
                  <Descriptions.Item label={formatMessage('pages.workflows.startTimeLabel')}>
                    {new Date(selectedWorkflow.startTime).toLocaleString()}
                  </Descriptions.Item>
                )}
                {selectedWorkflow.endTime && (
                  <Descriptions.Item label={formatMessage('pages.workflows.endTimeLabel')}>
                    {new Date(selectedWorkflow.endTime).toLocaleString()}
                  </Descriptions.Item>
                )}
              </Descriptions>
            </ProCard>

            {/* 执行进度 */}
            <ProCard title={formatMessage('pages.workflows.progress')} headerBordered>
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
            <ProCard title={formatMessage('pages.workflows.stepDetails')} headerBordered>
              <Space direction="vertical" style={{ width: '100%' }} size="middle">
                {selectedWorkflow.steps.map((step) => renderStepStatus(step, selectedWorkflow))}
              </Space>
            </ProCard>

            {/* 当前状态 */}
            {selectedWorkflow.status === WorkflowStatus.Running && (
              <Alert
                message={formatMessage('pages.workflows.runningNow')}
                description={intl.formatMessage(
                  { id: 'pages.workflows.currentStep' },
                  { step: selectedWorkflow.currentStepId || formatMessage('pages.common.none') },
                )}
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
