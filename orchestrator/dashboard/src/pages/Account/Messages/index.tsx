import { useIntl } from '@umijs/max';
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
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [items, setItems] = useState<MessageItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState<'all' | 'unread'>('all');

  const load = async (nextStatus = status) => {
    setLoading(true);
    try {
      const response = await listMessages({ status: nextStatus, pageSize: 50 });
      setItems(response.items || []);
    } catch (error: any) {
      message.error(error?.message || formatMessage('pages.accountMessages.loadFailed'));
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
      message.success(formatMessage('pages.account.messages.marked.read'));
      await load();
    } catch (error: any) {
      message.error(error?.message || formatMessage('pages.accountMessages.operationFailed'));
    }
  };

  const markAllRead = async () => {
    try {
      await markAllMessagesRead();
      message.success(formatMessage('pages.accountMessages.allMarkedRead'));
      await load();
    } catch (error: any) {
      message.error(error?.message || formatMessage('pages.accountMessages.operationFailed'));
    }
  };

  return (
    <PageContainer>
      <Card
        loading={loading}
        title={formatMessage('pages.accountMessages.title')}
        extra={
          <Space>
            <Button
              type={status === 'all' ? 'primary' : 'default'}
              onClick={() => {
                setStatus('all');
                load('all');
              }}
            >
              {formatMessage('pages.account.messages.all.button')}
            </Button>
            <Button
              type={status === 'unread' ? 'primary' : 'default'}
              onClick={() => {
                setStatus('unread');
                load('unread');
              }}
            >
              {formatMessage('pages.account.messages.unread.button')}
            </Button>
            <Button onClick={markAllRead}>{formatMessage('pages.account.messages.mark.all.read')}</Button>
            <Button onClick={() => load()}>{formatMessage('pages.common.refresh')}</Button>
          </Space>
        }
      >
        <List
          dataSource={items}
          locale={{
            emptyText:
              status === 'unread'
                ? formatMessage('pages.accountMessages.emptyUnread')
                : formatMessage('pages.accountMessages.empty'),
          }}
          renderItem={(item) => (
            <List.Item
              actions={[
                item.status !== 'read' ? (
                  <Button key="read" type="link" onClick={() => markRead(item.id)}>
                    {formatMessage('pages.account.messages.mark.read')}
                  </Button>
                ) : null,
              ].filter(Boolean)}
            >
              <List.Item.Meta
                title={
                  <Space wrap>
                    <Badge status={item.status === 'read' ? 'default' : 'processing'} />
                    <Text strong>{item.title || formatMessage('pages.accountMessages.systemNotice')}</Text>
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
