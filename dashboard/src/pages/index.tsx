import { Card, Row, Col, Statistic, Progress, Button, Space } from 'antd';
import {
  PlayCircleOutlined,
  PauseCircleOutlined,
  StopOutlined,
} from '@ant-design/icons';
import { useEffect, useState } from 'react';
import './index.less';

interface DashboardStats {
  totalScripts: number;
  runningScripts: number;
  totalWindows: number;
  cpuUsage: number;
  memoryUsage: number;
}

export default function IndexPage() {
  const [stats, setStats] = useState<DashboardStats>({
    totalScripts: 0,
    runningScripts: 0,
    totalWindows: 0,
    cpuUsage: 0,
    memoryUsage: 0,
  });
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    // TODO: 从 API 获取统计数据
    setStats({
      totalScripts: 5,
      runningScripts: 2,
      totalWindows: 12,
      cpuUsage: 15,
      memoryUsage: 256,
    });
  }, []);

  const handleStartAll = async () => {
    setLoading(true);
    // TODO: 调用 API 启动所有脚本
    setTimeout(() => setLoading(false), 1000);
  };

  const handleStopAll = async () => {
    setLoading(true);
    // TODO: 调用 API 停止所有脚本
    setTimeout(() => setLoading(false), 1000);
  };

  return (
    <div className="home-page">
      <Row gutter={[16, 16]}>
        <Col span={6}>
          <Card>
            <Statistic title="总脚本数" value={stats.totalScripts} suffix="个" />
          </Card>
        </Col>
        <Col span={6}>
          <Card>
            <Statistic
              title="运行中"
              value={stats.runningScripts}
              suffix="个"
              valueStyle={{ color: '#3f8600' }}
            />
          </Card>
        </Col>
        <Col span={6}>
          <Card>
            <Statistic title="窗口数量" value={stats.totalWindows} suffix="个" />
          </Card>
        </Col>
        <Col span={6}>
          <Card>
            <Statistic title="内存使用" value={stats.memoryUsage} suffix="MB" />
          </Card>
        </Col>
      </Row>

      <Card title="系统状态" style={{ marginTop: 16 }}>
        <Row gutter={[16, 16]}>
          <Col span={12}>
            <div className="status-item">
              <span>CPU 使用率</span>
              <Progress percent={stats.cpuUsage} status="active" />
            </div>
          </Col>
          <Col span={12}>
            <div className="status-item">
              <span>内存使用率</span>
              <Progress
                percent={(stats.memoryUsage / 8192) * 100}
                status="active"
              />
            </div>
          </Col>
        </Row>
      </Card>

      <Card title="快捷操作" style={{ marginTop: 16 }}>
        <Space>
          <Button
            type="primary"
            icon={<PlayCircleOutlined />}
            onClick={handleStartAll}
            loading={loading}
          >
            启动所有脚本
          </Button>
          <Button
            icon={<PauseCircleOutlined />}
            onClick={() => {}}
            loading={loading}
          >
            暂停所有脚本
          </Button>
          <Button
            danger
            icon={<StopOutlined />}
            onClick={handleStopAll}
            loading={loading}
          >
            停止所有脚本
          </Button>
        </Space>
      </Card>
    </div>
  );
}
