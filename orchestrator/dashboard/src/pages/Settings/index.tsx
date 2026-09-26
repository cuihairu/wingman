import React, { useEffect, useMemo, useState } from 'react';
import { Button, Card, Form, Input, InputNumber, Select, Space, Tag, App, Spin } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { useIntl, useModel } from '@umijs/max';
import { fetchJSON } from '@/services/core/http';

interface ServerSettings {
  serverPort?: string | number;
  logLevel?: string;
  maxScripts?: string | number;
  [key: string]: any;
}

export default function SettingsPage() {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const { message } = App.useApp();
  const initialState = useModel('@@initialState');
  const accessTokens = (initialState?.initialState?.currentUser as any)?.access || '';
  const canAdmin = useMemo(() => {
    const set = new Set(accessTokens.split(',').map((t: string) => t.trim().toLowerCase()));
    return set.has('*') || set.has('admin');
  }, [accessTokens]);

  const [settings, setSettings] = useState<ServerSettings>({});
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [form] = Form.useForm();

  const load = async () => {
    setLoading(true);
    try {
      const resp = await fetchJSON<{ success: boolean; data: ServerSettings }>('/api/settings');
      if (resp?.success) {
        setSettings(resp.data || {});
        form.setFieldsValue({
          logLevel: resp.data?.logLevel || 'info',
          maxScripts: Number(resp.data?.maxScripts ?? 10),
          serverPort: resp.data?.serverPort ?? '',
        });
      }
    } catch (err: any) {
      message.error(err?.message || formatMessage('pages.settings.loadFailed'));
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  const onSave = async () => {
    try {
      const values = await form.validateFields();
      setSaving(true);
      await fetchJSON('/api/settings', {
        method: 'PUT',
        body: JSON.stringify({
          logLevel: values.logLevel,
          maxScripts: String(values.maxScripts),
        }),
      });
      message.success(formatMessage('pages.settings.saved'));
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || formatMessage('pages.settings.saveFailed'));
    } finally {
      setSaving(false);
    }
  };

  return (
    <PageContainer>
      <Card
        title={formatMessage('pages.settings.title')}
        extra={!canAdmin && <Tag>{formatMessage('pages.settings.readOnlyTag')}</Tag>}
      >
        <Spin spinning={loading}>
          <Form form={form} layout="vertical" style={{ maxWidth: 480 }} disabled={!canAdmin}>
            <Form.Item label={formatMessage('pages.settings.serverPort')} name="serverPort">
              <Input disabled placeholder="9527" />
            </Form.Item>
            <Form.Item
              label={formatMessage('pages.settings.logLevel')}
              name="logLevel"
              rules={[
                { required: true, message: formatMessage('pages.settings.logLevelRequired') },
              ]}
            >
              <Select
                options={[
                  { label: 'Debug', value: 'debug' },
                  { label: 'Info', value: 'info' },
                  { label: 'Warn', value: 'warn' },
                  { label: 'Error', value: 'error' },
                ]}
              />
            </Form.Item>
            <Form.Item
              label={formatMessage('pages.settings.maxScripts')}
              name="maxScripts"
              rules={[
                { required: true, message: formatMessage('pages.settings.maxScriptsRequired') },
              ]}
            >
              <InputNumber min={1} max={100} style={{ width: '100%' }} />
            </Form.Item>
            <Form.Item>
              <Space>
                <Button type="primary" loading={saving} onClick={onSave} disabled={!canAdmin}>
                  {formatMessage('pages.common.save')}
                </Button>
                <Button onClick={load}>{formatMessage('pages.common.refresh')}</Button>
              </Space>
            </Form.Item>
          </Form>

          {Object.keys(settings).length > 0 && (
            <Card
              type="inner"
              title={formatMessage('pages.settings.rawSettings')}
              size="small"
              style={{ marginTop: 16 }}
            >
              <pre style={{ margin: 0, fontSize: 12, maxHeight: 240, overflow: 'auto' }}>
                {JSON.stringify(settings, null, 2)}
              </pre>
            </Card>
          )}
        </Spin>
      </Card>

      <Card title={formatMessage('pages.settings.debugTitle')} style={{ marginTop: 16 }}>
        <p style={{ color: '#666' }}>
          {formatMessage('pages.settings.debugDescriptionBefore')}{' '}
          <code>GET /api/debugger/info</code>
          {formatMessage('pages.settings.debugDescriptionAfter')}
        </p>
      </Card>
    </PageContainer>
  );
}
