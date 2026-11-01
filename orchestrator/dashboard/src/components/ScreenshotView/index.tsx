import React, { useEffect, useRef, useState } from 'react';
import { Card, Space, Switch, Button, Typography, Image, Empty, Result } from 'antd';
import { ReloadOutlined } from '@ant-design/icons';
import screenshotService from '@/services/screenshot';
import wsService from '@/services/websocket';

const { Text } = Typography;

interface ScreenshotViewProps {
  width?: number | string;
  height?: number | string;
}

const ScreenshotView: React.FC<ScreenshotViewProps> = ({ width = '100%', height = 400 }) => {
  const [imageUrl, setImageUrl] = useState<string>('');
  const [dimensions, setDimensions] = useState<{ width: number; height: number } | null>(null);
  const [timestamp, setTimestamp] = useState<number>(0);
  const [enabled, setEnabled] = useState<boolean>(true);
  const [loading, setLoading] = useState<boolean>(false);
  const [wsConnected, setWsConnected] = useState<boolean>(() => wsService.isConnected());
  const updateTimeoutRef = useRef<NodeJS.Timeout>();

  useEffect(() => {
    if (!enabled) return;
    const unsubscribe = screenshotService.subscribe((data) => {
      if (updateTimeoutRef.current) {
        clearTimeout(updateTimeoutRef.current);
      }
      updateTimeoutRef.current = setTimeout(() => {
        setImageUrl(data.image);
        setDimensions({ width: data.width, height: data.height });
        setTimestamp(data.timestamp);
        setLoading(false);
      }, 50);
    });
    return () => {
      unsubscribe();
      if (updateTimeoutRef.current) {
        clearTimeout(updateTimeoutRef.current);
      }
    };
  }, [enabled]);

  useEffect(() => {
    const unsubscribe = wsService.onConnectionState(setWsConnected);
    return () => {
      unsubscribe();
    };
  }, []);

  const handleToggle = (checked: boolean) => {
    setEnabled(checked);
    if (!checked) {
      setImageUrl('');
      setDimensions(null);
    }
  };

  const handleRefresh = () => {
    setLoading(true);
  };

  const formatTime = (ts: number) => {
    if (!ts) return '-';
    return new Date(ts).toLocaleTimeString();
  };

  return (
    <Card
      title="游戏画面"
      extra={
        <Space>
          <Text type="secondary">{dimensions && `${dimensions.width}x${dimensions.height}`}</Text>
          <Text type="secondary">更新于: {formatTime(timestamp)}</Text>
          <Switch checked={enabled} onChange={handleToggle} size="small" />
          <Button
            icon={<ReloadOutlined />}
            size="small"
            onClick={handleRefresh}
            loading={loading}
            disabled={!enabled}
            aria-label="刷新画面"
          />
        </Space>
      }
      bodyStyle={{ padding: 0 }}
    >
      <div
        style={{
          width: '100%',
          height,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: '#000',
        }}
      >
        {enabled && imageUrl ? (
          <img
            src={imageUrl}
            alt="Screenshot"
            style={{ maxWidth: '100%', maxHeight: '100%', objectFit: 'contain' }}
            onLoad={() => setLoading(false)}
          />
        ) : enabled ? (
          wsConnected ? (
            <Empty
              image={Empty.PRESENTED_IMAGE_SIMPLE}
              description={
                <span style={{ color: 'rgba(255, 255, 255, 0.65)' }}>
                  连接 Agent 后此处将实时显示游戏画面
                </span>
              }
            />
          ) : (
            <Result
              status="error"
              title={<span style={{ color: '#ffffff' }}>画面流未连接</span>}
              subTitle={
                <span style={{ color: 'rgba(255, 255, 255, 0.65)' }}>
                  无法接收游戏画面，请检查与 Agent 的连接后重试
                </span>
              }
            />
          )
        ) : (
          <Text type="secondary">预览已关闭</Text>
        )}
      </div>
    </Card>
  );
};

export default ScreenshotView;
