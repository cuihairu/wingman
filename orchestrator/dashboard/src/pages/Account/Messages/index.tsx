import React, { useEffect, useState } from 'react';
import { Badge, Button, Card, List, Space, Tag, Typography, message } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import {
  listMessages,
  markAllMessagesRead,
  markMessageRead,
  type MessageItem,
} from '@/services/api/messages';

const { Text } = Typography;

export default function AccountMessagesPage() {
  const [items, setItems] = useState<MessageItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState<'all' | 'unread'>('all');

  const load = async (nextStatus = status) => {
    setLoading(true);
    try {
      const response = await listMessages({ status: nextStatus, pageSize: 50 });
      setItems(response.items || []);
    } catch (error: any) {
      message.error(error?.message || '加载消息失败');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  const markRead = async (id?: string) => {
    if (!id) return;
    try {
      await markMessageRead(id);
      message.success('已标记为已读');
      await load();
    } catch (error: any) {
      message.error(error?.message || '操作失败');
    }
  };

  const markAllRead = async () => {
    try {
      await markAllMessagesRead();
      message.success('全部消息已标记为已读');
      await load();
    } catch (error: any) {
      message.error(error?.message || '操作失败');
    }
  };

  return (
    <PageContainer>
      <Card
        loading={loading}
        title="消息中心"
        extra={
          <Space>
            <Button
              type={status === 'all' ? 'primary' : 'default'}
              onClick={() => {
                setStatus('all');
                load('all');
              }}
            >
              全部
            </Button>
            <Button
              type={status === 'unread' ? 'primary' : 'default'}
              onClick={() => {
                setStatus('unread');
                load('unread');
              }}
            >
              未读
            </Button>
            <Button onClick={markAllRead}>全部标记已读</Button>
            <Button onClick={() => load()}>刷新</Button>
          </Space>
        }
      >
        <List
          dataSource={items}
          locale={{ emptyText: status === 'unread' ? '暂无未读消息' : '暂无消息' }}
          renderItem={(item) => (
            <List.Item
              actions={[
                item.status !== 'read' ? (
                  <Button key="read" type="link" onClick={() => markRead(item.id)}>
                    标记已读
                  </Button>
                ) : null,
              ].filter(Boolean)}
            >
              <List.Item.Meta
                title={
                  <Space wrap>
                    <Badge status={item.status === 'read' ? 'default' : 'processing'} />
                    <Text strong>{item.title || '系统通知'}</Text>
                    {item.category ? <Tag>{item.category}</Tag> : null}
                    {item.source ? <Tag color="blue">{item.source}</Tag> : null}
                  </Space>
                }
                description={
                  <Space direction="vertical" size={4}>
                    <Text>{item.content || '-'}</Text>
                    <Text type="secondary">
                      {item.createdAt ? new Date(item.createdAt).toLocaleString() : ''}
                    </Text>
                  </Space>
                }
              />
            </List.Item>
          )}
        />
      </Card>
    </PageContainer>
  );
}
