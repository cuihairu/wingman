import {
  Card,
  Table,
  Button,
  Space,
  Tag,
  Modal,
  Form,
  Input,
  Select,
} from 'antd';
import {
  PlayCircleOutlined,
  PauseCircleOutlined,
  StopOutlined,
  EditOutlined,
  DeleteOutlined,
  PlusOutlined,
} from '@ant-design/icons';
import { useState, useEffect } from 'react';
import type { ColumnsType } from 'antd/es/table';

interface ScriptData {
  key: string;
  name: string;
  path: string;
  status: 'stopped' | 'running' | 'paused';
  lastRun?: string;
  runCount: number;
}

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

const getAuthToken = () => localStorage.getItem('wingman_token') || '';

export default function ScriptsPage() {
  const [scripts, setScripts] = useState<ScriptData[]>([]);
  const [loading, setLoading] = useState(false);
  const [modalVisible, setModalVisible] = useState(false);
  const [editingScript, setEditingScript] = useState<ScriptData | null>(null);
  const [form] = Form.useForm();

  useEffect(() => {
    loadScripts();
  }, []);

  const loadScripts = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/scripts', {
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
        },
      });

      if (response.status === 401) {
        localStorage.removeItem('wingman_token');
        window.location.href = '/login';
        return;
      }

      const data: ApiResponse<ScriptData[]> = await response.json();
      if (data.success && data.data) {
        setScripts(data.data);
      }
    } catch (error) {
      console.error('Failed to load scripts:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleRun = async (record: ScriptData) => {
    try {
      const response = await fetch('/api/scripts', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ name: record.name }),
      });
      await loadScripts();
    } catch (error) {
      console.error('Failed to run script:', error);
    }
  };

  const handlePause = async (record: ScriptData) => {
    try {
      const response = await fetch(`/api/scripts/${record.key}/pause`, {
        method: 'PUT',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
        },
      });
      await loadScripts();
    } catch (error) {
      console.error('Failed to pause script:', error);
    }
  };

  const handleStop = async (record: ScriptData) => {
    try {
      const response = await fetch(`/api/scripts/${record.key}`, {
        method: 'DELETE',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
        },
      });
      await loadScripts();
    } catch (error) {
      console.error('Failed to stop script:', error);
    }
  };

  const handleEdit = (record: ScriptData) => {
    setEditingScript(record);
    form.setFieldsValue(record);
    setModalVisible(true);
  };

  const handleDelete = (key: string) => {
    Modal.confirm({
      title: '确认删除',
      content: '确定要删除这个脚本吗？',
      onOk: async () => {
        try {
          await fetch(`/api/scripts/${key}`, {
            method: 'DELETE',
            headers: {
              'Authorization': `Bearer ${getAuthToken()}`,
            },
          });
          await loadScripts();
        } catch (error) {
          console.error('Failed to delete script:', error);
        }
      },
    });
  };

  const handleAdd = () => {
    setEditingScript(null);
    form.resetFields();
    setModalVisible(true);
  };

  const handleModalOk = async () => {
    try {
      const values = await form.validateFields();
      const response = await fetch('/api/scripts', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(values),
      });
      setModalVisible(false);
      await loadScripts();
    } catch (error) {
      console.error('Failed to save script:', error);
    }
  };

  const columns: ColumnsType<ScriptData> = [
    {
      title: '脚本名称',
      dataIndex: 'name',
      key: 'name',
    },
    {
      title: '路径',
      dataIndex: 'path',
      key: 'path',
      ellipsis: true,
    },
    {
      title: '状态',
      dataIndex: 'status',
      key: 'status',
      render: (status: string) => {
        const statusConfig: Record<string, { text: string; color: string }> = {
          stopped: { text: '已停止', color: 'default' },
          running: { text: '运行中', color: 'processing' },
          paused: { text: '已暂停', color: 'warning' },
        };
        const config = statusConfig[status] || statusConfig.stopped;
        return <Tag color={config.color}>{config.text}</Tag>;
      },
    },
    {
      title: '运行次数',
      dataIndex: 'runCount',
      key: 'runCount',
    },
    {
      title: '最后运行',
      dataIndex: 'lastRun',
      key: 'lastRun',
    },
    {
      title: '操作',
      key: 'action',
      render: (_, record) => (
        <Space size="small">
          {record.status === 'running' ? (
            <>
              <Button
                type="text"
                icon={<PauseCircleOutlined />}
                onClick={() => handlePause(record)}
              />
              <Button
                type="text"
                danger
                icon={<StopOutlined />}
                onClick={() => handleStop(record)}
              />
            </>
          ) : (
            <Button
              type="text"
              icon={<PlayCircleOutlined />}
              onClick={() => handleRun(record)}
            />
          )}
          <Button
            type="text"
            icon={<EditOutlined />}
            onClick={() => handleEdit(record)}
          />
          <Button
            type="text"
            danger
            icon={<DeleteOutlined />}
            onClick={() => handleDelete(record.key)}
          />
        </Space>
      ),
    },
  ];

  return (
    <div>
      <Card
        title="脚本管理"
        extra={
          <Button type="primary" icon={<PlusOutlined />} onClick={handleAdd}>
            添加脚本
          </Button>
        }
      >
        <Table
          columns={columns}
          dataSource={scripts}
          loading={loading}
          pagination={{ pageSize: 10 }}
        />
      </Card>

      <Modal
        title={editingScript ? '编辑脚本' : '添加脚本'}
        open={modalVisible}
        onOk={handleModalOk}
        onCancel={() => setModalVisible(false)}
      >
        <Form form={form} layout="vertical">
          <Form.Item
            label="脚本名称"
            name="name"
            rules={[{ required: true, message: '请输入脚本名称' }]}
          >
            <Input placeholder="example.lua" />
          </Form.Item>
          <Form.Item
            label="脚本路径"
            name="path"
            rules={[{ required: true, message: '请输入脚本路径' }]}
          >
            <Input placeholder="C:\\scripts\\example.lua" />
          </Form.Item>
        </Form>
      </Modal>
    </div>
  );
}
