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

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

const getAuthToken = () => localStorage.getItem('wingman_token') || '';

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
    fetchStats();
    const interval = setInterval(fetchStats, 5000);
    return () => clearInterval(interval);
  }, []);

  const fetchStats = async () => {
    try {
      const response = await fetch('/api/status', {
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
        },
      });

      if (response.status === 401) {
        localStorage.removeItem('wingman_token');
        window.location.href = '/login';
        return;
      }

      const data: ApiResponse<DashboardStats> = await response.json();
      if (data.success && data.data) {
        setStats(data.data);
      }
    } catch (error) {
      console.error('Failed to fetch stats:', error);
    }
  };

  const handleStartAll = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/scripts', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
          'Content-Type': 'application/json',
        },
      });
      await fetchStats();
    } catch (error) {
      console.error('Failed to start scripts:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleStopAll = async () => {
    setLoading(true);
    try {
      const response = await fetch('/api/scripts', {
        method: 'DELETE',
        headers: {
          'Authorization': `Bearer ${getAuthToken()}`,
        },
      });
      await fetchStats();
    } catch (error) {
      console.error('Failed to stop scripts:', error);
    } finally {
      setLoading(false);
    }
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
