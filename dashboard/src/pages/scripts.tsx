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
    // TODO: 从 API 加载脚本列表
    setTimeout(() => {
      setScripts([
        {
          key: '1',
          name: 'hello_world.lua',
          path: 'C:\\\\scripts\\\\hello_world.lua',
          status: 'stopped',
          runCount: 5,
        },
        {
          key: '2',
          name: 'auto_loop.lua',
          path: 'C:\\\\scripts\\\\auto_loop.lua',
          status: 'running',
          lastRun: '2024-01-15 10:30:00',
          runCount: 120,
        },
        {
          key: '3',
          name: 'pixel_detection.lua',
          path: 'C:\\\\scripts\\\\pixel_detection.lua',
          status: 'stopped',
          runCount: 0,
        },
      ]);
      setLoading(false);
    }, 500);
  };

  const handleRun = (record: ScriptData) => {
    // TODO: 调用 API 运行脚本
    console.log('Run script:', record);
  };

  const handlePause = (record: ScriptData) => {
    // TODO: 调用 API 暂停脚本
    console.log('Pause script:', record);
  };

  const handleStop = (record: ScriptData) => {
    // TODO: 调用 API 停止脚本
    console.log('Stop script:', record);
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
      onOk: () => {
        setScripts(scripts.filter((s) => s.key !== key));
      },
    });
  };

  const handleAdd = () => {
    setEditingScript(null);
    form.resetFields();
    setModalVisible(true);
  };

  const handleModalOk = () => {
    form.validateFields().then((values) => {
      if (editingScript) {
        setScripts(
          scripts.map((s) =>
            s.key === editingScript.key ? { ...s, ...values } : s
          )
        );
      } else {
        const newScript: ScriptData = {
          ...values,
          key: Date.now().toString(),
          status: 'stopped',
          runCount: 0,
        };
        setScripts([...scripts, newScript]);
      }
      setModalVisible(false);
    });
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
