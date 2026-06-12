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
import { useRequest } from '@umijs/max';
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
      message.error('加载脚本失败');
      // 设置默认内容
      setScriptContent(`-- ${script.name}\n-- ${script.description || ''}\n\nfunction main()\n    print("Hello, Wingman!")\nend\n\nmain()\n`);
    }
  }, []);

  // 保存脚本
  const handleSaveScript = async () => {
    if (!selectedScript) return;

    try {
      await saveScriptContent(selectedScript.path, scriptContent);
      message.success('脚本已保存');
      refresh();
    } catch (error) {
      message.error('保存失败');
    }
  };

  // 运行脚本
  const handleRunScript = async (script: ScriptInfo) => {
    try {
      const response = await runScript(script.path);
      const executionId = response.data?.executionId || executionIdFor(script);
      setRunningExecutions((previous) => ({ ...previous, [script.path]: executionId }));
      message.success(`脚本 ${script.name} 已启动`);
      refresh();
      if (selectedScript?.path === script.path) {
        await refreshLogs({ ...script, executionId });
      }
    } catch (error) {
      message.error('启动失败');
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
      message.success(`脚本 ${script.name} 已停止`);
      refresh();
      if (selectedScript?.path === script.path) {
        await refreshLogs(script);
      }
    } catch (error) {
      message.error('停止失败');
    }
  };

  // 删除脚本
  const handleDeleteScript = async (script: ScriptInfo) => {
    Modal.confirm({
      title: '确认删除',
      content: `确定要删除脚本 "${script.name}" 吗？`,
      onOk: async () => {
        try {
          await deleteScript(script.path);
          message.success('删除成功');
          refresh();
        } catch (error) {
          message.error('删除失败');
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
      title: '脚本名称',
      dataIndex: 'name',
      key: 'name',
      render: (_, record) => (
        <Space>
          <FileTextOutlined />
          <Text strong>{record.name}</Text>
          {record.isRunning && <Tag color="green">运行中</Tag>}
        </Space>
      ),
    },
    {
      title: '描述',
      dataIndex: 'description',
      key: 'description',
      ellipsis: true,
      render: (_, record) => <Text type="secondary">{record.description || '-'}</Text>,
    },
    {
      title: '大小',
      dataIndex: 'size',
      key: 'size',
      width: 100,
      render: (_, record) => <Text type="secondary">{record.size ? `${record.size} B` : '-'}</Text>,
    },
    {
      title: '修改时间',
      dataIndex: 'modifiedTime',
      key: 'modifiedTime',
      width: 180,
      render: (_, record) => (
        <Text type="secondary">{record.modifiedTime ? new Date(record.modifiedTime).toLocaleString() : '-'}</Text>
      ),
    },
    {
      title: '操作',
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
              停止
            </Button>
          ) : (
            <Button
              type="primary"
              size="small"
              icon={<PlayCircleOutlined />}
              onClick={() => handleRunScript(record)}
            >
              运行
            </Button>
          )}
          <Button
            size="small"
            icon={<EditOutlined />}
            onClick={() => handleEditScript(record)}
          >
            编辑
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
        title: '脚本管理',
        breadcrumb: {},
      }}
      extra={[
        <Button
          key="refresh"
          icon={<ReloadOutlined />}
          onClick={refresh}
        >
          刷新
        </Button>,
        <Button
          key="create"
          type="primary"
          icon={<PlusOutlined />}
          onClick={() => setCreateModalVisible(true)}
        >
          新建脚本
        </Button>,
      ]}
    >
      <Row gutter={[16, 16]}>
        {/* 脚本列表 */}
        <Col xs={24} lg={editorVisible ? 8 : 24}>
          <ProCard
            title="脚本列表"
            headerBordered
            extra={
              <Space>
                <Text type="secondary">共 {scripts.length} 个脚本</Text>
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
                    <span>编辑: {selectedScript?.name}</span>
                  </Space>
                }
                headerBordered
                extra={
                  <Space>
                    <Button onClick={() => setEditorVisible(false)}>
                      关闭
                    </Button>
                    <Button
                      type="primary"
                      icon={<SaveOutlined />}
                      onClick={handleSaveScript}
                    >
                      保存
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
                    <span>执行日志</span>
                  </Space>
                }
                headerBordered
                className={styles.logCard}
                extra={
                  <Space>
                    {activeExecutionId && <Tag>{activeExecutionId}</Tag>}
                    <Button size="small" onClick={() => refreshLogs(selectedScript)}>
                      刷新
                    </Button>
                  </Space>
                }
              >
                <div className={styles.logContainer}>
                  {logs.length === 0 ? (
                    <Text type="secondary">暂无日志</Text>
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
        title="新建脚本"
        open={createModalVisible}
        onOpenChange={setCreateModalVisible}
        onFinish={async (values) => {
          try {
            await createScript(values.name, values.description);
            message.success('创建成功');
            setCreateModalVisible(false);
            refresh();
          } catch (error) {
            message.error('创建失败');
          }
        }}
      >
        <ProFormText
          name="name"
          label="脚本名称"
          placeholder="请输入脚本名称"
          rules={[
            { required: true },
            { pattern: /\.lua$/, message: '脚本名称必须以 .lua 结尾' },
          ]}
        />
        <ProFormTextArea
          name="description"
          label="描述"
          placeholder="请输入脚本描述"
        />
      </ModalForm>
    </PageContainer>
  );
};

export default Scripts;
