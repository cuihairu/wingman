import { Card, Table, Button, Space, Tag, Input, Row, Col } from 'antd';
import { ReloadOutlined, EyeOutlined } from '@ant-design/icons';
import { useState, useEffect } from 'react';
import type { ColumnsType } from 'antd/es/table';

interface WindowData {
  key: string;
  handle: number;
  title: string;
  bounds: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
  isForeground: boolean;
}

export default function WindowsPage() {
  const [windows, setWindows] = useState<WindowData[]>([]);
  const [loading, setLoading] = useState(false);
  const [searchText, setSearchText] = useState('');

  useEffect(() => {
    loadWindows();
    // 定时刷新
    const interval = setInterval(loadWindows, 2000);
    return () => clearInterval(interval);
  }, []);

  const loadWindows = async () => {
    setLoading(true);
    // TODO: 从 API 加载窗口列表
    setTimeout(() => {
      setWindows([
        {
          key: '1',
          handle: 123456,
          title: 'Wingman Dashboard - Chrome',
          bounds: { x: 100, y: 100, width: 1920, height: 1080 },
          isForeground: true,
        },
        {
          key: '2',
          handle: 234567,
          title: 'Visual Studio Code',
          bounds: { x: 0, y: 0, width: 1600, height: 900 },
          isForeground: false,
        },
        {
          key: '3',
          handle: 345678,
          title: 'Notepad',
          bounds: { x: 500, y: 300, width: 800, height: 600 },
          isForeground: false,
        },
      ]);
      setLoading(false);
    }, 500);
  };

  const handleActivate = (record: WindowData) => {
    // TODO: 调用 API 激活窗口
    console.log('Activate window:', record);
  };

  const filteredWindows = windows.filter((w) =>
    w.title.toLowerCase().includes(searchText.toLowerCase())
  );

  const columns: ColumnsType<WindowData> = [
    {
      title: '窗口标题',
      dataIndex: 'title',
      key: 'title',
      ellipsis: true,
    },
    {
      title: '句柄',
      dataIndex: 'handle',
      key: 'handle',
      width: 100,
    },
    {
      title: '位置',
      key: 'position',
      render: (_, record) =>
        `${record.bounds.x}, ${record.bounds.y}`,
    },
    {
      title: '大小',
      key: 'size',
      render: (_, record) =>
        `${record.bounds.width} x ${record.bounds.height}`,
    },
    {
      title: '状态',
      dataIndex: 'isForeground',
      key: 'isForeground',
      render: (isForeground: boolean) =>
        isForeground ? (
          <Tag color="blue">前台</Tag>
        ) : (
          <Tag>后台</Tag>
        ),
    },
    {
      title: '操作',
      key: 'action',
      width: 120,
      render: (_, record) => (
        <Space size="small">
          <Button
            type="link"
            size="small"
            icon={<EyeOutlined />}
            onClick={() => handleActivate(record)}
            disabled={record.isForeground}
          >
            激活
          </Button>
        </Space>
      ),
    },
  ];

  return (
    <div>
      <Card title="窗口监控">
        <Row gutter={[16, 16]}>
          <Col span={18}>
            <Input.Search
              placeholder="搜索窗口标题"
              allowClear
              onChange={(e) => setSearchText(e.target.value)}
              style={{ width: '100%' }}
            />
          </Col>
          <Col span={6}>
            <Button
              icon={<ReloadOutlined />}
              onClick={loadWindows}
              loading={loading}
              block
            >
              刷新
            </Button>
          </Col>
        </Row>

        <Table
          style={{ marginTop: 16 }}
          columns={columns}
          dataSource={filteredWindows}
          loading={loading}
          pagination={{ pageSize: 20, showSizeChanger: false }}
          size="small"
        />
      </Card>
    </div>
  );
}
