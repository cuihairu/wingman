import {
  DeleteOutlined,
  EditOutlined,
  FileTextOutlined,
  PlayCircleOutlined,
  PlusOutlined,
  ReloadOutlined,
  SaveOutlined,
  StopOutlined,
  CodeOutlined,
  ConsoleSqlOutlined,
} from '@ant-design/icons';
import {
  ModalForm,
  PageContainer,
  ProCard,
  type ProColumns,
  ProFormText,
  ProFormTextArea,
  ProTable,
} from '@ant-design/pro-components';
import { useIntl, useRequest } from '@umijs/max';
import {
  Button,
  Space,
  Typography,
  message,
  Modal,
  Row,
  Col,
  Tag,
} from 'antd';
import React, { useRef, useState, useEffect, useCallback } from 'react';
import { CodeEditor } from '@/components/MonacoDynamic';
import {
  ScriptInfo,
  ScriptLog,
  getScripts,
  getScriptContent,
  saveScriptContent,
  createScript,
  deleteScript,
  runScript,
  stopScript,
  getScriptLogs,
} from '@/services/wingman';
import styles from './index.less';

const { Text } = Typography;

function executionIdFor(script: ScriptInfo): string {
  return script.executionId || script.name.replace(/\.lua$/i, '');
}

const Scripts: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const actionRef = useRef();
  const [selectedScript, setSelectedScript] = useState<ScriptInfo | null>(null);
  const [editorVisible, setEditorVisible] = useState(false);
  const [scriptContent, setScriptContent] = useState('');
  const [logs, setLogs] = useState<ScriptLog[]>([]);
  const [runningExecutions, setRunningExecutions] = useState<Record<string, string>>({});
  const [createModalVisible, setCreateModalVisible] = useState(false);

  // 获取脚本列表
  const { data: scriptsData, loading, refresh } = useRequest(
    async () => {
      const response = await getScripts();
      return response.data || [];
    },
    {
      pollingInterval: 5000,
    },
  );

  const scripts: ScriptInfo[] = Array.isArray(scriptsData) ? (scriptsData as ScriptInfo[]) : [];

  const activeExecutionId = selectedScript ? runningExecutions[selectedScript.path] || executionIdFor(selectedScript) : '';

  const refreshLogs = useCallback(async (script?: ScriptInfo | null) => {
    const target = script || selectedScript;
    if (!target) {
      setLogs([]);
      return;
    }

    try {
      const response = await getScriptLogs(runningExecutions[target.path] || executionIdFor(target), 0, 200);
      setLogs(response.data || []);
    } catch (error) {
      setLogs([]);
    }
  }, [runningExecutions, selectedScript]);

  // 加载脚本内容
  const loadScriptContent = useCallback(async (script: ScriptInfo) => {
    try {
      const response = await getScriptContent(script.path);
      setScriptContent(response.data || '');
    } catch (error) {
      message.error(formatMessage('pages.scripts.loadFailed'));
      // 设置默认内容
      setScriptContent(`-- ${script.name}\n-- ${script.description || ''}\n\nfunction main()\n    print("Hello, Wingman!")\nend\n\nmain()\n`);
    }
  }, []);

  // 保存脚本
  const handleSaveScript = async () => {
    if (!selectedScript) return;

    try {
      await saveScriptContent(selectedScript.path, scriptContent);
      message.success(formatMessage('pages.scripts.saved'));
      refresh();
    } catch (error) {
      message.error(formatMessage('pages.scripts.saveFailed'));
    }
  };

  // 运行脚本
  const handleRunScript = async (script: ScriptInfo) => {
    try {
      const response = await runScript(script.path);
      const executionId = response.data?.executionId || executionIdFor(script);
      setRunningExecutions((previous) => ({ ...previous, [script.path]: executionId }));
      message.success(intl.formatMessage({ id: 'pages.scripts.started' }, { name: script.name }));
      refresh();
      if (selectedScript?.path === script.path) {
        await refreshLogs({ ...script, executionId });
      }
    } catch (error) {
      message.error(formatMessage('pages.scripts.startFailed'));
    }
  };

  // 停止脚本
  const handleStopScript = async (script: ScriptInfo) => {
    try {
      const executionId = runningExecutions[script.path] || executionIdFor(script);
      await stopScript(executionId);
      setRunningExecutions((previous) => {
        const next = { ...previous };
        delete next[script.path];
        return next;
      });
      message.success(intl.formatMessage({ id: 'pages.scripts.stopped' }, { name: script.name }));
      refresh();
      if (selectedScript?.path === script.path) {
        await refreshLogs(script);
      }
    } catch (error) {
      message.error(formatMessage('pages.scripts.stopFailed'));
    }
  };

  // 删除脚本
  const handleDeleteScript = async (script: ScriptInfo) => {
    Modal.confirm({
      title: formatMessage('pages.scripts.deleteConfirmTitle'),
      content: intl.formatMessage({ id: 'pages.scripts.deleteConfirmContent' }, { name: script.name }),
      onOk: async () => {
        try {
          await deleteScript(script.path);
          message.success(formatMessage('pages.scripts.deleted'));
          refresh();
        } catch (error) {
          message.error(formatMessage('pages.scripts.deleteFailed'));
        }
      },
    });
  };

  // 打开编辑器
  const handleEditScript = async (script: ScriptInfo) => {
    setSelectedScript(script);
    await loadScriptContent(script);
    await refreshLogs(script);
    setEditorVisible(true);
  };

  useEffect(() => {
    if (!editorVisible || !selectedScript) return;
    const timer = window.setInterval(() => {
      refreshLogs(selectedScript);
    }, 3000);
    return () => window.clearInterval(timer);
  }, [editorVisible, refreshLogs, selectedScript]);

  // 表格列定义
  const columns: ProColumns<ScriptInfo>[] = [
    {
      title: formatMessage('pages.scripts.name'),
      dataIndex: 'name',
      key: 'name',
      render: (_, record) => (
        <Space>
          <FileTextOutlined />
          <Text strong>{record.name}</Text>
          {record.isRunning && <Tag color="green">{formatMessage('pages.scripts.running')}</Tag>}
        </Space>
      ),
    },
    {
      title: formatMessage('pages.scripts.description'),
      dataIndex: 'description',
      key: 'description',
      ellipsis: true,
      render: (_, record) => <Text type="secondary">{record.description || '-'}</Text>,
    },
    {
      title: formatMessage('pages.scripts.size'),
      dataIndex: 'size',
      key: 'size',
      width: 100,
      render: (_, record) => <Text type="secondary">{record.size ? `${record.size} B` : '-'}</Text>,
    },
    {
      title: formatMessage('pages.scripts.modifiedTime'),
      dataIndex: 'modifiedTime',
      key: 'modifiedTime',
      width: 180,
      render: (_, record) => (
        <Text type="secondary">{record.modifiedTime ? new Date(record.modifiedTime).toLocaleString() : '-'}</Text>
      ),
    },
    {
      title: formatMessage('pages.common.action'),
      key: 'action',
      width: 200,
      render: (_: any, record: ScriptInfo) => (
        <Space>
          {record.isRunning ? (
            <Button
              type="primary"
              danger
              size="small"
              icon={<StopOutlined />}
              onClick={() => handleStopScript(record)}
            >
              {formatMessage('pages.scripts.stop')}
            </Button>
          ) : (
            <Button
              type="primary"
              size="small"
              icon={<PlayCircleOutlined />}
              onClick={() => handleRunScript(record)}
            >
              {formatMessage('pages.scripts.run')}
            </Button>
          )}
          <Button
            size="small"
            icon={<EditOutlined />}
            onClick={() => handleEditScript(record)}
          >
            {formatMessage('pages.scripts.edit')}
          </Button>
          <Button
            size="small"
            danger
            icon={<DeleteOutlined />}
            onClick={() => handleDeleteScript(record)}
          />
        </Space>
      ),
    },
  ];

  return (
    <PageContainer
      header={{
        title: formatMessage('pages.scripts.title'),
        breadcrumb: {},
      }}
      extra={[
        <Button
          key="refresh"
          icon={<ReloadOutlined />}
          onClick={refresh}
        >
          {formatMessage('pages.common.refresh')}
        </Button>,
        <Button
          key="create"
          type="primary"
          icon={<PlusOutlined />}
          onClick={() => setCreateModalVisible(true)}
        >
          {formatMessage('pages.scripts.create')}
        </Button>,
      ]}
    >
      <Row gutter={[16, 16]}>
        {/* 脚本列表 */}
        <Col xs={24} lg={editorVisible ? 8 : 24}>
          <ProCard
            title={formatMessage('pages.scripts.list')}
            headerBordered
            extra={
              <Space>
                <Text type="secondary">
                  {intl.formatMessage({ id: 'pages.scripts.totalCount' }, { count: scripts.length })}
                </Text>
              </Space>
            }
          >
            <ProTable<ScriptInfo>
              columns={columns}
              dataSource={scripts}
              loading={loading}
              rowKey="id"
              pagination={{
                pageSize: 20,
                showSizeChanger: true,
              }}
              search={false}
              options={false}
              toolBarRender={false}
              actionRef={actionRef}
            />
          </ProCard>
        </Col>

        {editorVisible && (
          <Col xs={24} lg={16}>
            <Space direction="vertical" size={16} style={{ width: '100%' }}>
              {/* 脚本编辑器 */}
              <ProCard
                title={
                  <Space>
                    <CodeOutlined />
                    <span>
                      {intl.formatMessage({ id: 'pages.scripts.editTitle' }, { name: selectedScript?.name || '' })}
                    </span>
                  </Space>
                }
                headerBordered
                extra={
                  <Space>
                    <Button onClick={() => setEditorVisible(false)}>
                      {formatMessage('pages.common.close')}
                    </Button>
                    <Button
                      type="primary"
                      icon={<SaveOutlined />}
                      onClick={handleSaveScript}
                    >
                      {formatMessage('pages.common.save')}
                    </Button>
                  </Space>
                }
                className={styles.editorCard}
              >
                <CodeEditor
                  value={scriptContent}
                  language="lua"
                  height={520}
                  theme="vs-dark"
                  onChange={setScriptContent}
                  options={{
                    fontSize: 14,
                    lineNumbers: 'on',
                    scrollBeyondLastLine: false,
                    tabSize: 2,
                  }}
                />
              </ProCard>

              {/* 执行日志 */}
              <ProCard
                title={
                  <Space>
                    <ConsoleSqlOutlined />
                    <span>{formatMessage('pages.scripts.executionLogs')}</span>
                  </Space>
                }
                headerBordered
                className={styles.logCard}
                extra={
                  <Space>
                    {activeExecutionId && <Tag>{activeExecutionId}</Tag>}
                    <Button size="small" onClick={() => refreshLogs(selectedScript)}>
                      {formatMessage('pages.common.refresh')}
                    </Button>
                  </Space>
                }
              >
                <div className={styles.logContainer}>
                  {logs.length === 0 ? (
                    <Text type="secondary">{formatMessage('pages.scripts.noLogs')}</Text>
                  ) : (
                    logs.map((log, index) => (
                      <div key={index} className={styles.logLine}>
                        <Text type="secondary">
                          {new Date(log.timestamp).toLocaleTimeString()}
                        </Text>
                        <Tag
                          color={
                            log.level === 'error'
                              ? 'red'
                              : log.level === 'warn'
                                ? 'orange'
                                : 'blue'
                          }
                        >
                          {log.level}
                        </Tag>
                        <Text>{log.message}</Text>
                      </div>
                    ))
                  )}
                </div>
              </ProCard>
            </Space>
          </Col>
        )}
      </Row>

      {/* 创建脚本对话框 */}
      <ModalForm
        title={formatMessage('pages.scripts.create')}
        open={createModalVisible}
        onOpenChange={setCreateModalVisible}
        onFinish={async (values) => {
          try {
            await createScript(values.name, values.description);
            message.success(formatMessage('pages.scripts.created'));
            setCreateModalVisible(false);
            refresh();
          } catch (error) {
            message.error(formatMessage('pages.scripts.createFailed'));
          }
        }}
      >
        <ProFormText
          name="name"
          label={formatMessage('pages.scripts.name')}
          placeholder={formatMessage('pages.scripts.namePlaceholder')}
          rules={[
            { required: true },
            { pattern: /\.lua$/, message: formatMessage('pages.scripts.nameRule') },
          ]}
        />
        <ProFormTextArea
          name="description"
          label={formatMessage('pages.scripts.description')}
          placeholder={formatMessage('pages.scripts.descriptionPlaceholder')}
        />
      </ModalForm>
    </PageContainer>
  );
};

export default Scripts;
